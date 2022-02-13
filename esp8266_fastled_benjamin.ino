#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include <Timer.h>
#include <HSBColor.h>

/************************* WiFi Access Point *********************************/
#define WLAN_SSID       "Kiki-Wifi-2G"
#define WLAN_PASS       "Bensan1234"

//IPAddress staticIP(192, 168, 88, 203); //ESP static ip
IPAddress gateway(192, 168, 88, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
//IPAddress dns(192, 168, 88, 1);  //DNS

const char* deviceName = "BenjaminsHub";
/************************* MQTT Broker Setup *********************************/
#define mqtt_server      "192.168.5.10"
#define mqtt_serverport  1883                   // use 8883 for SSL
#define mqtt_username    "openhab"
#define mqtt_password    "2wbNjp8zCL"
/************************* Publisher Paths  *********************************/
#define path_ambientLight  "benjaminsroom/ambientLight"
#define path_patternColor "benjaminsroom/pattern/color"
#define path_patternRainbow "benjaminsroom/pattern/rainbow"
#define path_patternGlitter "benjaminsroom/pattern/glitter"
#define path_patternConfetti "benjaminsroom/pattern/confetti"
#define path_patternSinelon "benjaminsroom/pattern/sinelon"
#define path_patternJuggle "benjaminsroom/pattern/juggle"
#define path_patternBpm "benjaminsroom/pattern/bpm"
#define path_signalStrenght "benjaminsroom/rssi"
#define path_hum  "benjaminsroom/humidity"
#define path_temp "benjaminsroom/temperature"
#define path_hic "benjaminsroom/heatIndex"
#define path_eatNotification "kitchen/dinnerNotification"
/************************* Fast LED  *********************************/
#define NUM_LEDS 142
#define DATA_PIN 13
#define FRAMES_PER_SECOND  120//120
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
/************************* DHT22 *********************************/
#define DHTPIN 14
#define DHTTYPE DHT22
/************************* Variables  *********************************/
int globalHue, globalSat, globalBright;
int oldHue, oldBright;
long signalStrenght;
bool onFromOff = true;
/***********************************************************************/
void changeBrightness();
void turnOnFromOff(int hue, int sat, int bright, int time);
void changeColorTo(int hue, int sat, int bright);
void connectToWAP();
void setupMQTTsub();
void MQTT_connect();
void checkSubscription();
void publishSignalStrenght();
void eatNotification();
void takeAndPublishDHTreading();
void steadyColor();
void color();
void white();
void rainbow();
void rainbowWithGlitter();
void confetti();
void sinelon();
void juggle();
void bpm();
/***********************************************************************/
// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_LEDS];

typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {steadyColor, color, white, rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm};//steadyColor

int gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue=0; // rotating "base color" used by many of the patterns

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, mqtt_server, mqtt_serverport, mqtt_username, mqtt_password);

// Setup subscription for monitoring topic for changes.
Adafruit_MQTT_Subscribe sub_ambientLight = Adafruit_MQTT_Subscribe(&mqtt, path_ambientLight);
Adafruit_MQTT_Subscribe sub_patternRainbow = Adafruit_MQTT_Subscribe(&mqtt, path_patternRainbow);
Adafruit_MQTT_Subscribe sub_patternColor = Adafruit_MQTT_Subscribe(&mqtt, path_patternColor);
Adafruit_MQTT_Subscribe sub_patternGlitter = Adafruit_MQTT_Subscribe(&mqtt, path_patternGlitter);
Adafruit_MQTT_Subscribe sub_patternConfetti = Adafruit_MQTT_Subscribe(&mqtt, path_patternConfetti);
Adafruit_MQTT_Subscribe sub_patternSinelon = Adafruit_MQTT_Subscribe(&mqtt, path_patternSinelon);
Adafruit_MQTT_Subscribe sub_patternJuggle = Adafruit_MQTT_Subscribe(&mqtt, path_patternJuggle);
Adafruit_MQTT_Subscribe sub_patternBpm = Adafruit_MQTT_Subscribe(&mqtt, path_patternBpm);
Adafruit_MQTT_Subscribe sub_eatNotification = Adafruit_MQTT_Subscribe(&mqtt, path_eatNotification);

DHT dht(DHTPIN, DHTTYPE);
Timer timer;

// This function sets up the ledsand tells the controller about them
void setup() {
    delay(2000);
    Serial.begin(115200);
    
    Serial.println(F("\n\n\nBooting..."));

    connectToWAP();
    setupMQTTsub();

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

    timer.every(30000, publishSignalStrenght);
    //timer.every(30000, takeAndPublishDHTreading);

    randomSeed(analogRead(A0));

    //globalHue = random(360);
    //globalSat = 100;
    //globalBright = 100;

    LEDS.setBrightness(0);
    oldBright = 0;
    globalBright = 0;
    oldHue = -1;
}

void loop() {
  ArduinoOTA.handle();
  MQTT_connect();
  checkSubscription();

  //signalStrenght = WiFi.RSSI();

  gPatterns[gCurrentPatternNumber]();
  
  changeBrightness();

  FastLED.show();  
  //FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS(20) {gHue++;}
  //EVERY_N_SECONDS(5) {changeColorTo(random(360),100,100);} // slowly cycle the "base color" through the rainbow

  timer.update();
}
/************************* Functions  *********************************/
void changeBrightness()
{
  if(oldBright == globalBright)
    return;

  while(globalBright != oldBright)
  {
    if(oldBright > globalBright)
       oldBright--;
    else
       oldBright++;

    int calcBright = map(oldBright, 0, 100, 0, 255);
    LEDS.setBrightness(calcBright);

    FastLED.show();
    delay(1);
  }
  oldBright = globalBright;
}
void turnOnFromOff(int hue, int sat, int bright, int time)
{
  int calcBright = map(globalBright, 0, 100, 0, 255);
  LEDS.setBrightness(calcBright);

  int calcHue = map(hue, 0, 360, 0, 255);
  int calcSat = map(sat, 0, 100, 0, 255);
  int locBright = map(bright, 0, 100, 0, 255);

  FastLED.show();

  leds[NUM_LEDS/2] = CHSV(calcHue,calcSat,locBright);

  int j=(NUM_LEDS/2)-1;
  int k=(NUM_LEDS/2)+1;

  for(int i = 0; i < (NUM_LEDS/2); i++) {
    leds[j] = CHSV(calcHue,calcSat,locBright);
    leds[k] = CHSV(calcHue,calcSat,locBright);
    FastLED.show();
    delay(time);

    j--;
    k++;
  }

  //globalHue = hue;
  globalSat = sat;
  oldBright = globalBright;
}
void changeColorTo(int hue, int sat, int bright)
{
   while(hue != globalHue)
   {
      if(globalHue > hue)
         globalHue--;
      else
         globalHue++;

      for( int i = 0; i < NUM_LEDS; i++) 
      {
         int calcHue = map(globalHue, 0, 360, 0, 255);
         int calcSat = 255; //map(sat, 0, 100, 0, 255);
         int calcBright = 255; //map(bright, 0, 100, 0, 255);
         leds[i] = CHSV(calcHue,calcSat,calcBright);
      }
      FastLED.show();
      delay(1);
   }
   globalSat = sat;
   oldHue = globalHue;
   //globalBright = bright;
}
void connectToWAP()
{
  // Connect to WiFi access point.
  Serial.print("Connecting to "); Serial.println(WLAN_SSID);
  
  WiFi.disconnect();
  WiFi.hostname(deviceName);
  
  //WiFi.config(staticIP, gateway, subnet);
  
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  WiFi.mode(WIFI_STA);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting in 5 secs...");
    delay(5000);
    ESP.restart();
  }
}
void setupMQTTsub()
{
  // Setup MQTT subscription
  mqtt.subscribe(&sub_ambientLight);
  mqtt.subscribe(&sub_patternRainbow);
  mqtt.subscribe(&sub_patternColor);
  mqtt.subscribe(&sub_patternGlitter);
  mqtt.subscribe(&sub_patternConfetti);
  mqtt.subscribe(&sub_patternSinelon);
  mqtt.subscribe(&sub_patternJuggle);
  mqtt.subscribe(&sub_patternBpm);
  mqtt.subscribe(&sub_eatNotification);

  // Begin OTA
  ArduinoOTA.setPort(8266); // Port defaults to 8266
  ArduinoOTA.setHostname("benjaminsroom");   // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword((const char *)"Kde458asj9tri");   // No authentication by default

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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("");
  Serial.println("Ready & WiFi connected");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
}
void checkSubscription()
{
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(10)))
  {
    if (subscription == &sub_ambientLight) //[Lighting] - Ambient Lighting
    {
      String myString((char*)sub_ambientLight.lastread);

      if(myString == "OFF")
      {
        globalBright = 0;
        return;
      }
      if(myString == "ON")
      {
        return;
      }

      int commaIndex = myString.indexOf(',');
      int secondCommaIndex = myString.indexOf(',', commaIndex + 1);

      String firstValue = myString.substring(0, commaIndex);
      String secondValue = myString.substring(commaIndex + 1, secondCommaIndex);
      String thirdValue = myString.substring(secondCommaIndex + 1); // To the end of the string

      globalHue = firstValue.toInt();
      globalSat = secondValue.toInt();
      globalBright = thirdValue.toInt();//map(buff, 0, 100, 0, 255);

      Serial.println("AmbientLight Command Received");

      gCurrentPatternNumber = 0;
    }
    if (subscription == &sub_patternRainbow) //[Dimmable] - Rainbow
    {
      String myString((char*)sub_patternRainbow.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 3;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternColor) //[Dimmable] - Color
    {
      String myString((char*)sub_patternColor.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 1;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternGlitter) //[Dimmable] - Glitter
    {
      String myString((char*)sub_patternGlitter.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 4;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternConfetti) //[Dimmable] - Confetti
    {
      String myString((char*)sub_patternConfetti.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 5;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternSinelon) //[Dimmable] - Sinelon
    {
      String myString((char*)sub_patternSinelon.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 6;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternJuggle) //[Dimmable] - Juggle
    {
      String myString((char*)sub_patternJuggle.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 7;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_patternBpm) //[Dimmable] - BPM
    {
      String myString((char*)sub_patternBpm.lastread);

      if(myString == "ON")
      {
        globalBright = 100;
      }
      else
      {
        globalBright = myString.toInt();
      }

      gCurrentPatternNumber = 8;

      //mqtt.publish(path_ambientLight, "0,0,0");
    }
    if (subscription == &sub_eatNotification) //[Switchable] - Eating Notification
    {
      String myString((char*)sub_eatNotification.lastread);

      if(myString == "ON")
      {
        eatNotification();
      }
    }
  }
}
void publishSignalStrenght()
{
  int rssi;
  rssi = map(signalStrenght, -120, 0, 0, 100);

  char buffer[10];
  dtostrf(rssi,5, 1, buffer);
  
  mqtt.publish(path_signalStrenght,buffer);

  //mqtt.publish(path_signalStrenght, rssi);
}
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  //Serial.println("MQTT Connected!");

  Serial.println("\nbenjaminsroom Hub Online");
  mqtt.publish("benjaminsroom", "Benjamins Hub Online");
}

void nextPattern()
{
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
  //Serial.print("Current Pattern: "); Serial.println(gCurrentPatternNumber);
}
/************************* Patterns  *********************************/
void steadyColor() {
  int calcHue = map(globalHue, 0, 360, 0, 255);
  int calcSat = map(globalSat, 0, 100, 0, 255);
  int calcBright = map(globalBright, 0, 100, 0, 255);

  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(calcHue, calcSat, calcBright);
  }
}
void color() {
   int calcHue = map(gHue, 0, 360, 0, 255);
   int calcSat = 255; //map(globalSat, 0, 100, 0, 255);
   int calcBright = 255; //map(globalBright, 0, 100, 0, 255);

   for( int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(calcHue, calcSat, calcBright);
   }
}
void white() {
   //LEDS.setBrightness(255);
   for( int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::White;
   }
}
void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}
void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}
void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}
void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}
void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}
void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}
void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}
void eatNotification() {

  int oldBrightness = globalBright;
  int offBrightness = globalBright;
  globalBright = 100;
  changeBrightness();

  CRGB currentLedStatus[NUM_LEDS];

  
  if(offBrightness == 0)
  {
    for( int i = 0; i < NUM_LEDS; i++) {
      currentLedStatus[i] = CRGB::Black;
    }
  }
  else
  {
    for( int i = 0; i < NUM_LEDS; i++) {
      currentLedStatus[i] = leds[i];
    }
  }

  for (int i = 0; i < 2; ++i)
  {
    for( int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::White;
    }
    FastLED.show();
    delay(500);
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = currentLedStatus[i];
    }
    FastLED.show();
    delay(200);
  }

  globalBright = oldBrightness;
  changeBrightness();
}

void takeAndPublishDHTreading()
{
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  float hic = dht.computeHeatIndex(temp, hum, false);

  if (isnan(hum) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  char buffer[10];
  dtostrf(temp,5, 1, buffer);
  for (int i = 0; i < 10; i++)
  {
    if (buffer[i] == ' ')
        buffer[i] = '0';
  }
  mqtt.publish(path_temp,buffer);
  // Serial.print("\nBuffer: "); Serial.print(buffer);
  dtostrf(hum,5, 1, buffer);
  for (int i = 0; i < 10; i++)
  {
    if (buffer[i] == ' ')
        buffer[i] = '0';
  }
  mqtt.publish(path_hum,buffer);

  dtostrf(hic,5, 1, buffer);
  for (int i = 0; i < 10; i++)
  {
    if (buffer[i] == ' ')
        buffer[i] = '0';
  }
  mqtt.publish(path_hic,buffer);

  // Serial.print("\nHumidity: ");
  // Serial.print(hum);
  // Serial.print("\nTemperature: ");
  // Serial.print(temp);
}
