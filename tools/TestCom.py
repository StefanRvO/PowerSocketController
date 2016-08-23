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
        elif(len(length) == 0): return
        msg = length
        length = int.from_bytes(length, byteorder = "big", signed = False)
        while(len(msg) < length):
            msg += socket.recv(length - len(msg))
        num+= 1
        print("Got ping", num)



def reboot_msg():
    dest = bytes([0x00, 0x00, 0x00, 0x00])
    source = bytes([0x01, 0x02, 0x03, 0x04])
    tpe   = bytes([0x00, 0x11])
    msg = dest + source + tpe
    return  (len(msg) + 2).to_bytes(2, byteorder = "big", signed = False) + msg


def ping_msg():
    dest = bytes([0x00, 0x00, 0x00, 0x00])
    source = bytes([0x01, 0x02, 0x03, 0x04])
    tpe   = bytes([0x00, 0x03])
    msg = dest + source + tpe

    return  (len(msg) + 2).to_bytes(2, byteorder = "big", signed = False) + msg

def set_name(name):
    dest = bytes([0x00, 0x00, 0x00, 0x00])
    source = bytes([0x01, 0x02, 0x03, 0x04])
    tpe   = bytes([0x00, 0x05])
    payload = name.encode() + bytes([0x00])
    msg = dest + source + tpe + payload

    return  (len(msg) + 2).to_bytes(2, byteorder = "big", signed = False) + msg


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.connect((sys.argv[1], int(sys.argv[2])))

t = threading.Thread(target = recv_thread, args = (s,))
t.start();

while(True):
#    msg = set_name("TEST___\0")
    msg = ping_msg()
    s.sendall(msg)
    time.sleep( 0.002)
 #   msg = reboot_msg()
 #   s.sendall(msg)
 #   time.sleep(2)
