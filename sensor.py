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
d = [0.0 for i in range(6)]
vel = [0.0 for i in range(5)]
while True:
    if i > 5:
        i = 0
    # Read distance value from TOF
    dist = TOF.get_distance()

    # Convert distance read to 1-10000 range (closer means bigger value)
    #newDist = numpy.interp(-1*dist, [-255,-1], [0,20000])

    #print(newDist)
    print(dist)
    d[i] = float(dist)

    for j in range(5):
        vel[j] = (abs(d[j+1] - d[j]))/0.05
        #print("vel " + str(j) + " " + str(vel[j]))
    velocity = sum(vel)/4.0

    #do scaling
    velocity = velocity/10.0


    f = open("distance_velocity.txt", "w")
    f.write(str(dist) + ";\n" + str(velocity) + ";\n")
    f.close()
    f = open("distance_velocity.txt", "r")
    print(f.read())
    i+= 1
    time.sleep(0.1)