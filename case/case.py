from itertools import *
import math
import cairo as c
import sys

SCALE = float(sys.argv[1])
OUTPUT_FILE = sys.argv[2]

RADIUS = 17.5 * SCALE
INNER_FONT_FAMILY = "Helvetica"
OUTER_FONT_SIZE = 4 * SCALE
INNER_FONT_SIZE = 3.25 * SCALE
LINE_THICK = 0.25 * SCALE
LIGHT_HOLE_DIAM = 2 * SCALE
INNER_LED_PROTRUSION = 1.5 * 0.5 * LIGHT_HOLE_DIAM
OUTER_LED_PROTRUSION = 1.5 * 0.5 * LIGHT_HOLE_DIAM

# conservative choices - it doesn't matter if there's some top/left padding.
XOFF = RADIUS * 2
YOFF = RADIUS * 2
WIDTH = RADIUS * 4
HEIGHT = RADIUS * 4

# from north clockwise
INNER_LABELS = [
    ("25",-0.25), ("32",-0.25), ("40",-0.25), ("50",-0.25), ("64",-0.25), ("80",-0.25), ("100",-0.25), ("125",-0.25), ("160",-0.25), ("200",-0.25), ("250",-0.25),
    ("320",-0.25),("400",-0.35), "500", "640", "800", "1k", ("1.2k",0.1), "6", "8", "10", ("12",0.05), ("16",0.1),
    "20"
]

# from north clockwise
OUTER_LABELS = [
    "8", "11", "16", ("22",0.5), ("32",0.25), ("45",0.55), ("1S",-0.25), ("2",-0.25), ("4",-0.1), ("8",-0.2), "15", "30", "60",
    ("125",1), ("250",0.75), ("500",0.6), ("1k",0.2), ("2k",0.25), "1", ("1.4",0.4), ("2",0.5), ("2.8",0.4), ("4",0.5), ("5.6",-0.325),
]

def inner_n_to_deg_clock_rot(n):
    a = 360 / len(INNER_LABELS)

    if n == 0:
        return 90
    elif n <= len(INNER_LABELS)/2:
        return n*a - 90
    else:
        return (n - len(INNER_LABELS)/2)*a - 90

assert len(INNER_LABELS) == len(OUTER_LABELS)

if __name__ == '__main__':
    with c.SVGSurface(OUTPUT_FILE, WIDTH, HEIGHT) as surface:
        ctx = c.Context(surface)

        ctx.set_line_width(LINE_THICK)

        ctx.move_to(XOFF, YOFF)
        ctx.line_to(XOFF, YOFF + RADIUS*0.5)
        ctx.stroke()

        ctx.move_to(XOFF - 25 * SCALE, YOFF - 50)
        ctx.line_to(XOFF - 25 * SCALE, YOFF + 50)
        ctx.stroke()

        ctx.move_to(XOFF + 25 * SCALE, YOFF - 50)
        ctx.line_to(XOFF + 25 * SCALE, YOFF + 50)
        ctx.stroke()

        ctx.arc(XOFF, YOFF, RADIUS, 0, 2*math.pi)
        ctx.stroke()

        ctx.select_font_face(INNER_FONT_FAMILY, c.FONT_SLANT_NORMAL, c.FONT_WEIGHT_BOLD)

        for i, lab in zip(count(0), INNER_LABELS):
            adeg = (360 / len(INNER_LABELS)) * i
            arad = adeg * (math.pi / 180)
            x, y = XOFF + math.sin(arad) * RADIUS, YOFF - math.cos(arad) * RADIUS

            ctx.arc(x, y, LIGHT_HOLE_DIAM*0.5, 0, 2*math.pi)
            ctx.stroke()

        ctx.set_font_size(INNER_FONT_SIZE)
        for i, lab in zip(count(0), INNER_LABELS):
            labshift = 0
            labtext = lab
            if type(lab) == type(()):
                labtext, labshift = lab
            labshift *= SCALE
                        
            adeg = (360 / len(INNER_LABELS)) * i
            arad = adeg * (math.pi / 180)
            clockwise_deg_rot = inner_n_to_deg_clock_rot(i)
            clockwise_rad_rot = clockwise_deg_rot * (math.pi/180)

            r = RADIUS - INNER_LED_PROTRUSION
            x, y = XOFF + math.sin(arad) * r, YOFF - math.cos(arad) * r

            _xbearing, _ybearing, width, height, _dx, _dy = ctx.text_extents(labtext)
            if i > 0 and i <= len(INNER_LABELS)/2:
                x -= math.sin(arad) * width - math.cos(arad) * (height/2)
                y += math.cos(arad) * width + math.sin(arad) * (height/2)
            else:
                x -= math.cos(arad) * (height/2)
                y -= math.sin(arad) * (height/2)

            x += math.sin(arad) * labshift
            y -= math.cos(arad) * labshift

            ctx.move_to(x, y)
            ctx.save()
            ctx.rotate(clockwise_rad_rot)
            ctx.translate(-width, 0)
            ctx.show_text(labtext)
            ctx.restore()

        ctx.set_font_size(OUTER_FONT_SIZE)
        for i, lab in zip(count(0), OUTER_LABELS):
            labshift = 0
            labtext = lab
            if type(lab) == type(()):
                labtext, labshift = lab
            labshift *= SCALE

            adeg = (360 / len(INNER_LABELS)) * i
            arad = adeg * (math.pi / 180)

            r = RADIUS + OUTER_LED_PROTRUSION
            x, y = XOFF + math.sin(arad) * r, YOFF - math.cos(arad) * r

            x += math.sin(arad) * labshift
            y -= math.cos(arad) * labshift

            _xbearing, _ybearing, width, height, _dx, _dy = ctx.text_extents(labtext)

            if len(labtext) < 3 or '.' in labtext:
                diag = math.sqrt(width*width+height*height)

                x -= width/2 - math.sin(arad) * height * 0.5 - math.sin(arad) * width * 0.2
                y += height/2 - math.cos(arad) * height * 0.5 - math.cos(arad) * width * 0.2

                ctx.move_to(x, y)
                ctx.show_text(labtext)
            else:
                x += math.sin(arad) * width - math.cos(arad) * (height/2)
                y -= math.cos(arad) * width + math.sin(arad) * (height/2)

                clockwise_deg_rot = inner_n_to_deg_clock_rot(i)
                clockwise_rad_rot = clockwise_deg_rot * (math.pi/180)

                ctx.move_to(x, y)
                ctx.save()
                ctx.rotate(clockwise_rad_rot)
                ctx.show_text(labtext)
                ctx.restore()
