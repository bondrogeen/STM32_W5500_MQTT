#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <EthernetWebServer_STM32.h>
#include <OneWire.h>
#include <Wire.h>
#include <BME280_t.h>

#define ASCII_ESC 27
#define MYALTITUDE  150.50
#define ETHERNET_NAME "W5x00 Ethernet Shield"
#define DEVICE_NAME   "STM32F1"

BME280<> BMESensor; 
OneWire  ds(PB5); 
EthernetWebServer server(80);
EthernetClient ethClient;
PubSubClient client(ethClient);

char bufout[10];
byte mac[6];

uint8_t devices[5][8];
uint8_t counDevice;
bool conversion = true;

bool new_state = true;
const char* user = "admin";
const char* pass = "admin";

const char *mqtt_server = "m20.cloudmqtt.com";
const uint16_t mqtt_port = 12398;
const char *mqtt_user = "kcmuormw";
const char *mqtt_pass = "Cc0gsGn9E2ag";

const char *offline_msg = "Offline";
const char *online_msg = "Online";
const char PREFIX_TOPIC[] = "Device/";
const char TOPIC_SENSORS[] = "/sensors/";
const char TOPIC_RELAY_1[] = "/relay/1";
const char TOPIC_RELAY_2[] = "/relay/2";

const char TOPIC_EFFECT[] = "/effect";
const char TOPIC_BRIGHTNESS[] = "/brightness";
const char TOPIC_COLOR[] = "/color";
const char TOPIC_STATE[] = "/state";
const char TOPIC_LWT[] = "/LWT";

uint16_t reset_counter = 0;
uint32_t last_time = 0;

static unsigned long *UID = (unsigned long *)0x1FFFF7E8;
char chip_id[9];
uint16_t count = sprintf(chip_id, "%08X",(uint32_t)(UID[0]));
const size_t chip_id_size = sizeof(chip_id);

char prefix_topic[chip_id_size + sizeof(PREFIX_TOPIC)];
const size_t prefix_topic_size = sizeof(prefix_topic);

char topic_sensors[prefix_topic_size + sizeof(TOPIC_SENSORS)];
char topic_relay1[prefix_topic_size + sizeof(TOPIC_RELAY_1)];
char topic_relay2[prefix_topic_size + sizeof(TOPIC_RELAY_2)];

char topic_effect[prefix_topic_size + sizeof(TOPIC_EFFECT)];
char topic_brightness[prefix_topic_size + sizeof(TOPIC_BRIGHTNESS)];
char topic_color[prefix_topic_size + sizeof(TOPIC_COLOR)];
char topic_state[prefix_topic_size + sizeof(TOPIC_STATE)];
char topic_lwt[prefix_topic_size + sizeof(TOPIC_BRIGHTNESS)];



void generateMqttTopics() {
  strcpy(prefix_topic, PREFIX_TOPIC);
  strcat(prefix_topic, chip_id);

  strcpy(topic_sensors, prefix_topic);
  strcat(topic_sensors, TOPIC_SENSORS);

  strcpy(topic_relay1, prefix_topic);
  strcat(topic_relay1, TOPIC_RELAY_1);

  strcpy(topic_relay2, prefix_topic);
  strcat(topic_relay2, TOPIC_RELAY_2);

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

void getUniId(){
  mac[0] = (UID[0] >> 0)  & 0xFF;
  mac[1] = (UID[0] >> 8)  & 0xFF;
  mac[2] = (UID[1] >> 16) & 0xFF;
  mac[3] = (UID[1] >> 0)  & 0xFF;
  mac[4] = (UID[2] >> 8)  & 0xFF;
  mac[5] = (UID[2] >> 16) & 0xFF;
}

bool isTrue(char *value) {
  return (!strcmp(value, "on") || !strcmp(value, "1") || !strcmp(value, "true"));
}

void process(char *topic, char *message) {
  if (!strcmp(topic, topic_relay1)) {
    digitalWrite(PC13, isTrue(message) ? LOW : HIGH);
    Serial.println("111111111");
  }

}

void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  for (uint8_t i=0; i<length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message);
  process(topic, message);
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

const String postForms = 
"<html>\
<head>\
<title>EthernetWebServer POST handling</title>\
<style>\
</style>\
</head>\
<body>\
<h1>POST plain text to /postplain/</h1><br>\
<form method=\"post\" enctype=\"text/plain\" action=\"/postplain/\">\
<input type=\"text\" name=\'{\"hello\": \"world\", \"trash\": \"\' value=\'\"}\'><br>\
<input type=\"submit\" value=\"Submit\">\
</form>\
<h1>POST form data to /postform/</h1><br>\
<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">\
<input type=\"text\" name=\"hello\" value=\"world\"><br>\
<input type=\"submit\" value=\"Submit\">\
</form>\
</body>\
</html>";

void getAddr() {  
    uint8_t addr[8];
    uint8_t i = 0;
    while (ds.search(addr) && OneWire::crc8(addr, 7) == addr[7] && i < 5) {
      // Serial.print("Device : ");
      for (uint8_t y = 0; y < 8; y++) {
        // Serial.print(addr[y], HEX);
        devices[i][y] = addr[y];
      }
      // Serial.println();
      i++;
    }
    counDevice = i;
    // Serial.println(i);
}

void conv() {
  ds.reset();
  ds.write(0xCC, 1);
  ds.write(0x44, 1);
}

void sendTopic(const char* root, const char* topic, int8_t num, float message, bool isNull=true){
  if (client.connected()) {
    char   dest[10];
    if (isNull) dtostrf(message, 6, 2, dest);
    String romcode = root;
    romcode = romcode + topic;
    if (num != -1) romcode = romcode + num;
    client.publish((char*) romcode.c_str(), isNull ? dest : "");
  }
}

void getTemp() {
  for (uint8_t i = 0; i < counDevice; i++) {
    byte data[12];
    ds.reset();
    ds.select(devices[i]);
    ds.write(0xBE);
    for (uint8_t x = 0; x < 9; x++) data[x] = ds.read();
    if (OneWire::crc8(data, 8) == data[8]) sendTopic(topic_sensors, "temperature/", i+1, ((data[1] << 8) | data[0]) * 0.0625);
  }
}

void scan() {
  BMESensor.refresh();
  // sprintf(bufout,"%c[1;0H",ASCII_ESC);
  // Serial.print(bufout);
  sendTopic(topic_sensors, "temperature/0", -1, BMESensor.temperature);
  sendTopic(topic_sensors, "humidity", -1, BMESensor.humidity);
  float relativepressure = BMESensor.seaLevelForAltitude(MYALTITUDE);
  sendTopic(topic_sensors, "pressure", -1, relativepressure  / 100.0F);
  sendTopic(topic_sensors, "altitude", -1, BMESensor.pressureToAltitude(relativepressure));  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(chip_id, mqtt_user, mqtt_pass, topic_lwt, 0, false, offline_msg)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_state, online_msg);
      client.publish(topic_relay1,"");
      client.publish(topic_relay2,"");
      client.subscribe(topic_relay1);
      client.subscribe(topic_relay2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void blink() {
  // state = !state;
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Start..."));
  Wire.begin();
  BMESensor.begin();

  getUniId();
  generateMqttTopics();
  delay(5000);

  pinMode(PC13, OUTPUT);
  pinMode(PB4, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(interruptPin), blink, CHANGE);

  // Serial.println(UID[0]);
  // Serial.println(UID[1]);
  // Serial.println(UID[2]);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  server.on("/inline", []() {
    server.send(200, "text/plain", "This works as well");
  });

  server.on("/", []() {
    if (!server.authenticate(user, pass)) {
      return server.requestAuthentication();
    }
    server.send(200, "text/html", postForms);
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
    scan();
  }

  if (Ethernet.linkStatus() == LinkON) {
    // digitalWrite(PC13, LOW);
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
    // digitalWrite(PC13, HIGH);
    new_state = true;
  }
}