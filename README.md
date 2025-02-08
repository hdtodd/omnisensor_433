# omnisensor_433
## A Multi-Sensor System Based on `rtl_433`

Omnisensor\_433 supports transmission of data from multiple sensors and types of sensors from a single microcontroller using a single, flexible `rtl_433` message transmission protocol.

The omnisensor_433 system includes:
*  a microcontroller program, `omni.ino`, to collect sensor data and transmit it via an ISM-band (e.g., 433MHz) transmitter
*  an `rtl_433` decoder, `omni.c`, to decode those transmissions
*  a flexible protocol that enables up to 16 different formats for the transmitted 8-byte data payload.

## Getting Started

To get started, you will need:
*  a system running `rtl_433` (RTL_SDR dongle and `rtl_433` software installed and operational); clone http://github.com/merbanan/rtl_433/ .
*  a Raspberry Pi Pico-2 microcontroller
*  a computer system running the Arduino IDE with Pico-2 support installed
*  a 433MHz transmitter (or other ISM-band frequency legal in your locale): available from Amazon, for example, for ~$2US.

Then follow these steps:
1.  Download the omnisensor\_433 package (http://github.com/hdtodd/omnisensor_433).
2.  Connect pin 4 of the Pico 2 to the data pin of your transmitter; transmitter GND to Pico GND; transmitter VCC to Pico VSYS or 3V3.
3.  Copy the `omni.c` decoder file into the `rtl_433/src/devices` directory on your rtl_433 system.   Stop any instances of `rtl_433` you might already have running on that system.  Follow the instructions for installing a new decoder into `rtl_433` (see the section "How to add the decoder and write new code" in https://github.com/merbanan/rtl_433/wiki/Adding-a-new-remote-device-and-writing-the-decoder-C-code and the build instructions in https://github.com/merbanan/rtl_433/blob/master/docs/BUILDING.md ). 
4.  Verify that `omni` is one of the protocols your new `rtl_433` will recognize and decode: `rtl_433 -R` will list all the protocols; `omni` should be at or near the end of that list.  Record its protocol number.
5.  Copy the `omni.ino` program into a folder named `omni` on the computer running Arduino IDE.
6.  Connect the Pico 2 via USB into the computer running the Arduino IDE.
7.  Open the `omni.ino` file in the Arduino IDE; set the device type to Pico 2 and the port to the USB port to which the Pico 2 is attached; compile and download the `omni.ino` code into the Pico 2.
8.  Open the Arduino IDE monitoring screen to verify that the Pico 2 is composing and transmitting messages.
9.  Start your *new* `rtl_433` with `rtl_433 -R <omni protocol number>`; monitor that console for packets transmitted by the Pico 2 and confirm that the hexadecimal string received by `rtl_433` matches the string transmitted by the Pico 2.
10.  When you've verified that the data are being correctly sent and received, you can restart `rtl_433` with your normal configuration file: the only change will be that it now reports packets it receives from an `omni` device with a format that matches those that it knows about.

Congratulations!  At this point, you have successfully implemented a remote sensor transmitter and rtl_433 receiver/decoder.  With no sensor attached, the Pico 2 is simply reporting its core temperature, in ˚Centigrade, and its VCC (VSYS) USB voltage in volts.

## How It Works

## Adding Sensor Data

## Adding Your Own Sensor System

## `rtl_433` Monitoring Tools

## The `omni` Protocol

## Release History

*  V1.0: First operational version, 2025.02.10

## Author

Written by David Todd, hdtodd@gmail.com, 2025.02

