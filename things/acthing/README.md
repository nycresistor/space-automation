![AC Controller](https://farm5.staticflickr.com/4627/26499450208_bc56138dfd_z_d.jpg)

More info: https://trmm.net/AC_controller 

Each time the button is pressed it sends the command twice.

```
     3440 us               430       430    430
    +------------+         +--+      +--+   +--+
 ___|            |_________|  |______|  |___|  |___
                 430+1320    1 = 1320  0=430
```
_(if the graph looks funny make your window wider)_

```
 
  The DHT22 pinout is
    ___________
   /           \
  /______O______\
  |             |
  |             |
  |             |
  |   AM2302    |
  +-------------+
   |   |   |   |
  Vcc Data NC Gnd
 
```

The device listens for MQTT commands or serial port commands to send the OFF command.  Sample Home Assistant yaml configuration:

Configuration (HA):

```yaml
sensor:
  - platform: mqtt
  	name: 'Temperature'
  	state_topic: '/18fe34d44eb8/sensor/temperature'
  	unit_of_measurent: 'ºC'
  - platform: mqtt
    name: 'Humidity'
    state_topic: '/18fe34d44eb8/sensor/humidity'
    unit_of_measurent: '%'
```
