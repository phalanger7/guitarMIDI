

/*
 * Firmware for XAD GC1: A Guitar MIDI controller
 * Created in 2024 by Alexander Adema
 * 
 * This software controls a MIDI interface designed to control guitar software such as ampsims (change preset, control individual effects; looper)
 * It is designed for an Arduino Micro Pro controller, It can be run as-is on some other Arduino's but not all, dyor. 
 * The controller this software is for contains 5 momentary switches, an input for an expression pedal and an OLED status screen. I used a 1.3" 128x64 monochrome OLED display with 1106 chip, using the Adafruit library. 
 * If you use a different display you may need to change the graphics library.
 * It is meant to be used with software guitar amp-sims. I use it with NeuralDSP plugins under REAPER DAW, but should work with any software that can use MIDI program change and note data for its plugins. It uses MIDI over USB.
 *
 * Hardware used:
 * 
 * - Arduino micro Pro clone with usb-c
 * - 4 momentary footswitches (adding more would be easy)
 * - a 'stereo' 1/4in jack socket for expression pedal, i used an M-Audio EX=P but any brand should work
 * - a 1.3in monochrome OLED with 128x64 pixels (1106 type)
 * 
 * Functionality:
 * - 1 Control button that switches between 4 modes: Pedal FX mode (default), select preset from Bank A, select preset from Bank B, Looper
 * - 3 buttons which function depends on mode: pedal control (mode 0), pick a preset from Bank A (mode 1), pick from Bank B (mode 2), REC/DUB + PLAY/STOP + CLEAR last
 * - EXP pedal (2x?)
 * - an OLED display that shows the current mode and preset, and flashes up when a pedal is pressed, as feedback that this switch is activated
 * 
 * I have kept the control mechanism as simple as possible, to facilitate ease of use over maximizing options. For that reason there are at the moment no double-tap functions or for pressing two 
 * buttons at once, etc. I did implement it but the added complication of operating was not worth the extra mappable switch.
 * I have also considered adding a small OLED below each button as a dynamic label. But since there is no universal way to get the current preset, pedal status etc from plugins, their
 * functionality would be limited. For this reason there are also no status LEDs for the pedals since we don't know whether it's switching the pedal on or off.
 * 
 * The control logic / usage is as follows:
 * 
 * At startup and after every preset change, the pedal mode is activated - pedals 2, 3 and 4 can be used to directly switch virtual pedals in the ampsim.
 * To select a preset, press the control button once (bank A) or twice (bank B) and then click one of the 3 other switches to select the desired preset from the bank.
 * THis allows for 6 presets per virtual amp, which for me is enough. It would be easy to add more banks
 * 
 * To activate the Looper mode, click the control switch 3 times.
 * 
 * MIDI links are set up as follows:
 * 
 * Bank A preset 1-3: Program Change on Channel 2, Program 2-4
 * Bank B preset 1-3 PC Channel 2, program 5-7
 * Pedals: Channel 3, notes = 37 (C#3), 38 (D3), 39 (D#3)
 * LOOPER mode: Channel 3, notes = 40 (E3), 41 (F3), 42 (F#3) - Asssign to Play/Stop, Record/Overdub, Clear last
 * 
 * Firmware version 
 * 0.17 - First feature-complete version with all switches enabled
 * 0.18 - corrected output notes
 * 0.19 - screensaver added since the OLED seems pretty prone to burn-in
 * todo: 
 *  * - screensaver: na 60min zonder op een knop te drukken gaat het scherm op zwart, of een screensaver animatie na 5min
 * - een diode tussen schermvoeding en vcc tbv voltage drop
 * 
*/

///////////////////
// Includes
///////////////////

#include <Adafruit_GrayOLED.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/fontbig.h>  // this custom font contains a small subset of 

// custom/local libraries
#include "MIDIUSB.h"
#include <ResponsiveAnalogRead.h>  // [https://github.com/dxinteractive/ResponsiveAnalogRead](https://github.com/dxinteractive/ResponsiveAnalogRead)

//////////////////
// defines
//////////////////

#define PROGVERSION 0.21
#define i2c_Address 0x3c  // address of the OLED display
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  
#define screensaverTimeout 400000   // 6 minutes

#define LONGPRESS 750   //750ms = control
#define expPin A0   // the input pin for the expression pedal, this connects to tip, sleeve is 5v and base is ground (check)


//////////////////////////
// Global variables
//////////////////////////

// The controller mode:
// 0 = fx control
// 1 = preset bank A
// 2 = preset bank B
// 3 = looper

byte controllerMode = 0;
char currentBank[] = "A"; // actual current bank
char visibleBank[] = "A"; // placeholder for selection
byte currentPreset = 1;
bool controllerWaiting = false;
bool loopMode = false;
bool controllerHush = false;

unsigned long lastButtonPress = 0;

//const int POT_THRESHOLD = 3;   
// for exp pedal
int sensorValue = 0;
int lastVal = 0;
int tempAnalog = 0;



/////////////////////////////////////////////
// BUTTONS
/////////////////////////////////////////////

const int N_BUTTONS = 4;                                //  total numbers of buttons
const int BUTTON_ARDUINO_PIN[N_BUTTONS] = { 8,5,4,6 };  // pins of each button connected straight to the Arduino

int buttonCState[N_BUTTONS] = {};  // stores the button current value
int buttonPState[N_BUTTONS] = {};  // stores the button previous value

//#define pin13 1 // uncomment if you are using pin 13 (pin with led), or comment the line if not using
//byte pin13index = 12;  // put the index of the pin 13 of the buttonPin[] array if you are using, if not, comment

// debounce
unsigned long lastDebounceTime[N_BUTTONS] = { 0 };  // the last time the output pin was toggled
unsigned long debounceDelay = 150;                   // the debounce time; increase if the output flickers



/////////////////////////////////////////////
// EXPRESSION PEDAL
/////////////////////////////////////////////


const int N_POTS = 1;                            // total numbers of pots (slide & rotary)
const int POT_ARDUINO_PIN[N_POTS] = { A0 };  // pins of each pot connected straight to the Arduino

int potCState[N_POTS] = { 0 };  // Current state of the pot
int potPState[N_POTS] = { 0 };  // Previous state of the pot
int potVar = 0;                 // Difference between the current and previous state of the pot

int midiCState[N_POTS] = { 0 };  // Current state of the midi value
int midiPState[N_POTS] = { 0 };  // Previous state of the midi value

const int TIMEOUT = 300;              // Amount of time the potentiometer will be read after it exceeds the varThreshold
const int varThreshold = 2;          // Threshold for the potentiometer signal variation
boolean potMoving = true;             // If the potentiometer is moving
//unsigned long PTime[N_POTS] = { 0 };  // Previously stored time
//unsigned long timer[N_POTS] = { 0 };  // Stores the time that has elapsed since the timer was reset

int reading = 0;
// Responsive Analog Read
float snapMultiplier = 0.01;                      // (0.0 - 1.0) - Increase for faster, but less smooth reading
ResponsiveAnalogRead responsivePot[N_POTS] = {};  // creates an array for the responsive pots. It gets filled in the Setup.

int potMin = 0;
int potMax = 127;
const int POT_THRESHOLD = 3;  



/////////////////////////////////////////////
// MIDI
byte midiCh = 2;  // MIDI channel to be used - start with 1 for MIDI.h lib or 0 for MIDIUSB lib
byte note = 36;   // Lowest note to be used
byte cc = 1;      // Lowest MIDI CC to be used

void updatescreen()
{
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  if (controllerMode == 0)
  { // Mode 0: Show the current patch in big font

    display.setFont(&FreeSans18sub);
    display.setTextSize(1);
    display.setCursor(35, 40);
    display.println(String(currentBank) + "-" + String(currentPreset));
    display.setFont();
    display.setTextSize(2);
  }
  else

      if (controllerMode == 1 || controllerMode == 2)
  { // mode 1-2: wait for preset selection
    if (controllerWaiting)
    {
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(1);
      display.setCursor(1, 17);
      display.println("Bank " + String(visibleBank) + " ");
      display.setFont(&FreeSans12pt7b);

      // display.setTextColor(SH110X_BLACK, SH110X_WHITE); // 'inverted' text
      display.setCursor(1, 56);
      display.println("Select");
    }
  }
  else if (controllerMode == 3)
  { // Mode 3: Looper
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);
    display.setCursor(1, 17);
    display.println("Looper");
    //  display.setFont();
    display.setCursor(1, 56);
    display.println("Ply|Rec|Clr");
  }
  display.invertDisplay(false);
  display.display();
  // controllerWaiting = false;
  delay(50);
}

  ///////////////////////////////////////////
  ///// showpedal
  //    light up the display to visually indicate the switch was made

void showpedalpress(byte pedal, bool looping) {
    display.setFont();
    display.clearDisplay();
    display.setTextSize(3);
    display.setCursor(20, 10);
  
      display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);
  display.setCursor(20,25);
  
    if (looping == false) { display.println("PEDAL"); } else { 
      if (pedal == 1) { display.println(" PLAY"); } else
      if (pedal == 2) { display.print("RECORD"); } else
      if (pedal == 3) { display.print("CLEAR"); } 
    }
    display.setCursor(55, 36);
    display.setCursor(50, 48);    
    if (looping == false) { display.println(String(pedal)); }
    if (not looping) { display.invertDisplay(true); }
    display.display();
    delay(100);
 //   if (looping) { delay(500); }
 }


void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOff);
}

//void controlChange(byte channel, byte control, byte value) {
//  midiEventPacket_t event = { 0x0B, 0xB0 | channel, control, value };
//  MidiUSB.sendMIDI(event);
//}



////////////////////////////////////////////////////////////
// BUTTONS
//
// this handles all the pedal preses and the logic for the different modes. 
///////////////////////////////////////////////////////////

void programChange(byte channel, byte program) {           
  midiEventPacket_t pc = {0x0C, 0xC0 | channel, program, 0};
  MidiUSB.sendMIDI(pc);
}

void buttons()
{
  byte midiProgram = 0;
  bool controllerHush = false;  

  for (int i = 0; i < N_BUTTONS; i++)
  {
    buttonCState[i] = digitalRead(BUTTON_ARDUINO_PIN[i]); // read pins from arduino

  //  if (i == pin13index)
  //  {
  //    buttonCState[i] = !buttonCState[i]; // inverts the pin 13 because it has a pull down resistor instead of a pull up
 //   }

    if ((millis() - lastDebounceTime[i]) > debounceDelay)
    { // its a new event


// voor beide:      if ((i >= 1 && i <= 3) && buttonCState[i] == LOW)
// dit werkt wel maar alleen als de 2e knop eerst wordt ingedrukt, niet andersom.

      if (buttonPState[i] != buttonCState[i])
      { // state has changed
        lastDebounceTime[i] = millis();
        lastButtonPress = millis();

        if (i == 0 && (buttonCState[i] == LOW))   // #0: controller knop is ingedrukt
        {                                     
          controllerWaiting = false;
          controllerMode++;
          if (controllerMode == 4)
          {                   // we're going around
            loopMode = false; // which means exit loop mode
            controllerMode = 0;
            controllerHush = true;
          }
          else if (controllerMode == 3)   // #3: entering loop mode
          {
            loopMode = true;
          } else
          if (controllerMode == 1)        // #1, #2: wait for selection from bank
          {
            strcpy(visibleBank, "A");
            controllerWaiting = true;
          } else
          if (controllerMode == 2)
          {
            strcpy(visibleBank, "B");
            controllerWaiting = true;
          }
          updatescreen();
        }

        if ((i >= 1 && i <= 3) && buttonCState[i] == LOW)
        {                                   // Switch 1 - 3
          if (controllerWaiting == true)
          {                                  // controller was waiting for a preset selection
            strcpy(currentBank, visibleBank);
            currentPreset = i;
            if (currentBank[0] == 'A')
            {
              midiProgram = currentPreset + cc;
            }
            if (currentBank[0] == 'B')
            {
              midiProgram = currentPreset + cc + 3;
            }
        //    Serial.print(String(midiProgram) + "\n");
            programChange(1, midiProgram);
            controllerMode = 0;
            controllerWaiting = false; // mission preset accomplished
            controllerHush = true;     // prevent note off msg
          }
          else if (loopMode == true)
          {
            showpedalpress(i, true);
            noteOn(midiCh, note + i + 3, 127); // channel, note, velocity
            MidiUSB.flush();
            delay(10);
            noteOn(midiCh, note + i + 3, 0); // channel, note, velocity
            MidiUSB.flush();
          // !!!!!  updatescreen();
            // showpedalpress(i+1);
          }
        //  Serial.print("Mode: " + String(controllerMode) + " Bank: " + String(currentBank) + " Preset: " + String(currentPreset) + "\n");
          //   updatescreen();
        }

        // -- Send the MIDI data
        {
          if (buttonCState[i] == LOW)
          {

            // Sends the MIDI note ON accordingly to the chosen board


            if (controllerMode == 0 && controllerHush == false)
            {                                                     // we're controlling pedals

              noteOn(midiCh, note + i, 127); // channel, note, velocity
              MidiUSB.flush();
              showpedalpress(i, false);
              controllerHush == true;
            }

     //       Serial.println("controllerMode=" + String(controllerMode));

          }
          else
          { // pedal up; release note

            // Sends the MIDI note OFF accordingly to the chosen board
            if (controllerMode == 0 && controllerHush == false)
            {                              // we're controlling pedals
              noteOn(midiCh, note + i, 0);} // channel, note, velocity
              MidiUSB.flush();
              updatescreen();

          }
          buttonPState[i] = buttonCState[i];
        }
        //  if (controllerMode < 3) {controllerMode = 0;} // revert to pedal mode when event is processed
        //  updatescreen();
      }
    }
  }
}



/////////////////////////////////////////////
// EXPRESSION PEDAL

void controlChange(byte channel, byte control, byte value) {
   midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void potentiometers() {

  // Reinit Analog Value
  sensorValue = 0;

  // Read analog value
  tempAnalog = analogRead(expPin);
  
  // Convert 10 bit to 7 bit
  tempAnalog = map(tempAnalog, 0, 1023, 0, 127);
  tempAnalog = constrain(tempAnalog, 0, 127);

  // Send pedal status
    //int nValue = analogRead(POT_PIN);
  if(abs(tempAnalog - lastVal) < POT_THRESHOLD)
      return;
//  lastVal = tempAnalog;
      
  if(tempAnalog != lastVal) // probably should require a change of atleast 3
  {
    // CC 11   (Our default value for expression pedal is using 
    //          MIDI control change number 11
    //          Change this to change control the change number)
    controlChange(1, 11, tempAnalog);
    
    // !Important, Flush after send
    MidiUSB.flush();
  }      
  // Store current value to be used laters
  lastVal = tempAnalog;
  delay(5);    // this should probably be enabled
}

/////////////////////////////////////////////
// SETUP
void setup() {
  lastButtonPress = millis();


  // 31250 for MIDI class compliant | 115200 for Hairless MIDI
  Serial.begin(31250);  // we tryna comply ye

  delay(250); // wait for the OLED to power up
  display.begin(i2c_Address, true); // Address 0x3C default
  display.clearDisplay();
  display.display();
  delay(2000);
//  display.dim(true);
  display.clearDisplay(); // cleear buffer
  display.setContrast (0); // dim display 0..255 somehow doesnt get very dim
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);
  display.setCursor(1,17);
  display.setTextColor(SH110X_WHITE);
  display.print("XAD gc1");
  display.setCursor(1,47);  
  display.setFont();
  display.setTextSize(2);
  display.print("FW: v");
  display.println(String(PROGVERSION));  
  
  display.display();

  delay(2000);
 // Serial.println("v"+String(PROGVERSION));
display.setContrast (0); // dim display

  updatescreen();

  // Buttons
  // Initialize buttons with pull up resistors
  for (int i = 0; i < N_BUTTONS; i++) {
    pinMode(BUTTON_ARDUINO_PIN[i], INPUT_PULLUP);
  }

#ifdef pin13  // initialize pin 13 as an input
  pinMode(BUTTON_ARDUINO_PIN[pin13index], INPUT);
#endif

//for (int i = 0; i < N_POTS; i++) {
  responsivePot[0] = ResponsiveAnalogRead(0, true, snapMultiplier);
  responsivePot[0].setAnalogResolution(1023);  // sets the resolution
 //}
}


/////////////////////////////////////////////
// LOOP
void loop() {

  buttons();
  potentiometers();

       if ((millis() - lastButtonPress) > screensaverTimeout) {     // activate screensaver
         display.clearDisplay();
         display.display();
         delay(250);
       }  
}
