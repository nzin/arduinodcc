#include <DCC_Decoder.h>
#define kDCC_INTERRUPT            0


#include <EEPROM.h>
#define EEPROM_LIGHTMODE 0     // we store the last "light mode" selected by the user (via the push button)
#define EEPROM_SWITCHSTATUS 1  // we store the last switch status (send via dcc)
#define EEPROM_ADDRESS 2  // we store our dcc address

#define LIGHT0 9
#define LIGHT1 10
#define LIGHT2 11
#define LEDCONTROL 12 
#define PUSHBUTTON 5
#define LEARNINGBUTTON 3
#define RELAY 4

#define MAXLIGHTMODE 2 // mode0=continuous, mode1=flicketing, mode2=roundrobin
int lightMode=0;
int switchStatus=HIGH;
int learningMode=LOW;



/**
 * this is just a function to show via the onboard PCB led, the state of the decoder
 */
void showAcknowledge(int nb) {
  for (int i=0;i<nb;i++) {
    digitalWrite(LEDCONTROL, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(LEDCONTROL, LOW);    // turn the LED off by making the voltage LOW
    delay(100);               // wait for a second
  }
}




/**
 * work done on connected lights depending on their state
 */
void treatLight(int switchStatus,int lightMode) {
  static int valLight[3]={0,0,0};
  static int blink=-1;
  static int chain=0;
  
  int tmp;
  
  switch (lightMode) {
    /*
     * mode 0:
     * - switch status = HIGH: we switch on the light in a fading in way.
     * - switch status = LOW: we switch off the light in a fading off way.
     */
    case 0:
      if (switchStatus==HIGH) {
        for (int i=0;i<3;i++) if (valLight[i]<255) valLight[i]+=5;
      } else {
        for (int i=0;i<3;i++) if (valLight[i]>0) valLight[i]-=5;
      }
      break;
    /*
     * mode 1:
     * - switch status = HIGH: we switch on the light in a fading in way.
     * - switch status = LOW: we switch off the light in a fading off way.
     * BUT randomly (1% of the time), we switch off a light and switch on it just after, to flicker it
     */
    case 1:
      if (switchStatus==HIGH) {
        for (int i=0;i<3;i++) if (valLight[i]<255) valLight[i]+=5;
      } else {
        for (int i=0;i<3;i++) if (valLight[i]>0) valLight[i]-=5;
      }
      
      /* 1% chance to flicker 1 of the 3 lights */
      if ((blink=random(100))<3) {
        tmp=valLight[blink];
        valLight[blink]=0;
        analogWrite(LIGHT0,valLight[0]);
        analogWrite(LIGHT1,valLight[1]);
        analogWrite(LIGHT2,valLight[2]);
        delay(50);
        valLight[blink]=tmp;
      }
      break;
    /*
     * mode 2: we switch on the first light, then the second light, then the 3rd light
     */
    case 2:
      chain++;
      if (chain==20) {
        valLight[0]=0;
        valLight[1]=255;
      } else if (chain==40) {
        valLight[1]=0;
        valLight[2]=255;
      } else if (chain==60) {
        valLight[2]=0;
        valLight[0]=255;
        chain=0;
      }
      break;
  }
  analogWrite(LIGHT0,valLight[0]);
  analogWrite(LIGHT1,valLight[1]);
  analogWrite(LIGHT2,valLight[2]);
}



/**
 * when a DCC packet is received, this function is called
 * for our needs, we decode the address:
 * - if we are in learning mode, we store this address as our new address
 * - if we are in normal mode, we switch on, or off light and relay, depending on the "enable" state command
 */
void BasicAccDecoderPacket_Handler(int address, boolean activate, byte data)
{
  // Convert NMRA packet address format to human address
  address -= 1;
  address *= 4;
  address += 1;
  address += (data & 0x06) >> 1;
    
  int enable = (data & 0x01) ? HIGH : LOW;
    
  // if we are in learning mode, we store our dcc address
  if (learningMode==HIGH) {
    EEPROM.write(EEPROM_ADDRESS,address%256);
    EEPROM.write(EEPROM_ADDRESS+1,address/256);
    showAcknowledge(3);
  } else {
    // if we are in normal mode, then... we act accordingly
    int myaddress=EEPROM.read(EEPROM_ADDRESS)+(EEPROM.read(EEPROM_ADDRESS+1)*256);
    if (myaddress==address) {
      switchStatus=enable;
      EEPROM.write(EEPROM_SWITCHSTATUS,switchStatus);
      digitalWrite(RELAY,switchStatus);
    }
  }
}



/*
 * standard arduino initialisation function
 * we initialize lights, button, and relay pins
 */
void setup() {
  // put your setup code here, to run once:
  pinMode(LIGHT0, OUTPUT);     
  pinMode(LIGHT1, OUTPUT);     
  pinMode(LIGHT2, OUTPUT);     
  pinMode(LEDCONTROL, OUTPUT);     
  pinMode(PUSHBUTTON, INPUT);
  pinMode(LEARNINGBUTTON,INPUT);
  pinMode(RELAY,OUTPUT);
  
  lightMode=EEPROM.read(EEPROM_LIGHTMODE);
  if (lightMode>MAXLIGHTMODE) lightMode=0;
  switchStatus=EEPROM.read(EEPROM_SWITCHSTATUS);
  if (switchStatus!=HIGH && switchStatus!=LOW) switchStatus=HIGH;
  digitalWrite(RELAY,switchStatus);
  
  showAcknowledge(lightMode+1);
  randomSeed(analogRead(A3));
  
  DCC.SetBasicAccessoryDecoderPacketHandler(BasicAccDecoderPacket_Handler, true);
//  ConfigureDecoder();
  DCC.SetupDecoder( 0x00, 0x00, kDCC_INTERRUPT );
}



void loop() {
  static int pushbuttonOldval=0,pushbuttonVal=0;
  static int learningbuttonOldval=0,learningbuttonVal=0;
  static unsigned long timer=0;
  
  ////////////////////////////////////////////////////////////////
  // Loop DCC library
  DCC.loop();
  
  ////////////////////////////////////////////////////////////////
  // check if the push button has been pressed
  pushbuttonVal=digitalRead(PUSHBUTTON);
  if (pushbuttonVal==LOW && pushbuttonOldval!=pushbuttonVal) {
    lightMode++;
    if (lightMode>MAXLIGHTMODE) lightMode=0;
    EEPROM.write(EEPROM_LIGHTMODE,lightMode);
    showAcknowledge(lightMode+1);
  }
  pushbuttonOldval=pushbuttonVal;
  
  ////////////////////////////////////////////////////////////////
  // check if the learning button has been enabled
  learningbuttonVal=digitalRead(LEARNINGBUTTON);
  if (learningbuttonOldval!=learningbuttonVal) {
    learningMode=learningbuttonVal;
    if (learningMode==HIGH) showAcknowledge(3);
  }
  learningbuttonOldval=learningbuttonVal;
  
  ////////////////////////////////////////////////////////////////
  // check if we have light to change from state
  if (millis()-timer>50) {
    treatLight(switchStatus,lightMode);
    timer=millis();
  }
}

