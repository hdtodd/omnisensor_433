"""
   Procedures to process a configuration file (".ini") and then
   command-line options into a list of parameters used to control
   program execution

   Use:
       Create the class variable 'params' with the required variables
       Initiate the 'config' variable for the 'configurationparser()' procedure
       Invoke the procedure
           set_ini_cli_params(config, params)

       Upon return, 'config' contains the final values for the variable settings
           and 'params' contains the final values for the program operating variables

    David Todd, HDTodd@gmail.com, 2025.05.23
"""

import argparse
import configparser, os
from Omni00AP import AP_NAME, AP_VERSION, AP_DESCRIPTION, AP_EPILOG, AP_PATH

####################################################################################
#  Command-line processing: Create the command parser & parse cmd line into 'config'

def set_ini_cli_params(config, params):
    # Set parameters from values stored in config[]
    def set_params():
        if 'Server' in config.sections():
            if 'host' in config['Server']:
                params.host = config['Server']['host']
            if 'port' in config['Server']:
                if config['Server']['port'].isnumeric():
                    params.port = config.getint('Server','port')
                else:
                    print("Invalid .ini port '{}' assignment ignored".format(config['Server']['port']))
            if 'topic' in config['Server']:
                params.topic = config['Server']['topic']
            if 'username' in config['Server']:
                params.username = config['Server']['username']
            if 'password' in config['Server']:
                params.password = config['Server']['password']
        if 'Locale' in config.sections():
            if 'degC' in config['Locale']:
                params.degC = config.getboolean('Locale','degC')
        return

    # Start by processing the command line and adding settings into config[]
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=AP_DESCRIPTION, epilog=AP_EPILOG)
    parser.add_argument("-H", "--host", dest="host", type=str, 
                        help="MQTT hostname to connect to ")
    parser.add_argument("-P", "--port", dest="port", type=str,
                        help="MQTT port #")
    parser.add_argument("-T", "--topic", dest="topic", type=str,
                        help="rtl_433 MQTT event topic to subscribe to")
    parser.add_argument("-c", "--config", dest="config", type=str,
                        help="Specify path & filename to configuration file")
    specifyTemp = parser.add_mutually_exclusive_group()
    specifyTemp.add_argument("-C", "--Celsius", dest="degC", action="store_true", default=False,
                             help="Display temperatures in Celsius (default is Fahrenheit)")
    specifyTemp.add_argument("-F", "--Fahrenheit", dest="degC", action="store_false", default=True,
                             help="Display temperatures in Fahrenheit (default)")
    parser.add_argument("-u", "--username", type=str, dest="username",
                        help="(If needed) MQTT username; defaults to blank")
    parser.add_argument("-p", "--password", type=str, dest="password",
                        help="(If needed) MQTT password; defaults to blank")
    parser.add_argument("-v", "--version", action="version", version=AP_NAME+" "+AP_VERSION)
    parser.add_argument("-d", "--debug", dest="debug", action="store_true",
                        help="Print actions and messages during program execution")
    args = parser.parse_args()

    # Set into params any initial parameters changed by CLI such as -d or -c
    if args.debug:
        params.debug = args.debug
    if params.debug:
        print("\nHere are the command-line options entered:")
        for name, value in args.__dict__.items():
            print("{} = {}".format(name, value))

    # Process the .ini file into config[]
    filename = ""
    if args.config:
        if os.path.isfile(args.config):
            filename = args.config
        else:
            print("?Configuration file", args.config, "not found")
            exit(1)
    else:
        pathList = AP_PATH.split(":")
        if params.debug:
            print("Searching the following paths for the .ini file:")
            print(pathList)
        for path in pathList:
            epath = os.path.expanduser(path)
            filename = os.path.join(epath, AP_NAME+".ini")
            if os.path.isfile(filename):
                break
        
    if params.debug:
        print("\nProcessing .ini file", filename)
    config.read(filename)
    if params.debug:
        print("\nHere are the parameters seen in .ini and DEFAULTS")
        for section in config.sections():
            print("\nSection ", section)
            for option in config.options(section):
                print("\t", option, " : ", config[section][option])

    # Set parameters from .ini values stored in config[]
    set_params()

    if params.debug:
        print("\nAFTER processing .ini and BEFORE cli, here are the values assigned to class 'params'")
        for name, value in params.__dict__.items():
            print("{} = {}".format(name, value))

#   Now process cli parameters back into config[]
#   May overwrite .ini entries into config[]
    if args.host and 'Server' in config.sections():
        if 'host' in config['Server']:
            config['Server']['host'] = args.host 
    if args.port and 'Server' in config.sections():
        if 'port' in config['Server']:
            config['Server']['port'] = args.port
    if args.topic and 'Server' in config.sections():
        if 'topic' in config['Server']:
            config['Server']['topic'] = args.topic
    if 'Locale' in config.sections():
        config['Locale']['degC'] = "true" if args.degC is not None and args.degC else "false"
    if args.username and 'Server' in config.sections():
        if 'username' in config['Server']:
            config['Server']['username'] = args.username
    if args.password and 'Server' in config.sections():
        if 'password' in config['Server']:
            config['Server']['password'] = args.password

    # And finally, set parameters after they may have been changed by the
    #   command line options
    set_params()

    if params.debug:
        print("\nAFTER processing .ini AND THEN cli, here are the values assigned to class 'params'")
        for name, value in params.__dict__.items():
            print("{} = {}".format(name, value))

    return
