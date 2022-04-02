import pyaudio
import wave
import analyse
import numpy
import os
import time
import READ_LSM9DS1
import zmq
import random
import sys
import struct

CHUNK = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 2
RATE = 44100
RECORD_SECONDS = 5
WAVE_OUTPUT_FILENAME = "output.wav"

p = pyaudio.PyAudio()

stream = p.open(format=FORMAT,
                channels=CHANNELS,
                input_device_index = 1,
                rate=RATE,
                input=True)
                #frames_per_buffer=CHUNK)


print("* recording")

imu = READ_LSM9DS1.createIMU()

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://132.206.74.101:50000")
publisher_id = random.randrange(0,9999)

def send2Pd(message=''):
    os.system("echo '" + message + "' | pdsend 3000")
    
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
    rawsamps = stream.read(1024, exception_on_overflow=False)
    samps = numpy.fromstring(rawsamps, dtype=numpy.int16)
     
    #print(analyse.loudness(samps))
    if(analyse.loudness(samps)>-5):
        print('pressed')
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
    
    topic = "FSR"    
    message = "%s" % (analyse.loudness(samps))
    print(message)
    socket.send_multipart([topic.encode(), message])
    
    READ_LSM9DS1.readMag(imu)
    cm = READ_LSM9DS1.getMag(imu)
    if(cm[0] > 0.5):
        print('tilt')
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
        time.sleep(0.1)
        triggerOutput()
    
    topic = "IMU"
    message = "x: %s y: %s z: %s" % (cm[0],cm[1],cm[2])
    print(message)
    socket.send_multipart([topic.encode(), message])


    
