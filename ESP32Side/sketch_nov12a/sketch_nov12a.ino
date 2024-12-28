#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h> 
#include <Adafruit_ADXL345_U.h>

// WiFi credentials
const char* ssid = "pencaricuanmobile";
const char* password = "123456789";

// MQTT Broker details (RabbitMQ)
const char* mqtt_server = "rmq2.pptik.id";
const int mqtt_port = 1883;
const char* mqtt_user = "/pm_module:pm_modue";
const char* mqtt_password = "hl6GjO5LlRuQT1n";

// MQTT topics for each axis
const char* topic = "sensor/accel/xyz";

// Virtual Host (optional, depending on your RabbitMQ setup)

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified();

void setup_sensor() {
  if (!accel.begin()) {
    Serial.println("No ADXL345 sensor detected.");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
}

void publish_sensordata() {
  sensors_event_t event; 
  accel.getEvent(&event);
    
  // Create a JSON payload for more structured data
  char payload[100];
  snprintf(payload, sizeof(payload), 
            "{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}", 
            event.acceleration.x, 
            event.acceleration.y, 
            event.acceleration.z);
  
  // Publish the JSON payload
  client.publish(topic, payload);
  
  // Debug output
  Serial.print("Published: ");
  Serial.println(payload);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
        
    // Use vhost if needed (for RabbitMQ)
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_sensor();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  randomSeed(micros());
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  publish_sensordata();
  delay(1000);  // Update every 1000ms for smoother gauge movement
}
