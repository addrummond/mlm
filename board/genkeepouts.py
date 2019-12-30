import math

def pts_to_zone(layer, pts):
    pts_expr = ' '.join(["(xy %f %f)" % p for p in pts])
    return """(zone (net 0) (net_name "") (layer "%s") (tstamp 0) (hatch none 0.508)
  (connect_pads (clearance 0.1))
  (min_thickness 0.254)
  (keepout (tracks allowed) (vias allowed) (copperpour not_allowed))
  (fill (thermal_gap 0.508) (thermal_bridge_width 0.508))
  (polygon
    (pts %s)
  )
)""" % (layer, pts_expr)

CENTER = (100, 100)
RAD = 17.5
N = 24
KEEPOUT_DIAM = 3.7
KEEPOUT_FACES = 64
LAYER = "B.Cu"

def pts_for(x, y):
    pts = []
    for j in range(KEEPOUT_FACES):
        angle = (j/KEEPOUT_FACES) * math.pi * 2.0 
        kx = x + math.cos(angle) * KEEPOUT_DIAM * 0.5
        ky = y + math.sin(angle) * KEEPOUT_DIAM * 0.5

        pts.append((kx, ky))
    return pts

zones = []
for i in range(N):
    angle = math.pi * 2.0 * (i/N)
    x = CENTER[0] + math.cos(angle) * RAD
    y = CENTER[1] + math.sin(angle) * RAD

    pts = pts_for(x, y)
    zones.append(pts_to_zone(LAYER, pts))

# +/- 1/3 leds
zones.append(pts_to_zone(LAYER, pts_for(92.5, 91)))
zones.append(pts_to_zone(LAYER, pts_for(107.5, 91)))

for z in zones:
    print(z)
    