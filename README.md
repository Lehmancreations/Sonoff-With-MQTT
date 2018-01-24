# Sonoff-With-MQTT
Sonoff Firmware with MQTT and Multiple incoming topics and support for DHT22 sensor.

Topic is the main MQTT topic that turns the relay on and off
Topic2 is a second incoming MQTT topic that allows you to turn the led on and off.
When button is pressed it publishes a to MQTT topic this allows you to handle the press in OpenHAB or Node-Red

This firmware also allows you to hook a DHT22 sensor up to the extra GPIO pin (14) on the Sonoff and get temperature and humidity readings via a MQTT topic.


This is a simple sketch for sonoff devices that allows MQTT. This is based off of https://github.com/SuperHouse/BasicOTARelay but I 
customized for my needs



Please see the <a href="https://github.com/Lehmancreations/Sonoff-With-MQTT/wiki">Wiki</a> for more information and how to install
