const uint8_t photoPin = A0;
bool photoState = false;
bool prevPhotoState = false;
bool photoActiv = false;
bool button2State = false;
bool prevBut2State = false;
bool timedPress = false;
bool bsent = false;
unsigned long timerStart = 0;
unsigned long timerEnd = 0;

String msg;

void setup() {
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  pinMode(5, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  if ((digitalRead(3)) == HIGH) { //read state of upper pushbutton (for toggling photo resistor)
    photoState = true;
  } else {
    photoState = false;
  }
  
  if ((digitalRead(4)) == HIGH) { //read state of lower pushbutton (for changing modes)
    button2State = true;
  } else {
    button2State = false;
    bsent = false;
  }
  
  if ((photoState) and (not(prevPhotoState))) { 
    if (not(photoActiv)) {  //start activating the photo resistor
      Serial.println("O");
      photoActiv = true;
      digitalWrite(5, HIGH);    
    } else {  //deactivate photoresistor
      Serial.println("F");
      photoActiv = false;   
      digitalWrite(5, LOW);    
    }
  }
  prevPhotoState = photoState;

  //if user presses lower pushbutton for less than 1.5 secs, send a 'C' to serial (represents changing between standard/chord playing mode, or finish chord building)
  //if user presses lower pusshbutton for more than 1.5 secs, send a 'B' to serial (represents changing to chord key assigning/building modes)
  if ((button2State) and (not(prevBut2State))) {
    timerStart = millis();
  } else if (not (button2State) and (prevBut2State)) {
    timerEnd = millis();
    if ((timerEnd - timerStart) < 1500) {
      Serial.println("C");
    }
  } else if ((button2State) and ((millis() - timerStart) >= 1500) and (not(bsent))) {
    Serial.println("B");
    bsent = true;
  }
  
  prevBut2State = button2State;

  if (photoActiv) { //then send photoresistor reading, mapped to 127 for accordance with MIDI ControlChange
    int photoVal = analogRead(photoPin);
    int photoValMidi = map(photoVal, 0, 1023, 0, 127);
    msg = String("P:" + String(photoValMidi));
    Serial.println(msg);
  }

  delay(10);

}
