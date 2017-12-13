/*
 * Simple example MQTT client to run on the Itead Studio Sonoff with OTA
 * update support. Subscribes to a topic and watches for a message of
 * either "0" (turn off LED and relay) or "1" (turn on LED and relay)
 * 
 * Script based on https://github.com/SuperHouse/BasicOTARelay but customized to fit my needs
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
/* WiFi Settings */
const char* ssid     = "********";
const char* password = "********";

/* Sonoff Outputs */
const int relayPin = 12;  // Active high
const int ledPin   = 13;  // Active low

/* MQTT Settings */
const char* mqttTopic = "house/sonoff1";   // MQTT topic
const char* mqttTopic2 = "house/sonoff1/led"; //MQTT topic 2

IPAddress broker(10,1,10,4);          // Address of the MQTT broker
#define CLIENT_ID "sonoff1"         // Client ID to send to the broker
const char* mqttuser = "********";
const char* mqttpass = "********";

/* Button Settings */
long last_message_time        = 0;
byte last_button_state        = 0;
long minimum_message_interval = 5000;
byte button_pin = 0;


WiFiClient wificlient;
PubSubClient client(wificlient);


/**
 * MQTT callback to process messages
 */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

   if (strcmp(topic,mqttTopic)==0){

  // Examine only the first character of the message
  if(payload[0] == 49)              // Message "1" in ASCII (turn outputs ON)
  {
    client.publish("house/sonoff1/state", "1"); // Send a status update
    digitalWrite(relayPin, HIGH);
  } else if(payload[0] == 48)       // Message "0" in ASCII (turn outputs OFF)
  {
    client.publish("house/sonoff1/state", "0"); //send a status update
    digitalWrite(relayPin, LOW);
  } else {
    Serial.println("Unknown value");
  }
  }


 if (strcmp(topic,mqttTopic2)==0){
 if(payload[0] == 49)              // Message "1" in ASCII (turn outputs ON)
  {
    digitalWrite(ledPin, LOW);      // LED is active-low, so this turns it on
    
  } else if(payload[0] == 48)       // Message "0" in ASCII (turn outputs OFF)
  {
    digitalWrite(ledPin, HIGH);     // LED is active-low, so this turns it off
   
  } else {
    Serial.println("Unknown value");
  }

  
}

}




/**
 * Attempt connection to MQTT broker and subscribe to command topic
 */
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_ID, mqttuser,mqttpass)) {
      Serial.println("connected");
      client.subscribe(mqttTopic);
      client.subscribe(mqttTopic2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**
 * Setup
 */
void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.hostname("sonoff1");
  WiFi.begin(ssid, password);
  Serial.println("WiFi begun");

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Proceeding");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname("sonoff1");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR   ) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR  ) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR    ) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  /* Set up the outputs. LED is active-low */
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP); // The input will be pulled LOW to trigger an action
  digitalWrite(ledPin, HIGH);
  digitalWrite(relayPin, LOW);
  

  /* Prepare MQTT client */
  client.setServer(broker, 1883);
  client.setCallback(callback);
}

/**
 * Main
 */
void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
  }
  
  if (client.connected())
  {
    client.loop();

    /* Read the state of a connected button and act if it has been pressed */
  if(digitalRead(button_pin) == LOW)
  {
    if(last_button_state == HIGH)
    {
      // HIGH to LOW transition: we just detected a button press!
      if(millis() - last_message_time > minimum_message_interval)  // Allows debounce / rate limiting
      {
        // Send a message
        client.publish("house/sonoff1/button","1");
        last_message_time = millis();
      }
    }
    last_button_state = LOW;
  } else {
    last_button_state = HIGH;
  }
  }
}
