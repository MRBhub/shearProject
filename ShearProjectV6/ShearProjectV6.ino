#include <Keypad.h>  // Library for keypad functions

#include <SD.h>      // Library for SD card functions
#include "Wire.h"
#define DS1307_ADDRESS 0x68
/*
 SD card datalogger to record shearline statistics
 The circuit:

 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11  jumper to pin 51
 ** MISO - pin 12  jumper to pin 50
 ** CLK - pin 13   jumper to pin 52
 ** CS - pin 4     jumper to pin 53
 */



// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 4;


//Define the master signals that will be used to control Finite State Machine behaviors. 
boolean eStop=LOW;

const byte loadRequested=B0;
const byte loadInProgress=B1;
byte loadNeeded=B0;

const byte shearRequested=B0;
const byte shearInProgress=B1;
byte shearNeeded=B1;
const byte shearBusy=B0;
const byte shearIdle=B10;
byte shearClear=B10;

const byte rollRequested=B0;
const byte rollInProgress=B1;
byte rollNeeded=B1;
const byte rollIdle=B0;
const byte rollBusy=B1;
byte rollClear=B0;

const byte dumpRequested=B0;
const byte dumpInProgress=B1;
byte dumpNeeded=B1;
const byte dumpIdle=B0;
const byte dumpBusy=B1;
byte dumpClear=B0;

const byte endoRequested=B0;
const byte endoInProgress=B1;
byte endoNeeded=B1;

const byte bundleInProgress=B0;
const byte bundleNearDone=B1;
const byte bundleDone=B11;
byte bundleState=B0;

const byte orderInProgress=B0;
const byte orderDone=B1;
const byte doNothing=B11;
byte orderState=B0;

const byte bundleStart=B0;
const byte bundleFinish=B1;
const byte orderStart=B10;
const byte orderFinish=B11;
byte datalog=B10;

int shearCount=0;
int bundleCount=1;
int pieceCnt;
int shearToFill; 
int bundlesToCut;
int pocket=0;    // an index for which pocket we are currently filling...  values are either 0 or 1
boolean lastBundle=LOW;
boolean saveIntConveyor;
boolean saveRollConveyor;
boolean nearDone=LOW;
boolean finishBundle=LOW;

void setup() {
   attachInterrupt(5,eStopSet,CHANGE);
   attachInterrupt(4,eStopReset,CHANGE);
   Serial.begin(9600); // start serial communications used for writing to the serial monitor
   getOrderDetails();

   Wire.begin(); // start realtime clock
   pinMode(24,OUTPUT);
    digitalWrite(24,HIGH);
 
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);
 
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
  datalog=orderStart;
  printDate(datalog);
}  

void eStopSet(){
  const int intConveyor=23;
  pinMode (intConveyor,OUTPUT);
  pinMode(24,OUTPUT);
  const int dumpRoller=29;
  pinMode(dumpRoller,OUTPUT);
  digitalWrite(24,LOW);
  digitalWrite(dumpRoller,LOW);
  digitalWrite(intConveyor,LOW);
  eStop=HIGH;
}

void eStopReset() {
  // need to reload operating pin values
  const int intConveyor=23;
  pinMode (intConveyor,OUTPUT);
  const int dumpRoller=29;
  pinMode(dumpRoller,OUTPUT);
  eStop=LOW;
  digitalWrite(24,HIGH);
  digitalWrite(dumpRoller,saveRollConveyor);
  digitalWrite(intConveyor,saveIntConveyor);
}

void getOrderDetails() {
    const byte ROWS = 4; //four rows
    const byte COLS = 3; //three columns
    char keys[ROWS][COLS] = {
      {'1','2','3'},
      {'4','5','6'},
      {'7','8','9'},
      {'*','0','#'}
    };

  byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
  byte colPins[COLS] = {8, 7, 6}; //connect to the column pinouts of the keypad

    Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );char digitOneChar;
    char digitTwoChar;
    char digitThreeChar;
    int digitOne;
    int digitTwo;
    int digitThree;

    boolean needPieces=HIGH;
    boolean needBundle=HIGH;
    
    while(needPieces) {
      Serial.println("enter the number of pieces in a bundle ");
      int i=1;

      while ( i<=3) {
        char key = keypad.getKey();
        if (key != NO_KEY){
          Serial.println(key);
          switch (i){
            case 1:
              digitOneChar = key; i++;
              break;
            case 2:
              digitTwoChar = key; i++;
              break;
            case 3:
              digitThreeChar = key; i++;
              break;
          } 
        }
      }
      digitOne = digitOneChar - 48;
      digitTwo = digitTwoChar - 48;
      digitThree = digitThreeChar - 48;
      digitOne = digitOne * 100;
      digitTwo = digitTwo * 10;
      pieceCnt = digitOne + digitTwo + digitThree;
      Serial.println(pieceCnt); 
      int n=pieceCnt;
      while ( n > 11 ){
        n = n - 12;
      }
      if ( n == 0 ) {
          needPieces=LOW;
      } else {
          Serial.println( "Piece count needs to be a multiple of 12 ");
      }
    }
    
     shearToFill=pieceCnt/12;
     
     Serial.print("number of shear strokes per bundle is "); Serial.println(shearToFill);
   needPieces=HIGH; 
    
 while(needPieces) {
   Serial.println("enter two digits for the number of bundles to cut ");
    int i=1;
    while ( i<=2) {
      char key = keypad.getKey();
      if (key != NO_KEY){
         Serial.println(key);
      switch (i){
          case 1:
            digitOneChar = key; i++;
            break;
          case 2:
            digitTwoChar = key; i++;
            break;
        } 
      }
    }
    digitOne = digitOneChar - 48;
    digitTwo = digitTwoChar - 48;
    digitOne = digitOne * 10;
    bundlesToCut = digitOne + digitTwo;
   
    if (bundlesToCut>0 && bundlesToCut<=99) {
      needPieces=LOW;
    } else{
      Serial.println ("enter a bundle count between 1 and 99");
    }
  }
}

byte bcdToDec(byte val)  {
// Convert binary coded decimal to normal decimal numbers
  return ( (val/16*10) + (val%16) );
}
void soundAlarm(int alarmStart, int alarmDuration) {
 
  int alarmPin=48;
  float alarmFreq=1400;
  float period=(1.0/alarmFreq)*1000000;
  
  long elapseTime =0;
  if (millis() < alarmStart+alarmDuration) {
    digitalWrite(alarmPin,HIGH);
    delayMicroseconds(period/2);
    digitalWrite(alarmPin,LOW);
    delayMicroseconds(period/2);
  }
  digitalWrite(alarmPin,LOW);
}


void printDate(byte SF){

  // Reset the register pointer
  Wire.beginTransmission(DS1307_ADDRESS);

  byte zero = 0x00;
  Wire.write(zero);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 7);
  int second = bcdToDec(Wire.read());
  int minute = bcdToDec(Wire.read());
  int hour = bcdToDec(Wire.read() & 0b111111); //24 hour time
  int weekDay = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  int monthDay = bcdToDec(Wire.read());
  int month = bcdToDec(Wire.read());
  int year = bcdToDec(Wire.read());
  String sdateString;
  String fdateString;
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    switch(SF){
      case bundleStart:{
        String sdateString = "S " + String(month)+ "/"+String(monthDay)+"/"+String(year)+","+String(hour)+":"+String(minute)+":"+String(second)+",";
        dataFile.print(sdateString);
        break;
      }
      case bundleFinish:{
        String fdateString = "F " + String(month)+ "/"+String(monthDay)+"/"+String(year)+","+String(hour)+":"+String(minute)+":"+String(second);
        dataFile.println(fdateString);
        break;
      }
      case orderStart:{
        String fdateString = "Order Start " + String(month)+ "/"+String(monthDay)+"/"+String(year)+","+String(hour)+":"+String(minute)+":"+String(second)+", P, "+String(pieceCnt)+", B ,"+String(bundlesToCut);
        dataFile.println(fdateString);
        break;
      }
      case orderFinish:{
        String fdateString = "Order Finish " + String(month)+ "/"+String(monthDay)+"/"+String(year)+","+String(hour)+":"+String(minute)+":"+String(second);
        dataFile.println(fdateString);   
      }
    }
    dataFile.close();
  } 
}

// Main Program functions start here.


void loadTheBars(){
  // three states, readyToLoad, loading and loaded
  const int intConveyor=23;
  const int barLoadedSw=9;
  pinMode (barLoadedSw,INPUT);
  pinMode(intConveyor,OUTPUT);
  const byte readyToLoad=B0;
  const byte loading=B1;
  const byte loaded=B10;
  static byte loadSignal=B10;
  if (loadSignal==loaded){
    if(loadNeeded==loadRequested){
      loadSignal=readyToLoad;
      loadNeeded=loadInProgress;
    }
  }


  switch (loadSignal) {
    case readyToLoad:
      // guage stop is fully down
      digitalWrite ( intConveyor,HIGH);
      saveIntConveyor=HIGH;
      loadSignal=loading;

      if (shearCount == 0){
        datalog=bundleStart;
        printDate(datalog);  // if the first shear cut of a bundle, print the start date time stamp in the data log file
        bundleState=bundleInProgress;
      }
      break;
    case loading:
      // rolling the intermediate conveyor
      if (analogRead(barLoadedSw)<300) {
        // potentially debounce
        break; // wait for signal that all bars are at the starting line. 
      } 
      loadSignal=loaded;
      shearNeeded=shearRequested;
      // if the light beam or guage arm switches indicate all bars are there, set ready to shear
      // intentionally fall through and execute case loaded once...
    case loaded:
   
         //    Cross FSM Signal let the shear go
      break;
  }
}

void shearTheBars(){
  const int intConveyor=23;
  pinMode(intConveyor,OUTPUT);
  const int shearPin=25;
  pinMode(shearPin,OUTPUT);
  static unsigned long shearTime;
  const byte readyToShear=B0;
  const byte shearing=B1;
  const byte sheared=B10;
  static byte shearSignal=B10;
  // three states; ready to shear, shearing, sheared
  if (shearSignal==sheared){
    if(shearNeeded==shearRequested){
      if (shearClear==shearIdle){
        shearClear=shearBusy;
        shearSignal=readyToShear;
        shearNeeded=shearInProgress;
      }
    }
  }

  switch (shearSignal) {
    case readyToShear:
      // 
      digitalWrite (intConveyor, LOW);
      saveIntConveyor=LOW;
      digitalWrite (shearPin,HIGH);
      shearSignal=shearing;
      shearTime=millis();
      break;
    case shearing:
      // just waiting on the shear cycle to complete
      if (millis()-shearTime<4250) {
        break;  // marking time until 4.25 seconds pass assuming we just have to power for the 4.25 seconds it takes to shear
      } 
      shearSignal=sheared;
      digitalWrite (shearPin,LOW);
      shearCount++;
 
      if(shearCount==shearToFill) {
        finishBundle=HIGH;
      }
      rollNeeded=rollRequested;// Cross FSM Signal communicate with the rollToDump FSM
      // intentionally fall through to sheared case
    case sheared:
      break;
  }
}

void rollToDump () {
  // four states; ready to roll; lift guage up, rolling, rolled;
  const int guagePin=27;
  const int dumpRoller=29;
  const int guageClearSw=8;
  const byte readyToRoll=B0;
  const byte guageUp=B1;
  const byte rolling=B10;
  const byte rolled=B11;
  static byte rollSignal=B11;
  int dumpStopGuageSw[2] = {42,44};  //  assume we need to assign to stops 10 and 23 for pockets 1 & 2 respectively
  int pocketWait[2] = {3000,4000};
  pinMode(dumpRoller,OUTPUT);
  pinMode(guagePin,OUTPUT);
  pinMode(guageClearSw,INPUT);
  static unsigned long rollTime;
  static boolean guageClear=LOW;
  static unsigned long guageTime;
  int guageInt;
  
  if (rollSignal==rolled){
    if(rollNeeded==rollRequested){
      rollSignal=readyToRoll;
      rollNeeded=rollInProgress;
    }
  }  
  switch (rollSignal) {
    case readyToRoll:
      // first have to raise the guage arms
      digitalWrite(dumpStopGuageSw[pocket],HIGH);
      digitalWrite (guagePin,HIGH); // right now low or high?
      guageTime=millis();
      rollSignal=guageUp;
      break;
    case guageUp:
      if ((millis()-guageTime<1000)) {
        break; // mark time until .5 seconds pass
      }
      rollSignal=rolling; // set signal, record time, start the rollers
      
      digitalWrite(dumpRoller,HIGH);
      saveRollConveyor=HIGH;
      rollTime=millis();
      guageClear=LOW;
      break;
      
    case rolling:

      if (!guageClear) {
        guageInt=analogRead(guageClearSw);
        if (guageInt<300) {
        // potentially debounce
        break; // wait for signal that all bars are past the guage stop. 
        }
        shearClear=shearIdle;    
        guageClear=HIGH;
        digitalWrite(guagePin,LOW); // lower the guage arm
        loadNeeded=loadRequested; // Cross FSM Signal Now that the guage arm is down, we can load the next set of bars
        }
      // still rolling the guage table conveyor
      if (millis()-rollTime<pocketWait[pocket]) {
        break;
      } 
        // turn off guage table rollers?  decide 
      digitalWrite(dumpRoller,LOW);
      saveRollConveyor=LOW;
      rollSignal=rolled;
      dumpNeeded=dumpRequested;
      digitalWrite(dumpStopGuageSw[pocket],LOW);
      break;
      // intentionally fall through to rolled case
    case rolled:
      break;
  }
}

void dumpBars (int dumpPin, int matPin) {
  long int delaytime;
  
  static unsigned long dumpTime;
  static boolean dumpState;
  boolean unsafe=LOW;
  pinMode(dumpPin,OUTPUT);
  pinMode (matPin, INPUT);
  const byte readyToDump=B0;
  const byte dumping=B1;
  const byte barDumped=B10;
  const byte dumped=B10;
  static byte dumpSignal=B10;
  

  // three states; readytodump, dumping, dumped;
  // define safety rules  ok to dump bars while pressure mat is on?
  unsafe=digitalRead(matPin);
   if (dumpSignal==dumped){
    if(dumpNeeded==dumpRequested){
      if (dumpClear==dumpIdle) {
        dumpSignal=readyToDump;
        dumpNeeded=dumpInProgress;
        dumpClear=dumpBusy;
      }
    }
  }
 switch (dumpSignal) {
    case readyToDump:
      // check safety mat prior to activating dump arms
      if (unsafe) {
        break; // early exit from the readyToDump case
      }
      digitalWrite (dumpPin,HIGH);
      dumpSignal=dumping;
      dumpState=HIGH;
      dumpTime=millis();
      break;
    case dumping:
      // just waiting on the dump cycle to complete
      if (millis()-dumpTime<1550) {
        break;  // half up and half down
      } 
      if (millis()-dumpTime < 3000) {
        digitalWrite(dumpPin,LOW);
        break;
      } 
   //   set ready to roll new bars
        dumpSignal=barDumped;
        endoNeeded=endoRequested; // Cross FSM Signal communicate with the endo FSM 
        rollClear=rollIdle;
        break;
    case dumped:
      // should never get called from Master scheduler but just in case.
      break;
  }
}

void endoBars(int endoPin, int matPin) {
  static unsigned long endoTime;
  static boolean endoState;
  boolean unsafe=LOW;
  pinMode(endoPin,OUTPUT);
  pinMode (matPin, INPUT);
  const byte readyToEndo=B0;
  const byte endoing=B1;
  const byte endoed=B10;
  static byte endoSignal=B10;
  unsafe=digitalRead(matPin);
    if (endoSignal==endoed){
      if(endoNeeded==endoRequested){
        endoSignal=readyToEndo;
        endoNeeded=endoInProgress;
      }
    }
  switch (endoSignal) {
    case readyToEndo:
      // check safety mat prior to activating dump arms
      if (unsafe) {
        break; // early exit from the readyToEndo case
      } 
        digitalWrite (endoPin,HIGH);
        endoSignal=endoing;
        endoTime=millis();
        break;
      
    case endoing:
    
      // just waiting on the endo cycle to complete
      if ((millis()-endoTime)<750) {
        break;  // half up and half down
      } 
        digitalWrite(endoPin,LOW);
        if ((millis()-endoTime)<1500) {
          break;  // marking time until 1.5 seconds pass
        } 
//        dumpSignal=readyToDump;// Cross FSM Signal communicate with the endo FSM
        endoSignal=endoed;
        dumpClear=dumpIdle;
     
        if (shearCount==(shearToFill-2)) {
          bundleState=bundleNearDone;
        }
        
        if (shearCount == (shearToFill)) {
           bundleState=bundleDone;
           datalog=bundleFinish;
           printDate(datalog);  // write the bundle done time to the data log
           if (lastBundle==HIGH) { // tell the machine controller that we are done with the order
             lastBundle=LOW;
           }
        }
        finishBundle=LOW;
      
    case endoed:
      
    break; 
  }
}
  
  
void makebundle() {
   int pocketPin[2] = {39,41};
   int endoPin[2] = {43,45};   
   int safetyMatPin[2] = {38,40};

   if ( !eStop && !finishBundle) {   // don't load bars if eStop is pushed and completing a bundle
    loadTheBars ();
   }
   if ( !eStop && !finishBundle) {   // don't shear bars if eStop is pushed and completing a bundle
    shearTheBars ();
   }
   if ( !eStop) {
    rollToDump ();
   }
   if ( !eStop ) {
    dumpBars (pocketPin[pocket],safetyMatPin[pocket]);
   }
   if ( !eStop ) {
    endoBars (endoPin[pocket], safetyMatPin[pocket]);
  }
}

void makeorder(int shearStrokesToFill, int bundles) {
  const int bundleClose=46;
  pinMode(bundleClose,OUTPUT);

  const int nearDone=shearStrokesToFill-2;
  const int done=shearStrokesToFill;
  switch (bundleState) {
  case bundleNearDone:
     
      // finished bundle last shear stroke need to switch pockets

      digitalWrite(bundleClose,HIGH);
      makebundle();
      
          
  break;
  case bundleDone:
      digitalWrite(bundleClose,LOW);
      // Endo Machine just completed the last cut in a bundle
      soundAlarm(millis(),3000);
      if (bundleCount < bundles) {
        shearCount=0;
        bundleCount++;
        //switch pockets
        if (pocket==0) {
          pocket++;
          } else {
            pocket--;
          }
          //sound the horn?
          bundleState=bundleInProgress;
          makebundle();
      } else {
        orderState=orderDone;
      }
      break;
    default:
         makebundle();
  }
}
void shutMachineDown() {
  for(int i=20; i<55; i++) {
    digitalWrite(i,LOW);
  }
}
void loop() {
  // get at making them
  switch (orderState) {
  case orderInProgress: 
    makeorder(shearToFill,bundlesToCut);
    break;
  case orderDone: 
    shutMachineDown();
    datalog=orderFinish;
    printDate(datalog);
    Serial.println("Order Complete, thank you ");
  case doNothing:
    // do nothing
    orderState=doNothing; // set order state such that the machine will just do nothing
  }
}


