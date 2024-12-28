import pika
from pika.exchange_type import ExchangeType

def on_message_received(ch, method_frame, header_frame, body):
    print(f" [1] Received {body}")

connection_params = pika.ConnectionParameters('localhost')
connection = pika.BlockingConnection(connection_params)
channel = connection.channel()
channel.exchange_declare(exchange='pubsub', exchange_type=ExchangeType.fanout)
queue = channel.queue_declare(queue='', exclusive=True)
channel.queue_bind(exchange='pubsub', queue=queue.method.queue)
channel.basic_consume(queue=queue.method.queue, on_message_callback=on_message_received, auto_ack=True)
print(' [*] Waiting for messages. To exit press CTRL+C')
channel.start_consuming()