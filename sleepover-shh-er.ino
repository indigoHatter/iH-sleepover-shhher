/* Sleepover "Shh"-er
 * Uses a sound sensor as input for a shift-registered bank of LEDs to show relative level of and proximity to peak volume, and a buzzer when that volume is reached.
 * Additionally, uses an IR remote for adjusting "baseline" volume for where "normal" is, as well as the allowed range, and the brightness of LEDs.
 *
 * Written by Robert "Porter" Blakeley
 * 2023-02-26
 *
 * Uses portions of several pre-written programs from the Elegoo Arduino tutorial, YC's ELT162 class material, and a lot of homemade solutions to link these together.
 * This spanned a few frustrating days of troubleshooting, too. Hah.
*/

// DEFINE included libraries
  //Sound libraries
    //#include "pitches.h" //disabled in favor of manually defining selected pitches, below
    //notes for "Welcome To My House"
      #define NOTE_As3  233.08
      #define NOTE_C3   130.81
      #define NOTE_D3   146.83
      #define NOTE_G2    98.00
      #define NOTE_Ds2   77.78
  //IR remote sensor
    //NOTE: default is timer2, but tone() also uses this timer. See below for fix.
    #include <IRremote.h>
    #include <IRremoteInt.h> //Edit IRremoteInt.h timers for respective Arduino chipset to timer1 for this to work. For Uno, this is caught in that "else" statement, such as for Duemilanove, Diecimila, LilyPad, Mini, Fio, etc
// DEFINE global variables
  byte LEDshift = 0x00; // LED shift register. Start at (0000 0000).
  int LEDbright = 128; // 255-0 (PWM) for LED brightness. 0 = max, 255 = off. Start at 50%.
  int baselineSoundRange = 485; // this should be the baseline sound level... slightly below the average sound level so it can handle quietness, too
  int SoundRange = 35; // how much difference in sound to tolerate?
  int upperSoundRange = baselineSoundRange+SoundRange; // trigger sound level
// DEFINE constants for configuring program
  // none in use - all are now set as global variables, save for this override trick no longer in use
  //int overrideValue = -1; // for troubleshooting. -1 disables, 0-7 will correlate to shift reg values, all else = shift reg error
// DEFINE initialize namedPins
  // use constexpr ("constant expression") to declare constant 'variables' that do not use memory during runtime by acting as substitution during compile, while still maintaining type safety
  // https://learn.microsoft.com/en-us/cpp/cpp/constexpr-cpp
  constexpr uint8_t      dataPin =  7; //    DS  [S1] on 74HC595 (pin 14), out, |green|
  constexpr uint8_t     latchPin =  6; // ST_CP [RCK] on 74HC595 (pin 12), out, |~blue|
  constexpr uint8_t     clockPin =  5; // SH_CP [SCK] on 74HC595 (pin 11), out, |~purple|
  constexpr uint8_t        OEPin =  9; //    OE [???] on 74HC595 (pin 13), out, |~white|, 0-255, active low. Here will control LED brightness.
  constexpr uint8_t      buzzPin =  8; // (+) on passive buzzer, out, |blue|
  constexpr uint8_t     soundPin = A0; // A0 on sound detection module, |@grey|, in (0-1023)
  constexpr uint8_t IRreceivePin =  3; // Y on IR receiver module, |~white|
// DEFINE declared objects
  IRrecv IR(IRreceivePin); // create instance of 'IR' associated with IRreceivePin
  decode_results results;  // create instance of 'results'

void setup() {
  // initialize shift register/LEDs
    pinMode(dataPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    updateShiftRegister(LEDshift);
    pinMode(OEPin, OUTPUT);
    analogWrite(OEPin, LEDbright);
  // initialize buzzer
    pinMode(buzzPin, OUTPUT);
  // initialize sound detection module
    //pinMode(soundPin, INPUT); //unnecessary to set but defining this anyway
    //pinMode(IRreceivePin, INPUT); //unnecessary to set but defining this anyway
  // start the receiver
    IR.enableIRIn();
  // initialize serial monitor/plotter
    Serial.begin(19200);
    Serial.println("...initializing...");
}

/****************************************************************************************/
// main body of program
void loop() {
  if (IR.decode(&results)) {doRemote();} // if remote button is pressed, do the remote stuff
  int soundLevel = analogRead(soundPin); // analog values (0-1023) representing loudness, but will likely only have a range of ~20, with median adjusted by pot on sensor
    Serial.print("Sound "); Serial.print(soundLevel); Serial.print(" ");
    serialMon();    
  byte LEDbyte = convertLeveltoLEDs(soundLevel);
    //for troubleshooting//Serial.print("LED "); Serial.print(LEDbyte); //Serial.print(" ");
  updateShiftRegister(LEDbyte);
  if (LEDbyte == 0xFF) {playSong();}
} // end main loop()


/****************************************************************************************/
/****************************************************************************************/
/*  Reference: inputs from remote
    ! = defined in current program
    -----------------------------
    ! Button held  4294967295  0xFFFFFFFF
      Power          16753245  0x00FFA25D
      EQ             16750695  0x00FF9867
    ! Vol+           16736925  0x00FF629D
    ! Vol-           16754775  0x00FFA857
      Play/Pause     16712445  0x00FF02FD
      Func/Stop      16769565  0x00FFE21D
    ! Rewind         16720605  0x00FF22DD
    ! FF             16761405  0x00FFC23D
      ST/REPT        16756815  0x00FFB04F
    ! Down           16769055  0x00FFE01F
    ! Up             16748655  0x00FF906F
      0              16738455  0x00FF6897
      1              16724175  0x00FF30CF
      2              16718055  0x00FF18E7
      3              16743045  0x00FF7A85
      4              16716015  0x00FF10EF
      5              16726215  0x00FF38C7
      6              16734885  0x00FF5AA5
      7              16728765  0x00FF42BD
      8              16730805  0x00FF4AB5
      9              16732845  0x00FF52AD
*/
/****************************************************************************************/

// do remote control stuff - change values, etc
void doRemote() {
  unsigned long decodeValue = results.value;
  unsigned long   prevValue;
  unsigned long passedValue;
  if (decodeValue == 0xFFFFFFFF) {passedValue = prevValue;} // if button is being held, repeat the previous commands
    else {passedValue = decodeValue; prevValue = decodeValue;} // otherwise, do what the new command is, and remember that new input for above check
  switch(passedValue) {
    case 0x00FFE01F: //down    // LED brightness down
      //Serial.print("LED brightness from "); Serial.print(LEDbright);
      LEDbright+=8;
      if (LEDbright>=255) {LEDbright=255; Serial.println("LED brightness at minimum value (255).");}
       else {Serial.print("LED brightness changed to "); Serial.println(LEDbright);}
      analogWrite(OEPin, LEDbright);
      break;
    case 0x00FF906F: //up      // LED brightness up
      //Serial.print("LED brightness from "); Serial.print(LEDbright);
      LEDbright-=8;
      if (LEDbright<=0) {LEDbright=0; Serial.println("LED brightness at maximum value (0).");}
       else {Serial.print("LED brightness changed to "); Serial.println(LEDbright);}
      analogWrite(OEPin, LEDbright);
      break;
    case 0x00FFA857: //vol-    // decrease baseline volume
      baselineSoundRange--;
      if (baselineSoundRange<=128) {
        baselineSoundRange=128;
        Serial.println("Baseline sound range at minimum value (128).");
       }
       else {Serial.print("Baseline sound range decreased to "); Serial.println(baselineSoundRange);}
      upperSoundRange = baselineSoundRange+SoundRange;
      break;
    case 0x00FF629D: //vol+    // increase baseline volume
      baselineSoundRange++;
      if (baselineSoundRange>=(1023-SoundRange)) {
        baselineSoundRange=(1023-SoundRange);
        Serial.println("Baseline sound range at maximum value (1023-SoundRange).");
       }
       else {Serial.print("Baseline sound range increased to "); Serial.println(baselineSoundRange);}
      upperSoundRange = baselineSoundRange+SoundRange;
      break;
    case 0x00FF22DD: //rewind  // decrease SoundRange
      SoundRange--;
      if (SoundRange<=5) {SoundRange=1; Serial.println("Sound range at minimum value (5).");}
       else {Serial.print("Sound range decreased to "); Serial.println(SoundRange);}
      upperSoundRange = baselineSoundRange+SoundRange;
      break;
    case 0x00FFC23D: //ff      // increase SoundRange
      if (upperSoundRange>=1023) {Serial.println("Sound range at maximum value (pushes upperSoundRange to 1023).");}
       else {SoundRange++; Serial.print("Sound range increased to "); Serial.println(SoundRange);}
      upperSoundRange = baselineSoundRange+SoundRange;
      break;
  }
  //Serial monitor for troubleshooting
    //Serial.print("Raw value of IR input: "); Serial.println(results.value);
    //Serial.print("Passed value: "); Serial.println(passedValue);
    //delay(500);
    //Serial.println("...");
  IR.resume(); // receive the next value
}

// takes input of 0-1024, but expects a range of lower-to-upperSoundRange value
byte convertLeveltoLEDs(int inputValue) {
  // setup function
    byte outputValue; // return value
  // begin function
  int mappedValue = map(inputValue, baselineSoundRange, upperSoundRange, 0, 7);
    //for troubleshooting//Serial.print("Mapped LEDbyte "); Serial.print(mappedValue); Serial.print(" "); // for troubleshooting
  //for troubleshooting//mappedValue = 0; // for troubleshooting
  switch (mappedValue) {
    case  0: outputValue = 0x01; break; // 0000 0001
    case  1: outputValue = 0x03; break; // 0000 0011
    case  2: outputValue = 0x07; break; // 0000 0111
    case  3: outputValue = 0x0F; break; // 0000 1111
    case  4: outputValue = 0x1F; break; // 0001 1111
    case  5: outputValue = 0x3F; break; // 0011 1111
    case  6: outputValue = 0x7F; break; // 0111 1111
    case  7: outputValue = 0xFF; break; // 1111 1111
    /*default: // used for troubleshooting
      outputValue = 0xAA; // 1010 1010 - indicates unexpected error, such as outOfRange
      int rangeMiss;
      if (inputValue > upperSoundRange) {rangeMiss = inputValue-upperSoundRange;}
       else if (inputValue < baselineSoundRange) {rangeMiss = inputValue-baselineSoundRange;}
       else {rangeMiss = 0;}
      Serial.print("!!Out of range by ");
        if (rangeMiss == 0) {Serial.print("UNKNOWN ERROR!");}
        else {Serial.print(rangeMiss);}
        Serial.print("!! ");
      break;
    */
  }
  return outputValue; // byte
}

// update shift register, q0 = 0000 000x, etc
void updateShiftRegister(byte inputValue) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, inputValue);
  digitalWrite(latchPin, HIGH);
}

// make buzzer play a melody (currently configured for "Welcome To My House")
void playSong() {
  //FUTURE IDEA: could create several melodies and use a random # to pick one of them, or let the remote override the random #.
    // Implementation Thought: Use switch/case to define each melody/duration/breathe per song, so that song-player function doesn't need to accomodate multiple variables.
      // Example:
        // char chosenMelody;
        // if (userChosenMelody != 0) {chosenMelody = userChosenMelody;} else {chosenMelody = rand(1,3);} // user can choose 0 to reset to random
        // switch (chosenMelody) {case 1: melody/duration/breathe for Welcome; case 2: melody/duration/breathe for another song, etc;}
  // melody for "Welcome To My House"
    int      length = 7;
    double melody[] = {NOTE_As3,  NOTE_C3,  NOTE_D3,  NOTE_G2,    0,  NOTE_Ds2,  NOTE_As3};
    int  duration[] = {     250,      250,      250,     1250,    0,       750,      1250};
    int   breathe[] = {     100,      100,      100,      100,  250,       250,       500};

  Serial.print("\nPlaying sound...\n");
  
  for (int a = 0; a < length; a++) { // use this line to play the full sound clip
  //for (int a = 0; a < 1; a++) { // use this line to play a single note
    tone(buzzPin, melody[a], duration[a]);
    delay(duration[a]);
    noTone(buzzPin);
    delay(breathe[a]);
  }
  noTone(buzzPin);
  delay(5000); // wait 5 seconds after the sound plays -- maybe this should be adjustable but I don't care right now. Whoo!
    analogRead(soundPin); // pull the sound value and do nothing with it, to help clear the buffer and move on from the loud event so we aren't forced into an infinite loop of sound
  noTone(buzzPin);
}

//outputs range values to the serial monitor
//Note: if using Serial Plotter, expect value2, value4, and value6 to show your range.
void serialMon() {
  Serial.print("Range "); Serial.print(baselineSoundRange); Serial.print(" ");
  Serial.print("to "); Serial.print(upperSoundRange); Serial.print(". ");
  Serial.print("\n");
  //Troubleshooting Serial Monitor stuff (though some is left elsewhere in the exact function being worked on)
    //Serial.print("--------------------"); Serial.print("\n");
    //delay(1000);
}

/****************************************************************************************/
/*** Leftover references & notes ***/
 // None left! All deleted. :)


// Goodbye! :)