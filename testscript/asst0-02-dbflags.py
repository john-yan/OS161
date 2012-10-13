#!/usr/bin/python

import core
import sys

def checkDBValue(res):
	check = 'Current value of dbflags is ' + res
	test.send_command("dbflags")
	test.look_for_and_print_result(check)

def setDBValue(value, on):
	path = "df " + str(value) + " " + on
	test.send_command(path)

def failDBValue(value, on):
	print "Turning " + on + " dbflags value " + str(value)
	check = 'Usage: df nr on\/off'
	setDBValue(value, on)
	test.look_for_and_print_result(check)

def testDBValue(value, on, res):
	print "Turning " + on + " dbflags value " + str(value)
	setDBValue(value, on)
	return checkDBValue(res)

def testDBFlags(kernel_name):
	global test
	test = core.TestUnit(kernel_name, "Testing dbflags")
	#Check if we have the dbflags menu option
	test.send_command("?o")
	test.look_for_and_print_result('\[dbflags\] Debug flags')

def main():
	on = "on"
	off = "off"
	path = str(sys.argv[1])
	testDBFlags(path)
	checkDBValue("0x0")
	testDBValue(1, on, "0x1")
	testDBValue(1, off, "0x0")
	testDBValue(5, on, "0x10")
	failDBValue(13, on)



if __name__ == "__main__":
	main()
