/*
 * ESP32 Multi-Sensor Monitoring System with Simulation
 * 
 * This system simulates and processes data from multiple sensors:
 * - PZEM004T power meter (voltage, current, frequency)
 * - MAX31855 thermocouple (temperature)
 * - ADXL345 accelerometer (x, y, z acceleration)
 * 
 * The system uses FreeRTOS tasks to handle:
 * 1. Data collection/simulation (100Hz sampling)
 * 2. Data averaging (10 samples)
 * 3. MQTT publishing
 */

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

// MQTT Topics
const char* power_topic = "sensor/power";
const char* temp_topic = "sensor/temperature";
const char* accel_topic = "sensor/accel";

// Simulation configuration
struct SimConfig {
    // Power parameters
    float baseVoltage = 230.0;      // Base voltage (V)
    float baseCurrent = 5.0;        // Base current (A)
    float baseFreq = 50.0;          // Base frequency (Hz)
    
    // Temperature parameters
    float baseTemp = 25.0;          // Base temperature (°C)
    
    // Acceleration parameters (in m/s²)
    float baseAccelX = 0.0;
    float baseAccelY = 0.0;
    float baseAccelZ = 9.81;        // Earth's gravity
    
    // Noise parameters
    float powerNoise = 0.02;        // 2% noise for power measurements
    float tempNoise = 0.5;          // ±0.5°C noise
    float accelNoise = 0.1;         // ±0.1 m/s² noise
};

// Global simulation configuration
SimConfig simConfig;

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

// Network clients
WiFiClient espClient;
PubSubClient client(espClient);

// Task handles
TaskHandle_t CollectionTask;
TaskHandle_t AverageTask;
TaskHandle_t PublishTask;

// Function to generate random noise within specified range
float generateNoise(float magnitude) {
    return ((float)random(0, 1000) / 1000.0 - 0.5) * 2 * magnitude;
}

// Simulate PZEM004T power meter readings
void simulatePowerReadings(float &voltage, float &current, float &frequency) {
    voltage = simConfig.baseVoltage * (1.0 + generateNoise(simConfig.powerNoise));
    current = simConfig.baseCurrent * (1.0 + generateNoise(simConfig.powerNoise));
    frequency = simConfig.baseFreq * (1.0 + generateNoise(simConfig.powerNoise));
}

// Simulate MAX31855 thermocouple readings
float simulateTemperature() {
    return simConfig.baseTemp + generateNoise(simConfig.tempNoise);
}

// Simulate ADXL345 accelerometer readings
void simulateAcceleration(float &x, float &y, float &z) {
    x = simConfig.baseAccelX + generateNoise(simConfig.accelNoise);
    y = simConfig.baseAccelY + generateNoise(simConfig.accelNoise);
    z = simConfig.baseAccelZ + generateNoise(simConfig.accelNoise);
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

// Task 1: Data Collection/Simulation
void CollectionTask_code(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz sampling
    
    for(;;) {
        // Generate simulated sensor readings
        float v, c, f, t, ax, ay, az;
        
        simulatePowerReadings(v, c, f);
        t = simulateTemperature();
        simulateAcceleration(ax, ay, az);
        
        xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
        
        // Store readings if buffer not full (< 10 samples)
        if (sensorData.voltage.count < 10) {
            // Store power readings
            sensorData.voltage.data[sensorData.voltage.count] = v;
            sensorData.current.data[sensorData.current.count] = c;
            sensorData.frequency.data[sensorData.frequency.count] = f;
            sensorData.temperature.data[sensorData.temperature.count] = t;
            
            // Store acceleration readings
            sensorData.accelX.data[sensorData.accelX.count] = ax;
            sensorData.accelY.data[sensorData.accelY.count] = ay;
            sensorData.accelZ.data[sensorData.accelZ.count] = az;
            
            // Update counters and flags
            sensorData.voltage.count++;
            sensorData.current.count++;
            sensorData.frequency.count++;
            sensorData.temperature.count++;
            sensorData.accelX.count++;
            sensorData.accelY.count++;
            sensorData.accelZ.count++;
            
            // Mark buffers as ready when 10 samples collected
            if (sensorData.voltage.count == 10) {
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
        
        // Maintain precise sampling frequency
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Task 2: Data Averaging
void AverageTask_code(void* parameter) {
    for(;;) {
        bool readyToAverage = false;
        
        // Check if buffers are ready for averaging
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
            
            // Reset buffers for next batch
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
            
            // Mark new data available for publishing
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
        // Ensure MQTT connection
        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        bool newData = false;
        
        // Check for new data to publish
        xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
        newData = sensorData.averaged.isNew;
        xSemaphoreGive(sensorData.mutex);
        
        if (newData) {
            xSemaphoreTake(sensorData.mutex, portMAX_DELAY);
            
            // Publish power measurements
            doc.clear();
            doc["voltage"] = sensorData.averaged.voltage;
            doc["current"] = sensorData.averaged.current;
            doc["frequency"] = sensorData.averaged.frequency;
            Serial.println(sensorData.averaged.voltage);
            Serial.println(sensorData.averaged.current);
            Serial.println(sensorData.averaged.frequency);

            char payload[200];
            serializeJson(doc, payload);
            client.publish(power_topic, payload);
            
            // Publish temperature measurements
            doc.clear();
            doc["celsius"] = sensorData.averaged.temperature;
            Serial.println(sensorData.averaged.temperature);
            
            serializeJson(doc, payload);
            client.publish(temp_topic, payload);
            
            // Publish acceleration measurements
            doc.clear();
            doc["x"] = sensorData.averaged.accelX;
            doc["y"] = sensorData.averaged.accelY;
            doc["z"] = sensorData.averaged.accelZ;
            Serial.println(sensorData.averaged.accelX);
            Serial.println(sensorData.averaged.accelY);
            Serial.println(sensorData.averaged.accelZ);

            serializeJson(doc, payload);
            client.publish(accel_topic, payload);
            
            // Reset new data flag
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

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Set CPU frequency
    setCpuFrequencyMhz(160);
    
    // Initialize random seed for simulation
    randomSeed(analogRead(0));
    
    // Create mutex for shared data protection
    sensorData.mutex = xSemaphoreCreateMutex();
    
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
        1                      // Core ID
    );
}

void loop() {
    // Empty - all functionality is handled by tasks
}