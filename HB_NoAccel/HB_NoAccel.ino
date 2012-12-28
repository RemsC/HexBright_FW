/* 

  Zebralight-esque firmware for HexBright FLEX 
  v1.0  Dec 28, 2012
  Code based on original HexBright firmware and 4-way button code forum post by Jeff Saltzman at Arduino.cc
  
*/

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                340
#define TIMEOUT                 1250
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define APIN_TEMP               0
#define APIN_CHARGE             3
// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3
#define MODE_BLINKING           4
#define MODE_ADJUST             5
#define MODE_INIT               6
// Events
#define IDLE 0
#define SINGLE_CLICK 1
#define DOUBLE_CLICK 2
#define PRESS_HOLD 3
#define LONG_HOLD 4

// State
byte mode = 0;
byte event = 0;
unsigned long eventTimeLast = 0;


//=================================================
//  MULTI-CLICK:  One Button, Multiple Events

// Button timing variables
int debounce = 20;          // ms debounce period to prevent flickering when pressing or releasing the button
int DCgap = 250;            // max ms between clicks for a double click event
int holdTime = 250;        // ms hold period: how long to wait for press+hold event
int longHoldTime = 1500;    // ms long hold period: how long to wait for press+hold event

// Button variables
boolean buttonVal = LOW;   // value read from button
boolean buttonLast = LOW;  // buffered value of the button's previous state
boolean DCwaiting = false;  // whether we're waiting for a double click (down)
boolean DConUp = false;     // whether to register a double click on next release, or whether to wait and click
boolean singleOK = true;    // whether it's OK to do a single click
long downTime = -1;         // time the button was pressed down
long upTime = -1;           // time the button was released
boolean ignoreUp = false;   // whether to ignore the button release because the click+hold was triggered
boolean waitForUp = false;        // when held, whether to wait for the up event
boolean holdEventPast = false;    // whether or not the hold event happened already
boolean longHoldEventPast = false;// whether or not the long hold event happened already

int checkButton(boolean init) {    
    byte event = 0;
    buttonVal = digitalRead(DPIN_RLED_SW);
    if (init) buttonVal = HIGH;
    // Button pressed down
    if ((buttonVal == HIGH && buttonLast == LOW && (millis() - upTime) > debounce))
    {
        downTime = millis();
        ignoreUp = false;
        waitForUp = false;
        singleOK = true;
        holdEventPast = false;
        longHoldEventPast = false;
        if ((millis()-upTime) < DCgap && DConUp == false && DCwaiting == true)  DConUp = true;
        else  DConUp = false;
        DCwaiting = false;
        // Serial.println("Event: 1st Click");

    }
    // Button released
    else if (buttonVal == LOW && buttonLast == HIGH && (millis() - downTime) > debounce)
    {        
        if (not ignoreUp)
        {
            upTime = millis();
            if (DConUp == false) DCwaiting = true;
            else
            {
                event = 2;
                DConUp = false;
                DCwaiting = false;
                singleOK = false;
                // Serial.println("Event: Double Click");

            }
        }
    }
    // Test for normal click event: DCgap expired
    if ( buttonVal == LOW && (millis()-upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true && event != 2)
//    if ( buttonVal == LOW && (millis()-upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true && event != 2)
    {
        event = 1;
        DCwaiting = false;
        // Serial.println("Event: Click");
    }
    // Test for hold
    if (buttonVal == HIGH && (millis() - downTime) >= holdTime) {
        // Trigger "normal" hold
        if (not holdEventPast)
        {
            event = 3;
            waitForUp = true;
            ignoreUp = true;
            DConUp = false;
            DCwaiting = false;
            downTime = millis();
            holdEventPast = true;
            //  Serial.println("Event: Press Hold");
        }
        // Trigger "long" hold
        if ((millis() - downTime) >= longHoldTime)
        {
            if (not longHoldEventPast)
            {
                event = 4;
                longHoldEventPast = true;
                // Serial.println("Event: Long Hold");
            }
        }
    }
    buttonLast = buttonVal;
    return event;
}

void setup()
{
  // We just powered on!  That means either we got plugged 
  // into USB, or the user is pressing the power button.
  pinMode(DPIN_PWR,      INPUT);
  digitalWrite(DPIN_PWR, LOW);

  // Initialize GPIO
  pinMode(DPIN_RLED_SW,  INPUT);
  pinMode(DPIN_GLED,     OUTPUT);
  pinMode(DPIN_DRV_MODE, OUTPUT);
  pinMode(DPIN_DRV_EN,   OUTPUT);
  digitalWrite(DPIN_DRV_MODE, LOW);
  digitalWrite(DPIN_DRV_EN,   LOW);
  pinMode(DPIN_PWR, OUTPUT);
  digitalWrite(DPIN_PWR, HIGH);

  // Initialize serial busses
  Serial.begin(9600);
  Wire.begin();

  downTime = millis() - debounce - debounce - 10;
  upTime = millis() - debounce - 5;
  
  event = checkButton(HIGH);
  mode = MODE_INIT;
  eventTimeLast = millis();
  pinMode(DPIN_PWR, OUTPUT);
  digitalWrite(DPIN_PWR, HIGH);


  Serial.println("Powered up!");
}

void loop()
{
  static unsigned long lastTempTime;
  unsigned long time = millis();
  
  // Check the state of the charge controller
  int chargeState = analogRead(APIN_CHARGE);
  if (chargeState < 128)  // Low - charging
  {
    digitalWrite(DPIN_GLED, (time&0x0100)?LOW:HIGH);
  }
  else if (chargeState > 768) // High - charged
  {
    digitalWrite(DPIN_GLED, HIGH);
  }
  else // Hi-Z - shutdown
  {
    digitalWrite(DPIN_GLED, LOW);    
  }
  
  // Check the temperature sensor
  if (time-lastTempTime > 1000)
  {
    lastTempTime = time;
    int temperature = analogRead(APIN_TEMP);
    Serial.print("Temp: ");
    Serial.println(temperature);
    if (temperature > OVERTEMP && mode != MODE_OFF)
    {
      Serial.println("Overheating!");

      for (int i = 0; i < 6; i++)
      {
        digitalWrite(DPIN_DRV_MODE, LOW);
        delay(100);
        digitalWrite(DPIN_DRV_MODE, HIGH);
        delay(100);
      }
      digitalWrite(DPIN_DRV_MODE, LOW);

      mode = MODE_LOW;
    }
  }

  // Do whatever this mode does
  switch (mode)
  {
  case MODE_BLINKING:
    digitalWrite(DPIN_DRV_EN, (time%100)<75);
    break;
  }
  
  // Periodically pull down the button's pin, since
  // in certain hardware revisions it can float.
  pinMode(DPIN_RLED_SW, OUTPUT);
  pinMode(DPIN_RLED_SW, INPUT);
  
  // Check for mode changes
  event = checkButton(LOW);
  byte newMode = modeSwitch(event, mode, time);

  // Do the mode transitions
  if (newMode != mode)
  {
    switch (newMode)
    {
    case MODE_INIT:
      Serial.println("Mode = init");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      break;  
    case MODE_OFF:
      Serial.println("Mode = off");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, LOW);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      break;
    case MODE_LOW:
      Serial.println("Mode = low");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 64);
      break;
    case MODE_MED:
      Serial.println("Mode = medium");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_HIGH:
      Serial.println("Mode = high");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_BLINKING:
      Serial.println("Mode = blinking");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      break;
    }
  // Remember mode state so we can shutdown after use rather than cycle
    eventTimeLast = time;
    Serial.print("eventTime : ");
    Serial.print(time);
    Serial.print(" eventTimeLast : ");
    Serial.println(eventTimeLast);

    mode = newMode;
  }

}

byte modeSwitch(byte event, byte mode, unsigned long time) {
  byte newMode = mode;
  switch (mode)
  {
  case MODE_INIT:
  case MODE_OFF:
    if (event == SINGLE_CLICK)
      newMode = MODE_LOW;
    if (event == PRESS_HOLD)
      newMode = MODE_MED;
    if (event == DOUBLE_CLICK)
      newMode = MODE_HIGH;
    if (event == LONG_HOLD)
      newMode = MODE_BLINKING;
    break;
  case MODE_LOW:
    if (event == SINGLE_CLICK && (time - eventTimeLast)<=TIMEOUT)
      newMode = MODE_MED;
    if (event == SINGLE_CLICK  && (time - eventTimeLast)>TIMEOUT)
      newMode = MODE_OFF;
    if (event == PRESS_HOLD)
      newMode = MODE_MED;
    if (event == DOUBLE_CLICK)
      newMode = MODE_HIGH;
    break;
  case MODE_MED:
    if (event == SINGLE_CLICK && (time - eventTimeLast)<=TIMEOUT)
      newMode = MODE_HIGH;
    if (event == LONG_HOLD)
      newMode = MODE_BLINKING;
    if (event == SINGLE_CLICK  && (time - eventTimeLast)>TIMEOUT)
      newMode = MODE_OFF;
    if (event == PRESS_HOLD)
      newMode = MODE_LOW;
    if (event == DOUBLE_CLICK)
      newMode = MODE_HIGH;
    break;
  case MODE_HIGH:
/*     if (event == SINGLE_CLICK   && (time - eventTimeLast)<=TIMEOUT)
      newMode = MODE_BLINKING;
    if (event == SINGLE_CLICK  && (time - eventTimeLast)>TIMEOUT) */
    if (event == SINGLE_CLICK)
      newMode = MODE_OFF;
    if (event == PRESS_HOLD)
      newMode = MODE_MED;
    if (event == DOUBLE_CLICK)
      newMode = MODE_LOW;
    break;
  case MODE_BLINKING:
    if (event == SINGLE_CLICK )
      newMode = MODE_OFF;
    break;
  }
  return newMode;
}
