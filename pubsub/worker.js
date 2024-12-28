const mqtt = require('mqtt');
const { MongoClient } = require('mongodb');

// MQTT Broker Configuration
const MQTT_BROKER_URL = 'mqtt://192.168.100.111';
const MQTT_TOPIC = 'sensor/accel/xyz';
const MQTT_USERNAME = 'admin';
const MQTT_PASSWORD = 'admin';

// MongoDB Configuration
const MONGO_URL = 'mongodb://localhost:27017';
const DB_NAME = 'sensordb';
const COLLECTION_NAME = 'acceleration';

async function main() {
    // MQTT Client Setup with Authentication
    const mqttClient = mqtt.connect(MQTT_BROKER_URL, {
        clientId: 'accelerometer-logger',
        username: MQTT_USERNAME,
        password: MQTT_PASSWORD,
        keepalive: 60,
        clean: true
    });

    // MongoDB Client Setup
    const mongoClient = new MongoClient(MONGO_URL);

    try {
        // Connect to MongoDB
        await mongoClient.connect();
        const database = mongoClient.db(DB_NAME);
        const collection = database.collection(COLLECTION_NAME);

        // MQTT Connection Handler
        mqttClient.on('connect', () => {
            console.log('Connected to MQTT Broker');
            mqttClient.subscribe(MQTT_TOPIC, (err) => {
                if (err) {
                    console.error('MQTT Subscription Error:', err);
                }
            });
        });

        // MQTT Message Handler
        mqttClient.on('message', async (topic, message) => {
            try {
                const payload = JSON.parse(message.toString());
                
                // Prepare MongoDB Document
                const document = {
                    timestamp: new Date(),
                    metadata: {
                        sensorId: 'ACCEL001',
                        location: 'Room1'
                    },
                    acceleration: {
                        x: payload.x,
                        y: payload.y,
                        z: payload.z
                    }
                };

                // Insert into MongoDB
                const result = await collection.insertOne(document);
                console.log('Inserted document:', result.insertedId);
            } catch (error) {
                console.error('Processing Error:', error);
            }
        });

        // Error Handlers
        mqttClient.on('error', (error) => {
            console.error('MQTT Client Error:', error);
        });

        mongoClient.on('error', (error) => {
            console.error('MongoDB Client Error:', error);
        });

    } catch (error) {
        console.error('Initialization Error:', error);
    }
}

// Start the application
main().catch(console.error);