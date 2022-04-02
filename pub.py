import zmq
import sys
import struct
import random


# set to true to debug and print to console
verbose = False


# Connect to appropriate address as publisher (PUSH)
def connectSocket():
    context = zmq.Context()
    socket = context.socket(zmq.PUSH)
    socket.connect("tcp://132.206.74.101:50000")
    publisher_id = random.randrange(0,9999)
    return [socket, publisher_id]


# Send message with topic in appropriate format
def sendMsg(socket, topic, message):
    if verbose:
        print(message)
    socket.send_multipart([topic.encode(), message])


# Pack FSR data and send it with topic
def sendFSRdata(socket, data):
    UUID = "FSR sensor"
    endianess = 0
    devicecaps = 0
    deviceType = 3
    deviceID = 3
    message = struct.pack('<32sBIHBi', UUID, endianess, devicecaps, deviceType, deviceID, data)
    sendMsg(socket, "FSR", message)


# Pack IMU data and send it with topic
def sendIMUdata(socket, cm, ca, cg):
    UUID = "IMU sensor"
    endianess = 0
    devicecaps = 0
    deviceType = 4
    deviceID = 4
    message = struct.pack('<32sBIHB9f', UUID, endianess, devicecaps, deviceType, deviceID, cm[0], cm[1], cm[2], ca[0], ca[1], ca[2], cg[0], cg[1], cg[2])
    sendMsg(socket, "IMU", message)


# Pack TOF data and send it with topic
def sendTOFdata(socket, data):
    UUID = "TOF sensor"
    endianess = 0
    devicecaps = 0
    deviceType = 2
    deviceID = 2
    message = struct.pack('<32sBIHBi', str.encode(UUID), endianess, devicecaps, deviceType, deviceID, data)
    sendMsg(socket, "TOF", message)
