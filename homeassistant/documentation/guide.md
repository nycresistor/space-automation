# Important Stuff

To start home assistant if it is not running:

```bash
hass -c /home/holly/space-automation/homeassistant
```

or, if that doesn't work for some reason:

```bash
hass -c /etc/space-automation/homeassistant
```

The configuration files for Home Assistant live in `/home/holly/space-automation/homeassistant/` and symlink to the files in `/etc/space-automation/homeassistant` where they can be accessed for editing by anyone in the group automation.  If you would like to edit the configs, ask to be added to the automation group by an administrator (George, Holly, etc.)

The directory is under version control, don't forget to check in and push your changes.

# Light settings

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

# Useful commandline tools

```bash
$ hass --script check_config
```

You can get configuration data from the lights with [scenegen](https://github.com/home-assistant/scenegen). So you can set up all your lights exactly how you want them for a scene, then run scenegen and use the output to fill in your scene configuration file.  However, this will not give you `rgb_color` data, only `color_temp` and `brightness`, so you'll need to put any colors in by hand.

```bash
$ python3 scenegen.py http://10.0.[redacted]
```
**If you include the slash at the end of the url it won't work.**

# Configuration

## Front End

### How do I make a tab?

A tab is called a `view`.  So in one of the group files make a group for what you want in the tab and set view to yes:

```yaml
lights:
    view: yes
    name: Ligts
    entities:
        - group: main_room_card
        - group: back_room_card
```

The main page of the Home Assistant app and webview is called the `default_view`.  If you want to customize it:

```yaml
default_view:
    view: yes
    entities:
        - group: lights_card
        - group: motion_sensors
        - media_player.spotify
```

### How do I make a card on a view?

In the `groups.yaml`, or in `groups/cards.yaml`

```yaml
lights_card:
    name: Lights
    entities:
        - group.main_room
        - group.back_room
```

And then reference that card group in the default_view group, or whatever group that represents the tab that you want it on.


