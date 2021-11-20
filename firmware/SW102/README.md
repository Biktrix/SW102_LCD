# SW102_Display

This is an alternate version of the SW102 firmware. Most of the settings are compatible with casainho's version, so you may refer
to the original wiki pages, but please keep in mind that they describe a different version of the firmware:
- https://github.com/OpenSource-EBike-firmware/Color_LCD/wiki/Bafang-LCD-SW102
- https://github.com/OpenSource-EBike-firmware/SW102_LCD_Bluetooth/wiki

## How to build on Windows

TBD

## How to build on Linux

* Extract https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q3-update/+download/gcc-arm-none-eabi-4_9-2015q3-20150921-linux.tar.bz2 into /usr/local/gcc-arm-none-eabi-4_9-2015q3.
* Run "make"

## Debugging bluetooth linux

### Using a NRF dongle

Use https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
Install this https://github.com/NordicSemiconductor/nrf-udev

Use this command to BLE update a target:
nrfutil dfu ble -ic NRF52 -p /dev/ttyACM0 --help

### Using a regular BLE dongle

Install this fork of nrfutil https://github.com/anszom/pc-nrfutil

Use this command to BLE update a target:
nrfutil  dfu ble-native -pkg sw102-otaupdate-xxx.zip  -a (your target BLE address)

### Post-installation

Note that the bootloader used in the open-source firmware has an (issue)[https://github.com/OpenSourceEBike/SW102_LCD_Bluetooth-bootloader/pull/3] which was only recently fixed. In order to avoid problems, when activating the display *for the first time after flashing* you may need to hold the power button for a long time (up to 10 seconds) until you see the boot animation. Otherwise, the bootloader's processing may be interrupted and the SW102 will return to DFU mode. In this case, please re-run the DFU procedure.
