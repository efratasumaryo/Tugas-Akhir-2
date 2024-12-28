import pika

# RabbitMQ connection details
rabbitmq_host = '192.168.100.111'
rabbitmq_port = 5672  # Default RabbitMQ port
rabbitmq_user = 'subscriber'
rabbitmq_password = 'admin'
exchange_name = 'amq.topic'
topic_key = 'test/topic'

# Set up connection and channel
credentials = pika.PlainCredentials(rabbitmq_user, rabbitmq_password)
connection = pika.BlockingConnection(pika.ConnectionParameters(host=rabbitmq_host, port=rabbitmq_port, credentials=credentials))
channel = connection.channel()

# Declare a queue and bind it to the amq.topic exchange with the topic key
queue_name = channel.queue_declare(queue='', exclusive=True).method.queue
channel.queue_bind(exchange=exchange_name, queue=queue_name, routing_key=topic_key)

# Callback function
def callback(ch, method, properties, body):
    print(f"Received message: {body.decode()} from topic: {method.routing_key}")

# Start consuming messages from the exchange-bound queue
channel.basic_consume(queue=queue_name, on_message_callback=callback, auto_ack=True)
print("Waiting for messages...")
channel.start_consuming()