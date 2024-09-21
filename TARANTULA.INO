// Defining pin
#define Pinky_Pin     A0
#define Ring_Pin      A1
#define MIddle_Pin    A2
#define Index_Pin     A3
#define Thumb_Pin     A4

// Interface Class for Communication
class communication {
  public:
    virtual bool isOpen() = 0;
    virtual void start() = 0;
    virtual void output(char* data) = 0;
};

// USB Communication
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
};

// Global Communication Pointer
communication* comm;

// Setup Function
void setup() {
  comm = new USBComm(); 
  comm->start();
}

// Loop Function
void loop() {
  if (comm->isOpen()){
    int fingerPos[5] = {     // Read Finger Positions
      analogRead(Thumb_Pin), 
      analogRead(Index_Pin), 
      analogRead(MIddle_Pin), 
      analogRead(Ring_Pin), 
      analogRead(Pinky_Pin)
    };

    // data output
    comm->output(encode(fingerPos));

    delay(4);
  }
}

// Data Encoder
char* encode(int* finger){
  static char Encoded[75];
  sprintf(Encoded, "A%dB%dC%dD%dE%dF%dG%dP%d%s%s%s%s%s%s%s%s\n", 
    finger[0], finger[1], finger[2], finger[3], finger[4],
    "", "", "","",
    "", "", "", "", "", "", ""
  );
  return Encoded;
}
