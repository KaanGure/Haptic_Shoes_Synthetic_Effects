# RPI

import socket

ip_listen = "0.0.0.0"
receive_port = 11000

sock_receive = socket.socket(socket.AF_INET,
                     socket.SOCK_DGRAM)


sock_receive.bind((ip_listen, receive_port))
print(f'Start listening to {ip_listen}:{receive_port}')

while True:
    data, addr = sock_receive.recvfrom(1024) # buffer
    num = int(data)
    print(f"received message: {data}")
    print(f"processed message: {num}")

    MYFILE="distance_velocity.txt"
    # read the file into a list of lines
    lines = open(MYFILE, 'r').readlines()

    # now edit the last line of the list of lines
    new_last_line = (lines[-1].rstrip() + str(num) + ";\n")
    lines[-1] = new_last_line

    # now write the modified list back out to the file
    open(MYFILE, 'w').writelines(lines)