# This is a somewhat more elaborate version of the calculation in
# windowcalcs.md. It takes the thickness of the window and the refractive index
# of acrylic into account. As it turns out, this only makes a difference to the
# result on the order of one degree.

__version__ = '0.1.0'

import itertools
import math
import numpy as np
import shapely.geometry as sg

# All dimensions in mm
UPPER_WINDOW_DIM = 10
LOWER_WINDOW_DIM = 7.2
ACRYLIC_REFRACTIVE_INDEX = 1.491
AIR_REFRACTIVE_INDEX = 1.00029
WINDOW_THICKNESS = 2
SENSOR_TO_WINDOW = 3.4
SENSOR_DIM = 0.305

SENSOR_STEPS = 100
ANGLE_STEPS = 90 * 4

def pt(x, y):
    return sg.Point(x, y)

def add(p1, p2):
    return (p1[0] + p2[0], p1[1] + p2[1])

def get_hits():
    hits_for_angles = np.zeros(ANGLE_STEPS)

    lower_window = sg.LineString([(-LOWER_WINDOW_DIM/2, SENSOR_TO_WINDOW), (LOWER_WINDOW_DIM/2, SENSOR_TO_WINDOW)])
    upper_window = sg.LineString([(-UPPER_WINDOW_DIM/2, SENSOR_TO_WINDOW+WINDOW_THICKNESS), (UPPER_WINDOW_DIM/2, SENSOR_TO_WINDOW+WINDOW_THICKNESS)])

    for s in range(SENSOR_STEPS):
        distance_along_sensor = (s / SENSOR_STEPS) * SENSOR_DIM

        for a, ai in zip(range(ANGLE_STEPS), itertools.count(0)):
            a = (a/ANGLE_STEPS) * math.pi/2 - math.pi/2

            on_sensor = (distance_along_sensor - SENSOR_DIM/2, 0)
            in_distance = add(on_sensor, (math.sin(a) * 1000, math.cos(a) * 1000))

            ray = sg.LineString([on_sensor, in_distance])

            lwi = ray.intersection(lower_window)
            if len(lwi.coords) >= 1:
                angle_of_refraction = math.asin(AIR_REFRACTIVE_INDEX/ACRYLIC_REFRACTIVE_INDEX) * math.sin(a)
                d = math.atan(angle_of_refraction) * WINDOW_THICKNESS
                on_upper_window_x = lwi.coords[0][0] + d
                if on_upper_window_x >= -UPPER_WINDOW_DIM/2 and on_upper_window_x <= UPPER_WINDOW_DIM/2:
                    # The ray hits the sensor.
                    hits_for_angles[ai] += 1

    first_nonzero = None
    first_max = None
    for h, hi in zip(hits_for_angles, itertools.count(0)):
        angle = (hi/ANGLE_STEPS) * math.pi/2 - math.pi/2
        if first_nonzero is None and h > 0:
            first_nonzero = angle * 180/math.pi
        if first_max is None and h == SENSOR_STEPS:
            first_max = angle * 180/math.pi

    return dict(first_nonzero=first_nonzero, first_max=first_max)

print(get_hits())