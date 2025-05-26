# Omni00 v1.0
## A format 00 message decoder for rtl_433 Omni Multisensor

Omnisensor\_433 supports transmission of data from multiple sensors and types of sensors to an rtl\_433 receiving system from a single microcontroller using a single, flexible `rtl_433` message transmission protocol.

However, the 'omni' decoder for `rtl_433` was also designed to support "pass-through" transmission of data from the microcontroller to the end-consumer process without requiring modifications to the `rtl_433` 'omni.c' decoder.  Messages formatted as "format 0" ("fmt=0" in the .ino code) are published in JSON format by 'omni.c' as an 8-byte hexadecimal data payload. The data fields encoded by the microcontroller can be decoded into the appropriate fields by the end-consuming process from that hexadecimal string.

This program, 'Omni00.py', demonstrates how to do that decoding.

## Quickstart

To get started, you will need a system that runs the 'omnisensor' code in this repository:
*  a system running `rtl_433`; ensure that the 'omni' protocol is supported (protocol number 277);
*  a microcontroller system running 'omni.ino' or ['WP_433.ino'](http://github.com/hdtodd/WP_433).

Then:
*  Modify the '.ino' code to use "fmt=0" rather than "fmt=1" and download that code to the microcontroller.
*  Confirm that the `rtl_433` server is receiving the microcontroller broadcasts by monitoring its JSON MQTT feed with a command such as `mosquitto_sub -h <your rtl_433 server> -t "rtl_433/<your rtl_433 server>/events"; watch for publications by "Omni Multisensor" on channel 0 (the '.ino' "fmt" value is reported as "channel" by `rtl_433`' "omni.c" decoder); you should see a field labeled "payload" with 16 hexadecimal nibbles (8 bytes);
*  Start the Python script in this repository with "./Omni00.py"; you should see *just* the "Omni Multisenso" broadcasts, but with both the hexadecimial payload data *and* the decoded data fields; verify that the data are correct by referring back to the Arduino IDE display of data being transmitted by the microcontroller.

For example, on the Arduino IDE serial monitor display, you might see:
```
11:44:53.445 -> Transmit msg 5219	iTemp=22.40˚C, oTemp=21.50˚C, iHum=45.00%, light=50.00%, Press=1003.70hPa, VCC=4.88volts	in hex: 0x 09 0E 00 D7 2D 32 27 35 BC 91 
11:44:53.445 -> 	The msg packet, length=80, as a string of bits: 00001001000011100000000011010111001011010011001000100111001101011011110010010001
```

and from `mosquitto_subscribe` you would see:
```
{"time":"2025-05-26 11:44:53","protocol":277,"model":"Omni Multisensor","id":9,"channel":0,"temperature_C":22.4,"voltage_V":4.88,"payload":"0e00d72d322735bc","mic":"CRC","mod":"ASK","freq":433.93354,"rssi":-0.259911,"snr":21.52537,"noise":-21.7853}
```

and from `Omni00.py` you would see:
```
11:44:53 Omni Multisensor/9/0 Payload =  0e00d72d322735bc  = 0xe 0x0 0xd7 0x2d 0x32 0x27 0x35 0xbc 
			      itemp=22.4℃, otemp=21.5℃, ihum=45%, light=50%, press=1003.7 hPa, volts=4.88V
```

## Adding Your Own Sensor Data

Follow the guidance in the `omnisensor_433` README in this repository for encoding your fields on your microcontroller.  Use "fmt = 0" as the Omni Multisensor protocol format ("channel" number as reported by the `rtl_433` decoder).  Download your '.ino' file to your microcontroller.

Verify that the broadcasts from your microcontroller are being received and processed by `rtl_433` by monitoring the MQTT publications with the `mosquitto_sub` command above.

Modify the code in `Omni00.py` to decode the hexadecimal payload data, reversing the process by which it was encoded on the microcontroller.

Start your `Omni00.py` script and confirm that the data are being correctly decoded by it by monitoring both the Arduino IDE serial monitor display window and the output from `Omni00.py`.

Finally, adapt the code in `Omni00.py` to implement your intended end-user service.

## The `omni` Protocol

The omni signaling protocol is OOK PWM (on-off keying with pulse-width modulation). It uses fixed period of 600μsec for data bits: "0" is a 400μs pulse followed by a 200μs gap; a "1" is a 200μs pulse followed by a 400μs gap.  The data portion of a message is preambled by four long 600μsec pulses + 600μsec gaps.  This signaling protocol is similar to the Lacrosse TX141TH-BV2.

A single data packet looks as follows:

1) preamble - 600μs high followed by 600μs low, repeated 4 times:
```
     ----      ----      ----      ----
    |    |    |    |    |    |    |    |
          ----      ----      ----      ----
```
2) a train of 80 data pulses with fixed 600μs period follows immediately:
```
     ---    --     --     ---    ---    --     ---
    |   |  |  |   |  |   |   |  |   |  |  |   |   |
         --    ---    ---     --     --    ---     -- ....
```
A logical 0 is 400μs of high followed by 200μs of low.

A logical 1 is 200μs of high followed by 400μs of low.

Thus, in the example pictured above the bits are 0 1 1 0 0 1 0 ...

The omni microcontroller sends 4 identical packets of (4-pulse preamble followed by 80 data bits) in a single burst, for a
total of 336 bits requiring ~212μs.

The last packet in a burst is followed by a postamble low of at least 1250μs.

These 4-packet bursts repeat every 30 seconds. 

The message in each packet is 10 bytes / 20 nibbles:

    [fmt] [id] 16*[data] [crc8] [crc8]

- fmt is a 4-bit message data format identifier
- id is a 4-bit device identifier
- data are 16 nibbles = 8 bytes of data payload fields,
      interpreted according to 'fmt'
- crc8 is 2 nibbles = 1 byte of CRC8 checksum of the first 9 bytes:
      polynomial 0x97, init 0xaa

A format=0 message simply reports the core temperature and input power voltage of the microcontroller and is the format used if no data sensor is present.  For format=0 messages, the message nibbles are to be read as:

     fi tt t0 00 00 00 00 00 vv cc

     f: format of datagram, 0-15
     i: id of device, 0-15
     t: Pico 2 core temperature: °C *10, 12-bit, 2's complement integer
     0: bytes should be 0 if format is really fmt=0; otherwise, undefined
     v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
     c: CRC8 checksum of bytes 1..9, initial remainder 0xaa,
        divisor polynomial 0x97, no reflections or inversions

A format=1 message format is provided as a more complete example.  In the `.ino` file, the code uses the Bosch BME68x environmental sensor as a data source.
It is an indoor-outdoor temperature/humidity/pressure/VOC sensor, and the message packet has the following fields:
indoor temp, outdoor temp, indoor humidity, light intensity %,
barometric pressure, sensor power VCC.
The data fields are binary values, 2's complement for temperatures.
For format=1 messages, the message nibbles are to be read as:

     fi 11 12 22 hh ll pp pp vv cc

     f: format of datagram, 0-15
     i: id of device, 0-15
     1: sensor 1 temp reading (e.g, indoor),  °C *10, 12-bit, 2's complement integer
     2: sensor 2 temp reading (e.g, outdoor), °C *10, 12-bit, 2's complement integer
     h: sensor 1 humidity reading (e.g., indoor),  %RH as 8-bit integer
     l: light intensity reading as 8-bit integer %
     p: barometric pressure * 10, in hPa, as 16-bit integer, 0..6553.5 hPa
     v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
     c: CRC8 checksum of bytes 1..9, initial remainder 0xaa,
        divisor polynomial 0x97, no reflections or inversions


## Release History

| Version | Changes |
|---------|---------|
| V1.0    | 2025.06.01 First operational version. |

## Author

Written by David Todd, hdtodd@gmail.com, 2025.05
