from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
import socket
from quisk_hardware_model import Hardware as BaseHardware

# OSDR-TRX (WIFI) Quisk Configuration File
# 
# IMPORTANT: To be able to control the OpenRadio board from within Quisk,
# you will need to compile and upload the 'openradio_quisk' firmware, which
# is available from: https://github.com/darksidelemm/open_radio_miniconf_2015
#



# SOUND CARD SETTINGS
#
# Uncomment these if you wish to use PortAudio directly
# name_of_sound_capt = "portaudio:(hw:2,0)"
# name_of_sound_play = "portaudio:(hw:1,0)"

# Uncomment these lines if you wish to use Pulseaudio
# name_of_sound_capt = "pulse"
name_of_sound_play = "pulse"

# SERIAL PORT SETTINGS
# Set this as appropriate for your OS.
# openradio_serial_port = "/dev/ttyUSB0"
# openradio_serial_rate = 57600


# SDR-TRX Frequency limits.
# These are just within the limits set in the openradio_quisk firmware.
openradio_lower = 3500000
openradio_upper = 29999999
SAMPLE_RATE = 48000
#PICO_ADDRESS = '192.168.1.4'
# PICO_ADDRESS = '192.168.1.101'
COMMAND_PORT = 12346
DATA_PORT = 12345


class Hardware(BaseHardware):
    sample_rate = SAMPLE_RATE
    def open(self):
        print("Opening SDR-TRX.")
        # Called once to open the Hardware
        # Create a UDP socket
        self.data_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.command_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Bind the socket to a specific IP address and port
        self.pico_address = (PICO_ADDRESS, 12346)  # Set the destination address for the SDR-TRX  
        # Poll for version. Should probably confirm the response on this.
        version = self.send_command( 'VER\r\n')  # Get the version of the SDR-TRX
        print(version)   # Return an informative message for the config screen
        self.send_command("STOP_I2S_UDP\r\n")  # For now tell the SDR-TRX to stop sending I2S data via UDP
        print(type(version))
        t = (str(version) + ". Capture from sound card %s." % self.conf.name_of_sound_capt)
        return t

    def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
        # Called whenever quisk requests a frequency change.
        # This sends the FREQ command to set the centre frequency of the OpenRadio,
        # and will also move the 'tune' frequency (the section within the RX passband
        # which is to be demodulated) if it falls outside the passband (+/- sample_rate/2).
        print("Setting VFO to %d." % vfo)
        if vfo < openradio_lower:
            vfo = openradio_lower
            print("Outside range! Setting to %d" % openradio_lower)

        if vfo > openradio_upper:
            vfo = openradio_upper
            print("Outside range! Setting to %d" % openradio_upper)

        success = self.send_command("FREQ," + str(vfo))

        # If the tune frequency is outside the RX bandwidth, set it to somewhere within that bandwidth.
        if tune > (vfo + self.sample_rate / 2) or tune < (vfo - self.sample_rate / 2):  # Need to get sample_rate
            tune = vfo + 10000
            print("Bringing tune frequency back into the RX bandwidth.")

        if success:
            print("Frequency change succeeded!")
        else:
            print("Frequency change failed.")

        return tune, vfo


    # SDR-TRX UDP Routines for Parameters (Commands for the SDR-TRX)

    def send_command(self, command):
        try:
            message = str(command).encode('utf-8')
            # Send command to the server
            self.command_sock.sendto(message, self.pico_address)

            # Receive response from the server
            self.command_sock.settimeout(1)  # Set a timeout for receiving response
            data, _ = self.command_sock.recvfrom(4096)
            response = data.decode('utf-8')
            print(response)
            # Rest of the code...
            data, _ = self.command_sock.recvfrom(4096)
            response = data.decode('utf-8')
            print(response)

            # Check the response
            if response == 'OK\r\n':
                print('Command successful')
                return True
            elif response == 'Error\r\n':
                print('Command not recognized')
                return False
            elif response.endswith('\r\nOK\r\n'):
                print('Received data:', response[:-6])
                return response[:-6] # This removes the OK\r\n from the end of the response.
        except Exception as e:
            print('Error:', e)

