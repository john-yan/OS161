#!/usr/bin/python

import core
import sys

def testlock(i):
	global test
	kernel_name = str(sys.argv[1])
	test = core.TestUnit(kernel_name, "Testing locks: " + str(i))
	test.send_command("sy2")
        return test.look_for(['Test failed', 'Lock test done.'])

def main():
        # run the test several times because the test can succeed 
        # even if locks are not implemented correctly
        result = 0
        for i in range(10):
            index = testlock(i+1)
            if index <= 0:
                result = -1 # failure
                break
        test.print_result(result)

if __name__ == "__main__":
	main()
