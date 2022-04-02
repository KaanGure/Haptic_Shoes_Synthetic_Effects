import numpy
import time
import zmq
import random
import sys
import struct
import READ_VL6180
import pub

# Create TOF sensor
TOF = READ_VL6180.createTOF()
i = 0
while True:
    if i > 5:
        i = 0
    # Read distance value from TOF
    dist = TOF.get_distance()

    # Convert distance read to 1-10000 range (closer means bigger value)
    #newDist = numpy.interp(-1*dist, [-255,-1], [0,20000])

    #print(newDist)
    print(dist)
    vel = [0 for i in range(5)]
    vel[i] = dist

    velocity = sum(vel)/0.05

    f = open("distance_velocity.txt", "w")
    f.write(str(dist) + ";\n" + str(velocity) + ";\n")
    f.close()
    f = open("distance_velocity.txt", "r")
    print(f.read())
    time.sleep(0.01)