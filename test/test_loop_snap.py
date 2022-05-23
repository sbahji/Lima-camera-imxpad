#!/usr/bin/env python
#########################################################
#Arafat NOUREDDINE
#2014/11/19
#Purpose : Test LimaDetector state
#########################################################
import os
import sys
import PyTango
import time
import datetime

proxy = ''
#------------------------------------------------------------------------------
# build exception
#------------------------------------------------------------------------------
class BuildError(Exception):
  pass
  
#------------------------------------------------------------------------------
# Colors
#------------------------------------------------------------------------------
class bcolors:
    PINK = '\033[95m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    UNDERLINE = '\033[4m'

def disable(self):
    self.PINK = ''
    self.BLUE = ''
    self.GREEN = ''
    self.YELLOW = ''
    self.FAIL = ''
    self.ENDC = ''
    self.UNDERLINE = ''


#------------------------------------------------------------------------------
def snap(proxy, integration, latency):

    print '\nSnap() \n------------------'
    #Configure the device    

    #Display time when state is STANDBY (just before load())
    timeBegin = datetime.datetime.now()
    print timeBegin.isoformat(), ' - ', proxy.state()
    proxy.exposureTime = integration
    #start the acquisition
    proxy.Snap()

    #Display time when state is RUNNING (just after timeSnap())
    timeSnap = datetime.datetime.now()
    print timeSnap.isoformat(), ' - ', proxy.state()

    #Loop while state is RUNNING (acquisition in progress...)
    state = proxy.state()
    while (state==PyTango.DevState.RUNNING):
        state = proxy.state()
        if state == PyTango.DevState.STANDBY:
            break
        print '\r', '...',
        time.sleep(0.001)

    #Display time when state is STANDBY (just after acquisition is finish)
    timeEnd = datetime.datetime.now()
    print '\n', timeEnd.isoformat(), ' - ', proxy.state()
    print '\nDuration = ', ((timeEnd-timeSnap).total_seconds()*1000),'(ms)'	
    time.sleep(latency)	
    
    return
    #return proxy.image


#------------------------------------------------------------------------------
# Usage
#------------------------------------------------------------------------------
def usage():
  print "Usage: [python] test_loop_snap.py <my/device/proxy>  <integration time s> <latency in s> <nb_loops>"
  sys.exit(1)


#------------------------------------------------------------------------------
# run
#------------------------------------------------------------------------------
def run(proxy_name = 'test/lima/imxpad.1', integration = 1, latency = 1, nb_loops = 1):
    # print arguments
    print '\nProgram inputs :\n--------------'
    print 'proxy_name\t = ', proxy_name
    print 'integration\t = ', integration    
    print 'latency\t = ', latency    
    print 'nb_loops\t = ', nb_loops
    proxy = PyTango.DeviceProxy(proxy_name)
    #Configure the device
    print '\nConfigure Device attributes :\n--------------'
    proxy.Stop()
    nb_loops = int(nb_loops)
    integration = float(integration)
    latency = float(latency)
    alias = '1'
    print '\n'
    try:
        current_loop = 0
        while(current_loop<nb_loops):
            print '\n========================================================'
            print '\t' + bcolors.PINK + 'Loop : ', current_loop, bcolors.ENDC,
            print '\n========================================================'
            snap(proxy, integration, latency)		
            current_loop=current_loop+1
            state = proxy.state()
            if (state!=PyTango.DevState.STANDBY):
            #    raise Exception('FAIL : Acquisition is end with state (%s)' %(state))
			    print bcolors.RED + 'Device is in FAULT !',bcolors.ENDC,
            print '\noutput :\n--------------'
        print '\nProgram outputs :\n--------------'
    except Exception as err:
	   sys.stderr.write('--------------\nERROR :\n--------------\n%s\n' %err)

#------------------------------------------------------------------------------
# Main Entry point
#------------------------------------------------------------------------------
if __name__ == "__main__":
#    if len(sys.argv) < 4:
#        usage()
    print run(*sys.argv[1:])


    
