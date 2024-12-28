const mqtt = require('mqtt');
const { MongoClient } = require('mongodb');

const MQTT_BROKER_URL = 'mqtt://192.168.100.111:1883';
const MQTT_TOPIC = 'sensor/accel/xyz';
const MQTT_USERNAME = 'admin';
const MQTT_PASSWORD = 'admin';

const MONGO_URL = 'mongodb://localhost:27017';
const DB_NAME = 'sensordb';
const COLLECTION_NAME = 'acceleration';

async function main() {
    const mongoClient = new MongoClient(MONGO_URL);

    try {
        await mongoClient.connect();
        console.log('Connected to MongoDB');
        const database = mongoClient.db(DB_NAME);
        const collection = database.collection(COLLECTION_NAME);

        const mqttClient = mqtt.connect(MQTT_BROKER_URL, {
            clientId: 'accelerometer-logger',
            username: MQTT_USERNAME,
            password: MQTT_PASSWORD,
            keepalive: 60,
            clean: true
        });

        mqttClient.on('connect', () => {
            console.log('Connected to MQTT Broker');
            mqttClient.subscribe(MQTT_TOPIC, (err) => {
                if (err) {
                    console.error('MQTT Subscription Error:', err);
                } else {
                    console.log(`Subscribed to topic: ${MQTT_TOPIC}`);
                }
            });
        });

        mqttClient.on('message', async (topic, message) => {
            try {
                const payload = JSON.parse(message.toString());
                
                // Explicitly check for x, y, z with type number and strict comparison
                if (
                    typeof payload.x === 'number' && 
                    typeof payload.y === 'number' && 
                    typeof payload.z === 'number'
                ) {
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

                    const result = await collection.insertOne(document);
                    console.log('Inserted document:', result.insertedId);
                } else {
                    console.error('Invalid payload type:', payload);
                }
            } catch (error) {
                console.error('Processing Error:', error);
            }
        });

        mqttClient.on('error', (error) => {
            console.error('MQTT Client Error:', error);
        });

    } catch (error) {
        console.error('Initialization Error:', error);
    }
}

main().catch(console.error);