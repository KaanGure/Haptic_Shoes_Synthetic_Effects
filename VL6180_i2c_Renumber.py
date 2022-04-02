#Program used to create code to change multiple I2C addresses on the VL6180X

import RPi.GPIO as GPIO
import time
from ST_VL6180X import VL6180X

#Constants
i2c_default = 0x28

#Ports and Pins

sensor0_i2c = 0x28
sensor0_standby = 11
sensor0_intrpt = 12

pinouts = [sensor0_standby,sensor0_intrpt] 

i2caddress = sensor0_i2c


#Configure GPIO Bus and Set Lines High
GPIO.setmode(GPIO.BOARD)
for i in pinouts:
    GPIO.setup(i,GPIO.OUT)
    GPIO.output(i,True)
#Wait 20 mSec between configs
    time.sleep(0.02)

time.sleep(0.1)
#Renumber I2C Addresses
##
#Sensor 0
#Configure and Change Address
#Error Handler in Event that no Default I2c Addresses are present
try:
    sensor0 = VL6180X(i2c_default)
except IOError:
    print("No I2C Device with that address is present")
else:
    sensor0.change_address(i2c_default,sensor0_i2c)
    sensor0.default_settings()
    print("sensor 0 is at address ",hex(sensor0_i2c))
    time.sleep(0.100)

GPIO.cleanup()
