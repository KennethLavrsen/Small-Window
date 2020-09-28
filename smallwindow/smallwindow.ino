// ESP8266 Wimos D1 mini 4M/1M SPIFFS

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "secrets.h"

// ****** All the configuration happens here ******
// Wifi
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress ip(192, 168, 1, 44); // update this to the desired IP Address
IPAddress dns(192, 168, 1, 1); // set dns to local
IPAddress gateway(192, 168, 1, 1); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network

// MQTT
const char* mqttServer        = MQTT_SERVER;
const char* mqttUsername      = MQTT_USER;
const char* mqttPassword      = MQTT_PASSWORD;
const int   mqttPort          = 1883;
const char* mqttTopicAnnounce = "smallwindow/announce";
const char* mqttTopicSet      = "smallwindow/set";
const char* mqttTopicState    = "smallwindow/state";

// Host name and OTA password
const char* hostName    = "smallwindow";
const char* otaPassword = OTA_PASSWORD;

// Time we run motor in seconds
#define MOTORTIME 14000

// Open and Closed Positions as seen by A/D
#define OPEN        200
#define CLOSED      990

// Pins for H-bridge motor driver
#define PIN_B1A D6
#define PIN_B1B D7

// ****** End of configuration ******


// Objects
ESP8266WebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);

// Globals
bool windowOpen = false;
bool windowOpening = false;
bool windowClosing = false;

unsigned long motorTime = 0;

int currentPosition;
int desiredPosition = 0;

unsigned long mqttReconnectTimer = 0;
unsigned long wifiReconnectTimer = 0;
unsigned long previousMillis;
char mqttBuf[10];


void mqttCallback(char* topic, byte* payload, unsigned int length) {

    // Creating safe local copies of topic and payload
    // enables publishing MQTT within the callback function
    // We avoid functions using malloc to avoid memory leaks
    
    char topicCopy[strlen(topic) + 1];
    strcpy(topicCopy, topic);
    
    char message[length + 1];
    for (int i = 0; i < length; i++) message[i] = (char)payload[i];
    
    message[length] = '\0';

    if ( strcmp(topicCopy, mqttTopicSet) == 0 ) {
        if ( strcmp(message, "open") == 0 ) openWindow();
        if ( strcmp(message, "close") == 0 ) closeWindow();
    }
    else return;
}

bool mqttConnect() {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(hostName, mqttUsername, mqttPassword)) {
        Serial.println("connected");
        // Once connected, publish an announcement...
        client.publish(mqttTopicAnnounce, "connected");
        client.subscribe(mqttTopicSet);
    }

    return client.connected();
}


void sendHTMLPage(void) {

    String webPage = "";
    webPage += "<html><head></head><body>\n";
    webPage += "<h1>Small Window</h1>";
    webPage += "<p>Position is ";
    webPage += currentPosition;
    webPage += "</p>";
    webPage += "<p><a href=\"/\"><button>Status Only</button></a></p>\n";
    webPage += "<p><a href=\"/Open\"><button>Open</button></a> <a href=\"/Close\"><button>Close</button></a></p>\n";
    webPage += "</body></html>\n";
    server.send(200, "text/html", webPage);
}


bool openWindow() {
    if (windowOpening) { return false; }
    windowOpen = true;
    windowOpening = true;
    windowClosing = false;
    digitalWrite(PIN_B1A, LOW);
    digitalWrite(PIN_B1B, HIGH);
    motorTime = millis();
    client.publish(mqttTopicState, "open", true );
    return true;
}

bool closeWindow(void) {
    if (windowClosing) { return false; }
    windowOpen = false;
    windowOpening = false;
    windowClosing = true;
    digitalWrite(PIN_B1B, LOW);
    digitalWrite(PIN_B1A, HIGH);
    motorTime = millis();
    client.publish(mqttTopicState, "closed", true );
    return true;
}

void setup_wifi() {
  
    Serial.print(F("Setting static ip to : "));
    Serial.println(ip);
   
    // Connect to WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  
    // ESP8266 does not follow same order as Arduino
    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet, dns); 
    WiFi.begin(ssid, password);
   
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");  
}

void setup() {

    pinMode(PIN_B1A, OUTPUT);
    pinMode(PIN_B1B, OUTPUT);

    digitalWrite(PIN_B1A, LOW);
    digitalWrite(PIN_B1B, LOW);

    //Setup UART
    Serial.begin(115200);
    Serial.println("");
    Serial.println("START");

    // Setup WIFI
    setup_wifi();
    wifiReconnectTimer = millis();

    // Setup Webserver
    server.on("/", [](){
        sendHTMLPage();
    });

    server.on("/Open",      [](){ openWindow();       sendHTMLPage(); });
    server.on("/Close",     [](){ closeWindow();      sendHTMLPage(); });

    server.begin();

    // Setup Over The Air (OTA) Reprogramming
    ArduinoOTA.setHostname(hostName);
    ArduinoOTA.setPassword(otaPassword);
    
    ArduinoOTA.onStart([]() {
        Serial.end();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
    
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    });
    ArduinoOTA.onEnd([]() {  });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {  });
    ArduinoOTA.onError([](ota_error_t error) {  });
    ArduinoOTA.begin();

    // Setup MQTT
    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback);
    mqttReconnectTimer = 0;
    mqttConnect();

    // Init globals
    currentPosition = analogRead(A0);
    previousMillis = millis();

}

void loop() {

    ESP.wdtFeed();
    delay(10);
    
    unsigned long currentTime = millis();

    // Handle WiFi
    if ( WiFi.status() != WL_CONNECTED ) {
        if ( currentTime - wifiReconnectTimer > 20000 )
            ESP.reset();
    } else
        wifiReconnectTimer = currentTime;

    // Handle MQTT
    if (!client.connected()) {
        if ( currentTime - mqttReconnectTimer > 5000 ) {
            mqttReconnectTimer = currentTime;
            if ( mqttConnect() ) {
                mqttReconnectTimer = 0;
            }
        }
    } else {
      client.loop();
    }

    // Handle Webserver Requests
    server.handleClient();
    // Handle OTA requests
    ArduinoOTA.handle();

    // A0 is between 0 and 1024 - we only need to care about percent.
    currentPosition = analogRead(A0);

    if ( ( windowOpening || windowClosing ) && ( millis() - motorTime > MOTORTIME ) ) {
        digitalWrite(PIN_B1A, LOW);
        digitalWrite(PIN_B1B, LOW);
        windowOpening = false;
        windowClosing = false;
    }

    if ( ( windowOpening && currentPosition < OPEN ) ||
         ( windowClosing && currentPosition > CLOSED) ) {
        digitalWrite(PIN_B1A, LOW);
        digitalWrite(PIN_B1B, LOW);
        windowOpening = false;
        windowClosing = false;         
    }

}
