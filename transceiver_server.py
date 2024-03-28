#!/usr/bin/env python3

import socket
import time
import lib.WSJTXClass as WSJTXClass
import serial
import yaml
import struct
import datetime
import os

#Import weakmon to use encoders
import sys
import sys
import os

sys.path.append(os.path.expandvars('$WEAKMON'))
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from ft8 import FT8Send
from ft4 import FT4Send



#Read configuration file
configs_file = open('transceiver_config.yml', 'r')
configs = yaml.load(configs_file, Loader=yaml.BaseLoader)

#Serial port for arduino
serial_port = configs['serial_port']
baudrate    = configs['baudrate']
try:
    puerto = serial.Serial(serial_port, baudrate, timeout=0.5)
except serial.serialutil.SerialException:
    print("\nNo se puede abrir puerto: " + serial_port + "\n")
    exit(1)

#Global variables
callsign = configs['callsign']
grid = configs['grid']
current_msg = ''
rx_callsign = ''
mode = 'FT8'
tx_freq = 1200

#Connection for WSJT-X
UDP_IP = "127.0.0.1"
UDP_PORT = 2237
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

#FT8 encoder
ft8_encoder = FT8Send()
ft4_encoder = FT4Send()

def encode_ft8(msg):
    try:
        a77 = ft8_encoder.pack(msg, 1)
        symbols = ft8_encoder.make_symbols(a77)
    except:
        print("FT8 encoder error, check message!")
        symbols = None
        time.sleep(3)
    return symbols

def encode_ft4(msg):
    try:
        a77 = ft4_encoder.pack(msg, 1)
        symbols = ft4_encoder.make_symbols(a77)
    except:
        print("FT4 encoder error, check message!")
        symbols = None
        time.sleep(3)
    return symbols

def load_symbols(symbols):
    print("Load symbols into transmitter..")
    puerto.write(b'm')
    count = 0
    for symbol in symbols:
        puerto.write(struct.pack('>B', symbol))
        count += 1
        #Wait to avoid Arduino serial buffer overflow
        if count % 50 == 0:
            time.sleep(0.05)
    puerto.write(b'\0')
    resp = puerto.read(512)
    if resp == b'm':
        print("Load OK")
    else:
        print(resp)
       

def change_freq(new_freq):
    global tx_freq
    print ("Change TX frequency to:", new_freq)
    puerto.write(bqq'o')
    for kk in range(2):
        puerto.write(struct.pack('>B', (new_freq >> 8*kk) & 0xFF))
    resp = puerto.read(1)        
    if resp == b'o':
        print("New freq OK")
        tx_freq = new_freq
        

def change_mode(new_mode):
    global mode
    puerto.write(b's')
    resp = puerto.read(1)        
    if resp == b's':
        mode = new_mode
        print("Switched to: {0}".format(new_mode))
        current_msg = ''    

def new_msg(msg):
    global current_msg
    global mode
    if msg != current_msg:
        print("Message: {0}".format(msg))
        if 'FT8' in mode:
            symbols = encode_ft8(msg)
        else:
            symbols = encode_ft4(msg)            
        if symbols.any():
            #symbols = [kk for kk in range(79)]
            load_symbols(symbols)
            current_msg = msg
        else:
            return
    else:
        time.sleep(0.005)

def transmit():
    if False:#not current_msg:
        print("No previous message!")
        time.sleep(1)
    else:
        print("TX!")
        puerto.write(b't')                

def check_time_window(utc_time):
    time_window = 15 if 'FT8' in mode else 7
    rm = utc_time.second % time_window
    if rm > 1 and rm < time_window-1:
        return False
    else:
        return True

#Check transmitter is initialized
print("\n\nWait for transmitter ready...")
while True:
    time.sleep(1)    
    puerto.write(b'r')
    x = puerto.read()
    if x == b'r':
        print("Transmitter ready!")
        break

        
try:
    while True:

        fileContent, addr = sock.recvfrom(1024)
        NewPacket = WSJTXClass.WSJTX_Packet(fileContent, 0)
        NewPacket.Decode()

        if NewPacket.PacketType == 1:
            StatusPacket = WSJTXClass.WSJTX_Status(fileContent, NewPacket.index)
            StatusPacket.Decode()

            #Check TX frequency and update transceiver
            new_freq = StatusPacket.TxDF
            new_mode = StatusPacket.TxMode.strip()
            
            if new_freq != tx_freq:
                change_freq(new_freq)

            if new_mode != mode:
                change_mode(new_mode)

            #Check if TX is enabled
            if StatusPacket.Transmitting == 1:
                #Check time, avoid transmitting out of the time slot
                utc_time = datetime.datetime.utcnow()
                tx_now = check_time_window(utc_time)                
                if tx_now:
                    puerto.write(b'p')
                message = StatusPacket.TxMessage
                message = message.replace('<', '')
                message = message.replace('>', '')                
                new_msg(message.strip())
                if tx_now:
                    transmit()
                print( "Time: {0}:{1}:{2}".format(utc_time.hour, utc_time.minute, utc_time.second))


finally:
    sock.close()
