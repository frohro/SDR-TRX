
from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
import socket
from quisk_hardware_model import Hardware as BaseHardware

name_of_sound_play = "pulse"

openradio_lower = 3500000
openradio_upper = 29999999
SAMPLE_RATE = 96000
PICO_ADDRESS = '192.168.1.104'
COMMAND_PORT = 12346
DATA_PORT = 12345




class Hardware():
    sample_rate = SAMPLE_RATE
    def open(self):
        print("Opening SDR-TRX.")
        # Called once to open the Hardware
        # Create a UDP socket
        self.data_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.command_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Bind the socket to a specific IP address and port
        # Set the destination address for the SDR-TRX  
        self.pico_address = (PICO_ADDRESS, COMMAND_PORT)
        # Poll for version. Should probably confirm the response on this.
        version = self.send_command( 'VER\r\n')  # Get the version of the SDR-TRX
        print(version)   # Return an informative message for the config screen
        #self.send_command("STOP_I2S_UDP\r\n")  # For now tell the SDR-TRX to stop sending I2S data via UDP

       # t = version + ". Capture from sound card %s." % self.conf.name_of_sound_capt
        return # t


    def send_command(self, message):
        message = str(message).encode("utf-8")
        #message ="VER\r\n".encode("utf-8")
        try:
            # Send data
            print('sending {!r}'.format(message))
            sent = self.command_sock.sendto(message, self.pico_address)

            # Receive response
            print('waiting to receive')
            data, server = self.command_sock.recvfrom(4096)
            print('received {!r}'.format(data)+' from '+str(server))
            print(data.decode("utf-8"))

        finally:
            print('closing socket')
            self.command_sock.close()

hardware_instance = Hardware()
hardware_instance.open()