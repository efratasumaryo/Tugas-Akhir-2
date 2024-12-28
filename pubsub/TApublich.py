import paho.mqtt.client as mqtt
import time

# RabbitMQ (MQTT plugin) server details
mqtt_broker = "192.168.100.111"  # Replace with your RabbitMQ server IP
mqtt_port = 1883                 # Default MQTT port
mqtt_user = "admin"              # RabbitMQ username
mqtt_password = "admin"          # RabbitMQ password
mqtt_topic = "test/topic"        # Topic to publish

# Create MQTT client with specified API version
client = mqtt.Client(client_id="PythonPublisher", protocol=mqtt.MQTTv311)  # Specify protocol version

# Set MQTT username and password
client.username_pw_set(mqtt_user, mqtt_password)

# Connect to the RabbitMQ broker
client.connect(mqtt_broker, mqtt_port)

# Publishing a message every 2 seconds
try:
    while True:
        message = "Hello from Python MQTT Publisher"
        print(f"Publishing message: {message}")
        client.publish(mqtt_topic, message)
        time.sleep(2)
except KeyboardInterrupt:
    print("Publisher stopped.")

# Disconnect the client
client.disconnect()
