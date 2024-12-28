#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <PZEM004Tv30.h>
#include <Adafruit_MAX31855.h>
#include <ArduinoJson.h>

// Previous WiFi and MQTT settings remain same
const char* ssid = "pencaricuanmobile";
const char* password = "123456789";
const char* mqtt_server = "rmq2.pptik.id";
const int mqtt_port = 1883;
const char* mqtt_user = "/pm_module:pm_modue";
const char* mqtt_password = "hl6GjO5LlRuQT1n";

const char* power_topic = "sensor/power";
const char* temp_topic = "sensor/temperature";

// data structures for three-task system
struct SensorBuffer {
    float data[10];
    size_t count = 0;
    bool isReady = false;
};

struct ProcessedData {
    float voltage;
    float current;
    float frequency;
    float temperature;
    bool isNew = false;
};

// Shared data structures with mutex protection
struct SharedData {
    SensorBuffer voltage;
    SensorBuffer current;
    SensorBuffer frequency;
    SensorBuffer temperature;
    ProcessedData averaged;
    SemaphoreHandle_t mutex;
} sensorData;

// Sensor instances
PZEM004Tv30 pzem(Serial2, 16, 17);
Adafruit_MAX31855 thermocouple(15, 14, 12);

WiFiClient espClient;
PubSubClient client(espClient);

// Task handles
TaskHandle_t CollectionTask;
TaskHandle_t AverageTask;
TaskHandle_t PublishTask;

float calculateAverage(SensorBuffer& buffer) {
    if (buffer.count == 0) return 0;
    float sum = 0;
    for (size_t i = 0; i < buffer.count; i++) {
        sum += buffer.data[i];
    }
    return sum / buffer.count;
}

// Task 1: Data Collection
void CollectionTask_code(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz
    
    for(;;) {
        float v = pzem.voltage();
        float c = pzem.current();
        float f = pzem.frequency();
        float t = thermocouple.readCelsius();
        
        if (!isnan(v) && !isnan(c) && !isnan(f) && !isnan(t)) {
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Store new readings if buffer not full
            if (sensorData.voltage.count < 10) {
                sensorData.voltage.data[sensorData.voltage.count] = v;
                sensorData.current.data[sensorData.current.count] = c;
                sensorData.frequency.data[sensorData.frequency.count] = f;
                sensorData.temperature.data[sensorData.temperature.count] = t;
                
                sensorData.voltage.count++;
                sensorData.current.count++;
                sensorData.frequency.count++;
                sensorData.temperature.count++;
                
                // Mark buffers as ready when full
                if (sensorData.voltage.count == 10) {
                    sensorData.voltage.isReady = true;
                    sensorData.current.isReady = true;
                    sensorData.frequency.isReady = true;
                    sensorData.temperature.isReady = true;
                }
            }
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Task 2: Averaging
void AverageTask_code(void* parameter) {
    for(;;) {
        bool readyToAverage = false;
        
        xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
        readyToAverage = sensorData.voltage.isReady;
        xSemaphoreGive(sensorData.mutex);
        
        if (readyToAverage) {
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Calculate averages
            sensorData.averaged.voltage = calculateAverage(sensorData.voltage);
            sensorData.averaged.current = calculateAverage(sensorData.current);
            sensorData.averaged.frequency = calculateAverage(sensorData.frequency);
            sensorData.averaged.temperature = calculateAverage(sensorData.temperature);
            
            // Reset buffers
            sensorData.voltage.count = 0;
            sensorData.current.count = 0;
            sensorData.frequency.count = 0;
            sensorData.temperature.count = 0;
            
            sensorData.voltage.isReady = false;
            sensorData.current.isReady = false;
            sensorData.frequency.isReady = false;
            sensorData.temperature.isReady = false;
            
            // Mark new data available
            sensorData.averaged.isNew = true;
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Task 3: Publishing
void PublishTask_code(void* parameter) {
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(512);
    
    StaticJsonDocument<200> doc;
    
    for(;;) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        bool newData = false;
        
        xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
        newData = sensorData.averaged.isNew;
        xSemaphoreGive(sensorData.mutex);
        
        if (newData) {
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Publish power data
            doc.clear();
            doc["voltage"] = sensorData.averaged.voltage;
            doc["current"] = sensorData.averaged.current;
            doc["frequency"] = sensorData.averaged.frequency;
            
            char payload[200];
            serializeJson(doc, payload);
            client.publish(power_topic, payload);
            
            // Publish temperature data
            doc.clear();
            doc["celsius"] = sensorData.averaged.temperature;
            
            serializeJson(doc, payload);
            client.publish(temp_topic, payload);
            
            sensorData.averaged.isNew = false;
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// WiFi connection function with retry mechanism
void setup_wifi() {
    delay(10);
    Serial.println("\nConnecting to WiFi network: ");
    Serial.println(ssid);
    
    // Set WiFi mode to station (client)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Initial connection attempt
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    // If connection successful
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
        // Reset ESP32 to try again
        ESP.restart();
    }
}

// MQTT reconnection function with retry mechanism
void reconnect() {
    int attempts = 0;
    
    // Try to reconnect 3 times before giving up
    while (!client.connected() && attempts < 3) {
        Serial.print("Attempting MQTT connection...");
        
        // Create a random client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        // Attempt to connect with credentials
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("connected");
            
            // Subscribe to any required topics here
            // client.subscribe("your/topic");
            
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
            attempts++;
        }
    }
    
    // If all attempts failed, restart the ESP32
    if (!client.connected()) {
        Serial.println("MQTT connection failed after 3 attempts. Restarting...");
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(160);
    
    // Create mutex for shared data
    sensorData.mutex = xSemaphoreCreateMutex();
    
    if (!thermocouple.begin()) {
        while (1) delay(10);
    }

    // Create tasks with appropriate priorities and core assignments
    xTaskCreatePinnedToCore(CollectionTask_code, "Collect", 10000, NULL, 3, &CollectionTask, 0);
    xTaskCreatePinnedToCore(AverageTask_code, "Average", 10000, NULL, 2, &AverageTask, 0);
    xTaskCreatePinnedToCore(PublishTask_code, "Publish", 10000, NULL, 1, &PublishTask, 1);
}

void loop() {
    // Empty, tasks handle everything
}