#!/usr/bin/python

import core
import sys

def testHelloWorld(kernel_name):
	test = core.TestUnit(kernel_name, "Testing Hello World")
	test.look_for_and_print_result("Hello World")
        # why do we need to send the quit command? -Ashvin
	# test.send_command("q")

def main():
	path = str(sys.argv[1])
	testHelloWorld(path)

if __name__ == "__main__":
	main()
