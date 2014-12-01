/* MPR121 Keypad Example Phone Dialing Code
 by: Jim Lindblom
     SparkFun Electronics
 created on: 1/6/10
 license: CC-SA 3.0
 
 Turns the MPR121 Touchpad into a phone keypad. Pressing a pad will
 print a 0-9, * or #.
 
 Hardware: 3.3V Arduino Pro Mini
           SDA -> A4
           SCL -> A5
           IRQ -> D2
           
 I wasn't having any luck using the Wire.h library, so I've adapted
 I2C code from an ATmega328 library to get this working.
 If you can get this working with the Wire.h library, I'd be thrilled to hear!
*/

#include "mpr121.h"
#include "i2c.h"
#include <LiquidCrystal.h>
#include <NewTone.h>

#define MPR121_R 0xB5	// ADD pin is grounded
#define MPR121_W 0xB4	// So address is 0x5A

#define PHONE_DIGITS 10  // 10 digits in a phone number

// Match key inputs with electrode numbers
#define STAR 0
#define SEVEN 1
#define FOUR 2
#define ONE 3
#define ZERO 4
#define EIGHT 5
#define FIVE 6
#define TWO 7
#define POUND 8
#define NINE 9
#define SIX 10
#define THREE 11

int irqpin = 0;  // D2

uint16_t touchstatus;
char phoneNumber[PHONE_DIGITS];

//LCD Shit

LiquidCrystal lcd(12,11,5,4,3,2);



//  VARIABLES
int pulsePin = 1;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 7;                // pin to blink led at each beat
int fadePin = 5;                  // pin to do fancy classy fading blink at each beat
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin


// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded! 
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

//Temperture Sensor Variables
float degreesF;
const int temperaturePin = 0; //temperature sensor

//Buffer Alarm Variables
const int buzzerPin = 9; //buzzer
const int songLength = 18; //buzzer
char notes[] = "cdfda ag cdfdg gf "; // a space represents a rest
int beats[] = {1,1,1,1,1,1,4,4,2,1,1,1,1,1,1,4,4,2};
int tempo = 150;




void setup()
{
  pinMode(irqpin, INPUT);
  digitalWrite(irqpin, HIGH);
  
  Serial.begin(9600);
  DDRC |= 0b00010011;
  PORTC = 0b00110000;  // Pull-ups on I2C Bus
  i2cInit();
  
  delay(100);
  mpr121QuickConfig();
  
  //Calling the heartbeat setup
  heartBeatSetUp();
  Serial.begin(9600); // temperature sensor

       //LCD
   lcd.begin(16, 2);
   lcd.clear();
}

void loop()
{
  getPhoneNumber();
  Serial.print("\nDialing... ");
   lcd.print("\nDialing... ");
  for (int i=0; i<PHONE_DIGITS; i++){
    if(phoneNumber[i]=='7'){
              buzzerSetup();
    }
    Serial.print(phoneNumber[i]);
    lcd.print(phoneNumber[i]);
  }
  while(1)
    ;
}

void getPhoneNumber()
{
  int i = 0;
  int touchNumber;
  
  Serial.println("Please Enter a phone number...");
  lcd.print("Please Enter a phone number...");
  
  while(i<PHONE_DIGITS)
  {
    while(checkInterrupt())
      ;
    touchNumber = 0;
    
    touchstatus = mpr121Read(0x01) << 8;
    touchstatus |= mpr121Read(0x00);
    
    for (int j=0; j<12; j++)  // Check how many electrodes were pressed
    {
      if ((touchstatus & (1<<j)))
        touchNumber++;
    }
    
    if (touchNumber == 1)
    {
      if (touchstatus & (1<<STAR))
        heartBeatLoop();
      else if (touchstatus & (1<<SEVEN))
          phoneNumber[i] = '7';
      else if (touchstatus & (1<<FOUR))
        phoneNumber[i] = '4';
      else if (touchstatus & (1<<ONE))
        phoneNumber[i] = '1';
      else if (touchstatus & (1<<ZERO))
        phoneNumber[i] = '0';
      else if (touchstatus & (1<<EIGHT))
        phoneNumber[i] = '8';
      else if (touchstatus & (1<<FIVE))
        phoneNumber[i] = '5';
      else if (touchstatus & (1<<TWO))
        phoneNumber[i] = '2';
      else if (touchstatus & (1<<POUND))
        phoneNumber[i] = '#';
      else if (touchstatus & (1<<NINE))
        phoneNumber[i] = '9';
      else if (touchstatus & (1<<SIX))
        phoneNumber[i] = '6';
      else if (touchstatus & (1<<THREE))
        phoneNumber[i] = '3';
        
      Serial.print(phoneNumber[i]);
      lcd.print(phoneNumber[i]);
      i++;
    }
    else if (touchNumber == 0)
      ;
    else{
      Serial.println("Only touch ONE button!");
      lcd.print("Only touch ONE button!");
    }
  }
}

byte mpr121Read(uint8_t address)
{
  byte data;
  
  i2cSendStart();
  i2cWaitForComplete();
  
  i2cSendByte(MPR121_W);	// write 0xB4
  i2cWaitForComplete();
  
  i2cSendByte(address);	// write register address
  i2cWaitForComplete();
  
  i2cSendStart();
  
  i2cSendByte(MPR121_R);	// write 0xB5
  i2cWaitForComplete();
  i2cReceiveByte(TRUE);
  i2cWaitForComplete();
  
  data = i2cGetReceivedByte();	// Get MSB result
  i2cWaitForComplete();
  i2cSendStop();
  
  cbi(TWCR, TWEN);	// Disable TWI
  sbi(TWCR, TWEN);	// Enable TWI
  
  return data;
}

void mpr121Write(unsigned char address, unsigned char data)
{
  i2cSendStart();
  i2cWaitForComplete();
  
  i2cSendByte(MPR121_W);// write 0xB4
  i2cWaitForComplete();
  
  i2cSendByte(address);	// write register address
  i2cWaitForComplete();
  
  i2cSendByte(data);
  i2cWaitForComplete();
  
  i2cSendStop();
}

void mpr121QuickConfig(void)
{
  // Section A
  // This group controls filtering when data is > baseline.
  mpr121Write(MHD_R, 0x01);
  mpr121Write(NHD_R, 0x01);
  mpr121Write(NCL_R, 0x00);
  mpr121Write(FDL_R, 0x00);

  // Section B
  // This group controls filtering when data is < baseline.
  mpr121Write(MHD_F, 0x01);
  mpr121Write(NHD_F, 0x01);
  mpr121Write(NCL_F, 0xFF);
  mpr121Write(FDL_F, 0x02);
  
  // Section C
  // This group sets touch and release thresholds for each electrode
  mpr121Write(ELE0_T, TOU_THRESH);
  mpr121Write(ELE0_R, REL_THRESH);
  mpr121Write(ELE1_T, TOU_THRESH);
  mpr121Write(ELE1_R, REL_THRESH);
  mpr121Write(ELE2_T, TOU_THRESH);
  mpr121Write(ELE2_R, REL_THRESH);
  mpr121Write(ELE3_T, TOU_THRESH);
  mpr121Write(ELE3_R, REL_THRESH);
  mpr121Write(ELE4_T, TOU_THRESH);
  mpr121Write(ELE4_R, REL_THRESH);
  mpr121Write(ELE5_T, TOU_THRESH);
  mpr121Write(ELE5_R, REL_THRESH);
  mpr121Write(ELE6_T, TOU_THRESH);
  mpr121Write(ELE6_R, REL_THRESH);
  mpr121Write(ELE7_T, TOU_THRESH);
  mpr121Write(ELE7_R, REL_THRESH);
  mpr121Write(ELE8_T, TOU_THRESH);
  mpr121Write(ELE8_R, REL_THRESH);
  mpr121Write(ELE9_T, TOU_THRESH);
  mpr121Write(ELE9_R, REL_THRESH);
  mpr121Write(ELE10_T, TOU_THRESH);
  mpr121Write(ELE10_R, REL_THRESH);
  mpr121Write(ELE11_T, TOU_THRESH);
  mpr121Write(ELE11_R, REL_THRESH);
  
  // Section D
  // Set the Filter Configuration
  // Set ESI2
  mpr121Write(FIL_CFG, 0x04);
  
  // Section E
  // Electrode Configuration
  // Enable 6 Electrodes and set to run mode
  // Set ELE_CFG to 0x00 to return to standby mode
  mpr121Write(ELE_CFG, 0x0C);	// Enables all 12 Electrodes
  //mpr121Write(ELE_CFG, 0x06);		// Enable first 6 electrodes
  
  // Section F
  // Enable Auto Config and auto Reconfig
  /*mpr121Write(ATO_CFG0, 0x0B);
  mpr121Write(ATO_CFGU, 0xC9);	// USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   mpr121Write(ATO_CFGL, 0x82);	// LSL = 0.65*USL = 0x82 @3.3V
  mpr121Write(ATO_CFGT, 0xB5);*/	// Target = 0.9*USL = 0xB5 @3.3V
}

byte checkInterrupt(void)
{
  if(digitalRead(irqpin))
    return 1;

  return 0;
}




////////////////////////////////////////////////////////////////////////////////////////

//Loop and SetUp functions

////////////////////////////////////////////////////////////////////////////////////////
void heartBeatSetUp(){
   pinMode(blinkPin,OUTPUT);         // pin that will blink to your heartbeat!
  pinMode(fadePin,OUTPUT);          // pin that will fade to your heartbeat!
  Serial.begin(115200);             // we agree to talk fast!
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS 
   // UN-COMMENT THE NEXT LINE IF YOU ARE POWERING The Pulse Sensor AT LOW VOLTAGE, 
   // AND APPLY THAT VOLTAGE TO THE A-REF PIN
   //analogReference(EXTERNAL);   
  
}

void heartBeatLoop(){
    sendDataToProcessing('S', Signal);     // send Processing the raw Pulse Sensor data
  if (QS == true){                       // Quantified Self flag is true when arduino finds a heartbeat
        fadeRate = 255;                  // Set 'fadeRate' Variable to 255 to fade LED with pulse
        sendDataToProcessing('B',BPM);   // send heart rate with a 'B' prefix
        sendDataToProcessing('Q',IBI);   // send time between beats with a 'Q' prefix
        QS = false;                      // reset the Quantified Self flag for next time    
     }
  
  
  
  ledFadeToBeat();
  
  delay(20);       //  take a break
}

void tempertureLoop(){
  lcd.setCursor(1,0);
  
   float voltage, degreesC;// degreesF;

  voltage = getVoltage(temperaturePin);

  degreesC = (voltage - 0.5) * 100.0;
  
  degreesF = degreesC * (9.0/5.0) + 32.0;

  Serial.print("voltage: ");
  Serial.print(voltage);
  Serial.print("  deg C: ");
  Serial.print(degreesC);
  Serial.print("  deg F: ");
  Serial.println(degreesF);
  lcd.clear();
  lcd.print("  deg F: ");
  lcd.print(degreesF);
  
 //  lcd.setCursor(0,1);
   
 // lcd.print("I am awesome!");

  // These statements will print lines of data like this:
  // "voltage: 0.73 deg C: 22.75 deg F: 72.96"

  // Note that all of the above statements are "print", except
  // for the last one, which is "println". "Print" will output
  // text to the SAME LINE, similar to building a sentence
  // out of words. "Println" will insert a "carriage return"
  // character at the end of whatever it prints, moving down
  // to the NEXT line.
   
  delay(1000); // repeat once per second (change as you wish!)
  
}

////////////////////////////////////////////////////////////////////////////////////////

//HeartBeat Functions

////////////////////////////////////////////////////////////////////////////////////////

void ledFadeToBeat(){
    fadeRate -= 15;                         //  set LED fade value
    fadeRate = constrain(fadeRate,0,255);   //  keep LED fade value from going into negative numbers!
    analogWrite(fadePin,fadeRate);          //  fade LED
  }


void sendDataToProcessing(char symbol, int data ){
    Serial.print(symbol);                // symbol prefix tells Processing what type of data is coming
    Serial.println(data);                // the data to send culminating in a carriage return
  }


///////////////////////////////////////////////////////////////////////

//Buzzer Functions

///////////////////////////////////////////////////////////////////////
void buzzerSetup(){
 
  pinMode(buzzerPin, OUTPUT);//BUZZER
  
  int i, duration;
  
  for (i = 0; i < songLength; i++) // step through the song arrays
  {
    duration = beats[i] * tempo;  // length of note/rest in ms
    
    if (notes[i] == ' ')          // is this a rest? 
    {
      delay(duration);            // then pause for a moment
    }
    else                          // otherwise, play the note
    {
      NewTone(buzzerPin, frequency(notes[i]), duration);
      delay(duration);            // wait for tone to finish
    }
    delay(tempo/10);              // brief pause between notes
  }
  
}

int frequency (char note)
{
   
  int i;
  const int numNotes = 8;  // number of notes we're storing
  
  // The following arrays hold the note characters and their
  // corresponding frequencies. The last "C" note is uppercase
  // to separate it from the first lowercase "c". If you want to
  // add more notes, you'll need to use unique characters.

  // For the "char" (character) type, we put single characters
  // in single quotes.
   lcd.setCursor(0,1);
   lcd.print("   Wake Up!");

  char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
  int frequencies[] = {262, 294, 330, 349, 392, 440, 494, 523};
  
  // Now we'll search through the letters in the array, and if
  // we find it, we'll return the frequency for that note.
  
  for (i = 0; i < numNotes; i++)  // Step through the notes
  {
    if (names[i] == note)         // Is this the one?
    {
      return(frequencies[i]);     // Yes! Return the frequency
    }
  }
  return(0);  // We looked through everything and didn't find it,
              // but we still need to return a value, so return 0.
              
}
 
float getVoltage(int pin)
{
  return (analogRead(pin) * 0.004882814);

}

