from asyncore import write
import time
while True:
    for i in range(10):
        f = open("distance_velocity.txt", "w")
        f.write(str(i*10) + ";\n" + str(i*10) + ";\n")
        f.close()
        f = open("distance_velocity.txt", "r")
        print(f.read())
        time.sleep(1)


