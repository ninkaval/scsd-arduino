#include <SoftwareSerial.h> 
#include <SD.h>
#include "Wire.h"

boolean MODE_SD_CARD = false; 

//_______ Protocol to communicate with RFID reader (0x25 is command for reading card ID)
char query[8] = {
  0xAA, 0x00, 0x03, 0x25, 0x26, 0x00, 0x00, 0xBB};

char green[8] = {
  0xAA, 0x00, 0x03, 0x87, 0x18, 0x01, 0x9D, 0xBB};

char red[8] = {
  0xAA, 0x00, 0x03, 0x88, 0x18, 0x01, 0x92, 0xBB};

#define CMD_GET_SNR    0x25
#define CMD_CTRL_LED1  0x87
#define CMD_CTRL_LED2  0x88
#define CMD_ZERO       0x88 //experimental from us

#define NOT_READ     -1
#define READ         0
#define READ_CORRECT 1
#define READ_WRONG   2

int rfidReadResult = NOT_READ; 

//_______ small button
#define rxPin_0 3
#define txPin_0 2

//_______ medium button
#define rxPin_1 5
#define txPin_1 4

//_______ large button
#define rxPin_2 7
#define txPin_2 6

#define commandStart 3 //pos nr. 3 corresponds protocol command  ˚
#define tagStart           5 
#define tagEnd             10

#define rfidLength_SaoPaulo  11 // # bytes in one rfid packet 
#define tagLength_SaoPaulo   5  // # bytes for the tag (id) in Sao Paulo (different in London!) 

SoftwareSerial rfid_0( rxPin_0, txPin_0 ); //small button
SoftwareSerial rfid_1( rxPin_1, txPin_1 ); //medium button
SoftwareSerial rfid_2( rxPin_2, txPin_2 ); //large button

#define baudRateSerial     9600
#define baudRateRfid       9600

//---------declare rotary switch pins
//const int firstRotaryPin   = 9;
//const int lastRotaryPin   = 13;

int activeRotaryPos       = -1; 
int rotaryPins[]          = { 
  9,10,11,12,13 };

//---------declare heart button
int heartBtn_LEDPin   = 14; // pin for the Heart LED (resistor in use is 2.2kOhm) ((pin 14 is actually A0 on the Arduino Board))
int heartBtn_InputPin   = 8;   // input pin (for a pushbutton)
int heartBtn_Value       = 0;     // variable for reading the pin status
int activeBtnState  = 0; 

//---------working variables 
byte val;
byte vals[rfidLength_SaoPaulo]; 
char tagString[512];  
int i;
int byteCounter; 
boolean tagReceived   = false; 

//---------define constants for writing data from different actions (different hw components) to the serial port 
#define CODE_KNOB  0
#define CODE_BTN   1
#define CODE_RFID  2
#define CODE_DELIMITER ':'


int rfidMsgLength  = tagLength_SaoPaulo + 2; 
char inBufferRFIDTag[7];  

char inBufferRotary[256];
char inBufferBtn[256];
char inBufferRFID[256];  //contains the whole string to save to SD card 

//---------SD card 
#define DS1307_ADDRESS    0x68
#define SDCardPin         8

byte   zero        = 0x00; //workaround for issue #527
//File   myFile;
int    counter     = 0; 

void setup()
{
  //---------init rotary switch 
  for (int i = 0; i< 5; i++){
    pinMode(rotaryPins[i], INPUT);
    digitalWrite(rotaryPins[i], HIGH); // turn on internal pullup resistor
  }

  //_________init heart button (not active for devices w. SD card module)  
  if (!MODE_SD_CARD) {
    pinMode(heartBtn_LEDPin, OUTPUT);  // declare LED as output
    pinMode(heartBtn_InputPin, INPUT);    // declare pushbutton as input
  }

  Serial.begin(baudRateSerial);

  //setupSDCard();
}


void loop()
{

  rfidReadResult = NOT_READ; 

  //--------------------------------------------------------------------------------------------------  
  //Read and send state of the rotary knob  
  int rotaryPos = getRotaryValue();
  if (rotaryPos != -1 && activeRotaryPos != rotaryPos) {
    Serial.print(CODE_KNOB);
    Serial.print(CODE_DELIMITER);
    Serial.print(rotaryPos, DEC);
    Serial.println();//important!!! write new line to delimit the end of this serial message

    activeRotaryPos = rotaryPos; //save new active pos

      //build the data string and write it to SD card
    //sprintf(inBufferRotary, "%d:%d", CODE_KNOB, rotaryPos);
    //writeSDcard(inBufferRotary);
  } 
  else {
    //Serial.print("Pos:");
    //Serial.println(activeRotaryPos);
  }

    if (!MODE_SD_CARD){
     
    //--------------------------------------------------------------------------------------------------  
    //Read and send state of the heart button 
    heartBtn_Value = digitalRead(heartBtn_InputPin);  // read input value
    if (heartBtn_Value == HIGH) {         // check if the input is HIGH (button released)
      digitalWrite(heartBtn_LEDPin, LOW);  // turn LED OFF 
    } 
    else {
      digitalWrite(heartBtn_LEDPin, HIGH);  // turn LED ON
    }
    if (activeBtnState != heartBtn_Value) {
      Serial.print(CODE_BTN);
      Serial.print(CODE_DELIMITER);
      Serial.print(heartBtn_Value, DEC);
      Serial.println();//important!!! write new line to delimit the end of this serial message
      activeBtnState = heartBtn_Value; 

      //build the data string and write it to SD card
      //sprintf(inBufferBtn, "%d:%d", CODE_BTN, activeBtnState);
      //writeSDcard(inBufferBtn);
    }
  }

  //--------------------------------------------------------------------------------------------------  
  //Read and send data from the rfid readers 
  //  rfidRead(rfid_0, 0);
  //  rfidRead(rfid_1, 1);  
  //  rfidRead(rfid_2, 2);  
  //  

  //boolean wellFormed; 
  //read RFID 0 
  rfidReadResult = rfidReadWrite(rfid_0, 1, inBufferRFIDTag);
  if (rfidReadResult == READ_CORRECT){
    setLEDGreen(rfid_0);
  } 
  else if (rfidReadResult == READ_WRONG){
    //Serial.println("read wrong length rfid");
    //setLEDRed(rfid_0);
  } 
  else{

  } 

  //read RFID 1 
  rfidReadResult = rfidReadWrite(rfid_1, 2, inBufferRFIDTag);  
  if (rfidReadResult == READ_CORRECT){
    setLEDGreen(rfid_1);
  }

  //read RFID 2 
  rfidReadResult = rfidReadWrite(rfid_2, 3, inBufferRFIDTag);  
  if (rfidReadResult == READ_CORRECT){
    setLEDGreen(rfid_2);
  }


}


/** Reads data from the specified rfid reader and writes the contents of the card id into the serial port (and specified buffer, just in case) 
 * TODO: make this func only read from rfid to the buffer, and write to serial from another one 
 * CURRENTLY: writing a whole byte/char array to the serial port breaks it into new lines, and we want one solid chunck, thats why we write to the serial port from here byte per byte */
int rfidReadWrite(SoftwareSerial & _rfid, int _sentimentID, char* strIDTag){
  int result = NOT_READ;  
  int j=0; 

  byteCounter = 0; 
  _rfid.begin(baudRateRfid);

  //init rfid 
  for (i=0 ; i<8 ; i++){
    _rfid.print(query[i]) ;
  }
  delay(15);//small delay so rfid reader is able to initialize (dont make it too large to avoid delays in reading the serial port later on 

  //read rfid serial contents 
  while(_rfid.available()>0){
    val=_rfid.read();
    vals[byteCounter] = val; //save the currently read byte into our array 
    byteCounter++;
  }

  //  if (byteCounter == 7) { 
  //    result = READ; 
  //  } //read has read something (at least) 

  // check if we have read at least the number of bytes from SP  
  if (byteCounter >= rfidLength_SaoPaulo){ 

    
    if (!MODE_SD_CARD){
      Serial.print(CODE_RFID);
      Serial.print(CODE_DELIMITER);
      Serial.print(_sentimentID);
      Serial.print(CODE_DELIMITER); 
    }
    else {
      j+= sprintf(strIDTag+j, "%d", 2);
      j+= sprintf(strIDTag+j, "%s", "-");
      j+= sprintf(strIDTag+j, "%d", _sentimentID);
      j+= sprintf(strIDTag+j, "%s", "-");
    }
    //    
    for (int i=0; i< byteCounter; i++){
      if (i >= tagStart && i < tagEnd){ 
        j += sprintf(strIDTag+j, "%d", vals[i]);
        //Serial.print(vals[i], DEC);         
      }
    }
    Serial.print(strIDTag);
    Serial.println(); //important!!! write new line to delimit the end of this serial message


    if (MODE_SD_CARD) {
      //----------------------------------    
      File myFile = SD.open("test1.txt",FILE_WRITE);
      //----------------------------------if the file opened okay, write to it:
      if (myFile) {
        myFile.print(strIDTag);
        myFile.print("-");
        writeDate(myFile);
        myFile.close();
        Serial.println("done.");
      } 
      else {
        Serial.println("Error opening file");
      }  
    }


    result = READ_CORRECT; 

  }

  else {
    result = READ_WRONG; 
    //    Serial.print("swipe count: ");
    //    Serial.print(byteCounter);
    //    Serial.println(); 

  }


  _rfid.end();
  return result; 
}


//---------------------------------------------------------------------------------------------
void setLEDGreen (SoftwareSerial & _setLEDGreen){

  _setLEDGreen.begin(9600);
  byteCounter = 0; 
  for (i=0 ; i<8 ; i++){
    _setLEDGreen.print(green[i]) ;
  }
  _setLEDGreen.end();
}


void setLEDRed (SoftwareSerial & _setLEDRed){

  _setLEDRed.begin(9600);
  byteCounter = 0; 
  for (i=0 ; i<8 ; i++){
    _setLEDRed.print(red[i]) ;
  }
  _setLEDRed.end();
}


//---------------------------------------------------------------------------------------------
/** Returns the position of the rotary switch, 1-5 or returns 0 if no rotary switch is hooked up */
int getRotaryValue() {
  for(int i=0; i< 5; i++) { 
    int val = digitalRead(rotaryPins[i]); // look at a rotary switch input
    if( val == LOW ) { // it's selected
      //Serial.print("Pin:");
      //Serial.println(rotaryPins[i]);
      return (i + 1); // return a value that ranges 1 - 5
    }
  }
  return -1; // error case
}




//---------------------------------------------------------------------------------------------
//SD Card stuff 

void setupSDCard()
{
  Serial.println("setupSDCard()");
  Wire.begin();
  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCardPin)) {
    Serial.println("SD initialization failed!");
    return;
  }
  Serial.println("SD initialization done.");
}

void writeSDcard(char* str){
  //    Serial.print("Incoming string to write: <");
  //    Serial.print(str);
  //    Serial.println(">");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File myFile = SD.open("test1.txt", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    myFile.print(str);
    myFile.print("-");
    writeDate(myFile);
    myFile.close();
    Serial.println("done.");
  } 
  else {
    Serial.println("Error opening file");
  }
}


void writeString(File& inputFile, char* str) {
  if (inputFile){
    inputFile.print(str);
  }
}

void writeDate(File& inputFile){

  if (inputFile){
    // Reset the register pointer
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire.write(zero);
    Wire.endTransmission();

    Wire.requestFrom(DS1307_ADDRESS, 7);

    int second   = bcdToDec(Wire.read());
    int minute   = bcdToDec(Wire.read());
    int hour     = bcdToDec(Wire.read() & 0b111111); //24 hour time
    int weekDay  = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
    int monthDay = bcdToDec(Wire.read());
    int month    = bcdToDec(Wire.read());
    int year     = bcdToDec(Wire.read());

    //print the date EG   3/1/11 23:59:59
    inputFile.print(month);
    inputFile.print("/");
    inputFile.print(monthDay);
    inputFile.print("/");
    inputFile.print(year);
    inputFile.print(" ");
    inputFile.print(hour);
    inputFile.print(":");
    inputFile.print(minute);
    inputFile.print(":");
    inputFile.println(second);    
  }
}


/* Convert normal decimal numbers to binary coded decimal */
byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

/* Convert binary coded decimal to normal decimal numbers */
byte bcdToDec(byte val)  {
  return ( (val/16*10) + (val%16) );
}






