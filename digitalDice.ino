#include <Arduino.h>
#include <TM1637Display.h>

// Module connection pins (Digital Pins)
#define CLK 2
#define DIO 3
#define BUTTON_IN 7
#define SPEAKER_OUT 9

// The amount of time (in milliseconds) between tests
#define TEST_DELAY   100

const uint8_t segments[] = {
  SEG_A ,
  SEG_B ,
  SEG_C ,
  SEG_D 
};
const uint8_t segments1[] = {
  SEG_B ,
  SEG_C ,
  SEG_D ,
  SEG_E 
};
const uint8_t segments2[] = {
  SEG_C ,
  SEG_D ,
  SEG_E ,
  SEG_F 
};
const uint8_t segments3[] = {
  SEG_D ,
  SEG_E ,
  SEG_F ,
  SEG_A 
};
const uint8_t segments4[] = {
  SEG_E ,
  SEG_F ,
  SEG_A ,
  SEG_B 
};
const uint8_t segments5[] = {
  SEG_F ,
  SEG_A ,
  SEG_B ,
  SEG_C 
};

TM1637Display display(CLK, DIO,TEST_DELAY);

void setup()
{
  pinMode(SPEAKER_OUT, OUTPUT);
  pinMode(BUTTON_IN, INPUT);
  Serial.begin(9600);
  display.clear();
}
void playerRoll(int* die1, int* die2){
  randomSeed(analogRead(A1));
  *die1 = random(1, 7);
  *die2 = random(1, 7);
  
    Serial.println(*die1);
    Serial.println(*die2);
}
void spinAnimation(){
  for(int j=0; j<10; j++){
    for(int i=0; i<7; i++){
      switch(i) {
      case 0:
        display.setSegments(segments);
        Serial.println("case:0");
        tone(SPEAKER_OUT, j*100, 16);
        break;
      case 1:
        display.setSegments(segments1);
        Serial.println("case:1");
        noTone(SPEAKER_OUT);
        break;
      case 2:
        display.setSegments(segments2);
        Serial.println("case:2");
        tone(SPEAKER_OUT, j*100, 16);
        break;
      case 3:
        display.setSegments(segments3);
        Serial.println("case:3");
        noTone(SPEAKER_OUT);
        break;
      case 4:
        display.setSegments(segments4);
        Serial.println("case:4");
        tone(SPEAKER_OUT, j*100, 16);
        break;
      case 5:
        display.setSegments(segments5);
        Serial.println("case:5");
        noTone(SPEAKER_OUT);
        break;
      default:
        break;
      }
    }
  }
  display.clear();
}
int brightness = 3;
void displayNum(int num, int num2){
  display.setBrightness(brightness, true);
  display.showNumberDecEx(num,0, false,1,0);
  display.showNumberDecEx(num2,0, false,1,3);
}
int pressed = 0;
int dice1, dice2;
void loop(){
  delay(100);
  pressed = digitalRead(BUTTON_IN);
  if(pressed == HIGH){
    Serial.println("pressed");
    spinAnimation();
    playerRoll(&dice1, &dice2);
  } else {
    Serial.println("not pressed");
    displayNum(dice1,dice2);
    noTone(SPEAKER_OUT);
  }
}