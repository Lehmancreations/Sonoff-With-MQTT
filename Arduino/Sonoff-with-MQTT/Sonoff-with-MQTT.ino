/*
 * Firmware for Itead Studio Sonoff with OTA
 * update support. Subscribes to a topic and watches for a message of
 * either "0" (turn relay) or "1" (turn on relay) 
 * Subscribes to second topic to turn on and off the LED with 1 or 0
 * Publishes 1 to a topic on button press, so you can handle in your HAB software
 * 
 * Script based on https://github.com/SuperHouse/BasicOTARelay but customized to fit my needs
 */



#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient 
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
//for LED status
#include <Ticker.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson  MUST be version 5 not version 6
#include "definitions.h" /* Used to define things*/

const char* firmwarever = "v1.2";

/*******************
 * Set up the web server
 *******************/
ESP8266WebServer server(80);


/* Sonoff Outputs */
const int relayPin = 12;  // Active high
const int ledPin   = 13;  // Active low

/* MQTT Settings */
const char* mqttTopic = "house/sonoff1";   // MQTT Relay topic
const char* mqttTopic2 = "house/sonoff1/led"; // MQTT LED topic 
const char* buttonPressTopic = "house/sonoff1/button"; // MQTT Button Press Topic
const char* willTopic = "house/sonoff1/status"; // Topic for birth and will messages
const char* willMessage = "offline"; // Message to send when sonoff is offline

// IPAddress broker(10,1,10,4);          // Address of the MQTT broker
#define CLIENT_ID "sonoff1"         // Client ID to send to the broker

/* Button Settings */
long last_message_time        = 0;
byte last_button_state        = 0;
long minimum_message_interval = 5000;
byte button_pin = 0;

/* Initialize pubsubclient*/
WiFiClient wificlient;
PubSubClient client(wificlient);

/***************************
 * Set the includes and settings for the DHT22
 ***************************/
#ifdef DHT22 //If the DHT22 is defined 
  #include "DHT.h" //https://github.com/adafruit/DHT-sensor-library
  #define DHTPIN 14
  #define DHTTYPE DHT22
  /* Allows rate limiting on sensor readings */
  long dht_last_message_time        = 0;
  long dht_minimum_message_interval = 10000;
  DHT dht(DHTPIN, DHTTYPE);
#endif

/*************************
 * Set up Ticker 
 *************************/
Ticker ticker;
void tick()  //for boot up status
{
  //toggle state
  int state = digitalRead(ledPin);  // get the current state of GPIO1 pin
  digitalWrite(ledPin, !state);     // set pin to the opposite state
}


/************************************
 * Define default values for the mqtt and file saving
 ***********************************/
char mqtt_server[40];
char mqtt_user[32];
char mqtt_pass[32];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/************************************
 * MQTT callback to process incoming messages
 ************************************/
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

/***********************************
 * Attempt connection to MQTT broker and subscribe to command topic
 ***********************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_ID, mqtt_user,mqtt_pass,willTopic,0,0,willMessage)) {
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
      // needed to not get caught in a loop if no MQTT info
      ArduinoOTA.handle(); // for OTA
      server.handleClient(); //for web server
    }
  }
}
/*************************************
 * Set up Wifi manager callback
 * gets called when WiFiManager enters configuration mode
 **************************************/

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}



/*******************************
 * Setup
 *******************************/
void setup() {
  Serial.begin(115200);

  /* Set up the outputs. LED is active-low */
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP); // The input will be pulled LOW to trigger an action
  digitalWrite(ledPin, HIGH);
  digitalWrite(relayPin, LOW);
  
 // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);


/****************************
 * Read Config
 ****************************/
 //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  
/**************************
 * WIFI Manager
 **************************/
 
  //Local intialization. Once its business is done, there is no need to keep it around
  wifi_station_set_hostname("sonoff1");
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 32);
  WiFiManager wifiManager;
 
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // set callback that gets called when save notify 
  wifiManager.setSaveConfigCallback(saveConfigCallback);

// add all your parameters here

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  
  /*************************************************
  * Try and connect and if fail go to AP mode
  ************************************************/
  if (!wifiManager.autoConnect("sonoff1")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //turn LED off
  digitalWrite(ledPin, HIGH);

  /********************************
   * Read and save the file system
   *******************************/

//read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

/**************************
 * Set OTA up
 **************************/

   ArduinoOTA.setHostname("sonoff1");
  
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
  
  /* Prepare MQTT client */
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  
/*********************************
 * If using the DHT22 start using the sensor
 **********************************/
#ifdef DHT22 // If using a DHT22 then start the sensor
 dht.begin();
#endif    

/*******************
 * Web handlers 
 *******************/
   server.on("/reset_wlan", []() {
   server.send(200, "text/plain", "Resetting WLAN and restarting..." );
    delay(1000);
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
  });

  server.on("/restart", []() {
    server.send(200, "text/plain", "restarting..." );
    delay(1000);
    ESP.restart();
  });

  server.on("/firmware", []() {
    server.send(200, "text/plain", firmwarever );
  
  });
server.begin();

}



/**************************
 * Main Loop Here
 **************************/
void loop() {
  ArduinoOTA.handle(); // for OTA
  server.handleClient(); //for web server

  
 /*********************
  * If connected to Wifi connect to MQTT
  *********************/
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
  }


  /*************************
   * IF MQTT connected then do the incoming loop for messages 
   *************************/
  if (client.connected())
  {
    client.loop();

    /************************************ 
    * Read the state of a connected button and act if it has been pressed 
    **************************************/
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

/**********************************
 * If DHT22 then read sensor
 **********************************/
 
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

      client.publish("house/sonoff1/humidity", String(h).c_str()); // Send a status update
       client.publish("house/sonoff1/temp", String(f).c_str()); // Send a status update
       dht_last_message_time = millis();

    }

#endif
  
  } // ends if MQTT client conected loop
} // ends main loop
