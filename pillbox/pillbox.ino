/*
 *  SW for the Arduino-based Smart Pillbox, written by Jorma Kuha. 
 *  For more information, please go to http://jorkke.github.io/pillbox.hmtl
 *  
 *  Version history: 
 *  June 21, 2020: First release.

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
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
const int switchPin = 6;
const int piezoPin = 8;

const int greenLed = 13;
const int yellowLed = 10;
const int redLed = 9;

const int enterSwitch = 7;
const int potAnalogPin = A5;

int switchState = 0;
int prevSwitchState = 0;
int reply;

bool ledToggle = false;


int potValue = 0;

int alarmIntervalStartCountHours = 0;
int alarmIntervalStartCountMinutes = 0;

long alarmIntervalRemainingSeconds = 0; // cannot be unsigned, goes negative

const long secondsBetweenAlarms = 12*60*60L; // 12 hours
const long w1Seconds = 120*60L; // warning 1 2 hours before the alarm
const long w2Seconds = 60*60;  // warning 2 1 hour before the alarm
const long alarmSeconds = 60*60; // alarm stays active 1 hour
const long alarm2Seconds = 120*60L; // alarm 2 stays active 2 hours after the alarm

// these values are used for testing: 
/*
const long secondsBetweenAlarms = 6*6;
const long w1Seconds = 12;
const long w2Seconds = 6;
const long alarmSeconds = 6;
const long alarm2Seconds = 12;
*/


// when alarm approaches, we give:
// first an "warning 1", 
// then "warning 2"
// then alarm itself
// then "high alarm 2"
// until box is opened or running out of time.
// All times are configurable

void(* resetFunc) (void) = 0;//declare reset function at address 0

enum State { Normal, Warning1, Warning2, Alarm, Alarm2 };
State newState = Normal;
State oldState = Normal;

struct StateBehaviour
{
    bool redLedSteady;
    bool redLedBlink;
    bool yellowLedSteady;
    bool yellowLedBlink;
    bool greenLedSteady;
    bool greenLedBlink;
    bool buzzTone;
    int  startInRelationToAlarm;
};

const StateBehaviour normal = {false, false, false, false, true, false, false, -1};
// not defining length in Minutes for normal state 

const StateBehaviour warning1 = {false, false, true, false, true, false, false, w1Seconds };  // yellow led steady
const StateBehaviour warning2 = {false, false, false, true, true, false, false, w2Seconds };  // yellow led blinking
const StateBehaviour alarm =    {true, false, false, false, true, false, false, alarmSeconds}; // red led steady
const StateBehaviour alarm2 =   {false, true, false, false, true, false, true, alarm2Seconds}; // red led blinking , also buzztone


void refreshStateBehaviour( const StateBehaviour& behave)
{
    lcd.clear();
    lcd.setCursor(0,0);
    switch (newState)
    {
        case Normal:
          lcd.print("Don't Panic!");      
        break;       
        case Warning1:
          lcd.print("First Warning!");
          break;

        case Warning2:
          lcd.print("Second Warning!");
          break;

        case Alarm: 
          lcd.print("Alarm! Take med!");
          break;

        case Alarm2:
          lcd.print("Alarm!!Take med!");
          break;  
    }
    lcd.setCursor(0,1);
    if (alarmIntervalRemainingSeconds >= 0)
    {
       lcd.print(" ");
    }
    else
    {
       lcd.print("-");
    }
    lcd.print(abs(alarmIntervalRemainingSeconds/(60*60) / 10 ));
    lcd.print(abs(alarmIntervalRemainingSeconds/(60*60) % 10));
    lcd.print(":");
    lcd.print(abs(alarmIntervalRemainingSeconds/(10*60) % 6));
    lcd.print(abs(alarmIntervalRemainingSeconds/(60) % 10));
    lcd.print(":");
    lcd.print(abs((alarmIntervalRemainingSeconds/10) % 6));
    lcd.print(abs(alarmIntervalRemainingSeconds % 10));
        

//////////////////////////////////////////////////
    if (behave.redLedBlink)
    {
      if (ledToggle)
      {
        digitalWrite(redLed, LOW);
      }
      else
      {
        digitalWrite(redLed, HIGH);
      }
    } else if (behave.redLedSteady)
    {
      digitalWrite(redLed, HIGH);
    }
    else
    {
      digitalWrite(redLed, LOW);
    };
//////////////////////////////////////////////////
    if (behave.yellowLedBlink)
    {
      if (ledToggle)
      {
        digitalWrite(yellowLed, LOW);
      }
      else
      {
        digitalWrite(yellowLed, HIGH);
      }
    } else if (behave.yellowLedSteady)
    {
      digitalWrite(yellowLed, HIGH);
    }
    else
    {
      digitalWrite(yellowLed, LOW);
    };
///////////////////////////////////////////////////
    if (behave.greenLedBlink)
    {
      if (ledToggle)
      {
        digitalWrite(greenLed, LOW);
      }
      else
      {
        digitalWrite(greenLed, HIGH);
      }
    } else if (behave.greenLedSteady)
    {
      digitalWrite(greenLed, HIGH);
    }
    else
    {
      digitalWrite(greenLed, LOW);
    };
///////////////////////////////////////////////////

    if (ledToggle)
    {
      ledToggle = false;
    }
    else
    {
      ledToggle = true;
    }

    if (behave.buzzTone)
    {
      tone (piezoPin, 400, 200);
    }    
};

void setAlarmStartCount()
{
  while (digitalRead(enterSwitch) == HIGH)
  {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Switch enter OFF");
      if (ledToggle)
      {
        ledToggle = false;
        digitalWrite(redLed, LOW);        
        digitalWrite(yellowLed, LOW);        
        digitalWrite(greenLed, LOW);        
      }
      else
      {
        ledToggle = true;
        digitalWrite(redLed, HIGH);
        digitalWrite(yellowLed, HIGH);
        digitalWrite(greenLed, HIGH);
      }
      delay(300);
   }
   
   digitalWrite(redLed, LOW);        
   digitalWrite(yellowLed, LOW);        
   digitalWrite(greenLed, LOW);        
   
   // let's read the time to the next alarm
   
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Time to med:");

  while (digitalRead(enterSwitch) == LOW)
  {
    lcd.setCursor(0,1);
    lcd.print("Hours: ");
    lcd.print(alarmIntervalStartCountHours= map(analogRead(potAnalogPin), 0, 1024, 23, -1));
    // The map-function is provided by the Arduino-library. It maps the values from one range to another range.
    //
    // Here we map the read analog value 0-1023 to hour values 23-0. 
    // they are written this way due to the way I happened to wire my pot - if you want to 
    // change the way the numbers increase/decrease, instead of "23, -1" write "0, 24" above)
    lcd.print(" ");
    delay(10);
  };

  while (digitalRead(enterSwitch) == HIGH)
  {
      lcd.setCursor(0,1);
      lcd.print("Switch enter OFF");
  };
  lcd.setCursor(0,1);
  lcd.print("                ");

  while (digitalRead(enterSwitch) == LOW)
  {
    lcd.setCursor(0,1);
    lcd.print("Minutes: ");
    alarmIntervalStartCountMinutes = map(analogRead(potAnalogPin), 0, 1024, 59, -5);        
    if (alarmIntervalStartCountMinutes <0) alarmIntervalStartCountMinutes = 0;    
    // here we not only map the values 0- 1023 to 59 - 0, but also use the map-fuction so that
    // the highest read voltage values are ignored. This is because they seem to be the most 
    // non-reliable when used in extreme conditions, such as with low-quality power supplies, 
    // high room temperatures (above 30 degrees celcius) etc.
    
    lcd.print( alarmIntervalStartCountMinutes );
    lcd.print(" ");
    delay(10);
  };

  alarmIntervalRemainingSeconds = ((unsigned long)alarmIntervalStartCountMinutes)*60 + 
    ((unsigned long)alarmIntervalStartCountHours)*60*60;
}


void setup() {
  // put your setup code here, to run once:
  lcd.begin(16, 2);
  lcd.print("Don't Panic!");
  delay(10);

  pinMode(enterSwitch, INPUT);
  pinMode(switchPin, INPUT);
  
  pinMode(greenLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(redLed, OUTPUT);

  while (digitalRead(enterSwitch) == LOW)
  {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Switch enter ON");
      if (ledToggle)
      {
        ledToggle = false;
        digitalWrite(redLed, LOW);        
        digitalWrite(yellowLed, LOW);        
        digitalWrite(greenLed, LOW);                
      }
      else
      {
        ledToggle = true;
        digitalWrite(redLed, HIGH);
        digitalWrite(yellowLed, HIGH);        
        digitalWrite(greenLed, HIGH);                
      }
      delay(300);
   }

  setAlarmStartCount();  
  
  // read the tilt-sensor states

  switchState = digitalRead(switchPin);
  prevSwitchState = switchState;    
}

void loop() {
  // put your main code here, to run repeatedly:

   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("Don't Panic!");

   unsigned long previousTime = millis();

   while(1)
   {
      while ((millis() - previousTime) < 1000L) {}; 
      // wait approx 1000 ms. More accurate than delay, taking into account the delay from the activities below. 
      previousTime = millis();
      
     alarmIntervalRemainingSeconds--;

      switchState = digitalRead(switchPin);
      if (switchState != prevSwitchState)
      {
         // changed - bpx opened
         prevSwitchState = switchState;

         oldState = newState;
       
         switch (oldState) 
         {
            case Normal:
                // if we are in Normal state, and the box is opened, we just ignore it
                break;

            // if we are in any other state, and the box is opened, we go to normal state 
            
            case Warning1:
            case Warning2:
            case Alarm:            
            case Alarm2:
                // Note: for Alarm & Alarm2,  alarmIntervalRemainingSeconds is now negative                
                
                alarmIntervalRemainingSeconds = secondsBetweenAlarms + alarmIntervalRemainingSeconds;          
                newState = Normal;
                break;                          
         } 
         refreshStateBehaviour( normal);                
      }
      else
      {
          // box was not opened
          // now scheck if state needs to be changed because of time expiring

          oldState = newState;
          if (alarmIntervalRemainingSeconds <= (  - alarm2.startInRelationToAlarm))
          {
            // situation is hopeless, no amount of alarms has helped
            // therefore just reset Arduino
            resetFunc();
          }
          else if (alarmIntervalRemainingSeconds < -alarm.startInRelationToAlarm)
          {
             newState = Alarm2;
             refreshStateBehaviour(alarm2);
          }
          else if (alarmIntervalRemainingSeconds < 0)
          {
             newState = Alarm;
             refreshStateBehaviour( alarm);
          }
          else if (alarmIntervalRemainingSeconds < warning2.startInRelationToAlarm)
          {
             newState = Warning2;
             refreshStateBehaviour( warning2);
          }
          else if (alarmIntervalRemainingSeconds < ( warning1.startInRelationToAlarm))
          {
             newState = Warning1;
             refreshStateBehaviour( warning1);
          }
          else
          {
             newState = Normal;
             refreshStateBehaviour( normal);
          }
      }
   }
};
