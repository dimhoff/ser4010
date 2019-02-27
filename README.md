SER4010 - UART controllable Si4010 based tunable OOK/FSK RF transmitter
=======================================================================
The SER4010 module is a tunable Sub-GHz RF OOK/FSK transmitter based on the
Silicon Labs Si4010 chip. It is controlled from a PC through standard UART
based serial connection. The encoding of the frames is fully done in software
on the PC, while the actual modulation and sending is offloaded to the module.
This allows for great flexibility in the transmitted signals, while having
accurate signal timing. While having the full programming capacity of a normal
PC platform.

The Si4010 chip can operate in a frequency range of 27 to 960 MHz. Although the
actual frequency band it can effectively operate on depends on the
antenna/matching network. Within its operating band the transmitter can be
tuned to allow matching the receivers frequency, causing better
reception/range.

Hardware connections
--------------------
The SER4010 firmware is controlled using a 3.3 Volt serial(RS-232 like)
connection.

The pins are mapped as follows:

  * VCC   = 3.3 Volt Supply
  * GPIO7 = TX data (connect to RX on remote side)
  * GPIO6 = RX data (connect to TX on remote side)
  * GND   = ground

The following communication parameters are used:

  * bit rate = 9600 Baud
  * Data bits = 8
  * Parity bits = None
  * Stop bits = 1

Suggested is to use a 3.3 volt compatible USB-to-serial adapter, like a FT232
or CP2102. The module can also be directly connected to a RaspberryPi, or
similar.

## SER4010 USB PCB
The hardware/ directory contains a PCB design for a USB connected SER4010
device. This hardware is still under development and is untested testing.

Compiling the SER4010 Firmware
------------------------------
**Note: This is only useful if you plan to develop on the firmware. Else use
the precompiled firmware from GitHub releases section.**

For compiling the SER4010 firmware the Silicon Labs IDE and Si4010 Example
Programs are required. These are both free downloads from the Silabs website.
Note that the Silicon Labs IDE only runs on MS Windows.

First copy some files from the Si4010 Example Programs archive into the SER4010
source:

  * Si4010\_example\_programs/fstep\_demo/src/fstep\_startup.a51 --> ser4010/firmware/ser4010/src/custom\_startup.a51
  * Si4010\_example\_programs/common --> ser4010/firmware/common

Next start the Silicon Labs IDE and open the
ser4010/firmware/ser4010/bin/ser4010.wsp workspace. It should now be possible
to compile the project. This will generate the ser4010.hex firmware file in the
ser4010/firmware/ser4010/out/ directory.

Compiling the Software
----------------------
Run the following to compile the libser4010 library and tools:

    # cd build
    # cmake ../
    # make

The software uses /dev/ttyUSB0 by default as serial port. If your module is
connected to a different serial port you can change the default serial device
to use by using the following cmake command instead of the above one:

    # cmake ../ -DDEFAULT_SERIAL_DEV=<your serial port>

For example for the Raspberry PI use '/dev/ttyAMA0' when the module is connected
to the internal serial port.

Loading the Firmware
--------------------
TODO: Extend chapter...

You have to load the SER4010 firmware into the Si4010 module. Either by
programming it once into the non-volatile NVM memory. Or by loading it after
every reboot into the modules RAM.

See the [si4010prog tool](https://github.com/dimhoff/si4010prog)

Serial port configuration
-------------------------
Make sure the serial port to which the module is connected does not have a
serial console or any other software using it. Else the communication will get
scrambled.

Note that the Raspberry PI 1/2 have a bootloader that always outputs a string
to the internal serial port at boot. This causes a communication-out-of-sync
error the first time you run any of the ser4010 tools. Just try again.

To use the internal serial port of the Raspberry PI 3 you must disable the
Bleutooth. This can be done using the pi3-disable-bt Device tree overlay.

Using the Software
------------------
All SER4010 tools provide a '-d' option to specify which serial port to use.
This option is only needed if you didn't specify the correct serial port at
compile time, see *Compiling the Software*, or when using multiple modules.
Valid arguments for the '-d' option are for example /dev/ttyS0, /dev/ttyUSB0 or
/dev/ttyAMA0.

To test if the communication with the module works you can use the
ser4010_test_comm utility from the tools/ directory. An example of a working
module output:

    # build/tools/ser4010_test_comm
    Communication OK

The tool will either output 'Communication OK' or an error. In case of error
verify you properly connected the device (switched TX/RX?). And verify you are
using the correct serial port and no other software is using the serial port.

## ser4010_dump
The ser4010_dump tool can be used to dump the current module configuration.
This is mainly for debugging. You don't have to manual change any of these
configuration parameters since all other SER4010 tools will automatically chose
the correct parameters for there application.

Example output:

    # build/tools/ser4010_dump
    Device Info:
    ------------
    Device Type: 0x0100
    Device Revision: 3
    
    ODS settings:
    ------------
    bModulationType: OOK (0)
    bClkDiv: 5
    bEdgeRate: 0
    bGroupWidth: 6
    wBitRate: 1100
    bLcWarmInt: 8
    bDivWarmInt: 5
    bPaWarmInt: 4
    
    PA settings:
    ------------
    fAlpha: 0.000000
    fBeta: 0.000000
    bLevel: 60
    bMaxDrv: 0
    wNominalCap: 256
    
    Encoder settings:
    ------------
    Encoding: None/NRZ (0)
    
    Freq settings:
    ------------
    frequency: 433900000.000000
    freq. deviation: 104

## ser4010_somfy
TODO:

## ser4010_kaku
TODO:
