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


# OpenRadio Frequency limits.
# These are just within the limits set in the openradio_quisk firmware.
openradio_lower = 3800000
openradio_upper = 30000000

# OpenRadio Hardware Control Class
#
import serial,time
from quisk_hardware_model import Hardware as BaseHardware

class Hardware(BaseHardware):
  def open(self):
    # SERIAL PORT SETTINGS
    # Set this as appropriate for your OS.
    openradio_serial_port = "/dev/ttyUSB0"
    openradio_serial_rate = 57600
    # Called once to open the Hardware
    # Open the serial port.
    try:
    	self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    except serial.serialutil.SerialException:
       openradio_serial_port = "/dev/ttyDummy" # Set to the port for interceptty.
       # To run interceptty: $ interceptty /dev/ttyACM0 /dev/ttyDummy
    try:
    	self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    except serial.serialutil.SerialException:
    	openradio_serial_port = "/dev/ttyUSB1" # Set to the second serial port for your OS.
    try:
    	self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    except serial.serialutil.SerialException:
       openradio_serial_port = "/dev/ttyACM0" # Set to the third serial port for your OS.
    try:
    	self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    except serial.serialutil.SerialException:
       openradio_serial_port = "/dev/ttyACM1" # Set to the third serial port for your OS.
    try:
    	self.or_serial = serial.Serial(openradio_serial_port,openradio_serial_rate,timeout=3)
    except serial.serialutil.SerialException:
    	print ("Radio not connected")
    	raise
    print("Opened Serial Port.")
    # Wait for the Pico to restart and boot.
    time.sleep(2)
    # Poll for version. Should probably confirm the response on this.
    version = str(self.get_parameter("VER"))

    print("This should be the version printed next.")
    print(version)
    # Return an informative message for the config screen
    t = str(version) + ". Capture from sound card %s." % self.conf.name_of_sound_capt
    return t

  def close(self):      
    # Called once to close the Hardware
    self.or_serial.close()

  def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
    # Called whenever quisk requests a frequency change.
    # This sends the FREQ command to set the centre frequency of the OpenRadio,
    # and will also move the 'tune' frequency (the section within the RX passband
    # which is to be demodulated) if it falls outside the passband (+/- sample_rate/2).
    time.sleep(0.1)
    if(vfo<openradio_lower):
      vfo = openradio_lower
      print("Outside range! Setting to %d" % openradio_lower)

    if(vfo>openradio_upper):
      vfo = openradio_upper
      print("Outside range! Setting to %d" % openradio_upper)

    # success = self.set_parameter("FREQ",str(vfo))
    self.set_parameter("FREQ",str(vfo))
    self.set_parameter("TX_FREQ",str(tune))

    print("sample_rate =")
    print(sample_rate)
    # If the tune frequency is outside the RX bandwidth, set it to somewhere within that bandwidth.
    if(tune>(vfo + sample_rate/2) or tune<(vfo - sample_rate/2)):
      tune = vfo + 10000
      print("Bringing tune frequency back into the RX bandwidth.")

    # if success:
    #   print("Frequency change succeeded!")
    # else:
    #   print("Frequency change failed.")

    return tune, vfo

#
# Serial comms functions, to communicate with the Pi Pico board
#

  def get_parameter(self,string):
    string = string + "\n"
    self.or_serial.write(string.encode())
    return self.get_argument()
    #return string.encode()
        
  def set_parameter(self,string,arg):
    string = string+","+arg+"\r"+"\n"
    self.or_serial.write(string.encode())
    print('arg is: ', arg)
    temp_arg = self.get_argument()
    print('temp_arg is: ', temp_arg)
    return True
    # if temp_arg == arg:
    #   return True
    # else:
    #   return False
    
  def get_argument(self):
    data1 = self.or_serial.readline()
    # Do a couple of quick checks to see if there is useful data here
    if len(data1) == 0:
      return -1
        
    # Maybe we didn't catch an OK line?
    if data1.startswith(b'OK'):
       data1 = self.or_serial.readline()
       print('Received: ', data1)
        
    # Check to see if we have a comma in the string. If not, there is no argument.
    if data1.find(b',') == -1:
      return -1
    
    data1 = data1.split(b',')[1].rstrip(b'\r\n')
    print("data1 =")
    print(data1)
    
    # Check for the OK string
    data2 = self.or_serial.readline()
    if data2.startswith(b'OK'):
      return data1

 #  The code below is to interface WSJT-X with SDR-TRX.

    def encode_ft8(self, msg):
        try:
            a77 = self.ft8_encoder.pack(msg, 1)
            symbols = self.ft8_encoder.make_symbols(a77)
        except Exception as e:
            print("FT8 encoder error, check message!")
            print(f"Exception type: {type(e).__name__}")
            print(f"Exception message: {str(e)}")
            symbols = None
            time.sleep(3)
        return symbols

    def encode_ft4(self, msg):
        try:
            a77 = self.ft4_encoder.pack(msg, 1)
            symbols = self.ft4_encoder.make_symbols(a77)
        except:
            print("FT4 encoder error, check message!")
            symbols = None
            time.sleep(3)
        return symbols

    def load_symbols(self, symbols):
        print("Load symbols into transmitter..")
        self.or_serial.write(b'm')
        count = 0
        for symbol in symbols:
            self.or_serial.write(struct.pack('>B', symbol))
            count += 1
            # Wait to avoid Arduino serial buffer overflow.  This may not be needed with the Pico.
            if count % 50 == 0:
                time.sleep(0.05)
        self.or_serial.write(b'\0')
        resp = self.or_serial.read(512)
        if resp == b'm':
            print("Load OK")
        else:
            print(resp)

    def change_freq(self, new_freq):
        #global self.tx_freq
        print("Change TX frequency to:", new_freq)
        self.or_serial.write(b'o')
        for kk in range(2):
            self.or_serial.write(struct.pack('>B', (new_freq >> 8 * kk) & 0xFF))
        time.sleep(0.05)    
        resp = self.or_serial.read(1)
        if resp == b'o':
            print("New freq OK")
            self.tx_freq = new_freq

    # def set_mode(self, new_mode):  # This was the old protocol from wsjt_transceiver.ino
    #     #global self.mode
    #     self.or_serial.write(b'e')
    #     time.sleep(0.05)
    #     resp = self.or_serial.read(1)
    #     print("Response: {0}".format(resp))
    #     if resp == b's':
    #         self.mode = new_mode
    #         print("Switched to: {0}".format(new_mode))

    def set_mode(self, new_mode):  # New more robust protocol.
        #global mode
        if new_mode == 'FT8':
            self.or_serial.write(b'e')
            time.sleep(.05)
            resp = self.or_serial.read(1) 
            print("resp = ", resp)       
            if resp == b'e':
                self.mode = new_mode
                print("Switched to: {0}".format(new_mode)) 
                self.current_msg = '' 
                return True
        elif new_mode == 'FT4':
            self.or_serial.write(b'f')
            time.sleep(0.05)
            resp = self.or_serial.read(1) 
            print("resp = ", resp)       
            if resp == b'f':
                self.mode = new_mode
                self.current_msg = ''
                print("Switched to: {0}".format(new_mode))
                return True
        else:
            return False


    def new_msg(self, msg):
        #global self.current_msg
        #global self.mode
        if msg != self.current_msg:
            print("Message: {0}".format(msg))
            if 'FT8' in self.mode:
                symbols = self.encode_ft8(msg)
            else:
                symbols = self.encode_ft4(msg)
            if symbols.any():
                # symbols = [kk for kk in range(79)]
                self.load_symbols(symbols)
                self.current_msg = msg
            else:
                return
        else:
            time.sleep(0.005)  #  Do we need this?

    def transmit(self):
        if False:  # not current_msg:
            print("No previous message!")
            time.sleep(1)
        else:
            print("TX!")
            self.or_serial.write(b't')
            # self.tx_now = False

    def check_time_window(self, utc_time):
        time_window = 15 if 'FT8' in self.mode else 7
        print("Time window: ", time_window)  # Is correct for FT8 and FT4.
        rm = utc_time.second % time_window
        if rm > 1 and rm < time_window - 1:  # Within one second of exact time for FT8.
            return False
        else:
            return True

    # See if HeartBeat will work for this:
    def HeartBeat(self):  # Should be called at 10 Hz from the main.
        # Check transmitter is initialized
        #raise Exception("In HeartBeat")
    
        # print("\n\nWait for transmitter ready...")
        if self.tx_ready_wsjtx == False:
            if self.tx_ready_wsjtx_sent == False:
                self.or_serial.write(b'r')
                time.sleep(0.05)
                self.tx_ready_wsjtx_sent = True
            x = self.or_serial.read()
            if x == b'r':
                print("Transmitter ready!")
                self.tx_ready_wsjtx = True
        if self.tx_ready_wsjtx == True:
            try:
                fileContent, addr = self.sock.recvfrom(1024)
                # print(addr)
            except Exception as e:
                if e == BlockingIOError:  # This is our way of pollig the socket.
                    # Maybe use select to stop the errors printing after we quit.
                    return BaseHardware.HeartBeat(self)
                else:
                    # print("Error in HeartBeat")
                    # print(f"Exception type: {type(e).__name__}")
                    # print(f"Exception message: {str(e)}")
                    pass
            try:
                NewPacket = WSJTXClass.WSJTX_Packet(fileContent, 0)
                NewPacket.Decode()

                if NewPacket.PacketType == 1:
                    # print('New Packet is type 1')
                    StatusPacket = WSJTXClass.WSJTX_Status(fileContent, NewPacket.index)
                    StatusPacket.Decode()
    
                    # Check TX frequency and update transceiver
                    new_freq = StatusPacket.TxDF
                    new_mode = StatusPacket.TxMode.strip()
                    

                    if new_freq != self.tx_freq:
                        # time.sleep(0.03)
                        print('Changing frequency from {0} to {1}'.format(self.tx_freq, new_freq))
                        try:
                            self.change_freq(new_freq)
                        except Exception as e:
                            print(f"Exception occurred: {e}")
                        print ('It is now ', self.tx_freq)

                    print('new_mode', new_mode, ' self.mode is: ', self.mode)
                    if new_mode != self.mode:  
                        print("Mode before: {0}".format(self.mode))
                        self.set_mode(new_mode)
                        print("New mode after: {0}".format(self.mode))

                    # Check if TX is enabled
                    if StatusPacket.Transmitting == 1:
                        # Check time, avoid transmitting out of the time slot
                        current_time = datetime.datetime.now()
                        utc_time = current_time.astimezone(datetime.timezone.utc)
                        self.tx_now = self.check_time_window(utc_time)
                        if self.tx_now:
                            self.or_serial.write(b'p')
                        message = StatusPacket.TxMessage
                        message = message.replace('<', '')
                        message = message.replace('>', '')
                        print('Message is: ', message)
                        self.new_msg(message.strip())
                        print("Encoded message is: ", self.current_msg)
                        # print("tx_now is: ", self.tx_now)
                        if self.tx_now:
                            print('Transmitting')
                            self.transmit()
                        print("Time: {0}:{1}:{2}".format(utc_time.hour, utc_time.minute, utc_time.second))
                    else:
                        print('Not Transmitting')
                    return BaseHardware.HeartBeat(self)
        
            except Exception as e:
                # print("Error in HeartBeat")
                # print(f"Exception type: {type(e).__name__}")
                # print(f"Exception message: {str(e)}")
                # traceback.print_exc()
                return BaseHardware.HeartBeat(self)
