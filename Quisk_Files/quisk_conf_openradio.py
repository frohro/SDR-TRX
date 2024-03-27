# OpenRadio v1.1 Quisk Configuration File
# 
# IMPORTANT: To be able to control the OpenRadio board from within Quisk,
# you will need to compile and upload the 'openradio_quisk' firmware, which
# is available from: https://github.com/darksidelemm/open_radio_miniconf_2015
#
# You will also need to install the pyserial package for python.
#

from __future__ import print_function
from __future__ import absolute_import
from __future__ import division


# SOUND CARD SETTINGS
#
# Uncomment these if you wish to use PortAudio directly
#name_of_sound_capt = "portaudio:(hw:2,0)"
#name_of_sound_play = "portaudio:(hw:1,0)"

# Uncomment these lines if you wish to use Pulseaudio
name_of_sound_capt = "pulse"
name_of_sound_play = "pulse"

# SERIAL PORT SETTINGS
# Set this as appropriate for your OS.
openradio_serial_port = "/dev/ttyUSB0"
openradio_serial_rate = 57600


# OpenRadio Frequency limits.
# These are just within the limits set in the openradio_quisk firmware.
openradio_lower = 100001
openradio_upper = 29999999

# OpenRadio Hardware Control Class
#
import serial,time
from quisk_hardware_model import Hardware as BaseHardware

class Hardware(BaseHardware):
  def open(self):
    # Called once to open the Hardware
    # Open the serial port.
    self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    print("Opened Serial Port.")
    # Wait for the Arduino Nano to restart and boot.
    time.sleep(2)
    # Poll for version. Should probably confirm the response on this.
    version = str(self.get_parameter(self, "VER"))
    print(version)
    # Return an informative message for the config screen
    t = version + ". Capture from sound card %s." % self.conf.name_of_sound_capt
    return t

  def close(self):      
    # Called once to close the Hardware
    self.or_serial.close()

  def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
    # Called whenever quisk requests a frequency change.
    # This sends the FREQ command to set the centre frequency of the OpenRadio,
    # and will also move the 'tune' frequency (the section within the RX passband
    # which is to be demodulated) if it falls outside the passband (+/- sample_rate/2).
    print("Setting VFO to %d." % vfo)
    if(vfo<openradio_lower):
      vfo = openradio_lower
      print("Outside range! Setting to %d" % openradio_lower)

    if(vfo>openradio_upper):
      vfo = openradio_upper
      print("Outside range! Setting to %d" % openradio_upper)

    success = self.set_parameter("FREQ",str(vfo))

    # If the tune frequency is outside the RX bandwidth, set it to somewhere within that bandwidth.
    if(tune>(vfo + sample_rate/2) or tune<(vfo - sample_rate/2)):
      tune = vfo + 10000
      print("Bringing tune frequency back into the RX bandwidth.")

    if success:
      print("Frequency change succeeded!")
    else:
      print("Frequency change failed.")

    return tune, vfo

#
# Serial comms functions, to communicate with the OpenRadio board
#

  def get_parameter(self,string):
    print("Getting parameter: ", string)
    self.or_serial.write(string+"\n")
    return self.get_argument()
        
  def set_parameter(self,string,arg):
    self.or_serial.write(string+","+arg+"\n")
    if self.get_argument() == arg:
      return True
    else:
      return False
    
  def get_argument(self):
    print("Getting argument.")
    data1 = self.or_serial.readline()
    print(data1)
    # Do a couple of quick checks to see if there is useful data here
    if len(data1) == 0:
       return -1
        
    # Maybe we didn't catch an OK line?
    if data1.startswith('OK'):
       data1 = self.or_serial.readline()
        
    # Check to see if we have a comma in the string. If not, there is no argument.
    if data1.find(',') == -1:
       return -1
    
    data1 = data1.split(',')[1].rstrip('\r\n')
    
    # Check for the OK string
    data2 = self.or_serial.readline()
    if data2.startswith('OK'):
       return data1
