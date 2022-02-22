#include <MIDI.h>
#define INPUT_SIZE 10
#define POLYPHONY 10  //10 notes max per chord
#define DEFAULTVOL 100
#define DEFAULTCHAN 1 //default midi channel

//mode 0: standard mode
//mode 1: chord playing mode
//mode 2: chord key assigning mode
//mode 3: chord building mode
//if receiving input that starts with 'P' from 2nd arduino, then send ControlChange signal

bool state = false;
bool prevState = false;
static const int modePins[] = {11,12,13};
static const uint8_t allPins[] = {A1,A2,A3,A4,A5,2,3,4,5,6,7,8,9};  //all pins for keys
bool pressed[13];
bool prevPressed[13];
int chordNotes[13][POLYPHONY];
int octave = 0;
int prevOctave = 0;
int mode = 0; 
int prevMode = 0;
int chordKey;
bool chordDone = false;
unsigned long timeTemp = 0;
bool modePin3Stat = false;

const int c5 = 72;  //MIDI key value for C5 note
int leftKey = 72; //stores MIDI key value of left most key 
const int potPin = A0;

MIDI_CREATE_DEFAULT_INSTANCE();
void setup() {
  pinMode(potPin, INPUT); 

  //reset chords
  for (int i = 0; i < 13; i++) {  
    for (int j = 0; j < POLYPHONY; j++) {
      chordNotes[i][j] = -1;
    }
  }

  //set input pins
  for (int i = 0; i < 13; i++) { //or i <= 4
    pinMode(allPins[i], INPUT);
    digitalWrite(allPins[i], HIGH);
  }

  //set output pins for LEDs, which indicate the current mode
  for (int i = 0; i < 3; i++) {
    pinMode(modePins[i], OUTPUT);
    digitalWrite(modePins[i], LOW);
  }
  digitalWrite(modePins[0], HIGH);

  for (int i = 0; i < 13; i++) {
    prevPressed[i] = false;
  }
  
  MIDI.begin();
  Serial.begin(9600);
  Serial.setTimeout(5);
}

void loop() {
  //check value of potentiometer to determine current octave
  int potVal = analogRead(potPin);
  float octaveVal = (((float) potVal / 1023) * 5) + 0.4;
  int octave = max(-2, round(octaveVal) - 3);

  if (octave != prevOctave) { //if octave has been changed
    prevOctave = octave;
    if (mode == 0) {  //if standard mode, then make sure all notes stop playing when octave changes
      for (int i = 0; i<13; i++) {
        int keyVal = leftKey + i;
        MIDI.sendNoteOff(keyVal, 0, 1);
      }
    }
    leftKey = c5 + (octave * 12); 
  }

  if (Serial.available() > 0) { //if input received from second arduino
    char serialIn[INPUT_SIZE + 1];
    byte size = Serial.readBytes(serialIn, INPUT_SIZE); //read input
    serialIn[size] = 0;

    if (serialIn[0] == 'P') { //'P' flag means this is the photo sensor reading converted into a value for MIDI control change
      char strVal[size - 2];
      strncpy(strVal, serialIn + 2, size - 2);
      int photoVal = atoi(strVal);  //get accompanying reading
      MIDI.sendControlChange(16, photoVal, DEFAULTCHAN);  //send reading to channel 16 (generic MIDI controlchange channel)
    }
    
    if (serialIn[0] == 'C') { //means lower pushbutton (mode button) was pressed for a short time
      if (mode == 0) {  //then switch to chord mode
        mode = 1;
      } else if (mode == 1) { //then switch to standard mode
        mode = 0; 
      } else if (mode == 3) { //then chord has been completed. switch to chord mode
         chordDone = true;
         for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
          MIDI.sendNoteOff(chordNotes[chordKey][chordNoteIndex], 0, DEFAULTCHAN);
         }
         mode = 1; 
         chordDone = false;
      }
    } 
    
    if (serialIn[0] == 'B') { //then lower pushbutton (mode button) has been pressed for 1.5 secs. switch to chord creation mode
      mode = 2;
    }
  }

  //set mode LEDs if prevMode different
  if (prevMode != mode) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(modePins[i], LOW);
    }

    if (mode == 0) {
      digitalWrite(modePins[0], HIGH);
    } else if (mode == 1) {
      digitalWrite(modePins[1], HIGH);
    } else if (mode == 2) {
      digitalWrite(modePins[2], HIGH);
    } else if (mode == 3) { //red LED blinks every sec when in chord building mode
      timeTemp = millis();
    }
    prevMode = mode;
  }

  //get key being pressed input
  for (int i = 0; i < 13; i++) {
    pressed[i] = !digitalRead(allPins[i]);
  }

  if (mode == 0) {  //standard mode
    for (int i = 0; i < 13; i++) {
      int keyVal = leftKey + i;
      state = pressed[i];
      prevState = prevPressed[i];

      //play note when key is pressed. stop playing not when key is released
      if ((state) and (not(prevState))) {
        MIDI.sendNoteOn(keyVal, DEFAULTVOL, DEFAULTCHAN);      
      } else if (not(state) and (prevState))  {
        MIDI.sendNoteOff(keyVal, 0, DEFAULTCHAN);
      }
    
      prevPressed[i] = state;
    }
  } else if (mode == 1) { //chord playing mode
    for (int i = 0; i < 13; i++) {
      state = pressed[i];
      prevState = prevPressed[i];

      //if key is pressed, play each note in the corresponding chord. if key is released, stop all notes in the chord.
      if ((state) and (not(prevState))) {
        for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
          if (chordNotes[i][chordNoteIndex] != -1) {
            MIDI.sendNoteOn(chordNotes[i][chordNoteIndex], DEFAULTVOL, DEFAULTCHAN);   
          }
        }
      } else if (not(state) and (prevState))  {
        for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
          if (chordNotes[i][chordNoteIndex] != -1) {
            MIDI.sendNoteOff(chordNotes[i][chordNoteIndex], 0, DEFAULTCHAN);   
          }
        }
      }
    
      prevPressed[i] = state;
    }
    
  } else if (mode == 2) { //chord key assigning mode
    //choose key to assign chord to
    bool chordKeySelected = false;
    while (not(chordKeySelected)) { //get the key to assign the chord to
      for(int i=0; i<13; i++) {
        pressed[i] = !digitalRead(allPins[i]);
        state = pressed[i];
        if (state) {
          chordKey = i;
          pressed[i] = false;

          //overwrite notes previously in this chord
          for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
            chordNotes[chordKey][chordNoteIndex] = -1;
          }

          chordKeySelected = true;
          mode = 3; //switch to chord building mode
        }
      }
    }
  } else if (mode == 3) {
    //LED blink every 1 sec
    if ((millis() - timeTemp) >= 1000) {
      digitalWrite(modePins[2], not(modePin3Stat));
      modePin3Stat = not(modePin3Stat);
      timeTemp = millis();
    }
    
    //selecting notes for the chord
    if (not(chordDone)) {  
      for (int i = 0; i < 13; i++) {
        int keyVal = leftKey + i;
        state = pressed[i];
        prevState = prevPressed[i];
        
        if ((state) and (not(prevState))) {
          //loop through chord to see if note being played is part of it
          bool noteUsed = false;
          for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
            if (keyVal == chordNotes[chordKey][chordNoteIndex]) { //if note is already used, then release the note from the chord
              noteUsed = true;
              chordNotes[chordKey][chordNoteIndex] = -1;
              MIDI.sendNoteOff(keyVal, 0, DEFAULTCHAN);
              break; 
            }
          }
          if (not(noteUsed)) {  //if note hasn't been used, then add the note to the chord
            bool spaceLeft = false;
            for (int chordNoteIndex = 0; chordNoteIndex < POLYPHONY; chordNoteIndex++) {
              if (chordNotes[chordKey][chordNoteIndex] == -1) {
                spaceLeft = true;
                chordNotes[chordKey][chordNoteIndex] = keyVal;
                break;
              }
            }
            if (not(spaceLeft)) { //if no space left, replace the first note in the chord
              MIDI.sendNoteOff(chordNotes[chordKey][0], 0, DEFAULTCHAN);
              chordNotes[chordKey][0] = keyVal;
            }
            MIDI.sendNoteOn(keyVal, DEFAULTVOL, DEFAULTCHAN); 
          }
        }
        prevPressed[i] = state;
      }
    }
  }
  
  delay(10);
}
