#include <math.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

//Initiate Oled
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PRESET_TEMP 110
#define OFF_TEMP 0
#define LOWER_BOUNDARY_TEMP 0
#define UPPER_BOUNDARY_TEMP 260
#define MOVING_AVG_POINTS 0 //points for moving avg. (0 to disable)
#define Resis_Thermistor 5000.0 //resistance for voltage divider
#define TEMP_INCREMENT 10
#define DAT 6
#define CLK 7
#define BTN 2
int heater = 5; //PWM PIN for HEATER
int ledPin = 13;
int speaker = 4;
int thermistor = A0; // thermistor pin
float a = 0.000767639032436511; //constants for thermistor
float b = 0.000206233511284758;
float c = 1.38941753419318E-07;
bool tempFlag = false; //true during a temperature change (used in oled function)
int newTemp = 0;
float currentTemp;
int bState = LOW;
int EALastState;
int offset = 0;
int last, setTemperature;
//moving avg initiation
float readings[MOVING_AVG_POINTS];
int readIndex = 0;
ClickEncoder *encoder;

void timerIsr() {
  encoder->service();
}

void oledDisplay(float currentTemp, int setPoint) {
  //updating current temperature
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(25, 0);
  display.print("SMD PLATE V1.0");

  display.fillRoundRect(5, 10, 69, 40, 4, WHITE); //current temperature border
  display.fillRoundRect(76, 10, 52, 40, 4, WHITE);//setpoint border

//  display.drawPixel(123,63, WHITE); 
  display.fillRoundRect(5, 52, 123, 10, 6, WHITE);

  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(7, 15);
  display.print("Temperature");
  display.setCursor(93, 15);
  display.print("Set");

if(setPoint == 0 && tempFlag != true){
    display.setCursor(58, 53);
    display.print("OFF");
}
else if (tempFlag != true){
    display.setCursor(33, 53);
    display.print("REGULATING...");
}
else{
    display.setCursor(58, 53);
    display.print("SET");
}

  
  display.setTextSize(2);
  display.setCursor(10, 30);
  if(currentTemp > 100){
    display.print(currentTemp,0);  
  }
  else{
    display.print(currentTemp,1);
  }
  display.setTextSize(1);
  display.print((char)247);
  display.print("C");

  if(tempFlag == true){     //blinking letters (when setpoint is to be changed)
    display.setTextSize(2);
    display.setTextColor(BLACK);
    display.setCursor(80, 30);
    display.print(setPoint);
    display.setTextSize(1);
    display.print((char)247);
    display.print("C");
    display.display();
    delay(20);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(80, 30);
    display.print(setPoint);
    display.setTextSize(1);
    display.print((char)247);
    display.print("C");
    display.display();
    delay(20);
    display.setTextSize(2);
    display.setTextColor(BLACK);
    display.setCursor(80, 30);
    display.print(setPoint);
    display.setTextSize(1);
    display.print((char)247);
    display.print("C");
    display.display();
  }
 else{    //non-blinking letters (when setpoint is determined)
    display.setTextSize(2);
    display.setTextColor(BLACK);
    display.setCursor(80, 30);
    display.print(setPoint);
    display.setTextSize(1);
    display.print((char)247);
    display.print("C");
    display.display();
 }
}


float readTemp(float rawTemp, float resistor) {
  float total = 0;
  float avg = 0;
  float reading = resistor / ((1023.0 / rawTemp) - 1.0); // 10K/ ((1023/ADC)-1)
  reading = log(reading);
  float temp = ((1.0 / ( a + (b * reading) + (c * reading * reading * reading))) - 273.15);
  //n-point moving average
  if(MOVING_AVG_POINTS != 0){
    if(readIndex >= MOVING_AVG_POINTS){ //wrap around after exceeding no. of points
      readIndex = 0;
    }
    readings[readIndex] = temp;
    for (int point = 0; point < MOVING_AVG_POINTS; point++){
        total += readings[point];
    }
    avg = total / MOVING_AVG_POINTS;
    readIndex++;
    //Serial.println(avg);
    return avg;
  }
  else{
    return temp;
  }
}
int setTemp(int temp){
  if(temp == -1){
      while (true){
        tempFlag = true;
        ClickEncoder::Button btn = encoder->getButton();
        if (btn != ClickEncoder::Open){ //checking for the exit click
          Serial.println("ExitClicked");
          speakerTone(2);
          break; //exit while loop
        }
        
        setTemperature += encoder->getValue()*TEMP_INCREMENT; //set temperature (increment by 10)  
        if(setTemperature < LOWER_BOUNDARY_TEMP){ //lower boundary
          setTemperature = LOWER_BOUNDARY_TEMP;
        }
        else if(setTemperature > UPPER_BOUNDARY_TEMP){ //upper boundary
          setTemperature = UPPER_BOUNDARY_TEMP;
        }
        if ((setTemperature != last)) {
          last = setTemperature;
          speakerTone(1);
        }
        float currentTemp = readTemp(analogRead(thermistor), Resis_Thermistor);
        //updating setTemp oled
        oledDisplay(currentTemp, setTemperature);
        regulateTemp(newTemp, currentTemp);
        
    }
    tempFlag = false;

  }
  else
    setTemperature = temp;
    last = setTemperature;
  return setTemperature;
}

void regulateTemp(int setTemp, float currentTemp) {
  if (setTemp <= currentTemp - offset ) {
    analogWrite(heater, 0); //LOW
    digitalWrite(ledPin, LOW);
  }
  else if (setTemp > currentTemp + offset) {
    analogWrite(heater, 51); //HIGH (20%  dutycycle)
    digitalWrite(ledPin, HIGH);
  }
}

void speakerTone(int par){ // 0 = selection sound, 1 = encoder sound, 2 = exit selection sound, 3 = HOLD(TURN OFF SOUND), 4 = DOUBLE CLICK, 5 = START-UP SOUND
  switch (par){
    case 0: tone(speaker, 523, 100); //C5
          break;
    case 1: tone(speaker, 1396, 10); //F6
          break;
    case 2: tone(speaker, 1046, 100); //C6
          break;
    case 3: tone(speaker, 1046, 100); //C6
            delay(100);
            tone(speaker, 784, 100); //G5
            delay(100);
            tone(speaker, 659, 100); //E5
            delay(100);
            tone(speaker, 523, 100); //C5     
          break;
    case 4: tone(speaker, 523, 100); //C5     
            delay(100);
            tone(speaker, 784, 100); //G5
          break;
    case 5: tone(speaker, 523, 100); //C5 
            delay(100);
            tone(speaker, 659, 100); //E5 
            delay(100);
            tone(speaker, 784, 100); //G5 
            delay(100);
            tone(speaker, 1046, 100); //C6 
          break;
  }
}

void setup() {
  pinMode (ledPin, OUTPUT);
  pinMode (heater, OUTPUT); //MIGHT NOT BE NEEDED FOR ANALOGWRITE

  for (int thisReading = 0; thisReading < MOVING_AVG_POINTS; thisReading++){ //set readings[] to [0,0,0,0] for calculation later
    readings[thisReading] = 0;
  }
  Serial.begin (9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  encoder = new ClickEncoder(DAT, CLK, BTN);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 
  
  last = -1;
  speakerTone(5);
}
void loop() {
  currentTemp = readTemp(analogRead(thermistor), Resis_Thermistor);

  ClickEncoder::Button btn = encoder->getButton(); //check encoder button
  if (btn != ClickEncoder::Open) {
    Serial.print("Button: ");
    switch (btn) {
      case ClickEncoder::Clicked:
        Serial.println("ClickEncoder::Clicked"); //checking for first click
        speakerTone(0);
        newTemp = setTemp(-1); //-1 for manual temp setting, any other value for fixed temperature
      break;
      case ClickEncoder::DoubleClicked:
        Serial.println("DOUBLE");
        newTemp = setTemp(PRESET_TEMP); //set temp preset
        speakerTone(4);

      break;
      case ClickEncoder::Held:
        Serial.println("HOLDING");
        if (newTemp != 0){
         newTemp = setTemp(OFF_TEMP); //OFF temp to 0C 
         speakerTone(3);
        }
      break;
    }
  }    
  regulateTemp(newTemp, currentTemp);
  oledDisplay(currentTemp, newTemp);
  Serial.print("current: ");
  Serial.print(currentTemp);
  Serial.print(" ");
  Serial.print("newTemp: ");
  Serial.println(setTemperature);
}
