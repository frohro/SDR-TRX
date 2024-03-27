import datetime
import time
import socket
import struct
import os
import yaml
from consolemenu import *
from consolemenu.items import *
import sys
from ft8 import FT8Send
from ft4 import FT4Send

# Import weakmon to use encoders
sys.path.append(os.path.expandvars('$WEAKMON'))

# Read configuration file
configs_file = open('transceiver_config.yml', 'r')
configs = yaml.safe_load(configs_file)

# UDP address and port
udp_address = configs['udp_address']
udp_port = 12347

# Global variables
callsign = configs['callsign']
grid = configs['grid']
current_msg = ''
rx_callsign = ''
mode = 'FT8'

# FT8 encoder
ft8_encoder = FT8Send()
ft4_encoder = FT4Send()

# UDP socket
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def encode_ft8(msg):
    try:
        a77 = ft8_encoder.pack(msg, 1)
        symbols = ft8_encoder.make_symbols(a77)
    except Exception as e:
        print("FT8 encoder error:", e)
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
    for symbol in symbols:
        udp_socket.sendto(struct.pack('>B', symbol), (udp_address, udp_port))
    udp_socket.sendto(b'\0', (udp_address, udp_port))
    time.sleep(1)

def change_freq():
    no_freq = True
    while(no_freq):
        new_freq = int(input("Enter new offset frequency (0 - 2000Hz): "))
        if (new_freq > 0 and new_freq < 2000):
            no_freq = False
    udp_socket.sendto(b'o', (udp_address, udp_port))
    for kk in range(2):
        udp_socket.sendto(struct.pack('>B', (new_freq >> 8*kk) & 0xFF), (udp_address, udp_port))
    time.sleep(1)

def change_mode():
    global mode
    global current_msg
    if 'FT8' in mode:
        print("Switch mode to FT4!")
        menu.remove_item(chft4_item)
        menu.append_item(chft8_item)
        mode = 'FT4'
    else:
        print("Switch mode to FT8!")
        menu.remove_item(chft8_item)
        menu.append_item(chft4_item)
        mode = 'FT8'
    udp_socket.sendto(b's', (udp_address, udp_port))
    current_msg = ''
    time.sleep(1)
    menu.show()

def change_udp():
    print("Switch to UDP mode!")
    close_udp_socket()
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.sendto(b'u', (udp_address, udp_port))
    time.sleep(1)
    menu.show()

def close_udp_socket():
    global udp_socket
    udp_socket.close()

def new_msg(msg):
    global current_msg
    global mode
    print(msg)
    if msg != current_msg:
        if 'FT8' in mode:
            symbols = encode_ft8(msg)
        else:
            symbols = encode_ft4(msg)
        if symbols is not None:
            load_symbols(symbols)
            current_msg = msg
        else:
            return
    transmit()

def transmit():
    if not current_msg:
        print("No previous message!")
        time.sleep(1)
    else:
        print("Waiting for slot..")
        while True:
            utc_time = datetime.datetime.now(datetime.timezone.utc)
            if 'FT8' in mode:
                if (utc_time.second % 15 == 14):
                    print("TX!")
                    udp_socket.sendto(b't', (udp_address, udp_port))
                    time.sleep(1)
                    break
            else:
                mscount = utc_time.second + utc_time.microsecond/1e6
                if (abs(mscount%7.5 - 6.5) < 0.05):
                    print("TX!")
                    udp_socket.sendto(b't', (udp_address, udp_port))
                    time.sleep(1)
                    break

def call_cq():
    msg = 'CQ ' + callsign + ' ' + grid
    new_msg(msg)

def resp_new():
    global rx_callsign
    rx_callsign = input("Respond to callsign: ")
    resp_submenu.show()

def resp_last():
    if not rx_callsign:
        print("No previous callsign!")
        time.sleep(1)
    else:
        resp_submenu.show()

def respond(response):
    if 'grid' in response:
        msg = rx_callsign + ' ' + callsign + ' ' + grid
    elif 'signal' in response:
        snr = input("Signal strength: ")
        msg = rx_callsign + ' ' + callsign + ' ' + snr
    elif 'RR73' in response:
        msg = rx_callsign + ' ' + callsign + ' RR73'
    new_msg(msg)

# Create the menus
menu = ConsoleMenu("WJSTX Transceiver Control ", "FT8 and FT4")
resp_submenu = ConsoleMenu("Respond with: ", "", exit_option_text="Go back")

# Main menu
ver_item = FunctionItem("Use UART, not UDP", change_udp, [])
cq_item = FunctionItem("Call CQ", call_cq, [])
retransmit_item = FunctionItem("Retransmit last", transmit, [])
resp_new_item = FunctionItem("Respond to new callsign", resp_new, [])
submenu_item = FunctionItem("Respond to last callsign", resp_last, [])
chfreq_item = FunctionItem("Change TX frequency", change_freq, [])
chft4_item = FunctionItem("Change mode to FT4", change_mode, [])
chft8_item = FunctionItem("Change mode to FT8", change_mode, [])
exit_item = ExitItem("Exit", menu)

# Resp submenu
resp_grid_item = FunctionItem("Respond with grid", respond, ['grid'])
resp_signal_item = FunctionItem("Respond with R + signal strength", respond, ['signal'])
resp_73_item = FunctionItem("Respond with RR73", respond, ['RR73'])

resp_submenu.append_item(resp_grid_item)
resp_submenu.append_item(resp_signal_item)
resp_submenu.append_item(resp_73_item)

menu.append_item(ver_item)
menu.append_item(cq_item)
menu.append_item(resp_new_item)
menu.append_item(submenu_item)
menu.append_item(retransmit_item)
menu.append_item(chfreq_item)
menu.append_item(chft4_item)
menu.append_item(chft8_item)
menu.append_item(exit_item)
menu.show()
