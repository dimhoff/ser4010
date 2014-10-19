ser4010
=======

UART controllable SI4010 based tunable OOK/FSK RF transmitter

Compiling the SER4010 Firmware
==============================
For compiling the SER4010 firmware the Silicon Labs IDE and SI4010 Example Programs are required. These are both free downloads from the Silabs website. Note that the Silicon Labs IDE only runs on MS Windows.

First copy some files from the SI4010 Example Programs archive into the SER4010 source:

  * Si4010\_example\_programs/fstep\_demo/src/fstep\_startup.a51 --> ser4010/firmware/ser4010/src/custom\_startup.a51
  * Si4010\_example\_programs/common --> ser4010/firmware/common

Next start the Silicon Labs IDE and open the ser4010/firmware/ser4010/bin/ser4010.wsp workspace. It should now be possible to compile the project. This will generate the ser4010.hex firmware file in the ser4010/firmware/ser4010/out/ directory.
