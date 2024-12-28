/*
 * ESP32 Multi-Sensor Monitoring System
 * 
 * This system collects data from multiple sensors and publishes it to an MQTT broker:
 * - PZEM004T power meter (voltage, current, frequency)
 * - MAX31855 thermocouple (temperature)
 * - ADXL345 accelerometer (x, y, z acceleration)
 * 
 * Features:
 * - Multi-core task distribution
 * - 100Hz sampling rate
 * - 10-sample averaging
 * - Unix timestamp for published data
 * - Automatic WiFi and MQTT reconnection
 * - Thread-safe data handling with mutex
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <PZEM004Tv30.h>
#include <Adafruit_MAX31855.h>
#include <Adafruit_ADXL345_U.h>
#include <ArduinoJson.h>
#include <time.h>

// Network Configuration
const char* ssid = "pencaricuanmobile";
const char* password = "123456789";

// MQTT Configuration
const char* mqtt_server = "rmq2.pptik.id";
const int mqtt_port = 1883;
const char* mqtt_user = "/pm_module:pm_modue";
const char* mqtt_password = "hl6GjO5LlRuQT1n";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// MQTT Topics
const char* power_topic = "sensor/power";
const char* temp_topic = "sensor/temperature";
const char* accel_topic = "sensor/accel";

// Sampling Configuration
const int BUFFER_SIZE = 10;         // Number of samples to average
const int SAMPLING_RATE = 100;      // Hz
const int SAMPLING_PERIOD = 10;     // milliseconds (1000/SAMPLING_RATE)

// Buffer structure for sensor data averaging
struct SensorBuffer {
    float data[10];          // Stores last 10 readings
    size_t count = 0;        // Current number of readings
    bool isReady = false;    // Flag indicating buffer is full
};

// Structure for processed (averaged) sensor data
struct ProcessedData {
    float voltage;
    float current;
    float frequency;
    float temperature;
    float accelX;
    float accelY;
    float accelZ;
    bool isNew = false;      // Flag indicating new data is available
};

// Main data structure shared between tasks
struct SharedData {
    // Buffers for raw sensor data
    SensorBuffer voltage;
    SensorBuffer current;
    SensorBuffer frequency;
    SensorBuffer temperature;
    SensorBuffer accelX;
    SensorBuffer accelY;
    SensorBuffer accelZ;
    
    ProcessedData averaged;  // Holds averaged sensor values
    SemaphoreHandle_t mutex; // Mutex for thread-safe access
} sensorData;

// Sensor object initialization
PZEM004Tv30 pzem(Serial2, 16, 17);        // RX, TX pins
Adafruit_MAX31855 thermocouple(15, 14, 12); // CLK, CS, DO pins
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Network clients
WiFiClient espClient;
PubSubClient client(espClient);

// Task handles
TaskHandle_t CollectionTask;
TaskHandle_t AverageTask;
TaskHandle_t PublishTask;

// Function to get current Unix timestamp
unsigned long getTimestamp() {
    time_t now;
    time(&now);
    return (unsigned long)now;
}

// Utility function to calculate average of sensor readings
float calculateAverage(SensorBuffer& buffer) {
    if (buffer.count == 0) return 0;
    float sum = 0;
    for (size_t i = 0; i < buffer.count; i++) {
        sum += buffer.data[i];
    }
    return sum / buffer.count;
}

// WiFi connection function with retry mechanism
void setup_wifi() {
    delay(10);
    Serial.println("\nConnecting to WiFi network: ");
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

// MQTT reconnection function with retry mechanism
void reconnect() {
    int attempts = 0;
    while (!client.connected() && attempts < 3) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
            attempts++;
        }
    }
    
    if (!client.connected()) {
        Serial.println("MQTT connection failed after 3 attempts. Restarting...");
        ESP.restart();
    }
}

// Task 1: Data Collection
void CollectionTask_code(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLING_PERIOD);
    
    sensors_event_t accelEvent;
    
    for(;;) {
        // Read all sensors
        float v = pzem.voltage();
        float c = pzem.current();
        float f = pzem.frequency();
        float t = thermocouple.readCelsius();
        accel.getEvent(&accelEvent);
        
        // Check if readings are valid
        if (!isnan(v) && !isnan(c) && !isnan(f) && !isnan(t)) {
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Store readings if buffer not full
            if (sensorData.voltage.count < BUFFER_SIZE) {
                sensorData.voltage.data[sensorData.voltage.count] = v;
                sensorData.current.data[sensorData.current.count] = c;
                sensorData.frequency.data[sensorData.frequency.count] = f;
                sensorData.temperature.data[sensorData.temperature.count] = t;
                sensorData.accelX.data[sensorData.accelX.count] = accelEvent.acceleration.x;
                sensorData.accelY.data[sensorData.accelY.count] = accelEvent.acceleration.y;
                sensorData.accelZ.data[sensorData.accelZ.count] = accelEvent.acceleration.z;
                
                // Update counters
                sensorData.voltage.count++;
                sensorData.current.count++;
                sensorData.frequency.count++;
                sensorData.temperature.count++;
                sensorData.accelX.count++;
                sensorData.accelY.count++;
                sensorData.accelZ.count++;
                
                // Mark buffers as ready when full
                if (sensorData.voltage.count == BUFFER_SIZE) {
                    sensorData.voltage.isReady = true;
                    sensorData.current.isReady = true;
                    sensorData.frequency.isReady = true;
                    sensorData.temperature.isReady = true;
                    sensorData.accelX.isReady = true;
                    sensorData.accelY.isReady = true;
                    sensorData.accelZ.isReady = true;
                }
            }
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Task 2: Data Averaging
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
            sensorData.averaged.accelX = calculateAverage(sensorData.accelX);
            sensorData.averaged.accelY = calculateAverage(sensorData.accelY);
            sensorData.averaged.accelZ = calculateAverage(sensorData.accelZ);
            
            // Reset buffers
            sensorData.voltage.count = 0;
            sensorData.current.count = 0;
            sensorData.frequency.count = 0;
            sensorData.temperature.count = 0;
            sensorData.accelX.count = 0;
            sensorData.accelY.count = 0;
            sensorData.accelZ.count = 0;
            
            // Reset ready flags
            sensorData.voltage.isReady = false;
            sensorData.current.isReady = false;
            sensorData.frequency.isReady = false;
            sensorData.temperature.isReady = false;
            sensorData.accelX.isReady = false;
            sensorData.accelY.isReady = false;
            sensorData.accelZ.isReady = false;
            
            // Mark new data available
            sensorData.averaged.isNew = true;
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Task 3: MQTT Publishing
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
            // Get timestamp at publish time
            unsigned long publishTimestamp = getTimestamp();
            
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Publish power measurements
            doc.clear();
            doc["voltage"] = sensorData.averaged.voltage;
            doc["current"] = sensorData.averaged.current;
            doc["frequency"] = sensorData.averaged.frequency;
            doc["timestamp"] = publishTimestamp;
            doc["samples"] = BUFFER_SIZE;
            doc["sample_rate"] = SAMPLING_RATE;
            
            char payload[200];
            serializeJson(doc, payload);
            client.publish(power_topic, payload);
            
            // Publish temperature measurements
            doc.clear();
            doc["celsius"] = sensorData.averaged.temperature;
            doc["timestamp"] = publishTimestamp;
            doc["samples"] = BUFFER_SIZE;
            doc["sample_rate"] = SAMPLING_RATE;
            
            serializeJson(doc, payload);
            client.publish(temp_topic, payload);
            
            // Publish acceleration measurements
            doc.clear();
            doc["x"] = sensorData.averaged.accelX;
            doc["y"] = sensorData.averaged.accelY;
            doc["z"] = sensorData.averaged.accelZ;
            doc["timestamp"] = publishTimestamp;
            doc["samples"] = BUFFER_SIZE;
            doc["sample_rate"] = SAMPLING_RATE;
            
            serializeJson(doc, payload);
            client.publish(accel_topic, payload);
            
            // Reset new data flag
            sensorData.averaged.isNew = false;
            
            xSemaphoreGive(sensorData.mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Set CPU frequency
    setCpuFrequencyMhz(160);
    
    // Initialize WiFi
    setup_wifi();
    
    // Initialize and sync time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time sync
    while (getTimestamp() < 1600000000) {
        Serial.println("Waiting for time sync...");
        delay(1000);
    }
    
    // Create mutex for shared data protection
    sensorData.mutex = xSemaphoreCreateMutex();
    
    // Initialize sensors
    if (!thermocouple.begin()) {
        Serial.println("Thermocouple initialization failed!");
        while (1) delay(10);
    }

    if (!accel.begin()) {
        Serial.println("ADXL345 initialization failed!");
        while (1) delay(10);
    }
    
    // Configure accelerometer range
    accel.setRange(ADXL345_RANGE_16_G);
    
    // Create tasks with priorities
    xTaskCreatePinnedToCore(
        CollectionTask_code,    // Task function
        "Collect",             // Task name
        10000,                 // Stack size (bytes)
        NULL,                  // Parameters
        3,                     // Priority
        &CollectionTask,       // Task handle
        0                      // Core ID
    );
    
    xTaskCreatePinnedToCore(
        AverageTask_code,      // Task function
        "Average",             // Task name
        10000,                 // Stack size
        NULL,                  // Parameters
        2,                     // Priority
        &AverageTask,          // Task handle
        0                      // Core ID
    );
    
    xTaskCreatePinnedToCore(
        PublishTask_code,      // Task function
        "Publish",             // Task name
        10000,                 // Stack size
        NULL,                  // Parameters
        1,                     // Priority
        &PublishTask,          // Task handle
        1                      // Core
    );
}

void loop() {
    // Empty - all functionality is handled by tasks
}