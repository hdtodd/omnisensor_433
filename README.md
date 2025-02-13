# omnisensor_433 v1.0
## A Multi-Sensor System Based on `rtl_433`

Omnisensor\_433 supports transmission of data from multiple sensors and types of sensors to an rtl\_433 receiving system from a single microcontroller using a single, flexible `rtl_433` message transmission protocol.

The omnisensor_433 system includes:
*  a microcontroller program, `omni.ino`, to collect sensor data and transmit it via an ISM-band (e.g., 433MHz) transmitter;
*  an `rtl_433` decoder, `omni.c`, to decode those transmissions;
*  a flexible protocol that enables up to 16 different formats for the transmitted 8-byte data payload.

## Getting Started

To get started, you will need:
*  a system running `rtl_433` (RTL-SDR dongle and `rtl_433` software installed and operational); clone http://github.com/merbanan/rtl_433/ ;
*  a Raspberry Pi Pico 2 microcontroller (not tested, but should work on Pico 1 as well; may interfere with WiFi on Pico w models);
*  a computer system running the Arduino IDE with Pico 2 support installed (https://github.com/earlephilhower/arduino-pico);
*  a 433MHz transmitter (or other ISM-band frequency legal in your locale): available from Amazon, for example, for ~$2US.

Then follow these steps:
1.  `git clone` the omnisensor\_433 package (http://github.com/hdtodd/omnisensor_433).
2.  Connect pin 4 of the Pico 2 to the data pin of your transmitter; transmitter GND to Pico GND; transmitter VCC to Pico VSYS or 3V3.
3.  Copy the `omni.c` decoder file into the `rtl_433/src/devices` directory on your rtl_433 system.   Stop any instances of `rtl_433` you might already have running on that system.  Follow the instructions for installing a new decoder into `rtl_433` (see the section "How to add the decoder and write new code" in https://github.com/merbanan/rtl_433/wiki/Adding-a-new-remote-device-and-writing-the-decoder-C-code and the build instructions in https://github.com/merbanan/rtl_433/blob/master/docs/BUILDING.md ).  Build a new `rtl_433` that will include the `omni` decoder.
4.  Verify that `omni` is one of the protocols your new `rtl_433` will recognize and decode: `rtl_433 -R help` will list all the protocols; `omni` should be at or near the end of that list.  Record its protocol number.
5.  Copy the `omni.ino` program into a folder named `omni` in the main Arduino folder on the computer running Arduino IDE.  Install the Arduino libraries for `Wire`, `Adafruit_BME680`, `Adafruit_Sensor`, and `SPI` if not previously installed.
6.  Connect the Pico 2 via USB into the computer running the Arduino IDE.
7.  Open the `omni.ino` file in the Arduino IDE; set the device type to Pico 2 and the port to the USB port to which the Pico 2 is attached; compile and download the `omni.ino` code into the Pico 2.
8.  Open the Arduino IDE monitoring screen to verify that the Pico 2 is composing and transmitting messages.
9.  Start your *new* `rtl_433` with `rtl_433 -R <omni protocol number> -F json:`; monitor that console for packets transmitted by the Pico 2 and confirm that the hexadecimal string received by `rtl_433` matches the string transmitted by the Pico 2.
10.  When you've verified that the data are being correctly sent and received, you can restart `rtl_433` with your normal configuration file: the only change will be that it now reports packets it receives from an `omni` device with a format that matches those that it knows about.

Congratulations!  At this point, you have successfully implemented a remote sensor transmitter and rtl_433 receiver/decoder.  With no sensor attached, the Pico 2 is simply reporting its core temperature, in ˚Centigrade, and its VCC (VSYS) USB voltage in volts.

## Adding Sensor Data

If you have a Bosch BME68x environmental sensor, the `omni.ino` code is prepared to report its readings.  As programmed, `omni` uses the I2C interface to the BME68x, but SPI is a compile-time option.  

For I2C, if you have a Pico with a Qwiic/STEMMA connector, it's easiest to use that to connect your Pico to your BME68x -- just one cable and connector.  Simply connect the BME68x into your Pioc 2 and restart or reload the Pico 2.  

Otherwise, you'll need to make these connections from your BME68x to the Pico:

*  connect BME SDI to Pico pin 6, SDA (appears not work on other Pico SDA pins!)
*  connect BME SCK to Pico pin 7, SCL (appears not work on other Pico SCL pins!)
*  connect BME GND to Pico GND
*  connect BME VCC to Pico VSYS

and restart.  

Once restarted, `omni` will print readings on the Arduino IDE monitor window, and monitoring MQTT messages from `rtl_433` will report the readings from device `omni`:

`
{"time":"2025-02-13 11:39:49","protocol":275,"model":"omni","id":9,"channel":1,"temperature_C":23.4,"temperature_2_C":22.5,"humidity":7.0,"humidity_2":50.0,"pressure_hPa":988.2,"voltage_V":4.83,"mic":"CRC","mod":"ASK","freq":433.92557,"rssi":-0.215767,"snr":14.81568,"noise":-15.0315}
`
NOTE: The message format variable, *fmt* in `omni.ino`, is reported as the value of *channel* in `rtl_433`'s output, in keeping with the `rtl_433` standard for field precedence:  model \> id \> channel.  "fmt" or "format" is not one of the standard `rtl_433` labels but "channel" is.

If you prefer to use SPI to connect the Pico 2 to the BME68x, you'll need to edit the `omni.ino` code before the `setup()` section to select SPI rather than I2C and wire the Pico to the BME68x using the SPI pins.

## Adding Your Own Sensor System

Omnisensor\_433 is a development foundation that supports implementation of a variety of sensor types within a fixed signaling protocol.  The signaling protocol (pulse and gap timing) is fixed: the `.ino` microcontroller code transmits a pattern that the `rtl_433` program recognizes and passes on to the decoder module, `omni.c`.  Within that framework, the microcontroller can be programmed to deliver 8-data-byte packets in up to 16 different formats.  And, of course, data from multiple sensor types can be combined into a single type of data packet, and longer readings from a single sensor could be transmitted via multiple packets with different formats.

As distributed, `omnisensor_433` provides two formats for data:
*  `fmt=0` has a temperature and a voltage field; other fields are 0.
*  `fmt=1` has two temperature, two humidity, one barometric pressure, and one voltage field.

The `omni.c` decoder for `rtl_433`, as distributed, recognizes both formats.  It reports the various values in the message for `fmt=1` messages.  But for `fmt=0` it reports the temperature and voltage *and the full 8-byte data message in hexadecimal format*.

If you want to transmit data from your own sensor or group of sensors -- something other than two temperatures, two humidities, one barometric pressure -- you'll need to write your own encoding and decoding procedures.  There are several approaches you might take, some more complicated than others, and this section provides guidance on how to get started.

But in any case, you'll need to work on encoding and decoding your sensor data.  To facilitate that, you might want to use the ISM_Emulator prototype code, which you can run on any computer with a C++ compiler.  That emulator system allows you to develop and debug the encoding/decoding software on a single computer, without a microcontroller or transmitter or even `rtl_433` receiver.  The ISM_Emulator distribution includes operational code for microcontrollers (Arduino Uno, Sparkfun SAMD21, and Raspberry Pico 2) to emulate transmission protocols for Acurite 509, Lacrosse TH141, and Lacrosse WS7000 remote temperature humidity sensors -- all recognized by `rtl_433`.  But it also includes C++ code that was used to protoype those protocols as well as the `omni` protocol used here.

So ISM_Emulator (http://github.com/hdtodd/ISM_Emulator) provides an easy way to begin developing your own sensor without dealing with the microcontroller, sensors, and wiring.

### Quickstart

Broadly speaking, to add your own sensor data to the `omni` system, you'll need to:

*  modify `omni.ino` to collect data from your physical sensors and encode them in an 8-byte data packet, and either:
*  modify the `rtl_433` file `./src/devices/omni.c` to decode that encoded packet,
*  or write code on a third computer system to receive MQTT JSON packets and decode the hexadecimal string that represents your data (using format `fmt=0` as the format of data transmitted by `omni.ino`).

Since the starting point for either approach is to encode your data on the microcontroller, we'll start there with the details:

1.  Make sure you understand how your sensor system works and the format of data it provides you.  If there is a procedure library for your device, install it, then download any testing or demonstration programs to the Pico to confirm that your device is functioning and is wired correctly.  Those testing programs will also show you how to call procedures to retrieve data from the sensor(s), and how to present the readings in human-readable form.
2.  List the set of data from your physical sensors that you want to transmit.  For each field:
    *  Name the field.
    *  List the range of values of the reading and the precision with which the readings are taken.
    *  Determine the number of bits needed to represent those values (see below for suggestions).
3.  List clearly for yourself the form in which the field values are received from the sensor or the sensor library you are using: is it presented as a 12-bit unsigned integer or a 16-bit signed integer or a floating-point value, for example.
4.  Then, for each field, note from the first step the format you plan to use to represent the data in the transmitted packet.
5.  For the packet, lay out the assignment of each of the fields in the 8-byte message packet.  You'll likely find it most convenient to map the fields to the message packet as 16 4-bit nibbles (see below for an example).
6.  Write the program statements needed to encode each of those data fields into the message packet.  You'll likely use binary shifts, ANDs and ORs to combine source data values into message bytes.
7.  Write the program statements needed to *decode* each of the data fields from the message packet. You'll need that code for the `omni.c` decoder for `rtl_433` or for the MQTT JSON packet decoder, should you choose to take that approach for processing the transmitted data.  Be particularly careful of manipulations that convert unsigned bit fields to signed (2's complement) integers.  You'll need to manipulate the fields with shifts, ANDs, and ORs so that the packed *unsigned* data are left-aligned in the integer (16- or 32-bit), convert to `int32_t` or `int16_t`, and then shift right (if the data field is less than 16 or 32 bits).  See example below.
8.  Build a prototype in C++ to test your encoding/decoding procedures:
    *  Make a copy of `prototypes/omni.cpp` in your ISM_Emulator github clone, with a different file name.
    *  Replace the encoding/decoding statements in `pack_msg()` / `unpack_msg()` procedures with those you wrote for your own encoding and decoding, using the names of the data fields you've chosen.
    *  *Don't change the signal timing.*
    *  In `main()`, change the code to use the names of the data fields your sensor(s) are supplying and provide reasonable values for those sensor readings (be sure to include negative values if those are possible sensor readings);  change the invocation of `om` pack/unpack procedures to use your sensor data field names.
9.  Compile and run your prototype C++ program (you may need `-std=c++11` as a C++ compiler option).
10.  You should see a detailed report on the data values you've encoded, the encoded message, the values as they've been decoded, the hexadecimal representation of the full 10-byte message, and the signal pulse/gap timings that `omni` will generate through the transmitter. Verify that the reported values match what you intended to send. The message format, in nibbles, is:
```
     fi dd dd dd dd dd dd dd dd cc
where
      f=format (should be 0);
      i=device id (your choice of 0-15);
      d are your encoded data bytes; and
      c is the computed CRC8 checksum.
```
11.  If necessary, calculate by hand to confirm that the message that would be transmitted contains the data you intended to transmit.  
12.  Transfer that code to the Arduino `.ino` code:
     *  Copy your object code into `omni.ino` in place of the `class omni` object defined there but keep the `omni` name for the class.
     *  Change any `cout` statements in your object `class` to corresponding `DBG_print()` statements in the `.ino` file.
     *  In the `setup()` section, change any initiation you need to set up your sensor system or sensor library.
     *  In the `loop()` section, add the names of the data values being retrieved from your sensor, use `fmt=0` as the message format type, change the `om.pack_msg()` and `om.unpack_msg()` parameters to use your data fields.
14.  Compile and download your `omni.ino` code into your microcontroller (Pico).  Watch the monitor window. The Pico should be sending messages at 5-second or 30-second intervals (select the appropriate `#define` in `omni.ino`), and the monitor window will show both the data values being transmitted, as received from your sensor(s), and as a string of 8 hexadecimal data bytes represent the data being sent by ISM-band transmission.

At this point, you are transmitting real data from your sensors to `rtl_433`!!

### Data Formats

Before going further, you might want to review how you've encoded your data to see if you're using the most expedient approach -- both in terms of how well you've used the available 8 data bytes and in terms of your efficiency in encoding and decoding the data fields.

If the 8 bytes seems too few to carry the data you want to send, look for ways to minimze the number of bits needed for each data field.  Consider using offsetting and scaling to minimize the size of the data field.  For example, to represent voltages up to 5V5 with a precision of 0.01 volt, you might expect to need to represent 5500 distinct values, which would require a 13-bit data field.  But if you can reasonably expect voltages to always be between 3V3 and 5V0, then offsetting by 3.0 and mulitiplying by 100.0, the value would be between 000 and 250, which can be represented by an 8-bit field. You can use the value 255 to represent an under- or over-voltage reading.

You may receive data from the sensor as BCD (binary-coded decimal).  And if the 8-byte data payload is not a constraint, it might be best to simply encode the BCD in the message packet that way rather than manipulate the data in the microcontroller to reformat it.  But if the payload size is a constraint, consider converting the BCD to an unsigned int representation or to a signed 2's complement field.  

### Using Your Transmitted Data -- Simple Approach

The simplest approach to using the data transmitted from your microcontroller-sensor(s) on another computer would be to decode that data on the other computer.  The `fmt=0` messages in `omni` nominally represent a 3-nibble temperature, 2-nibble voltage, and 11 nibbles of 0's (unused).  But the `omni` decoder, as distributed, reports the full 8-byte data string in hexadecimal.  So, in fact, the simplest approach is to record your data fields in the full 8 bytes of a `fmt=0` packet on the microcontroller, allow `rtl_433` to receive and, via MQTT, publish the packet as its full hexadecimal string, which allows any other computer on that network to subscribe to the MQTT feed, look for `omni` protocol messages of `fmt=0` type, and decode the hexadecimal string using the decoder that you wrote for prototyping.

This approach would allow you to use any other computer on the network, using any language from which you can subscribe to MQTT publications, to decode and process the transmittd data.  This simple approach avoids the need to test and implement the `omni.c` decoder for `rtl_433`, and then update it in the case of future releases of `omni.c` conflicting with your customizations.

### Writing Your Own `rtl_433` Decoder -- No-So-Simple Approach

You may want to write your own `rtl_433` decoder so that `rtl_433` reports the values of all your fields in its JSON format, for example, as it appears in the "Adding Sensor Data" section.  If you've written the decoder for `omni.ino`, you've done the hard part already.  But now you'll need to incorporate that decoder into the `rtl_433` infrastructure, and that's a bit more complicated.

First, review the process for building the `rtl_433` executable ( https://github.com/merbanan/rtl_433/blob/master/docs/BUILDING.md ).  You might also find it helpful to review the "Adding a New Decoder" section here: https://github.com/merbanan/rtl_433/blob/master/docs/CONTRIBUTING.md .

Now, examine the file `./src/devices/omni.c` in the directory of your `rtl_433` git clone.  That's the file you'll be modifying to create your own data format.  It has all the timings defined as needed to act as decoder from the `omni.ino` microcontroller code. Find the definitions that look like `OMNI_MSGFMT_nn`.  That's the list of message formats that have already been defined and for which there are already decoders in `omni.c`.  Starting at nn=15 and working backward, pick the next available number.  For example, if there are no decoders other than `OMNI_MSGFMT_00` and `OMNI_MSGFMT_01`, you'd use `OMNI_MSGFMT_15`.  Now would be a good time to make a backup copy of that original `omni.c` file, since you'll be editing this one.

In `loop()` in your `omni.ino` microcontroller code, after `if (gotReading)`, set `fmt=nn`, where `nn` is your chosen format number.  Compile and download your `.ino` file to the microcontroller.  If you monitor `rtl_433`'s output, you should now see records for your device `omni` with your format number, a temperature and voltage reading (which will likely be meaningless), and the hexadecimal string representing the data sent by your microcontroller (confirm that they match).

Now, back in `omni.c`, you need to make the following additions:

1.  After the other, similar `#define` statements, add your `#define OMNI_MSGFMT_nn nn` to the list, with "nn" the format number you've chosen.
2.  Following the model of similar array definitions for other `output_fields` formats, add `static char const *const output_fields_nn[] = {}`, where nn is your format number, and list the data fields there in the same way they're listed for `output_fields_01`, for example.  You might just copy the list from `output_fields_01` and replace the fields named "temperature_C" through "voltage_V" with the fields your microcontroller is transmitting.  Use one of the common data field names if appropriate: https://github.com/merbanan/rtl_433/blob/master/docs/DATA_FORMAT.md .
3.  In the code for function `static int omni_decode()`:
    *  In the segment of code for `switch (message_fmt)`, insert a new `case OMNI_MSGFMT_nn:`, where nn is your format number.
    *  As the first line in that `case`, insert `omni.fields = output_fields_nn;`.
    *  Insert your decoding statements, copied from the prototype or from your `omni.ino` code.  Be sure to instantiate the variables for the various fields you're decoding.  For real numbers, use `double` rather than `float` (`rtl_433` doesn't have a `DATA_FLOAT` descriptor for fields).
    *  Invoke `data_make()` to list the JSON labels `rtl_433` will report, the format for displaying your data fields, and the names of the fields into which you decoded your data.  You can simply copy the list from `case OMNI_MSGFMT_01` and replace lines `temperature_C` through `voltage_V` with your list of descriptors and fields, editing the formatting descriptors as necessary.
    *  Don't forget to end your `case` section with a `break;`.
4.  `cd` to your `rtl_433` directory and rebuild `rtl_433` (`cmake ...`).
5.  Stop `rtl_433` if you have one running.  Run your new `rtl_433` with `sudo ./build/src/rtl_433 -R 275 -F json: -c` and watch for your microcontroller sensor data to be reported, with your data fields and your `fmt=nn`.  (The trailing `-c` overrides any configuration file you may have set up as a production environment.)
6.  Edit your `omni.c` file if the data values reported by `rtl_433` don't match those your on the Arduino IDE monitor window for your microcontroller.
7.  When the values reported by `rtl_433` match those sent by your microcontroller, install your new `rtl_433` for production and restart with your standard configuration file.

You now have a microcontroller-based sensor system that is fully recognized by `rtl_433`, and you can integrate those sensor readings with your other monitoring systems.

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
      polynomial 0x97, init 0x00

A format=0 message simply reports the core temperature and input power voltage of the microcontroller and is the format used if no data sensor is present.  For format=0 messages, the message nibbles are to be read as:

     fi tt t0 00 00 00 00 00 vv cc

     f: format of datagram, 0-15
     i: id of device, 0-15
     t: Pico 2 core temperature: °C *10, 12-bit, 2's complement integer
     0: bytes should be 0 if format is really fmt=0; otherwise, undefined
     v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
     c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
            divisor polynomial 0x97, no reflections or inversions

A format=1 message format is provided as a more complete example.  It uses the Bosch BME68x environmental sensor as a data source.
It is an indoor-outdoor temperature/humidity/pressure sensor, and the message packet has the following fields:
    indoor temp, outdoor temp, indoor humidity, outdoor humidity,
    barometric pressure, sensor power VCC.
The data fields are binary values, 2's complement for temperatures.
For format=1 messages, the message nibbles are to be read as:

     fi 11 12 22 hh gg pp pp vv cc

     f: format of datagram, 0-15
     i: id of device, 0-15
     1: sensor 1 temp reading (e.g, indoor),  °C *10, 12-bit, 2's complement integer
     2: sensor 2 temp reading (e.g, outdoor), °C *10, 12-bit, 2's complement integer
     h: sensor 1 humidity reading (e.g., indoor),  %RH as 8-bit integer
     g: sensor 2 humidity reading (e.g., outdoor), %RH as 8-bit integer
     p: barometric pressure * 10, in hPa, as 16-bit integer, 0..6553.5 hPa
     v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
     c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
            divisor polynomial 0x97, no reflections or inversions


## How It Works

When `rtl_433` sees a signaling pattern that matches the pattern defined in `r_device omni` in the `omni.c` code, it invokes the `omni_decode()` function in the `omni.c` file.  That function determines if there are 2 matching patterns of 80 bits in the 320-bit data bit stream.  If so, the decoder presumes that it has a valid `omni` message.  It further validates the 80-bit message by confirming that the CRC byte computed from the prior 9 bytes matches the transmitted CRC byte.

If the message passes those validation tests, the message format number is extracted from the message packet, and the decoder for that format is selected in the `switch` statement to decode the fields and prepare the description for `rtl_433` to publish.

## Release History

*  V1.0: First operational version, 2025.02.10

## Author

Written by David Todd, hdtodd@gmail.com, 2025.02

