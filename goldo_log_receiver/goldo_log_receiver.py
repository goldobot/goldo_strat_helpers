import sys
import signal
import socket
import struct
#from time import sleep, clock, time
from time import sleep, time

def goldo_signal_handler(sig,frame):
    print (" DEBUG GOLDO : sys.exit(0)")
    sys.exit(0)


# don't forget du disable multicast RPF:
# sysctl net.ipv4.conf.gre2.rp_filter=0

if (len(sys.argv)!=4):
    print ("usage : python3 {} <mcast_addr> <mcast_port> <mcast_intf>".format(sys.argv[0]))
    sys.exit()

# 231.10.66.10
mcast_addr = sys.argv[1]
mcast_port = int(sys.argv[2])
mcast_intf = sys.argv[3]

rcv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rcv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
rcv.bind((mcast_addr,mcast_port))
#mreq=struct.pack("4sl",socket.inet_aton(mcast_addr),socket.INADDR_ANY)
#mreq=struct.pack("4s4s",socket.inet_aton(mcast_addr),socket.inet_aton("192.168.1.12"))
mreq=struct.pack("4s4s",socket.inet_aton(mcast_addr),socket.inet_aton(mcast_intf))
rcv.setsockopt(socket.IPPROTO_IP,socket.IP_ADD_MEMBERSHIP,mreq)

signal.signal(signal.SIGINT, lambda sig,frame: goldo_signal_handler(sig,frame))

while True:
    (buf,from_addr) = rcv.recvfrom(4000)
    my_str = buf.decode("utf-8")
    #print ("FROM {} : {}".format(from_addr, my_str))
    print (my_str,end='',flush=True)

rcv.close()


