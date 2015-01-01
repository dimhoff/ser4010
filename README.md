SER4010 - UART controllable Si4010 based tunable OOK/FSK RF transmitter
=======================================================================
The SER4010 module is a tunable Sub-GHz RF OOK/FSK transmitter based on the Silicon Labs Si4010 chip. It is controlled from a PC through standard UART based serial connection. The encoding of the frames is fully done in software on the PC, while the actual modulation and sending is offloaded to the module. This allows for great flexibility in the transmitted signals, while having accurate signal timing. While having the full programming capacity of a normal PC platform.

The Si4010 chip can operate in a frequency range of 27 to 960 MHz. Although the actual frequency band it can effectively operate on depends on the antenna/matching network. Within its operating band the transmitter can be tuned to allow matching the receivers frequency, causing better reception/range.

Hardware connections
--------------------
The SER4010 firmware is controlled using a 3.3 Volt serial(RS-232 like) connection.

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

Suggested is to use a 3.3 volt compatible USB-to-serial adapter, like a FT232 or CP2102. The module can also be directly connected to a RaspberryPi, or similar.

Compiling the Software
----------------------
Run the following to compile the libser4010 library and tools:

  * cd build
  * cmake ../
  * make

Compiling the SER4010 Firmware
------------------------------
**Note: This is only useful if you plan to develop on the firmware. Else use the precompiled firmware from GitHub releases section.**

For compiling the SER4010 firmware the Silicon Labs IDE and Si4010 Example Programs are required. These are both free downloads from the Silabs website. Note that the Silicon Labs IDE only runs on MS Windows.

First copy some files from the Si4010 Example Programs archive into the SER4010 source:

  * Si4010\_example\_programs/fstep\_demo/src/fstep\_startup.a51 --> ser4010/firmware/ser4010/src/custom\_startup.a51
  * Si4010\_example\_programs/common --> ser4010/firmware/common

Next start the Silicon Labs IDE and open the ser4010/firmware/ser4010/bin/ser4010.wsp workspace. It should now be possible to compile the project. This will generate the ser4010.hex firmware file in the ser4010/firmware/ser4010/out/ directory.
