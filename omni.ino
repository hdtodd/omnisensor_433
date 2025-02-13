/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

    omni.ino

/** @file
    Omni multi-sensor protocol.

    Copyright (C) 2025 H. David Todd <hdtodd@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

/* clang-format off */
/**
Omni multisensor protocol.

The protocol is for the extensible wireless sensor 'omni'
-  Single transmission protocol
-  Flexible 64-bit data payload field structure
-  Extensible to a total of 16 possible multi-sensor data formats

The 'sensor' is actually a programmed microcontroller (e.g.,
Raspberry Pi Pico 2 or similar) with multiple possible data-sensor
attachments.  A packet 'format' field indicates the format of the data
packet being sent.

NOTE: the rtl_433 decoder, omni.c, reports the packet format, "fmt" in the
code here, as "channel", in keeping with the standard nomenclature for
rtl_433 data fields.  "Format" is a better descriptor for the manner in
which the field is used here, but "channel" is a better, standard label.

The omni protocol is OOK modulated PWM with fixed period of 600μs
for data bits, preambled by four long startbit pulses of fixed period equal
to 1200μs. It is similar to the Lacrosse TX141TH-BV2.

A single data packet looks as follows:
1) preamble - 600μs high followed by 600μs low, repeated 4 times:

     ----      ----      ----      ----
    |    |    |    |    |    |    |    |
          ----      ----      ----      ----

2) a train of 80 data pulses with fixed 600μs period follows immediately:

     ---    --     --     ---    ---    --     ---
    |   |  |  |   |  |   |   |  |   |  |  |   |   |
         --    ---    ---     --     --    ---     -- ....

A logical 0 is 400μs of high followed by 200μs of low.
A logical 1 is 200μs of high followed by 400μs of low.

Thus, in the example pictured above the bits are 0 1 1 0 0 1 0 ...

The omni microcontroller sends 4 of identical packets of
4-pulse preamble followed by 80 data bits in a single burst, for a
total of 336 bits requiring ~212μs.

The last packet in a burst is followed by a postamble low
of at least 1250μs.

These 4-packet bursts repeat every 30 seconds. 

The message in each packet is 10 bytes / 20 nibbles:

    [fmt] [id] 16*[data] [crc8] [crc8]

- fmt is a 4-bit message data format identifier
- id is a 4-bit device identifier
- data are 16 nibbles = 8 bytes of data payload fields,
      interpreted according to 'fmt'
- crc8 is 2 nibbles = 1 byte of CRC8 checksum of the first 9 bytes:
      polynomial 0x97, init 0x00

A format=0 message simply transmits the core temperature and input power
voltage of the microcontroller and is the format used if no data
sensor is present.  For format=0 messages, the message
nibbles are to be read as:

     fi tt t0 00 00 00 00 00 vv cc

     f: format of datagram, 0-15
     i: id of device, 0-15
     t: Pico 2 core temperature: °C *10, 12-bit, 2's complement integer
     0: bytes should be 0
     v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
     c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
        divisor polynomial 0x97, no reflections or inversions

A format=1 message format is provided as a more complete example.
It uses the Bosch BME688 environmental sensor as a data source.
It is an indoor-outdoor temperature/humidity/pressure sensor, and the
message packet has the following fields:
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

When asserting/deasserting voltage to the signal pin, timing
is critical.  The strategy of this program is to have the
"playback" -- the setting of voltages at specific times to
convey information -- be as simple as possible to minimize
computer processing delays in the signal-setting timings.
So the program generates a "waveform" as a series of commands
to assert/deassert voltages and to delay the specified times
to communicate information.  Those commands are entered into
an array to represent the waveform.

The playback, then, just retrieves the commands to assert/deassert
voltages and delay specific length of time and executes them, with
minimal processing overhead.

This program was modeled, somewhat, on Joan's pigpiod (Pi GPIO daemon)
code for waveform generation.  But because the Arduino-like devices
are single-process/single-core rather than multitasking OSes, the code
here does not need to provide for contingencies in that multi-tasking
environment -- it just generates the waveform description which a subsequent
code module uses to drive the transmitter voltages.

The BME68x code for reading temp/press/hum/VOC was adapted from
the Adafruit demo program http://www.adafruit.com/products/3660
The BME68x temperature reading may need calibration against
an external thermometer.  The DEFINEd parameter 'BME_TEMP_OFFSET'
(below) can be used to perform an adjustment, if needed.
*/
/* clang-format on */

// For Pico 2
#define VSYSPin 29                     // VSYS is on pin 29 on Pico 2 (may conflict with WiFi!)
#define ADCRES  12                     // Analog-to-digital resolution is 12 bits
#define RES     ((float)(1 << ADCRES)) // Divisor for 12-bit ADC

// Delay transmissions 30 sec or 5 sec; loop takes ~600ms
#define DELAY 29419 // Time between messages in ms
// #define DELAY 4419 // Time between messages in ms

// Couldn't find the IDE macro that says if serial port is Serial or SerialUSB
// So try this; if it doesn't work, specify which
#ifdef ARDUINO_ARCH_SAMD
#define SERIALP SerialUSB
#else
#define SERIALP Serial
#endif

#define DEBUG // SET TO #undef to disable execution trace

#ifdef DEBUG
#define DBG_begin(...)   SERIALP.begin(__VA_ARGS__);
#define DBG_print(...)   SERIALP.print(__VA_ARGS__)
#define DBG_write(...)   SERIALP.write(__VA_ARGS__)
#define DBG_println(...) SERIALP.println(__VA_ARGS__)
#else
#define DBG_begin(...)
#define DBG_print(...)
#define DBG_write(...)
#define DBG_println(...)
#endif

#include "Adafruit_BME680.h"
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <Wire.h>

// 433MHz transmitter settings
#ifdef PICO_RP2350
#define TX  3           // transmit data line connected to Pico 2 GPIO 3
#define LED LED_BUILTIN // LED active on GPIO 25 when transmitting
#else
#define TX  4           // transmit data line connected to SAMD21 GPIO 4
#define LED 13          // LED active on GPIO 13 when transmitting
#endif

#define REPEATS 4 // Number of times to repeat packet in one transmission

// BME688 settings for Adafruit I2C connection
#define BME_SCK              13
#define BME_MISO             12
#define BME_MOSI             11
#define BME_CS               10

#define SEALEVELPRESSURE_HPA (1013.25)    // Standard conversion constant
#define BME_TEMP_OFFSET      (-1.7 / 1.8) // Calibration offset in Fahrenheit

/* "SIGNAL_T" are the possible signal types.  Each signal has a
    type (index), name, duration of asserted signal high), and duration of
    deasserted signal (low).  Durations are in microseconds.  Either or
    both duration may be 0, in which case the signal voltage won't be changed.

    Enumerate the possible signal duration types here for use as indices
    Append additional signal timings here and then initialize them
    in the device initiation of the "signals" array
    Maintain the order here in the initialization code in the device class
*/
enum SIGNAL_T {
    NONE     = -1,
    SIG_SYNC = 0,
    SIG_SYNC_GAP,
    SIG_ZERO,
    SIG_ONE,
    SIG_IM_GAP,
    SIG_PULSE
};

// Structure of the table of timing durations used to signal
typedef struct {
    SIGNAL_T sig_type;   // Index the type of signal
    String sig_name;     // Provide a brief descriptive name
    uint16_t up_time;    // duration for pulse with voltage up
    uint16_t delay_time; // delay with voltage down before next signal
} SIGNAL;

/*  libcrc8.c
    From http://github.com/hdtodd/CRC8-Library
    HD Todd, February, 2022

    Predefined table of CRC-8 lookup bytes computed using
    the polynomial 0x97, also know as "C2". Per Koopman,
    "arguably the best" for messages up to 119 bits long

   This table can be recomputed for a different polynomial
   See original library distribution on github
*/
uint8_t CRC8POLY       = 0x97;
uint8_t CRC8Table[256] = {
        0x00, 0x97, 0xb9, 0x2e, 0xe5, 0x72, 0x5c, 0xcb, 0x5d, 0xca, 0xe4, 0x73,
        0xb8, 0x2f, 0x01, 0x96, 0xba, 0x2d, 0x03, 0x94, 0x5f, 0xc8, 0xe6, 0x71,
        0xe7, 0x70, 0x5e, 0xc9, 0x02, 0x95, 0xbb, 0x2c, 0xe3, 0x74, 0x5a, 0xcd,
        0x06, 0x91, 0xbf, 0x28, 0xbe, 0x29, 0x07, 0x90, 0x5b, 0xcc, 0xe2, 0x75,
        0x59, 0xce, 0xe0, 0x77, 0xbc, 0x2b, 0x05, 0x92, 0x04, 0x93, 0xbd, 0x2a,
        0xe1, 0x76, 0x58, 0xcf, 0x51, 0xc6, 0xe8, 0x7f, 0xb4, 0x23, 0x0d, 0x9a,
        0x0c, 0x9b, 0xb5, 0x22, 0xe9, 0x7e, 0x50, 0xc7, 0xeb, 0x7c, 0x52, 0xc5,
        0x0e, 0x99, 0xb7, 0x20, 0xb6, 0x21, 0x0f, 0x98, 0x53, 0xc4, 0xea, 0x7d,
        0xb2, 0x25, 0x0b, 0x9c, 0x57, 0xc0, 0xee, 0x79, 0xef, 0x78, 0x56, 0xc1,
        0x0a, 0x9d, 0xb3, 0x24, 0x08, 0x9f, 0xb1, 0x26, 0xed, 0x7a, 0x54, 0xc3,
        0x55, 0xc2, 0xec, 0x7b, 0xb0, 0x27, 0x09, 0x9e, 0xa2, 0x35, 0x1b, 0x8c,
        0x47, 0xd0, 0xfe, 0x69, 0xff, 0x68, 0x46, 0xd1, 0x1a, 0x8d, 0xa3, 0x34,
        0x18, 0x8f, 0xa1, 0x36, 0xfd, 0x6a, 0x44, 0xd3, 0x45, 0xd2, 0xfc, 0x6b,
        0xa0, 0x37, 0x19, 0x8e, 0x41, 0xd6, 0xf8, 0x6f, 0xa4, 0x33, 0x1d, 0x8a,
        0x1c, 0x8b, 0xa5, 0x32, 0xf9, 0x6e, 0x40, 0xd7, 0xfb, 0x6c, 0x42, 0xd5,
        0x1e, 0x89, 0xa7, 0x30, 0xa6, 0x31, 0x1f, 0x88, 0x43, 0xd4, 0xfa, 0x6d,
        0xf3, 0x64, 0x4a, 0xdd, 0x16, 0x81, 0xaf, 0x38, 0xae, 0x39, 0x17, 0x80,
        0x4b, 0xdc, 0xf2, 0x65, 0x49, 0xde, 0xf0, 0x67, 0xac, 0x3b, 0x15, 0x82,
        0x14, 0x83, 0xad, 0x3a, 0xf1, 0x66, 0x48, 0xdf, 0x10, 0x87, 0xa9, 0x3e,
        0xf5, 0x62, 0x4c, 0xdb, 0x4d, 0xda, 0xf4, 0x63, 0xa8, 0x3f, 0x11, 0x86,
        0xaa, 0x3d, 0x13, 0x84, 0x4f, 0xd8, 0xf6, 0x61, 0xf7, 0x60, 0x4e, 0xd9,
        0x12, 0x85, 0xab, 0x3c};

uint8_t crc8(uint8_t *msg, int lengthOfMsg, uint8_t init)
{
    while (lengthOfMsg-- > 0)
        init = CRC8Table[(init ^ *msg++)];
    return init;
};

/* ISM_Device is the base class descriptor for creating transmissions
   compatible with various other specific OOK-PWM devices.  It contains
   the list of signals for the transmitter driver, the procedure needed
   to insert signals into the list, and the procedure to play the signals
   through the transmitter.
*/
class ISM_Device {
  public:
    // These are used by the device object procedures
    //   to process the waveform description into a command list
    SIGNAL_T cmdList[400];
    uint16_t listEnd   = 0;
    String Device_Name = "ISM Device";
    SIGNAL *signals;

    ISM_Device() {};

    // Inserts a signal into the commmand list
    void insert(SIGNAL_T signal)
    {
        cmdList[listEnd++] = signal;
        return;
    };

    // Drive the transmitter voltages per the timings of the signal list
    void playback()
    {
        SIGNAL_T sig;
        for (int i = 0; i < listEnd; i++) {
            sig = cmdList[i];
            if (sig == NONE) { // Terminates list but should never be executed
                DBG_print(F(" \tERROR -- invalid signal in 'playback()': "));
                DBG_print((int)cmdList[i]);
                DBG_print((cmdList[i] == NONE) ? " (NONE)" : "");
                return;
            };
            if (signals[sig].up_time > 0) {
                digitalWrite(TX, HIGH);
                delayMicroseconds(signals[sig].up_time);
            };
            if (signals[sig].delay_time > 0) {
                digitalWrite(TX, LOW);
                delayMicroseconds(signals[sig].delay_time);
            };
        }; // end loop
    }; // end playback()
}; // end class ISM_device

class omni : public ISM_Device {

  public:
    // omni timing durations
    // Timings adjusted for Pico 2 based on rtl_433 recording
    // Apparently more delay in pulse & less in gap execution
    /* clang-format off */
    int sigLen             = 6;
    SIGNAL omni_signals[6] = {
            {SIG_SYNC,     "Sync",     600,  600},  // 600, 600
            {SIG_SYNC_GAP, "Sync-gap", 200,  800},  // 200, 800
            {SIG_ZERO,     "Zero",     400,  200},  // 400, 200
            {SIG_ONE,      "One",      200,  400},  // 200, 400
            {SIG_IM_GAP,   "IM_gap",     0, 1250},
            {SIG_PULSE,    "Pulse",      0,    0}   // spare
    };
    /* clang-format on */

    // Instantiate the device by linking 'signals' to our device timing
    omni()
    {
        Device_Name = F("omni");
        signals     = omni_signals;
    };

    /* clang-format off */
    /* Routines to create 80-bit omni datagrams from sensor data
       Pack <fmt, id, iTemp, oTemp, iHum, oHum, press, volts> into a 72-bit
         datagram appended with a 1-byte CRC8 checksum (10 bytes total).
         Bit fields are binary-encoded, most-significant-bit first.

         Inputs:
         uint8_t  <fmt>   is a 4-bit unsigned integer datagram type identifier
         uint8_t  <id>    is a 4-bit unsigned integer sensor ID
         uint16_t <temp>  is a 16-bit signed twos-complement integer representing
                          10*(temperature reading)
         uint8_t  <hum>   is an 8-bit unsigned integer
                          representing the relative humidity as integer
         uint16_t <press> is a 16-bit unsigned integer representing
                          10*(barometric pressure) (in hPa)
         uint16_t <volts> is a 16-bit unsigned integer
                          representing 100*(voltage-3.00) volts
         uint8_t  <msg>   is an array of at least 10 unsigned 8-bit
                          uint8_t integers

         Output in "msg" as nibbles:

             fi 11 12 22 hh gg pp pp vv cc

             f: format of datagram, 0-15
             i: id of device, 0-15
             1: sensor 1 temp reading (e.g, indoor),  °C *10, 2's complement
             2: sensor 2 temp reading (e.g, outdoor), °C *10, 2's complement
             h: sensor 1 humidity reading (e.g., indoor),  %RH as integer
             g: sensor 2 humidity reading (e.g., outdoor), %RH as integer
             p: barometric pressure * 10, in hPa, 0..65535 hPa*10
             v: (VCC-3.00)*100, in volts,  000...255 volts*100
             c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
                    divisor polynomial 0x97, no reflections or inversions
    */
    /* clang-format on */

    void pack_msg(uint8_t fmt, uint8_t id, int16_t iTemp, int16_t oTemp,
            uint8_t iHum, uint8_t oHum, uint16_t press, uint16_t volts,
            uint8_t msg[])
    {
        msg[0] = (fmt & 0x0f) << 4 | (id & 0x0f);
        msg[1] = (iTemp >> 4) & 0xff;
        msg[2] = ((iTemp << 4) & 0xf0) | ((oTemp >> 8) & 0x0f);
        msg[3] = oTemp & 0xff;
        msg[4] = iHum & 0xff;
        msg[5] = oHum & 0xff;
        msg[6] = (press >> 8) & 0xff;
        msg[7] = press & 0xff;
        msg[8] = volts & 0xff;
        msg[9] = crc8(msg, 9, 0x00);
        return;
    };

    // Unpack message into data values it represents
    void unpack_msg(uint8_t msg[], uint8_t &fmt, uint8_t &id, int16_t &iTemp,
            int16_t &oTemp, uint8_t &iHum, uint8_t &oHum, uint16_t &press,
            uint8_t &volts)
    {
        if (msg[9] != crc8(msg, 9, 0x00)) {
            DBG_println(
                    F("Attempt unpack of invalid message packet: CRC8 checksum error"));
            fmt   = 0;
            id    = 0;
            iTemp = 0;
            oTemp = 0;
            iHum  = 0;
            oHum  = 0;
            press = 0;
            volts = 0;
        }
        else {
            fmt   = msg[0] >> 4;
            id    = msg[0] & 0x0f;
            iTemp = ((int16_t)((((uint16_t)msg[1]) << 8) | (uint16_t)msg[2])) >> 4;
            oTemp =
                    ((int16_t)((((uint16_t)msg[2]) << 12) | ((uint16_t)msg[3]) << 4)) >>
                    4;
            iHum  = msg[4];
            oHum  = msg[5];
            press = ((uint16_t)(msg[6] << 8)) | msg[7];
            volts = msg[8];
        };
        return;
    };

    void make_wave(uint8_t *msg, uint8_t msgLen)
    {
        listEnd = 0;

        // Repeat message "REPEAT" times in one transmission
        for (uint8_t j = 0; j < REPEATS; j++) {
            // Preamble
            for (uint8_t i = 0; i < 4; i++)
                insert(SIG_SYNC);

            // The data packet
            for (uint8_t i = 0; i < msgLen; i++)
                insert(((uint8_t)((msg[i / 8] >> (7 - (i % 8))) & 0x01)) == 0
                                ? SIG_ZERO
                                : SIG_ONE);
        };

        // Postamble and terminal marker for safety
        insert(SIG_IM_GAP);
        cmdList[listEnd] = NONE;
    }; // end .make_wave()
}; // end class omni

// Global variables

// Create BME object
// We use the I2C version but SPI is an option
Adafruit_BME680 bme; // I2C
// Adafruit_BME680 bme(BME_CS); // hardware SPI
// Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

omni om;          // The omni object as a global
int count    = 0; // Count the packets sent
bool haveBME = false;
bool first   = true;

void setup(void)
{
    DBG_begin(9600);
    analogReadResolution(ADCRES);
    delay(1000);
    DBG_println();
    // Force transmit pin and LED pin low as initial conditions
    pinMode(TX, OUTPUT);
    digitalWrite(TX, LOW);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    haveBME = bme.begin();
    if (haveBME) {
        // Set up oversampling and filter initialization
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); // 320*C for 150 ms
    };
}; // End setup()

void loop(void)
{
    uint8_t omniLen = 80;  // omni messages are 80 bits long
    uint8_t msg[10] = {0}; // and 10 bytes long
    uint8_t fmt, id = 9, ihum, ohum, volts;
    int16_t itemp, otemp;
    uint16_t press;
    float itempf, otempf, ihumf, ohumf, pressf, voltsf, voc;
    bool gotReading = false;

    if (first) {
        DBG_println(F("\nStarting omni multisensor test transmission"));
        if (!haveBME)
            DBG_println("BME not found; transmitting core temp and VCC");
    };

    // Get the readings from the BME688 if we have one
    if (haveBME)
        gotReading = bme.performReading();
    if (haveBME && !gotReading)
        DBG_println(F("BME688 failed to perform reading :-("));
    // If we have readings, use them
    if (gotReading) {
        // Pack readings for ISM transmission
        fmt   = 1;
        itemp = (uint16_t)((bme.temperature + 0.05 + BME_TEMP_OFFSET) *
                           10); // adjust & round
        otemp = (uint16_t)((analogReadTemp() + 0.05) * 10.0);
        ihum  = (uint16_t)((bme.humidity + 0.5) * 10.0); // round
        ohum  = 50;
        press = (uint16_t)(bme.pressure / 10.0);         // hPa * 10
        voc   = (uint16_t)(bme.gas_resistance / 1000.0); // KOhms
        volts =
                (uint8_t)(100.0 * (((float)analogRead(VSYSPin)) / RES * 3.0 * 3.3 - 3.0) +
                          0.5);
    }
    else {
        // Use Pico 2 core temp as "inside" temp, itemp
        fmt   = 0;
        itemp = (uint16_t)((analogReadTemp() + 0.05) * 10.0);
        otemp = 0;
        ihum  = 0;
        ohum  = 0;
        press = 0;
        voc   = 0;
        volts =
                (uint8_t)(100.0 * (((float)analogRead(VSYSPin)) / RES * 3.0 * 3.3 - 3.0) +
                          0.5);
    };

    // Pack the message, create the waveform, and transmit
    om.pack_msg(fmt, id, itemp, otemp, ihum, ohum, press, volts, msg);
    om.make_wave(msg, omniLen);
    digitalWrite(LED, HIGH);
    om.playback();
    digitalWrite(LED, LOW);
    digitalWrite(TX, LOW);

    // Write back on serial monitor the readings we're transmitting
    // Validates pack/unpack formatting and reconciliation on rtl_433
    om.unpack_msg(msg, fmt, id, itemp, otemp, ihum, ohum, press, volts);
    itempf = ((float)itemp) / 10.0;
    otempf = ((float)otemp) / 10.0;
    ihumf  = ((float)ihum);
    ohumf  = ((float)ohum);
    pressf = ((float)press);
    voltsf = ((float)volts) / 100.0 + 3.00;
    DBG_print(F("Transmit msg "));
    DBG_print(++count);
    DBG_print(F("\tiTemp="));
    DBG_print(itempf);
    DBG_print(F("˚C"));
    DBG_print(F(", oTemp="));
    DBG_print(otempf);
    DBG_print(F("˚C"));
    DBG_print(F(", iHum="));
    DBG_print(ihumf);
    DBG_print(F("%"));
    DBG_print(F(", oHum="));
    DBG_print(ohumf);
    DBG_print(F("%"));
    DBG_print(F(", Press="));
    DBG_print(pressf / 10.0);
    DBG_print(F("hPa"));
    DBG_print(F(", VCC="));
    DBG_print(voltsf);
    DBG_print(F("volts"));
    DBG_print(F("\tin hex: 0x "));
    for (uint8_t j = 0; j < 10; j++) {
        if (msg[j] < 16)
            DBG_print('0');
        DBG_print(msg[j], HEX);
        DBG_print(' ');
    };
    DBG_println();
    DBG_print(F("\tThe msg packet, length="));
    DBG_print((int)omniLen);
    DBG_print(F(", as a string of bits: "));
    for (uint8_t i = 0; i < omniLen; i++)
        DBG_print(((msg[i / 8] >> (7 - (i % 8))) & 0x01));
    DBG_println();

    first = false;
    delay(DELAY);
}; // end loop()
