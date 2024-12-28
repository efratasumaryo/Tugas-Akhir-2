#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <PZEM004Tv30.h>
#include <Adafruit_MAX31855.h>

// WiFi and MQTT settings
const char* ssid = "pencaricuanmobile";
const char* password = "123456789";
const char* mqtt_server = "rmq2.pptik.id";
const int mqtt_port = 1883;
const char* mqtt_user = "/pm_module:pm_modue";
const char* mqtt_password = "hl6GjO5LlRuQT1n";

const char* power_topic = "sensor/power";
const char* temp_topic = "sensor/temperature";

// Sensor instances
PZEM004Tv30 pzem(Serial2, 16, 17);
Adafruit_MAX31855 thermocouple(15, 14, 12);

WiFiClient espClient;
PubSubClient client(espClient);

// Shared variables
float voltage = 0;
float current = 0;
float frequency = 0;
volatile double celsius = 0;
volatile bool newPowerData = false;
volatile bool newTempData = false;

// Task handles
TaskHandle_t Task1;
TaskHandle_t Task2;

// Timing control variables
const unsigned long SAMPLE_INTERVAL = 100; // 100ms = 10Hz
unsigned long lastPowerSample = 0;
unsigned long lastTempSample = 0;

void Task1code(void* parameter) {
    for(;;) {
        unsigned long currentMillis = millis();
        
        // Sample power sensor at 10Hz
        if (currentMillis - lastPowerSample >= SAMPLE_INTERVAL) {
            float v = pzem.voltage();
            float c = pzem.current();
            float f = pzem.frequency(); 
            
            if (!isnan(v) && !isnan(c) && !isnan(f)) {
                voltage = v;
                current = c;
                frequency = f;
                newPowerData = true;
            }
            lastPowerSample = currentMillis;
        }
        
        // Sample temperature sensor at 10Hz
        if (currentMillis - lastTempSample >= SAMPLE_INTERVAL) {
            double temp = thermocouple.readCelsius();
            if (!isnan(temp)) {
                celsius = temp;
                newTempData = true;
            }
            lastTempSample = currentMillis;
        }
        
        // Short delay to prevent task watchdog timeouts
        vTaskDelay(1);
    }
}

void Task2code(void* parameter) {
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    
    for(;;) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        if (newPowerData) {
            char payload[100];
            snprintf(payload, sizeof(payload),
                    "{\"voltage\":%.2f,\"current\":%.2f,\"frequency\":%.1f}",
                    voltage, current, frequency);
            client.publish(power_topic, payload);
            newPowerData = false;
        }

        if (newTempData) {
            char payload[50];
            snprintf(payload, sizeof(payload), "{\"celsius\":%.2f}", celsius);
            client.publish(temp_topic, payload);
            newTempData = false;
        }
        
        // Shorter delay for more responsive MQTT publishing
        vTaskDelay(10);
    }
}

void setup_wifi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

void reconnect() {
    while (!client.connected()) {
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            break;
        }
        delay(5000);
    }
}

void setup() {
    Serial.begin(115200);
    
    if (!thermocouple.begin()) {
        while (1) delay(10);
    }

    // Increase priority of sensor task
    xTaskCreatePinnedToCore(Task1code, "SensorTask", 10000, NULL, 2, &Task1, 0);
    delay(500);
    xTaskCreatePinnedToCore(Task2code, "MQTTTask", 10000, NULL, 1, &Task2, 1);
}

void loop() {
    // Empty, tasks handle everything
}