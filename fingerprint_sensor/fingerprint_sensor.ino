/***************************************************
  Adafruit MQTT Library CC3000 Example

  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// Libraries
#include <Adafruit_SleepyDog.h>
#include <Adafruit_CC3000.h>
#include <SPI.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_CC3000.h"
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// CC3000 pins
#define ADAFRUIT_CC3000_IRQ   2  // MUST be an interrupt pin!
#define ADAFRUIT_CC3000_VBAT  5  // VBAT & CS can be any digital pins.
#define ADAFRUIT_CC3000_CS    10

// WiFi settings
#define WLAN_SSID       "your_wifi_ssid"  // can't be longer than 32 characters!
#define WLAN_PASS       "your_wifi_password"
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Adafruit IO parameters
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "adafruit_io_username"
#define AIO_KEY         "adafruit_io_key"
uint32_t state;

// CC3000 object
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT);

// Store the MQTT server, client ID, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
// Set a unique MQTT client ID using the AIO key + the date and time the sketch
// was compiled (so this should be unique across multiple devices for a user,
// alternatively you can manually set this to a GUID or other random value).
const char MQTT_CLIENTID[] PROGMEM  = "cc3000_fingerprint";
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the CC3000 MQTT class by passing in the CC3000 class and MQTT server and login details.
Adafruit_MQTT_CC3000 mqtt(&cc3000, MQTT_SERVER, AIO_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

// CC3000connect is a helper function that sets up the CC3000 and connects to
// the WiFi network. See the cc3000helper.cpp tab above for the source!
boolean CC3000connect(const char* wlan_ssid, const char* wlan_pass, uint8_t wlan_security);

// Fingerprint sensor feed
const char FINGERPRINT_FEED[] PROGMEM = AIO_USERNAME "/feeds/fingerprint";
Adafruit_MQTT_Publish fingerprint = Adafruit_MQTT_Publish(&mqtt, FINGERPRINT_FEED);

// Function to get fingerprint
int getFingerprintIDez();

// Init Software Serial
SoftwareSerial mySerial(3, 4);

// Fingerprint sensor instance
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Lock state
bool lockState = false;

// Your stored finger ID
int fingerID = 0;

// Counters
int activationCounter = 0;
int lastActivation = 0;
int activationTime = 10 * 1000;

void setup()  
{

  // Start Serial
  Serial.begin(115200);

  // Set the data rate for the sensor serial port
  finger.begin(57600);

  // Check if sensor is present
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1);
  }
  Serial.println("Waiting for valid finger...");

  // Initialise the CC3000 module
  Serial.print(F("\nInit the CC3000..."));
  if (!cc3000.begin())
      halt("Failed");

  while (! CC3000connect(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Retrying WiFi"));
    delay(1000);
  }
}

void loop()
{

  // Make sure to reset watchdog every loop iteration!
  Watchdog.reset();

  // Connect to Adafruit IO
  MQTT_connect();
  
  // Get fingerprint # ID
  int fingerprintID = getFingerprintIDez();

  // Activation ?
  if (fingerprintID == fingerID && lockState == false) {
    Serial.println(F("Access granted!"));
    lockState = true;
    state = 1;
    if (! fingerprint.publish(state)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
    lastActivation = millis();
 
  }

  // Set back to 0
  if ((activationCounter - lastActivation > activationTime) && lockState == true) {

    lockState = false;
    state = 0;
    if (! fingerprint.publish(state)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
    
  }

  activationCounter = millis();
  delay(50);            
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence); 
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID; 
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       if (ret < 0)
            CC3000connect(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);  // y0w, lets connect to wifi again
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}
