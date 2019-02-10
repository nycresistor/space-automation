import time
import espeak
import paho.mqtt.client as mqtt
broker_address="192.168.1.32"
import RPi.GPIO as GPIO

es = espeak.ESpeak()

test_msg = "Speaker is connected"

es.say(test_msg)

# setup button
GPIO.setmode(GPIO.BCM)
GPIO.setup(23, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
if GPIO.input(23) == 0:
    print("button pressed")

#define callback
def on_message(client, userdata, message):
    time.sleep(1)
    print("received message =",str(message.payload.decode("utf-8")))
    text_message = str(message.payload.decode("utf-8"))
    es.say(text_message)

def on_disconnect(client, userdata, rc):
    error_message = str(rc)
    es.say("disconnected because " + error_message)

# Client must be instantiated with a unique client ID
client= mqtt.Client("client-speakercube") 


#####
print("connecting to broker ", broker_address)
client.connect(broker_address)

######Bind function to callback
client.on_message=on_message

print("subscribing ")
client.subscribe("/voicecube/announce")
#time.sleep(20000000)

print("publishing ")
if GPIO.input(23) == 0:
    print("button pressed")
    client.publish("/voicecube/receive","test message")
    time.sleep(4)

#client.loop_forever() #start loop to process received messages

run = True
while run:
   client.loop()
   time.sleep(1)

   if GPIO.input(23) == 0:
       print("button pressed")
       client.publish("/voicecube/receive","test message")
       time.sleep(1)
