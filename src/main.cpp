#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <EthernetWebServer_STM32.h>
#include <OneWire.h>

OneWire  ds(PB5); 

uint8_t devices[5][8];
uint8_t counDevice;
bool conversion = true;

#define ETHERNET_NAME "W5x00 Ethernet Shield"
#define DEVICE_NAME   "STM32F1"

bool new_state = true;
const char* user = "admin";
const char* pass = "admin";

const char *will_msg = "Offline";
const char *online_msg = "Online";
const char PREFIX_TOPIC[] = "STM/";
const char TOPIC_TEMP[] = "/temp";
const char TOPIC_SPEED[] = "/speed";
const char TOPIC_EFFECT[] = "/effect";
const char TOPIC_BRIGHTNESS[] = "/brightness";
const char TOPIC_COLOR[] = "/color";
const char TOPIC_STATE[] = "/state";
const char TOPIC_LWT[] = "/LWT";

uint16_t reset_counter = 0;
uint32_t last_time = 0;

char chip_id[9];
uint32_t id = 0x4FC8A3B7;
uint16_t count = sprintf(chip_id, "%04X",(uint32_t)(id));
const size_t chip_id_size = sizeof(chip_id);
char prefix_topic[chip_id_size + sizeof(PREFIX_TOPIC) ];
const size_t prefix_topic_size = sizeof(prefix_topic);

char topic_temp[prefix_topic_size + sizeof(TOPIC_TEMP)];

char topic_speed[prefix_topic_size + sizeof(TOPIC_SPEED)];
char topic_effect[prefix_topic_size + sizeof(TOPIC_EFFECT)];
char topic_brightness[prefix_topic_size + sizeof(TOPIC_BRIGHTNESS)];
char topic_color[prefix_topic_size + sizeof(TOPIC_COLOR)];
char topic_state[prefix_topic_size + sizeof(TOPIC_STATE)];
char topic_lwt[prefix_topic_size + sizeof(TOPIC_BRIGHTNESS)];

void generateMqttTopics() {
  strcpy(prefix_topic, PREFIX_TOPIC);
  strcat(prefix_topic, chip_id);

  strcpy(topic_temp, prefix_topic);
  strcat(topic_temp, TOPIC_TEMP);

  strcpy(topic_speed, prefix_topic);
  strcat(topic_speed, TOPIC_SPEED);

  strcpy(topic_effect, prefix_topic);
  strcat(topic_effect, TOPIC_EFFECT);

  strcpy(topic_brightness, prefix_topic);
  strcat(topic_brightness, TOPIC_BRIGHTNESS);

  strcpy(topic_lwt, prefix_topic);
  strcat(topic_lwt, TOPIC_LWT);

  strcpy(topic_color, prefix_topic);
  strcat(topic_color, TOPIC_COLOR);

  strcpy(topic_state, prefix_topic);
  strcat(topic_state, TOPIC_STATE);
}


EthernetWebServer server(80);

byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

EthernetClient ethClient;
PubSubClient client(ethClient);

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient", "kcmuormw", "Cc0gsGn9E2ag", topic_lwt, 0, false, will_msg)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_state,"connect");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void getAddr() {  
    uint8_t addr[8];
    uint8_t i = 0;
    while (ds.search(addr) && OneWire::crc8(addr, 7) == addr[7] && i < 5) {
      Serial.print("Device : ");
      for (uint8_t y = 0; y < 8; y++) {
        Serial.print(addr[y], HEX);
        devices[i][y] = addr[y];
      }
      Serial.println();
      i++;
    }
    counDevice = i;
    Serial.println(i);
}

void conv() {
  ds.reset();
  ds.write(0xCC, 1);
  ds.write(0x44, 1); 
}

void getTemp() {
  for (uint8_t i = 0; i < counDevice; i++) {
    byte data[12];
    ds.reset();
    ds.select(devices[i]); 
    ds.write(0xBE);
    String romcode = topic_temp;
    romcode = romcode + (i+1);

    for (uint8_t x = 0; x < 9; x++) {
      data[x] = ds.read();      
      // Serial.print(data[x], HEX);
      // Serial.print(" ");
    }
    // Serial.print(" CRC=");
    // Serial.println(OneWire::crc8(data, 8), HEX);
    if (OneWire::crc8(data, 8) == data[8]) {
      float temperature =  ((data[1] << 8) | data[0]) * 0.0625;
      Serial.print("Temperature : ");
      Serial.println(temperature);
      char   dest[10];
      dtostrf(temperature, 6, 2, dest);
      if (client.connected()) client.publish((char*) romcode.c_str(), dest);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Start..."));
  generateMqttTopics();
  delay(3000);
  client.setServer("m20.cloudmqtt.com", 12398);
  client.setCallback(callback);

  server.on("/inline", []() {
    server.send(200, "text/plain", "This works as well");
  });

  server.on("/", []() {
    if (!server.authenticate(user, pass)) {
      return server.requestAuthentication();
    }
    server.send(200, "text/plain", "Login OK");
  });

  server.onNotFound(handleNotFound);
  server.begin();
  getAddr();
}

void loop() {
  uint32_t now = millis();

  if (now - last_time > 4000 && conversion) {
    conv();
    conversion = false;
  }
  if (now - last_time > 10000) {
    last_time = now;
    getTemp();
    conv();
    conversion = true;
  }

  if (Ethernet.linkStatus() == LinkON) {
    if (new_state) {
      Ethernet.begin(mac);
      new_state = false;
      delay(1500);
      Serial.print(F("IP : "));
      Serial.println(Ethernet.localIP());
    }
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    server.handleClient();
  } else {
    new_state = true;
  }
}