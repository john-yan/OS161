#!/usr/bin/python

import core
import sys

def main():
	global test
        result = 0
	kernel_name = str(sys.argv[1])
	test = core.TestUnit(kernel_name, "Testing condition variables")
	test.send_command("sy3")
        for i in range(5):
            for i in range(31, -1, -1):
                out = test.look_for('Thread ' + str(i))
                if out < 0:
                    result = -1 # failure
                    break
            if result == -1: # get out of outer loop on failure
                break
        if result == 0:
            test.look_for_and_print_result('CV test done')
        else:
            test.print_result(result)


if __name__ == "__main__":
	main()
