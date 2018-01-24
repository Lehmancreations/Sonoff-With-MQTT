/*
 * Firmware for Itead Studio Sonoff with OTA
 * update support. Subscribes to a topic and watches for a message of
 * either "0" (turn relay) or "1" (turn on relay) 
 * Subscribes to second topic to turn on and off the LED with 1 or 0
 * Publishes 1 to a topic on button press, so you can handle in your HAB software
 * 
 * Script based on https://github.com/SuperHouse/BasicOTARelay but customized to fit my needs
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient 
#include <ArduinoOTA.h>

// #define DHT22 // If defined then include and make use of the DHT22 Sensor 

#ifdef DHT22 //If the DHT22 is defined 
  #include "DHT.h" //https://github.com/adafruit/DHT-sensor-library
  #define DHTPIN 14
  #define DHTTYPE DHT22
  /* Allows rate limiting on sensor readings */
  long dht_last_message_time        = 0;
  long dht_minimum_message_interval = 10000;
  DHT dht(DHTPIN, DHTTYPE);
#endif

/* 
 *  The following include allows you to put credentials in an include file
 *  If you do not have in credentials.h file please uncomment the ssid and password for WiFi and the mqttuser and mqttpass in the MQTT settings area.
 *  You will then need to comment out the line directly following 
*/
#include "credentials.h" /* Comment out this line if you are not using a credentials.h file */

/* WiFi Settings */

// const char* ssid     = "******";
// const char* password = "******";

/* Sonoff Outputs */
const int relayPin = 12;  // Active high
const int ledPin   = 13;  // Active low

/* MQTT Settings */
const char* mqttTopic = "house/sonoff1";   // MQTT Relay topic
const char* mqttTopic2 = "house/sonoff1/led"; // MQTT LED topic 
const char* buttonPressTopic = "house/sonoff1/button"; // MQTT Button Press Topic
const char* willTopic = "house/sonoff1/status"; // Topic for birth and will messages
const char* willMessage = "offline"; // Message to send when sonoff is offline

IPAddress broker(10,1,10,4);          // Address of the MQTT broker
#define CLIENT_ID "sonoff1"         // Client ID to send to the broker
// const char* mqttuser ="******";
// const char* mqttpass = "******";

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
    client.publish(willTopic, "1"); // Send a status update
    digitalWrite(relayPin, HIGH);
  } else if(payload[0] == 48)       // Message "0" in ASCII (turn outputs OFF)
  {
    client.publish(willTopic, "0"); //send a status update
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
    if (client.connect(CLIENT_ID, mqttuser,mqttpass,willTopic,0,0,willMessage)) {
      Serial.println("connected");
      client.subscribe(mqttTopic);
      client.subscribe(mqttTopic2);
      client.publish(willTopic, "online"); //send online anouncement
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

#ifdef DHT22 // If using a DHT22 then start the sensor
 dht.begin();
#endif    
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
        client.publish(buttonPressTopic,"1");
        last_message_time = millis();
      }
    }
    last_button_state = LOW;
  } else {
    last_button_state = HIGH;
  }

#ifdef DHT22
/* Read DHT22 Sensor if defined  */

 if(millis() - dht_last_message_time > dht_minimum_message_interval)  // Allows debounce / rate limiting
      {

        float h = dht.readHumidity();
        // Read temperature as Fahrenheit (isFahrenheit = true)
        float f = dht.readTemperature(true);

        // Check if any reads fail
         if (isnan(h) || isnan(f)) {
             Serial.println("Failed to read from DHT sensor!");
             return;
         }

      client.publish("house/sonoff2/humidity", String(h).c_str()); // Send a status update
       client.publish("house/sonoff2/temp", String(f).c_str()); // Send a status update
       dht_last_message_time = millis();

    }

#endif
  
  }
}
