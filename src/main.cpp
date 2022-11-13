#include <Arduino.h>

#include <string.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeMono9pt7b.h>
#include <SPI.h>

#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9

// Rot und Blau vertauscht.. https://chrishewett.com/blog/true-rgb565-colour-picker/
#define BUGGY_WHITE 0xffff
#define BUGGY_BLUE 0xf800
#define BUGGY_GREEN 0x07e0
#define BUGGY_RED 0x001f
#define BUGGY_AMBER 0x05df

#define cpuledok 2
#define cpuledhigh 3
#define ramledok 4
#define ramledhigh 7
#define cpuvum 5
#define ramvum 6

#define redthreshold 90
#define loopsUntilScreenChanges 40
#define timeWithoutDataToStandby 3000

Adafruit_ST7735 tft=Adafruit_ST7735(TFT_CS,TFT_DC,TFT_RST);

bool standingBy=false;
bool dataStreamStarted=false;
bool dataScreenInitialized=false;
int mainLoopIndex=0;
int now=0;
int lastSeenData=0;

//Vars for serial reception
boolean newData=false;
const byte numChars=64;
char receivedChars[numChars];
char temporaryChars[numChars];
char stringFromPC[numChars]={0};
int cpuLoadFromPC=0;
int ramLoadFromPC=0;
char hourFromPC[16];
char minuteFromPC[16];
char hourDisplayed[16];
char minuteDisplayed[16];

// Functions
void switchLEDsOff()
{
  digitalWrite(LED_BUILTIN,LOW);
  digitalWrite(cpuledok,LOW);
  digitalWrite(cpuledhigh,LOW);
  digitalWrite(ramledok,LOW);
  digitalWrite(ramledhigh,LOW);
}

void controlVUassembly(unsigned short int vuID,unsigned short int value)
{
  float valueF=value*1.9;

  //0 für CPU, 1 für RAM
  switch (vuID)
  {
  case 0:
    analogWrite(cpuvum,valueF);
    if (value<redthreshold)
    {
      digitalWrite(cpuledok,HIGH);
      digitalWrite(cpuledhigh,LOW);
    }
    else
    {
      digitalWrite(cpuledok,LOW);
      digitalWrite(cpuledhigh,HIGH);
    }

    break;
  
  case 1:
    analogWrite(ramvum,valueF);
    if (value<redthreshold)
    {
      digitalWrite(ramledok,HIGH);
      digitalWrite(ramledhigh,LOW);
    }
    else
    {
      digitalWrite(ramledok,LOW);
      digitalWrite(ramledhigh,HIGH);
    }
    break;

  default:
    //standBy();
    break;
  }
}

void drawText(int cursorX,int cursorY,int textSize,int color,const String text)
{
  tft.setCursor(cursorX,cursorY);
  tft.setTextColor(color,ST7735_BLACK);
  tft.setTextSize(textSize);
  tft.println(text);
}

void standBy()
{
  standingBy=true;

  memset(receivedChars,0,sizeof(receivedChars));
  memset(temporaryChars,0,sizeof(temporaryChars));

  tft.fillScreen(ST7735_BLACK);
  tft.drawRoundRect(7,7,115,145,4,BUGGY_AMBER);
  drawText(36,60,1,BUGGY_AMBER,"Warte");
  drawText(46,80,1,BUGGY_AMBER,"auf");
  drawText(35,100,1,BUGGY_AMBER,"Daten.");

  analogWrite(cpuvum,0);
  analogWrite(ramvum,0);

  switchLEDsOff();

  dataStreamStarted=false;
  dataScreenInitialized=false;
}

void recieveData()
{
  static boolean receptionInProgress=false;
  static byte ndx=0;
  char startMarker='<';
  char endMarker='>';
  char rc;

  while (Serial.available()>0 && newData==false) {
    rc=Serial.read();

    if(receptionInProgress==true) {
      if(rc!=endMarker) {
        receivedChars[ndx]=rc;
        ndx++;

        if(ndx>=numChars) {
          ndx=numChars-1;
        }
      }
      else {
        receivedChars[ndx]='\0'; //String is terminated
        receptionInProgress=false;
        ndx=0;
        newData=true;
      }
    }
    else if (rc==startMarker) {
      receptionInProgress=true;
    }
  }
  
}

void parseData() {
  char * strtokIndex;

  strtokIndex=strtok(temporaryChars,",");
  strcpy(stringFromPC,strtokIndex);

  strtokIndex=strtok(NULL,",");
  cpuLoadFromPC=atoi(strtokIndex);

  strtokIndex=strtok(NULL,",");
  ramLoadFromPC=atoi(strtokIndex);

  strtokIndex=strtok(NULL,",");
  strcpy(hourFromPC,strtokIndex);

  strtokIndex=strtok(NULL,",");
  strcpy(minuteFromPC,strtokIndex);

  lastSeenData=millis();
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(cpuledok,OUTPUT);
  pinMode(cpuledhigh,OUTPUT);
  pinMode(ramledok,OUTPUT);
  pinMode(ramledhigh,OUTPUT);

  pinMode(cpuvum,OUTPUT);
  pinMode(ramvum,OUTPUT);

  //TFT
  tft.initR(INITR_BLACKTAB);
  tft.setFont(&FreeMono9pt7b);

  standBy();
  controlVUassembly(0,0);
  controlVUassembly(1,0);
  switchLEDsOff();
}

void loop() {
  recieveData();
  if(newData==true) {
    strcpy(temporaryChars,receivedChars);
    parseData();
    newData=false;

    standingBy=false;

    controlVUassembly(0,cpuLoadFromPC);
    controlVUassembly(1,ramLoadFromPC);

    dataStreamStarted=true;
  }

  now=millis();
  if((now-lastSeenData)>=timeWithoutDataToStandby && !standingBy) {
    standBy();
  }

  if (dataStreamStarted && !dataScreenInitialized) {
    tft.fillScreen(ST7735_BLACK);
    tft.drawRoundRect(7,7,115,145,4,BUGGY_WHITE);
    tft.drawFastHLine(15,110,(tft.width()-30),BUGGY_WHITE);
    drawText(55,140,2,BUGGY_WHITE,String(":"));
    tft.drawTriangle(25,20,35,30,15,30,BUGGY_WHITE);
    tft.drawTriangle(15,40,35,40,25,50,BUGGY_WHITE);

    dataScreenInitialized=true;
  }

  if(dataScreenInitialized && mainLoopIndex==0) {
    if(strcmp(hourDisplayed,hourFromPC)!=0) {
        tft.fillRect(15,118,42,27,ST7735_BLACK);
        drawText(15,140,2,BUGGY_WHITE,String(hourFromPC));

        strcpy(hourDisplayed,hourFromPC);
      }

    if(strcmp(minuteDisplayed,minuteFromPC)!=0) {
        tft.fillRect(70,118,42,27,ST7735_BLACK);  
        drawText(70,140,2,BUGGY_WHITE,String(minuteFromPC));

        strcpy(minuteDisplayed,minuteFromPC);
    }
  }

  if(mainLoopIndex>=loopsUntilScreenChanges){
    mainLoopIndex=0;
  }
  else {
    mainLoopIndex++;
  }
}