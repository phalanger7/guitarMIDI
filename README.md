# guitarMIDI - MIDI controller for guitarists
Guitar MIDI controller by Alexander Adema


This software controls a MIDI interface designed to control guitar software such as ampsims (change preset, control individual effects; looper)
It is designed for an Arduino Micro Pro controller, It can be run as-is on some other Arduino's but not all, dyor. 
The controller this software is for contains 5 momentary switches, an input for an expression pedal and an OLED status screen. I used a 1.3" 128x64 monochrome OLED display with 1106 chip, using the Adafruit library. 
If you use a different display you may need to change the graphics library.
It is meant to be used with software guitar amp-sims. I use it with NeuralDSP plugins under REAPER DAW, but should work with any software that can use MIDI program change and note data for its plugins. It uses MIDI over USB.

 Hardware used:
 
 - Arduino micro Pro clone with usb-c
 - 4 momentary footswitches (adding more would be easy)
 - a 'stereo' 1/4in jack socket for expression pedal, i used an M-Audio EX=P but any brand should work
 - a 1.3in monochrome OLED with 128x64 pixels (1106 type)
 
 Functionality:
 - 1 Control button that switches between 4 modes: Pedal FX mode (default), select preset from Bank A, select preset from Bank B, Looper
 - 3 buttons which function depends on mode: pedal control (mode 0), pick a preset from Bank A (mode 1), pick from Bank B (mode 2), REC/DUB + PLAY/STOP + CLEAR last
 - EXP pedal (2x?)
 - an OLED display that shows the current mode and preset, and flashes up when a pedal is pressed, as feedback that this switch is activated
 
 I have kept the control mechanism as simple as possible, to facilitate ease of use over maximizing options. For that reason there are at the moment no double-tap functions or for pressing two 
 buttons at once, etc. 
 I have also considered adding a small OLED below each button as a dynamic label. But since there is no universal way to get the current preset, pedal status etc from plugins, their
 functionality would be limited. For this reason there are also no status LEDs for the pedals since we don't know whether it's switching the pedal on or off.
 
 The control logic / usage is as follows:
 
 At startup and after every preset change, the pedal mode is activated - pedals 2, 3 and 4 can be used to directly switch virtual pedals in the ampsim.
 To select a preset, press the control button once (bank A) or twice (bank B) and then click one of the 3 other switches to select the desired preset from the bank.
 THis allows for 6 presets per virtual amp, which for me is enough. It would be easy to add more banks
 
 To activate the Looper mode, click the control switch 3 times.
 
 MIDI links are set up as follows:
 
 Bank A preset 1-3: Program Change on Channel 2, Program 2-4
 Bank B preset 1-3 PC Channel 2, program 5-7
 Pedals: Channel 3, notes = 36, 37, 38
 LOOPER mode: Channel 3, notes = 40, 41, 42
 
 Firmware version 0.17 - First feature-complete version with all switches enabled
 
 todo: 
 - screensaver: na 60min zonder op een knop te drukken gaat het scherm op zwart, of een screensaver animatie na 5min
 - een diode tussen schermvoeding en vcc tbv voltage drop
 - maybe a way to keep all 4 controllers available for FX and presets, this would mean the controller function would need to entered with a double-tap, long-press or other way.
   It would also have to deal with not triggering the FX pedal when you just want to go into the menu. For now I like the simplicity of a dedicated control button
