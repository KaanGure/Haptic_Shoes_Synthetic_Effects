# Quick and Dirty Sensor Read for multiple sensors
# Assumes i2c addresses have been change prior to run

import sys, time
#import VL6180_i2c_Renumber
from ST_VL6180X import VL6180X


def createTOF():
    #Initialize and report Sensor 0
    sensor0_i2cid = 0x29
    sensor0 = VL6180X(sensor0_i2cid)
    sensor0.get_identification()
    if sensor0.idModel != 0xB4:
        print("Not Valid Sensor, Id reported as ",hex(sensor0.idModel))
    else:
        print("Valid Sensor, ID reported as ",hex(sensor0.idModel))
    sensor0.default_settings()
    #Finish Initialize Sensor 0
    #---------------------------------
    return sensor0


if __name__ == "__main__":
    # Quick Test Code To read TOF sensor
    sensor0 = createTOF()
    while (True):
        L0 = int(sensor0.get_distance())
        print("     ",L0,"mm\n")
        time.sleep(0.05)
    

#Test results show code was successful reading both sensors
#27APR17 TAH
