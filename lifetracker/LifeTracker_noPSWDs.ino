/*
 *  SW for the Arduino-based Life Tracker, written by Jorma Kuha. 
 *  For more information, please go to http://jorkke.github.io/lifetracker/lifetracker.hmtl
 *  
 *  Version history: 
 *  July 5, 2020: First release.

Copyright (c) 2020 Jorma Kuha

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 * 
 */

#include <LiquidCrystal.h>
#include <SPI.h>
#include <ArduinoHttpClient.h>  // you need to install the library
#include <WiFiNINA.h>


LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


const int switchPin = 7;   // Emergency switch pin - Arduino Digital pin 7

/////// data for five networks here:
char ssid5[] = "<NETWORK5_NAME>";    // your network SSID (name)
char pass5[] = "<NETWORK5_PASSWD";      // your network password (use for WPA, or use as key for WEP)

char ssid1[] = "<NETWORK1_NAME>";        // both letter O
char pass1[] = "<NETWORK1_PASSWD>";    // your network password (use for WPA, or use as key for WEP)

char ssid2[] = "<NETWORK2_NAME>";        // both zero
char pass2[] = "<NETWORK2_PASSWD>";    // your network password (use for WPA, or use as key for WEP)

char ssid3[] = "<NETWORK3_NAME>";        // first letter, second zero
char pass3[] = "<NETWORK3_PASSWD>";    // your network password (use for WPA, or use as key for WEP)

char ssid4[] = "<NETWORK4_NAME>";        // first zero, second letter
char pass4[] = "<NETWORK4_PASSWD>";    // your network password (use for WPA, or use as key for WEP)


int status = WL_IDLE_STATUS;     // the Wifi radio's status

// Define Prowl API information
#define PROWL_API_KEY_1  "<ENTER_API_KEY_HERE>"
#define PUSH_APPLICATION_1 "Arduino LifeTracker"
#define PUSH_EVENT_1 "Example Activity Alarm"
#define PUSH_DESCRIPTION_1 "No movement for configured interval!"

#define PROWL_API_KEY_2  "<ENTER_API_KEY_HERE>"
#define PUSH_APPLICATION_2 "Arduino LifeTracker"
#define PUSH_EVENT_2 "Example Activity Alarm"
#define PUSH_DESCRIPTION_2 "Emergency switch pressed manually!"

enum LifeTrackerAction  { NoActivityAlarm, EmergencySwitchAlarm};

void sendProwlNotification( LifeTrackerAction action); // function definition later


///////////////////////////////////////////////////////////////////////////////////
struct MotionTracker
//////////////////////////////////////////////////////////////////////////////////
{
  const int pirPin = 6;   // PIR Out pin  - Digital Pin 6
  int pirStat;
  
  unsigned long timeToWorry = 1000L*60L*60L*9L; // 9 hours, stated in milliseconds
//  unsigned long timeToWorry = 1000L*60L*5; // 5 minutes, for testing

  
  unsigned long lastTimeMotionDetected;
  bool previousUpdateHigh; // yes, if on previous update motion was detected

  enum MotionStatus { MotionDetected, NoMotionDetectedRecently};
  MotionStatus motionStatus;  

  bool alarmStatus; // if alarm related to no motion given already or not
  
  MotionTracker()
  {
    lastTimeMotionDetected = 0;
    previousUpdateHigh = false;
    alarmStatus = false;
    motionStatus = NoMotionDetectedRecently;
    pirStat = LOW;
  };

  MotionStatus update()
  {
    
     pirStat = digitalRead(pirPin);
     Serial.println(pirStat);
     unsigned long currentTime = millis();
     switch (motionStatus)
     {
       case MotionDetected:
           if (pirStat == LOW) // no motion detected
           {
             motionStatus = NoMotionDetectedRecently;  
           }
           else
           {
             lastTimeMotionDetected = currentTime;           
           }
       break;
       
       case NoMotionDetectedRecently:
         if (pirStat == HIGH) // motion detected
         {
            motionStatus = MotionDetected;
            lastTimeMotionDetected = currentTime; 
            alarmStatus = false; // even if we had given an alarm of no motion, this movement "cancels" it           
         }
         else if (currentTime > (lastTimeMotionDetected + timeToWorry))
         {
           if (alarmStatus == false)
           {
             sendProwlNotification( NoActivityAlarm);
             alarmStatus =  true;
           }            
         }       
       break;
     }
         
     lcd.clear();
     lcd.setCursor(0,0);
     if (motionStatus == MotionDetected)
     {
       lcd.print("Liike havaittu! ");
       Serial.println("Motion detected!");
     }
     else
     {
       lcd.print("Ei havaintoa.   ");
       Serial.println("No motion detected!");       
     }        
  }    
};
//////////////////////////////////////////////////////////////////////////////////
MotionTracker motionTracker;


const char serverName[] = "api.prowlapp.com";
const char serverDirectory[] = "/publicapi/add";
int port = 80;
WiFiClient wifi;

void setup() {
  // setup code here, to run once:
  pinMode(motionTracker.pirPin, INPUT);    
  pinMode(switchPin, INPUT);    
  
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Don't Panic!");

  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    lcd.println("Wifi module");
    lcd.print("failed!");
    // don't continue
    while (true);
  }

/*
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
*/  

  while (status != WL_CONNECTED)
  {
    // attempt to connect to Wifi network:
    // try first network 1:
  
    Serial.print("Attempting to connect to WPA SSID, network 1: ");
    Serial.println(ssid1);
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Yritys 1:");
    lcd.setCursor(0,1);    
    lcd.print(ssid1);
    
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid1, pass1);

    // wait 10 seconds for connection:
    delay(10000);  
    if (status != WL_CONNECTED)
    {
      // attempt to connect to network 2:

      Serial.print("Attempting to connect to WPA SSID, network 2: ");
      Serial.println(ssid2);
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Yritys 2:");
      lcd.setCursor(0,1);    
      lcd.print(ssid2);      
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid2, pass2);

      // wait 10 seconds for connection:
      delay(10000);     
    }
    if (status != WL_CONNECTED)
    {
      // attempt to connect to network 3:

      Serial.print("Attempting to connect to WPA SSID, network 3: ");
      Serial.println(ssid3);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Yritys 3:");
      lcd.setCursor(0,1);    
      lcd.print(ssid3);      
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid3, pass3);

      // wait 10 seconds for connection:
      delay(10000);     
    }
    if (status != WL_CONNECTED)
    {
      // attempt to connect to network 4:

      Serial.print("Attempting to connect to WPA SSID, network 4: ");
      Serial.println(ssid4);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Yritys 4:");
      lcd.setCursor(0,1);    
      lcd.print(ssid4);      
      
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid4, pass4);

      // wait 10 seconds for connection:
      delay(10000);     
    }
    if (status != WL_CONNECTED)
    {
      // attempt to connect to network 5:

      Serial.print("Attempting to connect to WPA SSID, network 5: ");
      Serial.println(ssid5);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Yritys 5:");
      lcd.setCursor(0,1);    
      lcd.print(ssid5);      
      
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid5, pass5);

      // wait 10 seconds for connection:
      delay(10000);     
    }        
  }
  
  // you're connected now, so print out the data:

  lcd.clear();
  
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWifiData();

  // sendProwlNotification(NoActivityAlarm);
  //sendProwlNotification( AllIsWellNotification);
}


void loop() {
  // put your main code here, to run repeatedly:

   motionTracker.update();

   if (digitalRead(switchPin) == HIGH)
   {
       Serial.print("HIGH");
       sendProwlNotification(EmergencySwitchAlarm );
   }

  delay(1000); // 1 second
  
}


void sendProwlNotification( LifeTrackerAction action) 
{

  // Create HTTPClient
   HttpClient client(wifi, serverName, port);  

  // Create POST parameters
    String  postParameters;

    switch (action)
    {
      case NoActivityAlarm:
          postParameters = "apikey="  PROWL_API_KEY_1  "&application="  PUSH_APPLICATION_1   "&event="  PUSH_EVENT_1  "&description="  PUSH_DESCRIPTION_1;
      break;
      case EmergencySwitchAlarm:
          postParameters = "apikey="  PROWL_API_KEY_2  "&application="  PUSH_APPLICATION_2   "&event="  PUSH_EVENT_2  "&description="  PUSH_DESCRIPTION_2;      
      break;
    }
    
  // Send POST to server

  int statusCode = 0;
  do
  {
    client.post(serverDirectory, "application/x-www-form-urlencoded", postParameters);
    statusCode = client.responseStatusCode();
    
    Serial.print("Status code:");
    Serial.println(statusCode);
    Serial.print("Response:");
    Serial.println(client.responseBody());

    if (statusCode != 200)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Virhekoodi:");    
      lcd.setCursor(0,1);
      lcd.print(statusCode);
      delay(60000); // 60 seconds              
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Viestitys       ");    
      lcd.setCursor(0,1);
      lcd.print("onnistui!       ");         
      delay(60000); // 60 seconds     
    }

  } while (statusCode != 200); // not a success
} 

void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
