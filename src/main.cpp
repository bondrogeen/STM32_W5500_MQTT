#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <EthernetWebServer_STM32.h>

#define ETHERNET_NAME     "W5x00 Ethernet Shield"
#define DEVICE_NAME  "STM32F1"

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
    if (client.connect("arduinoClient", "kcmuormw", "Cc0gsGn9E2ag")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic","hello world");
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

void setup() {
  Serial.begin(115200);
  Serial.print(F("Start..."));
  client.setServer("m20.cloudmqtt.com", 12398);
  client.setCallback(callback);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  } 
  // Allow the hardware to sort itself out
  delay(1500);

  server.on("/inline", []() {
    server.send(200, "text/plain", "This works as well");
  });

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print(F("HTTP EthernetWebServer is @ IP : "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  if (Ethernet.linkStatus() == LinkON) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    server.handleClient();
  } else {

  }
}