import sys
import glob
import serial
import subprocess
import os

def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):

            pass
    return result


if __name__ == '__main__':
    dirpath = os.getcwd()
    bootloader = "/Users/mlaliberte/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/bin/bootloader_dio_40m.bin"
    appboot = "/Users/mlaliberte/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    #subprocess.run("pio run && pio run -t buildfs", shell=True)
    procs = []
    procs2 = []
    for port in serial_ports():
        if ("usbserial" in port):
            print(port)
            #args =("cmd","/c","esptool.py","-p",port,"--baud","2000000","write_flash","0x10000",dirpath+"/.pio/build/esp32dev/firmware.bin","0x8000",dirpath+"/.pio/build/esp32dev/partitions.bin","0x221000",dirpath+"/.pio/build/esp32dev/spiffs.bin")
            args_str = "python3 -m esptool --chip esp32 -p "+port+" erase_flash && python3 -m esptool --chip esp32 -p "+port+" --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 "+bootloader+" 0x8000 "+dirpath+"/.pio/build/esp32dev/partitions.bin 0xe000 "+appboot+" 0x10000 "+dirpath+"/.pio/build/esp32dev/firmware.bin 0x221000 "+dirpath+"/.pio/build/esp32dev/spiffs.bin"
            #args_str = "python3 -m esptool --chip esp32 -p "+port+" erase_flash"
            #args_str = "pio run -t nobuild -t upload --upload-port "+port+" && pio run -t nobuild -t uploadfs --upload-port "+port
            procs.append(subprocess.Popen(args_str,shell=True))
    for p in procs:
        p.wait()