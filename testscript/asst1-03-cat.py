#!/usr/bin/python

import core
import sys

# TODO: should make the code work for arbitrary number of animals, bowls, etc.

def catmouse(test, cmd):
    test.send_command(cmd)

    bowls = [ -1, -1] # cat or mouse nr who is eating
    mouse_cat = 0     # 1 when cats are eating, 2 when mouse are eating
    nr_eating = 0     # 0, 1, 2: depending on how may bowls are in use
    max_cats_eating = 0
    max_mice_eating = 0
    
    cat_iterations = [ -1 ] * 6   # current iteration nr for each cat
    mouse_iterations = [ -1 ] * 2 # current iteration nr for each mouse

    # look for 64 outputs = 
    # 8 (6 cats + 2 mouse) * 2 (start, end) * 4 iterations
    for i in range(64):
        out = test.look_for( \
            ['cat: (\d) starts eating: bowl (\d), iteration (\d)', \
                 'mouse: (\d) starts eating: bowl (\d), iteration (\d)', \
                 'cat: (\d) ends eating: bowl (\d), iteration (\d)', \
                 'mouse: (\d) ends eating: bowl (\d), iteration (\d)'])
        if out < 0:
            return -1 # no match failure

        nr, bowl, iteration = test.kernel().match.groups()
        nr = int(nr)
        bowl = int(bowl)
        iteration = int(iteration)

        # sanity check
        if bowl < 1 or bowl > 2:
            print 'bowl nr should be 1 or 2'
            return -1

        if iteration < 0 or iteration > 3:
            print 'iteration should be 0, 1, 2 or 3'
            return -1
        
        if out == 0 or out == 2:
            if nr < 0 or nr > 6:
                print 'cat nr should be 1-6'
                return -1
        else:
            if nr < 0 or nr > 1:
                print 'mouse nr should be 1-6'
                return -1

        # now check that the cat/mouse consistency is met
        bowl = bowl - 1

        if out == 0:
            if mouse_cat == 2:
                print 'mouse is already eating'
                return -1
            if bowls[bowl] != -1:
                print 'bowl = ' + str(bowl) + 'is already in use'
                return -1
            if nr_eating >= 2:
                print 'weird: too many cats eating' # shouldn't happen
                return -1
            if cat_iterations[nr] != iteration - 1:
                print 'cat iteration ' + str(iteration) + 'is not correct'
                return -1

            mouse_cat = 1
            bowls[bowl] = nr
            nr_eating = nr_eating + 1
            cat_iterations[nr] = iteration
            max_cats_eating = max(max_cats_eating, nr_eating)

        elif out == 1:
            if mouse_cat == 1:
                print 'cat is already eating'
                return -1
            if bowls[bowl] != -1:
                print 'bowl = ' + str(bowl) + 'is already in use'
                return -1
            if nr_eating >= 2:
                print 'weird: too many mouse eating' # shouldn't happen
                return -1
            if mouse_iterations[nr] != iteration - 1:
                print 'mouse iteration ' + str(iteration) + 'is not correct'
                return -1

            mouse_cat = 2
            bowls[bowl] = nr
            nr_eating = nr_eating + 1
            mouse_iterations[nr] = iteration
            max_mice_eating = max(max_mice_eating, nr_eating)

        elif out == 2:
            if bowls[bowl] != nr:
                print 'cat = ' + str(nr) + 'exiting without entering'
                return -1
            if nr_eating <= 0:
                print 'weird: too few cats eating' # shouldn't happen
                return -1

            bowls[bowl] = -1
            nr_eating = nr_eating - 1
            if nr_eating == 0:
                mouse_cat = 0

        elif out == 3:
            if bowls[bowl] != nr:
                print 'mouse = ' + str(nr) + 'exiting without entering'
                return -1
            if nr_eating <= 0:
                print 'weird: too few mouse eating' # shouldn't happen
                return -1

            bowls[bowl] = -1
            nr_eating = nr_eating - 1
            if nr_eating == 0:
                mouse_cat = 0

    if test.verbose():
        if max_cats_eating < 2:
            print 'Maximum number of cats eating at a time = ' \
                + str(max_cats_eating) + ' < 2'
        if max_mice_eating < 2:
            print 'Maximum number of mice eating at a time = ' \
                + str(max_mice_eating) + ' < 2'
    return 0

def main():
    global test
    result = 0

    # try cat with semaphores
    kernel_name = str(sys.argv[1])
    test = core.TestUnit(kernel_name, "Testing cat/mouse using semaphores")
    result = catmouse(test, '1a')

    if result < 0:
        # try cat with locks
        test = core.TestUnit(kernel_name, "Testing cat/mouse using locks")
        result = catmouse(test, '1b')

    test.print_result(result)

if __name__ == "__main__":
    main()

