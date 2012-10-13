#!/usr/bin/python

import pexpect
import sys
import os

class TestUnit:
	#Implicit assumptions, sys161 is in path

	def set_log_file(self):
		logfile = open('os161-marker.txt', 'a')
		kernel.logfile = logfile


	def __init__(self, path_to_kernel, message, grade=False):
		global kernel
                global verbose
                try:
                        verbose = 1
                except KeyError:
                        verbose = 1
                try:
                        verbose = 1
                except ValueError:
                        verbose = 1
		path = 'sys161 ' + str(path_to_kernel)
		kernel = pexpect.spawn(path, timeout = 10)
		#kernel.logfile = sys.stdout
                print message
		if grade == True:
			self.set_log_file()
        verbose = 1;

	def clean_kernel(self):
		kernel.logfile.close()

        def kernel(self):
                return kernel

        def verbose(self):
                return True;

	#We need to wait before we can actually send a command.
	def send_command(self, cmd):
		kernel.expect('OS\/161 kernel \[\? for menu\]\: ')
		#The fun bit is, we need to send the command character by
		#character to the simulator, otherwise we are going to have
		#a lot of fun ;-)
                if verbose > 1:
                        print "SENDING: " + cmd
		cmd_char = list(cmd)
		for i in cmd_char:
			kernel.send(i)
		kernel.send('\n')
		return

	def look_for(self, result):
		try:
                        if verbose > 1:
                                print "EXPECTING: " + str(result)
			index = kernel.expect(result)
                        if verbose > 0:
                                print "FOUND: " + kernel.match.group(0)
                except pexpect.TIMEOUT, e:
                        print "TIMEOUT ERROR"
			return -1
                except pexpect.EOF:
                        print "END OF FILE ERROR"
			return -1
                except Exception:
                        print "UNEXPECTED ERROR", sys.exc_info()[0]
			return -1
		return index

        def print_result(self, out):
                if out >= 0:
                        print "PASS"
                else:
                        print "FAIL"

	def look_for_and_print_result(self, result):
                out = self.look_for(result)
                self.print_result(out)

	def look_for_and_return_mark(self, result, mark):
		out = self.look_for(result)
		if out >= 0:
			return mark
		return 0

