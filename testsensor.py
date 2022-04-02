import numpy
import time
import zmq
import random
import sys
import struct
import READ_VL6180
import pub
from pythonosc import osc_message_builder
from pythonosc import udp_client

# Create TOF sensor
TOF = READ_VL6180.createTOF()


# Create ZMQ socket
[socket, publisher_id] = pub.connectSocket()


# Connect udp client to bach
client = udp_client.SimpleUDPClient("132.206.74.208", 14923)


# Create tiles array to be send to simulation
tiles = numpy.tile(0.0, 6*6*4)
client.send_message("/niw/client", tiles)

while True:

    # Read distance value from TOF
    dist = TOF.get_distance()

    # Convert distance read to 1-10000 range (closer means bigger value)
    #newDist = numpy.interp(-1*dist, [-255,-1], [0,20000])

    #print(newDist)
    print(dist)

    # Send converted value
    #tiles[20:24] = newDist
    #client.send_message("/niw/client", tiles)

    # Broadcast actual value read from TOF to ZMQ platform
    #pub.sendTOFdata(socket, dist)
    time.sleep(0.1)
