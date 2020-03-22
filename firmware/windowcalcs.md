```
      --------------------  <-  upper surface of window
    2mm ↕     ↔ 7.2mm
        ---------------  <- lower surface of window
              |
       3.4mm↕ |  
 sensor <-  __|__ ↔ 0.305mm
            |   | ↕ 0.7mm
------------------------------  <- pcb surface
```

Angle of view (half angle):

```
atan((7.2/2)/3.4) = 46.637°
```

Refractive indices:

```
air: 1.00029
acrylic: 1.4901
```

Refraction example:

```
angle of incidence = 40°
angle of refraction = 25.5626°
d = 2 * tan(25.5626°) = 0.957mm
```

Conclusion: effect of refraction should be negligible.

The LTR303 datasheet has a graph of relative responsivity against viewing angle.
Approximately 27.18% of the area of this graph is outside the 46.637° viewing
angle allowed by the window.

```
log2(1 + 0.2718) = 0.3468
```

Thus, the estimated attenuation from the restrictions imposed on angle of
view by the window is 0.3468 stops.