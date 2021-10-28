#include <SoftwareSerial.h>

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef stepperM_Pin
#define stepperM_Pin 14
#endif
#ifndef CW_Pin
#define CW_Pin 27
#endif

#define Gear_Rate 15

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void generatePulse(unsigned int);
void renewHTML();

const char ssid[] = "maichan";
const char passwd[] = "kuraki";
const IPAddress ip(192,168,138,1);
const IPAddress subnet(255,255,255,0);
const char* PARAM_INPUT_1 = "direction";
const char* PARAM_INPUT_2 = "steps";

// Variables to save values from HTML form
String direction;
String steps;
int16_t currentSteerAngle;
bool cw;

char index_html[1600];

const char pre_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Stepper Motor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<h1>Stepper Motor Controller</h1>
<hr>
Current Angle =
)rawliteral";

const char post_index_html[] PROGMEM = R"rawliteral(

    <form action="/" method="GET">

      <label for="steps">Steering Angle</label>
      <input type="number" name="steps">
      <input type="submit" value="GO!">
    </form>
</body>
</html>
)rawliteral";

// Variable to detect whether a new request occurred
bool newRequest = false;
bool onHandleMove;

int16_t remainPulse;

// define two tasks for Blink & AnalogRead
void taskPulse( void *pvParameters );

AsyncWebServer server(80);

String pre, post;

void renewHTML(){
  pre = (String)pre_index_html;
  pre.concat(String(currentSteerAngle));
  post = (String)post_index_html;
  pre.concat(post);
  pre.toCharArray(index_html, 1600);  
}

//HardwareSerial RS485(2);
HardwareSerial RS485(2);

void setup() {
  cw = true;
  renewHTML();
  currentSteerAngle = 0; // steering is centered
  onHandleMove = false;
  remainPulse = 0;
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
//  Serial1.begin(9600);
  RS485.begin(9600);
  pinMode(11, OUTPUT);
  digitalWrite(11, HIGH);

  startWiFi();
/*
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", index_html);
  });
  */

  // Handle request (form)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);

        // HTTP POST input1 value (direction)
        if (p->name() == PARAM_INPUT_1) {
          direction = p->value().c_str();
          Serial.print("Direction set to: ");
          Serial.println(direction);
        }
        // HTTP POST input2 value (steps)
        if (p->name() == PARAM_INPUT_2) {
          steps = p->value().c_str();
          Serial.print("Number of steps set to: ");
          Serial.println(steps);
        }

    }
    request->send(200, "text/html", index_html);
    newRequest = true;
  });

  server.begin();
}

void loop()
{
  // Check if there was a new request and move the stepper accordingly
  if (newRequest) {
    int16_t value = steps.toInt();
    if(value == 100){ //magic number for steering stop
      onHandleMove = false;
    }else if(value == 101){ //magic number for initial steering angle
      onHandleMove = false;
      currentSteerAngle = 0;
      renewHTML();
    }else if(value > 30){
      return;
    }else if(value < -30){
      return;
    }else if(!onHandleMove){
      Serial.print("commandValue:  ");
      Serial.println(value);
      generatePulse(value);

    }

    newRequest = false;
  }
  Serial.println("tomcat");
  RS485.println("tomcat");
  //Serial1.println("tomcat");
  vTaskDelay(50);
}

void startWiFi() {
  /*
  WiFi.begin(ssid, passwd);
  Serial.print("WiFi connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  */
  WiFi.softAP(ssid,passwd);
  delay(100);
  WiFi.softAPConfig(ip,ip,subnet);
  
  Serial.print(" connected. ");
  //Serial.println(WiFi.localIP());
}

void taskPulse(void *pvParameters)  // This is a task.
{
  //(void) pvParameters;

  pinMode(stepperM_Pin, OUTPUT);
  pinMode(CW_Pin, OUTPUT);
  if(cw){
    digitalWrite(CW_Pin, HIGH);
  }else{
    digitalWrite(CW_Pin, LOW);
  }

  int16_t movedValue = 0;
  
  while(onHandleMove) // A Task shall never return or exit.
  {
    if(remainPulse < 1) break;
    //Serial.print(remainPulse);
    //Serial.print(" ");
    digitalWrite(stepperM_Pin, HIGH);
    vTaskDelay(1);
    digitalWrite(stepperM_Pin, LOW);
    vTaskDelay(1);
    remainPulse--;
    movedValue++;
  }
  Serial.print("movedValue: ");
  Serial.println(movedValue);
  
  Serial.print("current Angle: ");
  

  onHandleMove = false;
  if(cw){
    currentSteerAngle += (int16_t)(movedValue / Gear_Rate);
  }else{
    currentSteerAngle -= (int16_t)(movedValue / Gear_Rate);
  }
  Serial.println(currentSteerAngle);
  renewHTML();
  vTaskDelete(NULL);
}

void generatePulse(unsigned int Angle) {
  int8_t remainAngle = Angle - currentSteerAngle;
  if(remainAngle == 0){
    return;
  }else if(remainAngle < -60){
    remainAngle = -60;
  }else if(remainAngle > 60){
    remainAngle = 60;
  }
  if(remainAngle > 0){
    cw = true;
  }else{
    cw = false;
    remainAngle *= -1;
  }
  remainPulse = remainAngle * Gear_Rate;
  
  Serial.print("remainPulse:  ");
  Serial.println(remainPulse);
  onHandleMove = true;
  
  xTaskCreatePinnedToCore(
    taskPulse
    ,  "taskPulse"   // A name just for humans
    ,  8096  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  5  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL
    ,  1);
}
