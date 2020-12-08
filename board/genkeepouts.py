import math

id = 0
def get_id():
  global id
  id += 1
  return "%i" % id

def pts_to_zone(layer, mask_layer, pts):
    pts_expr = ' '.join(["(xy %f %f)" % p for p in pts])
    return """(zone (net 0) (net_name "") (layer "%s") (tstamp %s) (hatch none 0.508)
    (connect_pads (clearance 0))
    (min_thickness 0.254)
    (keepout (tracks allowed) (vias allowed) (pads allowed ) (copperpour not_allowed) (footprints allowed))
    (fill (thermal_gap 0.508) (thermal_bridge_width 0.508))
    (polygon
      (pts %s)
    )
)
(zone (net 0) (net_name "") (layer "%s") (tstamp %s) (hatch edge 0.508)
(connect_pads (clearance 0.3))
(min_thickness 0.254) (filled_areas_thickness no)
(fill yes (thermal_gap 0.508) (thermal_bridge_width 0.508))
(polygon
    (pts %s)
)
(filled_polygon
    (layer "%s")
    (island)
    (pts %s)
)
)
""" % (layer, get_id(), pts_expr, mask_layer, get_id(), pts_expr, mask_layer, pts_expr)

CENTER = (100, 100)
RAD = 17.5
N = 24
KEEPOUT_DIAM = 3.3
KEEPOUT_FACES = 64
YSQUASH = 0.61
LAYER = "B.Cu"
MASK_LAYER = "B.Mask"

def pts_for(angle):
    pts = []
    for j in range(KEEPOUT_FACES):
        a = (j/KEEPOUT_FACES) * math.pi * 2.0
        kx = math.cos(a) * KEEPOUT_DIAM * 0.5 * YSQUASH
        ky = math.sin(a) * KEEPOUT_DIAM * 0.5
        kx, ky = math.cos(angle) * kx - math.sin(angle) * ky, math.sin(angle) * kx + math.cos(angle) * ky

        pts.append((kx, ky))
    return pts

zones = []
for i in range(N):
    angle = math.pi * 2.0 * (i/N)
    x = CENTER[0] + math.cos(angle) * RAD
    y = CENTER[1] + math.sin(angle) * RAD

    pts = [(xx + x, yy + y) for (xx, yy) in pts_for(angle)]
    zones.append(pts_to_zone(LAYER, MASK_LAYER, pts))

# +/- 1/3 leds
langle = (140.19442890773481 + 90) * math.pi / 180.0
rangle = -langle
zones.append(pts_to_zone(LAYER, MASK_LAYER, [(x + 92.5, y + 91) for (x, y) in pts_for(langle)]))
zones.append(pts_to_zone(LAYER, MASK_LAYER, [(x + 107.5, y + 91) for (x, y) in pts_for(rangle)]))

for z in zones:
    print(z)
    
