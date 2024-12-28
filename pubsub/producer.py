import pika
from pika.exchange_type import ExchangeType

connection_params = pika.ConnectionParameters('localhost')
connection = pika.BlockingConnection(connection_params)
channel = connection.channel()
channel.exchange_declare(exchange='pubsub', exchange_type=ExchangeType.fanout)
message = 'Hello World, this is publisher!'
channel.basic_publish(exchange='pubsub', routing_key='', body=message)
print(" [P] Sent %r" % message)
connection.close()