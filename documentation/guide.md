# Components

## Hexascroller

Hexascroller stuff is on golden in `/opt/hexa` and `/etc/systemd/system/hexascroller.service`

# Light settings

(NOTE:  We don't have any lights under control yet.  This is just a reference for if we do.)

The kelvin and mired scales are inverse:

* Kelvin: higher is BLUER.
* Mired: higher is WARMER.

In Home Assistant `color_temp` is mired and `kelvin` is kelvin.

* Morning sun, energizing bluish: 9000-6000 kelvin, 120 - 160 mired.
* Noon sun, regular daylight: 4100-3500 kelvin, 250 - 300 mired.
* Warm light, soft white: 3000-2700 kelvin, 300 - 400 mired.
* Candlelight: 2000 kelvin, 500 mired

__Tradfri lights__

The white spectrum lights take a `color_temp` from 250 to 450.

__LIFX__

These can get new settings while they're turned off, unlike the other bulbs.

__Hue__

