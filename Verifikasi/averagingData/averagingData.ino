#include <Arduino.h>

// Simplified buffer structure for testing
struct SensorBuffer {
    float data[10];          
    size_t count = 0;        
    bool isReady = false;    
};

// Simplified processed data structure
struct ProcessedData {
    float value;
    bool isNew = false;
};

// Main data structure
struct SharedData {
    SensorBuffer buffer;
    ProcessedData averaged;
    SemaphoreHandle_t mutex;
} testData;

// Utility function to calculate average
float calculateAverage(SensorBuffer& buffer) {
    if (buffer.count == 0) return 0;
    float sum = 0;
    for (size_t i = 0; i < buffer.count; i++) {
        sum += buffer.data[i];
    }
    return sum / buffer.count;
}

// Test function to simulate data collection
void addTestData(float value) {
    xSemaphoreTake(testData.mutex, portMAX_DELAY);
    
    if (testData.buffer.count < 10) {
        testData.buffer.data[testData.buffer.count] = value;
        testData.buffer.count++;
        
        if (testData.buffer.count == 10) {
            testData.buffer.isReady = true;
        }
        
        Serial.print("Added value: ");
        Serial.print(value);
        Serial.print(" (Sample ");
        Serial.print(testData.buffer.count);
        Serial.println(" of 10)");
    }
    
    xSemaphoreGive(testData.mutex);
}

// Simplified averaging task for testing
void testAveraging() {
    bool readyToAverage = false;
    
    xSemaphoreTake(testData.mutex, portMAX_DELAY);
    readyToAverage = testData.buffer.isReady;
    xSemaphoreGive(testData.mutex);
    
    if (readyToAverage) {
        xSemaphoreTake(testData.mutex, portMAX_DELAY);
        
        // Calculate average
        testData.averaged.value = calculateAverage(testData.buffer);
        
        // Reset buffer
        testData.buffer.count = 0;
        testData.buffer.isReady = false;
        testData.averaged.isNew = true;
        
        Serial.println("\n--- Averaging Complete ---");
        Serial.print("Calculated average: ");
        Serial.println(testData.averaged.value);
        Serial.println("Buffer reset for next batch");
        Serial.println("------------------------\n");
        
        xSemaphoreGive(testData.mutex);
    } else {
        Serial.println("Not enough samples for averaging yet");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Create mutex
    testData.mutex = xSemaphoreCreateMutex();
    
    Serial.println("\nStarting Averaging Task Test");
    Serial.println("----------------------------");
}

void loop() {
    // Test Case 1: Add 5 samples (shouldn't trigger averaging)
    Serial.println("\nTest Case 1: Adding 5 samples");
    for(int i = 1; i <= 5; i++) {
        addTestData(i * 1.0);
        testAveraging();
        delay(100);
    }
    
    // Test Case 2: Add 5 more samples (should trigger averaging)
    Serial.println("\nTest Case 2: Adding 5 more samples");
    for(int i = 6; i <= 10; i++) {
        addTestData(i * 1.0);
        testAveraging();
        delay(100);
    }
    
    // Test Case 3: Add 3 samples to new empty buffer
    Serial.println("\nTest Case 3: Adding 3 samples to new buffer");
    for(int i = 1; i <= 3; i++) {
        addTestData(i * 2.0);
        testAveraging();
        delay(100);
    }
    
    Serial.println("\nTest complete. Waiting 5 seconds before repeating...\n");
    delay(5000);
}