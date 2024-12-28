import pika
from pika.exchange_type import ExchangeType

# Callback function to process the received message
def on_message_received(ch, method_frame, header_frame, body):
    print(f" [2] Received {body.decode()}")

# Establish connection to RabbitMQ
connection_params = pika.ConnectionParameters('localhost')
connection = pika.BlockingConnection(connection_params)
channel = connection.channel()

# Declare the exchange (it should be the same as the publisher)
channel.exchange_declare(exchange='amq.topic', exchange_type=ExchangeType.topic)

# Declare a unique queue (it will be automatically deleted when connection closes)
queue = channel.queue_declare(queue='', exclusive=True)
queue_name = queue.method.queue

# Bind the queue to the exchange with the routing key "test/topic"
channel.queue_bind(exchange='amq.topic', queue=queue_name, routing_key='test/topic')

# Start consuming messages from the queue
channel.basic_consume(queue=queue_name, on_message_callback=on_message_received, auto_ack=True)

print(' [*] Waiting for messages. To exit press CTRL+C')
channel.start_consuming()
