#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Network Configuration
const char* ssid = "pencaricuanmobile";
const char* password = "123456789";

// MQTT Configuration
const char* mqtt_server = "rmq2.pptik.id";
const int mqtt_port = 1883;
const char* mqtt_user = "/pm_module:pm_modue";
const char* mqtt_password = "hl6GjO5LlRuQT1n";

// MQTT Topics (same as original)
const char* power_topic = "sensor/power";
const char* temp_topic = "sensor/temperature";
const char* accel_topic = "sensor/accel";

// Test data structure
struct TestData {
    // Power measurements
    float voltage = 220.5;
    float current = 1.2;
    float frequency = 50.0;
    
    // Temperature
    float temperature = 25.7;
    
    // Acceleration
    float accelX = 0.1;
    float accelY = -0.2;
    float accelZ = 9.8;
} dummyData;

WiFiClient espClient;
PubSubClient client(espClient);

// WiFi connection function
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to WiFi network: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
        ESP.restart();
    }
}

// MQTT reconnection function
bool reconnect() {
    int attempts = 0;
    
    while (!client.connected() && attempts < 3) {
        Serial.print("Attempting MQTT connection...");
        
        String clientId = "ESP32TestClient-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("connected");
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
            attempts++;
        }
    }
    return false;
}

// Function to simulate changing sensor values
void updateDummyData() {
    // Add small random variations to simulate real sensor data
    dummyData.voltage += random(-100, 100) / 100.0;
    dummyData.current += random(-10, 10) / 100.0;
    dummyData.frequency += random(-10, 10) / 100.0;
    dummyData.temperature += random(-10, 10) / 10.0;
    dummyData.accelX += random(-10, 10) / 100.0;
    dummyData.accelY += random(-10, 10) / 100.0;
    dummyData.accelZ += random(-10, 10) / 100.0;
}

// Function to publish dummy data
void publishData() {
    if (!client.connected()) {
        if (!reconnect()) {
            Serial.println("MQTT connection failed! Skipping publish...");
            return;
        }
    }
    
    StaticJsonDocument<200> doc;
    char payload[200];
    
    // Test 1: Publish power data
    doc.clear();
    doc["voltage"] = dummyData.voltage;
    doc["current"] = dummyData.current;
    doc["frequency"] = dummyData.frequency;
    
    serializeJson(doc, payload);
    
    Serial.print("Publishing power data: ");
    Serial.println(payload);
    if (client.publish(power_topic, payload)) {
        Serial.println("Power data published successfully");
    } else {
        Serial.println("Power data publish failed!");
    }
    
    // Test 2: Publish temperature data
    doc.clear();
    doc["celsius"] = dummyData.temperature;
    
    serializeJson(doc, payload);
    
    Serial.print("Publishing temperature data: ");
    Serial.println(payload);
    if (client.publish(temp_topic, payload)) {
        Serial.println("Temperature data published successfully");
    } else {
        Serial.println("Temperature data publish failed!");
    }
    
    // Test 3: Publish acceleration data
    doc.clear();
    doc["x"] = dummyData.accelX;
    doc["y"] = dummyData.accelY;
    doc["z"] = dummyData.accelZ;
    
    serializeJson(doc, payload);
    
    Serial.print("Publishing acceleration data: ");
    Serial.println(payload);
    if (client.publish(accel_topic, payload)) {
        Serial.println("Acceleration data published successfully");
    } else {
        Serial.println("Acceleration data publish failed!");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\nStarting MQTT Publishing Test");
    Serial.println("-----------------------------");
    
    // Connect to WiFi
    setup_wifi();
    
    // Configure MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(512);
    
    // Initial MQTT connection
    if (reconnect()) {
        Serial.println("Initial MQTT connection successful");
    } else {
        Serial.println("Initial MQTT connection failed!");
    }
}

void loop() {
    static unsigned long lastPublish = 0;
    const unsigned long publishInterval = 5000; // Publish every 5 seconds
    
    // Maintain MQTT connection
    client.loop();
    
    // Check if it's time to publish
    if (millis() - lastPublish >= publishInterval) {
        Serial.println("\n=== Publishing Test Data ===");
        
        // Update dummy data
        updateDummyData();
        
        // Publish all data
        publishData();
        
        lastPublish = millis();
        
        Serial.println("=== Test Complete ===\n");
    }
}