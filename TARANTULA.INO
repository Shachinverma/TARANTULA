// Defining pin

#define Pinky_Pin     A0
#define Ring_Pin      A1
#define Middle_Pin    A2
#define Index_Pin     A3
#define Thumb_Pin     A4
#define PIN_CALIB     13 
#define DEBUG_LED     LED_BUILTIN
#define PIN_MENU_BTN        8

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Defining Functions

#define ANALOG_MAX 1023
#define CALIBRATION_LOOPS -1
#define ALWAYS_CALIBRATING CALIBRATION_LOOPS == -1
#define CALIB_OVERRIDE false
#if USING_CALIB_PIN && COMMUNICATION == COMM_SERIAL && PIN_CALIB == 0 && !CALIB_OVERRIDE
  #error "You can't set your calibration pin to 0 over usb. You can calibrate with the BOOT button when using bluetooth only. Set CalibOverride to true to override this."
#endif
int prevPotValues[5] = {0, 0, 0, 0, 0};
const int eleMagPins[5] = {2, 3, 4, 5, 6};    // Electromagnets on D2-D6


int loops = 0;
int* fingerPos;

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// communicaiton

class communication {
  public:
    virtual bool isOpen() = 0;
    virtual void start() = 0;
    virtual void output(char* data) = 0;
    virtual bool readData(char* input) = 0;
};

class USBComm : public communication {
  private:
    bool m_isOpen;

  public:
    USBComm() {
      m_isOpen = false;
    }

    bool isOpen(){
      return m_isOpen;
    }

    void start(){
      Serial.begin(115200);
      m_isOpen = true;
    }

    void output(char* data){
      Serial.print(data);
      Serial.flush();
    }

    bool readData(char* input){
      byte size = Serial.readBytesUntil('\n', input, 100);
      input[size] = NULL;
      return input != NULL && strlen(input) > 0;
    }
};

communication* comm;

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Setup Function

void setup() {
  comm = new USBComm(); 
  comm->start();
  setupInputs();
  setupHaptics();  
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Loop Function

void loop() {
  if (comm->isOpen()){
    #if USING_CALIB_PIN
    bool calibButton = getButton(PIN_CALIB) != INVERT_CALIB;
    if (calibButton)
      loops = 0;
    #else
    bool calibButton = false;
    #endif
    
    bool calibrate = false;
    if (loops < CALIBRATION_LOOPS || ALWAYS_CALIBRATING){
      calibrate = true;
      loops++;
    }
    
    fingerPos = getFingerPositions(calibrate, calibButton);
    bool triggerButton = triggerGesture(fingerPos);
    bool grabButton = grabGesture(fingerPos);
    bool pinchButton = pinchGesture(fingerPos);
    bool menuButton = getButton(PIN_MENU_BTN);


    // data output
    comm->output(encode(fingerPos,triggerButton, grabButton, pinchButton, calibButton, menuButton));
    
    // Feedback
    char received[100];
    if (comm->readData(received)){
      int hapticLimits[5];
      if(String(received).length() >= 10) {
         decodeData(received, hapticLimits);
         writeHaptics(hapticLimits); 
      }
     }
    

    delay(5);
  }
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Data Encoder

char* encode(int* flexion, bool triggerButton, bool grab, bool pinch, bool calib, bool menu){
  static char stringToEncode[75];
  int trigger = (flexion[1] > ANALOG_MAX/2) ? (flexion[1] - ANALOG_MAX/2) * 2:0;
  sprintf(stringToEncode, "A%dB%dC%dD%dE%dP%d%s%s%s%s%s\n", 
  flexion[0], flexion[1], flexion[2], flexion[3], flexion[4],
   trigger,triggerButton?"I":"", grab?"L":"", pinch?"M":"", menu?"N":"", calib?"O":""
  );
  return stringToEncode;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Data Decoder

void decodeData(char* stringToDecode, int* hapticLimits){
  hapticLimits[0] = getArgument(stringToDecode, 'A'); //thumb
  hapticLimits[1] = getArgument(stringToDecode, 'B'); //index
  hapticLimits[2] = getArgument(stringToDecode, 'C'); //middle
  hapticLimits[3] = getArgument(stringToDecode, 'D'); //ring
  hapticLimits[4] = getArgument(stringToDecode, 'E'); //pinky
}

int getArgument(char* stringToDecode, char command){
  char* start = strchr(stringToDecode, command);
  if (start == NULL)
    return -1;
  else
    return atoi(start + 1);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------

#if ENABLE_MEDIAN_FILTER
  #include <RunningMedian.h>
  RunningMedian rmSamples[5] = {
      RunningMedian(MEDIAN_SAMPLES),
      RunningMedian(MEDIAN_SAMPLES),
      RunningMedian(MEDIAN_SAMPLES),
      RunningMedian(MEDIAN_SAMPLES),
      RunningMedian(MEDIAN_SAMPLES)
  };
#endif

int maxFingers[5] = {0,0,0,0,0};
int minFingers[5] = {ANALOG_MAX, ANALOG_MAX, ANALOG_MAX, ANALOG_MAX, ANALOG_MAX};

void setupInputs(){

  pinMode(PIN_MENU_BTN, INPUT_PULLUP);
  pinMode(PIN_CALIB, INPUT_PULLUP);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Finger position

int* getFingerPositions(bool calibrating, bool reset){
  int rawFingers[5] = {     // Read Finger Positions
      analogRead(Thumb_Pin), 
      analogRead(Index_Pin), 
      analogRead(Middle_Pin), 
      analogRead(Ring_Pin), 
      analogRead(Pinky_Pin)
    };


  #if ENABLE_MEDIAN_FILTER
  for (int i = 0; i < 5; i++){
    rmSamples[i].add( rawFingers[i] );
    rawFingers[i] = rmSamples[i].getMedian();
  }
  #endif

  //reset max and mins as needed
  if (reset){
    for (int i = 0; i <5; i++){
      maxFingers[i] = 0;
      minFingers[i] = ANALOG_MAX;
    }
  }
  
  if (calibrating){
    for (int i = 0; i <5; i++){
      if (rawFingers[i] > maxFingers[i])
        #if CLAMP_FLEXION
          maxFingers[i] = ( rawFingers[i] <= CLAMP_MAX )? rawFingers[i] : CLAMP_MAX;
        #else
          maxFingers[i] = rawFingers[i];
        #endif
      if (rawFingers[i] < minFingers[i])
        #if CLAMP_FLEXION
          minFingers[i] = ( rawFingers[i] >= CLAMP_MIN )? rawFingers[i] : CLAMP_MIN;
        #else
          minFingers[i] = rawFingers[i];
        #endif
    }
  }

  static int calibrated[5] = {511,511,511,511,511};
  
  for (int i = 0; i<5; i++){
    if (minFingers[i] != maxFingers[i]){
      calibrated[i] = map( rawFingers[i], minFingers[i], maxFingers[i], 0, ANALOG_MAX );
      #if CLAMP_ANALOG_MAP
        if (calibrated[i] < 0)
          calibrated[i] = 0;
        if (calibrated[i] > ANALOG_MAX)
          calibrated[i] = ANALOG_MAX;
      #endif
    }
    else {
      calibrated[i] = ANALOG_MAX / 2;
    }
  }
  return calibrated;
  
}

bool getButton(byte pin){
  return digitalRead(pin) != HIGH;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Gestures

bool grabGesture(int *flexion){
  return (flexion[Pinky_Pin] + flexion[Ring_Pin] + flexion[Middle_Pin] + flexion[Index_Pin]) / 4 <= ANALOG_MAX/2 ? 0:1;
}

bool pinchGesture(int *flexion){
  return (flexion[Index_Pin] + flexion[Thumb_Pin]) / 2 <= ANALOG_MAX/2 ? 0:1;
}

bool triggerGesture(int *flexion){
  return flexion[Index_Pin]<=(ANALOG_MAX/2)?0:1;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
// Feedback


void setupHaptics(){
  const int eleMagPins[5] = {2, 3, 4, 5, 6};
  for (int i = 0; i < 5; i++) {
    pinMode(eleMagPins[i], OUTPUT);
    digitalWrite(eleMagPins[i], LOW);
  }
}


void writeHaptics(int* hapticLimits){
  int tolerance = 50;  // Tolerance for matching the limit
  for (int i = 0; i < 5; i++) {
    int potValue = fingerPos[i];

    if (potValue >= hapticLimits[i] - tolerance && potValue <= hapticLimits[i] + tolerance) {
        digitalWrite(eleMagPins[i], HIGH);  // Turn on electromagnet
        delay(100);  // Keep it on for 0.1 second
        digitalWrite(eleMagPins[i], LOW);   // Turn off electromagnet    }
    }
  }
}



