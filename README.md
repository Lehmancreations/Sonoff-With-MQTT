# Sonoff-With-MQTT
Sonoff Firmware with MQTT and Multiple incoming topics  

Topic is the main MQTT topic that turns the relay on and off
Topic2 is a second incoming MQTT topic that allows you to turn the led on and off.
When button is pressed it publishes a to MQTT topic this allows you to handle the press in OpenHAB or Node-Red


This is a simple sketch for sonoff devices that allows MQTT. This is based off of https://github.com/SuperHouse/BasicOTARelay but I 
customized for my needs



On the newer Sonoffs you need to use DOUT as the flash mode