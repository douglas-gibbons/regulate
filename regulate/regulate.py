#!/usr/bin/env python
import paho.mqtt.client as mqtt
import os
username=os.environ["CONSUMER_USERNAME"]
password=os.environ["CONSUMER_PASSWORD"]

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("+/temperature")

def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))
    user = msg.topic.split('/')[0]
    if float(msg.payload) > 40:
      client.publish(user + "/control", payload="0", qos=0, retain=False)
    if float(msg.payload) < 35:
      client.publish(user + "/control", payload="1", qos=0, retain=False)
    
print("Running regulate with username", username)

client = mqtt.Client()
client.username_pw_set(username, password=password)
client.on_connect = on_connect
client.on_message = on_message

print("Connecting to MQTT Broker")
client.connect("tserve", 1883, 60)
print("Connected")

client.loop_forever()

