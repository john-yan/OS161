#!/usr/bin/python

import core
import sys

# invariants:
# 1. region: every car goes to the next correct region
# 2. queue: order of approach per direction is the order in which cars enter
#           the different regions in the intersection
# 3. crash: no region has multiple cars


def stoplight(test, nr_cars):
    test.send_command('1c')
    directions = {'N': 0, 'E': 1, 'S': 2, 'W': 3}
    regions = [ -1, -1, -1, -1 ]; # NW, NE, SE, SW
    max_occupied_regions = 0 # for statistics
    occupied_regions = 0
    queue = []
    cars_per_direction = [ 0 ] * len(directions) # for statistics
    all_cars = [ -1 ] * nr_cars
    for i in range(len(directions)):
        queue.append([])

    if test.verbose():
        print 'car never seen = -1, approaching car = 0, leaving car = 1'
    while True:
        state = test.look_for( \
            ['approaching: car =\s+(\d+), direction = ([NESW]), destination = ([NESW])', \
                 'region1:     car =\s+(\d+), direction = ([NESW]), destination = ([NESW])', \
                 'region2:     car =\s+(\d+), direction = ([NESW]), destination = ([NESW])', \
                 'region3:     car =\s+(\d+), direction = ([NESW]), destination = ([NESW])', \
                 'leaving:     car =\s+(\d+), direction = ([NESW]), destination = ([NESW])'])
        if state < 0:
            return -1 # no match failure

        nr, direction, destination = test.kernel().match.groups()

        # convert to integers
        nr = int(nr)
        direction = directions[direction]
        destination = directions[destination]
        if direction == destination:
            print 'car direction is the same as car destination'
            return -1
        if nr < 0 or nr >= nr_cars:
            print 'car ' + str(nr) + ' is bogus'
            return -1

        found_car = False
        if state == 0:   # approaching, queue the car
            if all_cars[nr] != -1:
                print 'car was already approaching or had left previously'
                return -1
            all_cars[nr] = 0 # mark car as approaching
            if test.verbose():
                print 'car ' + str(nr) + ' approaching: ' + str(all_cars)
            cars_per_direction[direction] = cars_per_direction[direction] + 1
            queue[direction].append([nr, direction, destination, \
                                         (direction - destination) % 4, state, -1])
        elif state >= 1 and state <= 3: # entering region 1, 2 or 3
            for car in queue[direction]:
                if car[0] != nr: # all cars ahead, coming from same direction
                    if car[4] <= state: # queue invariant
                        print 'car ' + str(nr) + ' has overtaken ' + str(car[0])
                        return -1
                else:
                    if car[1] != direction:
                        print 'car ' + str(nr) + ' changed direction'
                        return -1
                    if car[2] != destination:
                        print 'car ' + str(nr) + ' changed destination'
                        return -1
                    if car[3] < state: # region invariant
                        print 'car ' + str(nr) + ' should not be in this region!'
                        return -1
                    if car[4] != (state - 1): # region invariant
                        print 'car ' + str(nr) + ' did not go to all its regions'
                        return -1

                    reg = (direction - state + 1) % 4 # the region in the intersection
                    if regions[reg] != -1: # crash invariant
                        print 'car ' + str(nr) + ' crashes with car ' + str(regions[reg])
                        return -1
                    car[4] = state           # update state of car
                    regions[reg] = nr        # update region with this car
                    if car[5] >= 0:
                        regions[car[5]] = -1 # update previous region
                    else:
                            occupied_regions = occupied_regions + 1
                            max_occupied_regions = max(max_occupied_regions, occupied_regions)
                    car[5] = reg             # store current region
                    found_car = True
                    break
            if not found_car:
                print 'car ' + str(nr) + ' never approached the intersection'
                return -1

        else: # leaving
            for car in queue[direction]:
                if car[0] == nr: # all cars ahead coming from same direction
                    if car[1] != direction:
                        print 'car ' + str(nr) + ' changed direction'
                        return -1
                    if car[2] != destination:
                        print 'car ' + str(nr) + ' changed destination'
                        return -1
                    if car[4] != car[3]: # region invariant
                        print 'car ' + str(nr) + ' did not go to all its regions before leaving'
                        return -1
                    if car[5] < 0:
                        print 'car ' + str(nr) + 'car not in any region before leaving'
                        return -1 # this shouldn't happen
                    regions[car[5]] = -1 # update previous region
                    occupied_regions = occupied_regions - 1
                    found_car = True
                    queue[direction].remove(car)
                    all_cars[nr] = 1 # mark car as leaving
                    if test.verbose():
                        print 'car ' + str(nr) + ' leaving: ' + str(all_cars)
                    break
            if not found_car:
                print 'car ' + str(nr) + ' never approached the intersection before leaving'
                return -1
            if sum(all_cars) == nr_cars: # all cars have left, end of processing
                if test.verbose():
                    print 'maximum regions that were occupied = ' + str(max_occupied_regions)
                    print 'number of cars in each direction = ' + str(cars_per_direction)
                return 0

def main():
    kernel_name = str(sys.argv[1])
    test = core.TestUnit(kernel_name, "Testing stoplight")
    result = stoplight(test, 20)
    test.print_result(result)

if __name__ == "__main__":
    main()
