# This is a somewhat more elaborate version of the calculation in
# windowcalcs.md. It takes the thickness of the window and the refractive index
# of acrylic into account. (Refraction only has a very small influence, as it
# turns out.)

# Calcs:
#
#     Sensor window = 2.5mm
#     Viewing angle = 40.1 degrees (20.05 degrees one-sided)
#     Sensor has close approximation to cosine response.
#     sin(20.05) = 0.343 (integral of cos(20.05) from 0 to 90 degrees)
#     Relative sensitivity of 0.343 equates to 1.54 stops exposure compensation.
#

__version__ = '0.1.0'

import itertools
import math
import numpy as np
import shapely.geometry as sg

# All dimensions in mm
UPPER_WINDOW_DIM = 10
LOWER_WINDOW_DIM = 2.5 # 2.5 gives approx 40 degree viewing angle
AIR_REFRACTIVE_INDEX = 1.00029
ACRYLIC_REFRACTIVE_INDEX = 1.491
WINDOW_THICKNESS = 2
SENSOR_TO_CUTOUT = 2.1 - 0.7
SENSOR_TO_WINDOW = SENSOR_TO_CUTOUT + 2
SENSOR_DIM = 0.305
SUBJECT_DISTANCE = 10 * 1000

WINDOW_STEPS = 100
ANGLE_STEPS = 90 * 10

def pt(x, y):
    return sg.Point(x, y)

def add(p1, p2):
    return (p1[0] + p2[0], p1[1] + p2[1])

def get_hits():
    raw_hits_for_angles = np.zeros(ANGLE_STEPS)
    hits_for_angles = np.zeros(ANGLE_STEPS)

    sensor = sg.LineString([(-SENSOR_DIM/2, 0), (SENSOR_DIM/2, 0)])
    lower_window = sg.LineString([(-LOWER_WINDOW_DIM/2, SENSOR_TO_WINDOW), (LOWER_WINDOW_DIM/2, SENSOR_TO_WINDOW)])
    upper_window = sg.LineString([(-UPPER_WINDOW_DIM/2, SENSOR_TO_WINDOW+WINDOW_THICKNESS), (UPPER_WINDOW_DIM/2, SENSOR_TO_WINDOW+WINDOW_THICKNESS)])

    for s in range(WINDOW_STEPS):
        distance_along_upper_window = (s / WINDOW_STEPS) * UPPER_WINDOW_DIM

        for a, ai in zip(range(ANGLE_STEPS), itertools.count(0)):
            a = (a/ANGLE_STEPS) * math.pi/2 - math.pi/2

            on_upper_window = (distance_along_upper_window - UPPER_WINDOW_DIM/2, 0)
            on_subject = add(on_upper_window, (math.sin(a) * SUBJECT_DISTANCE, math.cos(a) * SUBJECT_DISTANCE))
            behind_meter = add(on_upper_window, (math.sin(a) * -100, math.cos(a) * -100))

            ray = sg.LineString([on_upper_window, behind_meter])
            if len(ray.intersection(sensor).coords) >= 1:
                raw_hits_for_angles[ai] += 1
            
            angle_of_refraction = math.asin((AIR_REFRACTIVE_INDEX/ACRYLIC_REFRACTIVE_INDEX) * math.sin(a))
            d = -(math.atan(angle_of_refraction) * WINDOW_THICKNESS)
            on_lower_window_x = on_upper_window[0] + d

            if on_lower_window_x >= -LOWER_WINDOW_DIM/2 and on_lower_window_x <= LOWER_WINDOW_DIM/2:
                behind_meter2 = (on_lower_window_x - math.sin(a) * 100, SENSOR_TO_WINDOW - math.cos(a) * 100)
                ray2 = sg.LineString([(on_lower_window_x, SENSOR_TO_WINDOW), behind_meter2])

                if len(ray2.intersection(sensor).coords) >= 1:
                    hits_for_angles[ai] += 1

    hit_ratios = hits_for_angles / raw_hits_for_angles

    first_nonzero = None
    first_max = None
    for h, hi in zip(hit_ratios, itertools.count(0)):
        angle = (hi/ANGLE_STEPS) * math.pi/2 - math.pi/2
        if first_nonzero is None and h >= 0.1:
            first_nonzero = angle * 180/math.pi
        if first_max is None and h >= 0.9:
            first_max = angle * 180/math.pi

    return dict(first_nonzero=first_nonzero, first_max=first_max, avg=(first_nonzero+first_max)/2)

print(get_hits())