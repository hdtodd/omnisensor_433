#!/usr/bin/env python3
# Omni00.py v1.0

"""
    @file
    Omni00 v1.2

    Copyright (C) 2025 H. David Todd <hdtodd@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
"""

"""
    This program shows how to extract data fields from
    the hexadecimal payload data decoded by the 'omni.c'
    decoder in rtl_433 for Omni Multisensor format 00 messages.
    This model can be used to encode other sensors in the
    microcontroller, transmit to the rtl_433 server, and
    decode at an end-consumer site without modifying the
    'omni.c' decoder in rtl_433.

    The demonstration uses format 01 data that has been
    transmitted by the microcontroller as format 00 messages
    by simply changing "fmt = 1" to "fmt = 0" before packing
    the data fields.  See 'omni.ino' in this repository or
    'WP_433.ino' from http://github.com/hdtodd/WP_433
    for the microcontroller code.

    For this demo, the temp, humidity, etc. data fields are
    filled correctly by the microcontroller, but the decoder 
    sees the message as a format 00 message and simply prints
    the payload as a hexadecimal string rather than extracting
    data fields.  But the data fields can be extracted from 
    the hexadecimal payload and displayed correctly.

    The foundation for this was borrowed from the DNT code
    (http://github.com/hdtodd/DNT) and uses threads to separate
    data collection from data processing, which might be required
    if you're using Python to process other sensor data.
    The data extraction is modeled directly from the 'omni.c'
    decoder in the rtl_433/src/devices directory of that repository.

    The message in each Omni packet is 10 bytes / 20 nibbles:

        [fmt] [id] 16*[data] [crc8] [crc8]

    - fmt is a 4-bit message data format identifier
    - id is a 4-bit device identifier
    - data are 16 nibbles = 8 bytes of data payload fields,
          interpreted according to 'fmt'
    - crc8 is 2 nibbles = 1 byte of CRC8 checksum of the first 9 bytes:
          polynomial 0x97, init 0xaa

    A format=0 message simply transmits the core temperature and input power
    voltage of the microcontroller and is the format used if no data
    sensor is present.  For format=0 messages, the message
    nibbles are to be read as:

         fi tt t0 00 00 00 00 00 vv cc

         f: format of datagram, 0-15
         i: id of device, 0-15
         t: Pico 2 core temperature: °C *10, 12-bit, 2's complement integer
         0: bytes should be 0 (but aren't required to be)
         v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
         c: CRC8 checksum of bytes 1..9, initial remainder 0xaa,
            divisor polynomial 0x97, no reflections or inversions

    A format=1 message format is provided as a more complete example.
    It records an  indoor-outdoor temperature/humidity/pressure sensor,
    and the message packet has the following fields:
        indoor temp, outdoor temp, indoor humidity, light intensity,
        barometric pressure, sensor power VCC.
    The data fields are binary values, 2's complement for temperatures.
    For format=1 messages, the message nibbles are to be read as:

         fi 11 12 22 hh ll pp pp vv cc

         f: format of datagram, 0-15
         i: id of device, 0-15
         1: sensor 1 temp reading (e.g, indoor),  °C *10, 12-bit, 2's complement integer
         2: sensor 2 temp reading (e.g, outdoor), °C *10, 12-bit, 2's complement integer
         h: sensor 1 humidity reading (e.g., indoor),  %RH as 8-bit integer
         l: light intensity, %, H as 8-bit integer
         p: barometric pressure * 10, in hPa, as 16-bit integer, 0..6553.5 hPa
         v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
         c: CRC8 checksum of bytes 1..9, initial remainder 0xaa,
                divisor polynomial 0x97, no reflections or inversions

    The Python code below processes ONLY format 00 messages but
    assumes that the 8-byte data payload contains the fields of
    a format 01 packet.

    David Todd, HDTodd@gmail.com, 2025.06.01
"""
import configparser
import argparse
import sys
import os
import codecs
import signal
from paho.mqtt import client as mqtt_client
import random
import json
from time import sleep
import datetime
import threading
import requests
from enum import IntEnum
from queue import Queue
from Omni00AP import AP_NAME, AP_VERSION, AP_DESCRIPTION, AP_EPILOG, AP_PATH
from ProcessIniCli import set_ini_cli_params 

# A variable instantiated as this class will contain the operating parameters
#   for the program.  List all operating parameters here.
class Parameters():
    debug    = False
    host     = ""
    port     = 0
    topic    = ""
    username = ""
    password = ""
    degC     = False

###############################################################################
# Global variable initialization

# MQTT connection management
# Parameters used to establish the mqtt connection to the rtl_433
#   receiver mqtt publisher
client_id = f'python-mqtt-{random.randint(0, 100)}'
tScale = "°F"
location = {}

# Variables and constants used globally
global exit_event
global msg_q
global source
global my_clk

dup_thresh = 2.0
last_time  = 0
last_dev   = ""

##############################################################################
# Convert time string (ts) from ISO format to epoch time                                 
# Or, if ts is in epoch time, convert to timestamp format.                               
# Return both formats for use in processing and displaying                               
def CnvTime(ts):
    if ts.find("-") > 0:
        try:
            eTime = datetime.datetime.fromisoformat(ts).timestamp()
            timestamp = ts
        except ValueError as e:
            err={}
            print("datetime error in input line converting time string: ", ts)
            print("datetime  msg:", err.get("error", str(e)))
            sys.exit(1)
    else:
        try:
            eTime = float(ts)
            timestamp = datetime.datetime.fromtimestamp(eTime)
        except ValueError as e:
            err = {}
            print("Datetime conversion failed on line with datetime string", ts)
            print("float() error msg:", err.get("error", str(e)))
            sys.exit(1)

    return(timestamp, eTime)
# End CnvTime()

###############################################################################
# process_msg() does the real work of understanding and then presenting
#   the data in the received message (from HTTP or MQTT)
# If it's a thermometer reading:
#   ignore if it's a duplicate, update display if it isn't

def process_msg():
    global exit_event
    global msg_q
    global my_clk
    global last_dev
    global last_time
    global params
    
    while not exit_event.is_set():
        msg = msg_q.get()
        # If it's a null message, that's our signal to quit                              
        if msg == None:
            mqtt.loop_stop()
            if params.debug:
                print("Stop processing thread in 'process_msg'")
            break

        # Try to parse the json payload from MQTT or HTTP
        try:
            y = json.loads(msg.payload.decode())
        except:
            # Nope, can't serialize the packet                                       
            print("Unable to load JSON fields from record:\n\t", end="")
            print(msg.payload.decode())
            continue

        # Got a real message: process it
        # Ignore tire pressure monitoring system temp reports
        if "type" in y and y["type"]=="TPMS":
            continue
        # If not a device record, just return
        if "model" not in y:
            continue

        #  Create the device identifier as "model/id/channel"
        model = y["model"]
        if model != "Omni Multisensor":
            continue
        if "channel" not in y:
            continue
        channel = str(y["channel"])
        if channel != "0":
            continue
        if "id" in y:
            id = str(y["id"])
        dev = model + "/" + id + "/" + channel
        
        # Process the record and print it out
        (isoTime,eTime)  = CnvTime(y["time"])
        if (dev == last_dev) and (eTime < last_time + dup_thresh):
            continue
        payload = str(y["payload"])
        dtime   = datetime.datetime.fromisoformat(y["time"]).strftime("%H:%M:%S")
        loc     = dev if dev.lower() not in location else location[dev.lower()]

        b = bytearray.fromhex(payload)

        print(dtime, loc, "Payload = ", payload, " = ", end="")
        for be in b:
            print(hex(be), "", end="")
        print("")

#       Decoding taken FROM OMNI.C DECODER (commented lines from omni.c)
#       Note that the payload data DOES NOT INCLUDE the first or last byte of
#         the transmission, so b[0] from payload data is b[1] from the transmission
#       itemp_c     = ((double)((int32_t)(((((uint32_t)b[1]) << 24) | ((uint32_t)(b[2]) & 0xF0) << 16)) >> 20)) / 10.0;
        itemp = ( ( (b[0]<<24) | (b[1]& 0xF0)<<16 ) >> 20 ) / 10.0
#	otemp_c     = ((double)((int32_t)(((((uint32_t)b[2]) << 28) | ((uint32_t)b[3]) << 20)) >> 20)) / 10.0;
        otemp = ( ( ((b[1]&0x0F)<<28) | (b[2]<<20) ) >>20 ) / 10.0
#       ihum        = (double)b[4];
        ihum  = float(b[3])
#       light       = (double)b[5];
        light = float(b[4])
#       press       = (double)(((uint16_t)(b[6] << 8)) | b[7]) / 10.0;
        press = ( (b[5]<<8) | b[6] ) / 10.0
#       volts       = ((double)(b[8])) / 100.0 + 3.00;
        volts = float(b[7]) / 100.0 + 3.00
        
        print("\t\t\t      itemp={}℃, otemp={}℃, ihum={}%, light={}%," \
              " press={} hPa, volts={}V" \
              .format(itemp, otemp, int(ihum), int(light), press, volts))

#        print("\t\t\t      itemp=", itemp, ", otemp=", otemp,
#              ", ihum=", ihum, ", light=", light,
#              ", press=", press, ", volts=",volts)
        last_dev = dev
        last_time = eTime
            
    # No return: thread exits when exit_event is set or
    #  when a "None" message is received from the message collector
# End process_msg()

"""
"""

############################################################################### 
#  MQTT functions and message reception
#  Message producer thread for MQTT stream                                               

# Connect to  MQTT host
def connect_mqtt() -> mqtt_client:
    global params
    def on_connect(mqtt, userdata, flags, rc):
        if rc == 0:
            if params.debug:
                print("Connected to MQTT host!")
        else:
            print("Failed attempt to connect to ", mqtt)
            print("  with userdata ", userdata)
            print("Return code %d\n", rc)
            sys.exit(1)
        return #from on_connect()
    
    # Work around paho-mqtt v1, v2+ Client instantiation parameter diffs
    try:
        mqtt = mqtt_client.Client(client_id, clean_session=False)
    except:
        mqtt = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION1, client_id, clean_session=False)
    mqtt.username_pw_set(params.username, params.password)
    mqtt.on_connect = on_connect
    if params.debug:
        print("connecting to ", params.host, params.port)
    mqtt.connect(params.host, params.port)
    mqtt_subscribe(mqtt)
    return mqtt

# Subscribe to rtl_433 publication & process records we receive
def mqtt_subscribe(mqtt: mqtt_client):
    # When we get an MQTT message, just pass it on to the
    #   message processor to handle, as it does for HTTP as well
    def on_message(mqtt, userdata, msg):
        global msg_q
        msg_q.put(msg)
        return

    mqtt.subscribe(params.topic)
    mqtt.on_message = on_message
    if params.debug:
        print("subscribed to mqtt feed")
    mqtt.loop_start()
    return #From mqtt_subscribe(), but 'on_message' is active as mqtt callback
# End MQTT functions and message reception


##############################################################################
# CNTL-C and QUIT button handler                                                         
def quit_prog(signum, stack_frame):
    global exit_event
    global msg_q
    global mqtt
    if params.debug:
        print("Quitting from quit_prog")
    exit_event.set()
    msg_q.put(None)
    mqtt.loop_stop()
    mqtt.disconnect()
    return

###############################################################################
# Main script

if __name__ == "__main__":

    global msg
    global source
    
    exit_event = threading.Event()
    msg_q = Queue(0)
    t = datetime.datetime.now()
    signal.signal(signal.SIGINT, quit_prog)

    params = Parameters()

    # Start by assigning default values to the 'config' dictionary
    # The 'config' dictionary will hold the key:value settings
    #   for all operating parameters after being set first
    #   from .ini file settings and then from command line options
    config = configparser.ConfigParser()
    # Set default values here
    config['Server'] = {
        'topic' :  'rtl_433/+/events',
        'port'  :  '1883'
        }
    config['Locale'] = {
        'degC'  :  'true'
        }

    # Process the .ini file and then command-line into 'config' dictionary
    #   and set the program's operating parameters from 'config'
    set_ini_cli_params(config,params)

    if 'Aliases' in config.sections():
        for key, value in config['Aliases'].items():
            location[key] = value

    if params.debug:
        print("\nOperational parameters after .ini and cli:")
        print(vars(params))
        print("Location aliases")
        for key, value in location.items():
            print(key, ":", value)

    mqtt = connect_mqtt()

    if params.debug:
        print("Start treads for receiving MQTT packets and for packet processing")

    # Start the consumer and producer threads.
    #  They will be terminated by "quit_prog": CNTL-C or QUIT button
    c = threading.Thread(target = process_msg, args=())
    p = threading.Thread(target = mqtt_subscribe, args=(mqtt,))
    c.start()
    p.start()

    # Exiting threads return control to here
    p.join()
    if params.debug:
        print("Message collecting thread, 'subscribe', exited but processing thread continues")

    c.join()
    if params.debug:
        print("Message processing thread, 'process_msg' exited; terminate normally")
    print("Quitting ...")
    sys.exit(0)

# End main script
