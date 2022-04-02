import pyaudio
import wave
import analyse
import numpy
import os
import time
import zmq
import random
import sys
import struct
import READ_FSR
import READ_LSM9DS1
import READ_VL6180
import pub

# Open input stream for FSR and create imu & TOF sensors
stream = READ_FSR.openStream()

imu = READ_LSM9DS1.createIMU()

TOF = READ_VL6180.createTOF()

[socket, publisher_id] = pub.connectSocket()


# Sends trigger msg to PD netreceiver 3000
def send2Pd(message=''):
    os.system("echo '" + message + "' | pdsend 3000")


# Sends appropriate trigger messages to PD to vibrate both actuators once (both output channels)
def triggerOutput():
    message = '0 1;'
    send2Pd(message) 
    message = '1 1;'
    send2Pd(message)
    time.sleep(0.5) 
    message = '0 0;'
    send2Pd(message)   
    message = '1 0;'
    send2Pd(message)


while True:
    forceAmp = READ_FSR.getFSRLoudness(stream)

    # Send 2 vibrations if FSR pressed
    if forceAmp > -5:
        print('pressed')
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()

    # Broadcast value read from FSR
    pub.sendFSRdata(socket, forceAmp)

    READ_LSM9DS1.readMag(imu)
    cm = READ_LSM9DS1.getMag(imu)
    READ_LSM9DS1.readAccel(imu)
    ca = READ_LSM9DS1.getAccel(imu)
    READ_LSM9DS1.readGyro(imu)
    cg = READ_LSM9DS1.getGyro(imu)

    # Send 3 vibrations if imu tilted
    if cm[0] > 0.5:
        print('tilt')
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()

    # Broadcast value read from imu's magnetometer
    pub.sendIMUdata(socket, cm, ca, cg)

    dist = TOF.get_distance()

    # Send 4 vibrations if TOF is close to object/surface
    if dist < 70:
        print('close distance')
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()

    # Broadcast value read from TOF
    pub.sendTOFdata(socket, dist)
