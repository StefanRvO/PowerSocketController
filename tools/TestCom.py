import sys
import socket
import threading
import time


def recv_thread(socket):
    num = 0
    while(True):
        length = socket.recv(2)
        if(len(length) == 1):
            length += socket.recv(1)
        msg = length
        length = int.from_bytes(length, byteorder = "big", signed = False)
        while(len(msg) < length):
            msg += socket.recv(length - len(msg))
        num+= 1
        print("Got ping", num)


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.connect((sys.argv[1], int(sys.argv[2])))

t = threading.Thread(target = recv_thread, args = (s,))
t.start();
while(True):
    msg = bytes([0x00, 0x00, 0x01, 0x02])
    msg += bytes([0x01, 0x02, 0x03, 0x04])
    msg += bytes([0x00, 0x03])
    msg_len = (len(msg) + 2).to_bytes(2, byteorder = "big", signed = False)
    s.sendall(msg_len)
    s.sendall(msg)
    time.sleep(.01)
