#!/usr/bin/python

import core
import sys

def testPrintChar(kernel_name):
	global test
	test = core.TestUnit(kernel_name, "Testing printchar")
	# check = 'H*e*l*l*o*w*o*r*l*d*\!*H*e*l*l*o*p*r*i*n*t*f*\!*'
	check = 'H.*e.*l.*l.*o.*w.*o.*r.*l.*d.*\!.*H.*e.*l.*l.*o.*p.*r.*i.*n.*t.*f.*\!'
	#check = 'H*e*l*l*o*p*r*i*n*t*f*\!'
	#check = 'H**e**l**l**o**p**r**i**n**t**f**\!'
	test.send_command("p /testbin/printchar")
	test.look_for_and_print_result(check)

def main():
	path = str(sys.argv[1])
	testPrintChar(path)



if __name__ == "__main__":
	main()
