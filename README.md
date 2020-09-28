# Small-Window
Arduino program for controlling a small window with a linear actuator with motor H-bridge and potentiometer position

The software is written for an ESP8266 D1-mini

It controlls a small linear actuator through an H-bridge driver. Position of the actuator is given by a simple potentiometer read from the A/D input of the ESP8266

Control happens through MQTT

You setup the secrets in secrets.h

Rest of the setup is done in header of the .ino file

MQTT topics

- mqttTopicAnnounce  "smallwindow/announce" Window controller announces itself at boot
- mqttTopicSet       "smallwindow/set"      Open and close the window by sending "open" and "close" to this topic
- mqttTopicState     "smallwindow/state"    Window controller reports current position on this topic "open" or "closed"
