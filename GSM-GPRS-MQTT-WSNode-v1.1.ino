/*
 Test-program to send data to a MQTT Server
 Server: "agriConnect.vn", default port: 1883, topic: "bt01"
 ----
 Connect agduino to mqtt broker via gprsbee(sim900 module)
 Sesors:
    A0: AM2301 data pin
    D2: SHT data pin
    D4: DS18B20 data pin
    D6: SHT clk pin
    D8: soft-UART RX pin (fixed in AltSoftSerial lib)
    D9: soft-UART TX pin (fixed in AltSoftSerial lib)
    D10:
    D11:
    D12:
    D13:
 Coded by AgriConnect on 10th April 2017.
*/
// Comes with IDE
#include <Wire.h>
#include <String.h>
#include <Math.h>
// Timer lib
#include <SimpleTimer.h>
// Soft-UART
#include <AltSoftSerial.h>
//#include <SoftwareSerial.h>
// Soil sensor 5TM lib
#include <SDI12.h>
// T&RH SHT11 sensor lib
#include <SHT1x.h>
// DS18B20 sensor lib
#include <OneWire.h>
#include <DallasTemperature.h>
// MQTT lib
#include <mqtt.h>

/*-------------------( Declare Constants and Pin Numbers )------------------*/
// Timer period: 60 seconds.
int timer_period = 60000;
// Network BT0, node's ID 01
char ws_mote_address[] = "BT001";
// Rain sensor
int rainSensor_pin = A0;
// T&RH SHT11 sensor
#define sht_io_clk 6
#define sht_io_data 2
// DS18B20 sensor
#define ds18b20_io_data 4
// Soil sensor
const int soilSensor_io_data = 3;
// Wind sensor
/*---------------------( Declare Objects and Variables )---------------------*/
// Rain sensor
int rainSensor_val;
int rainSensor_status = 0;
// T&RH SHT11 sensor
float air_tempC, air_humi;
SHT1x sht1x(sht_io_data, sht_io_clk);
// DS18B20 sensor: Setup a oneWire instance to communicate with the sensor
float node_tempC;
OneWire oneWire(ds18b20_io_data);
DallasTemperature sensors(&oneWire);
// Soil sensor
float soil_dielctric, soil_temp;
SDI12 mySDI12(soilSensor_io_data);
// Wind sensor
int wind_speed, wind_direction;
// GSM/GPRS
char atCommand[50];
byte mqttMessage[127];
int mqttMessageLength = 0;
String gprsStr = "";
int index = 0;
byte data1;
boolean gprsReady = false;
// Soft-UART (RX, TX)
AltSoftSerial altSerial;
//SoftwareSerial GPRS(10, 11);
// Timer1
SimpleTimer timer1;
/*-----------------------------( SETUP FUNC ) ------------------------------*/
void setup() {
  // GPIO configuration - GSM/GPRS
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  // Initialize the serial communications. Set speed to 9600 kbps.
  Serial.begin(9600);
  // Soft-UART interface - GSM/GPRS
  altSerial.begin(9600);
  //GPRS.begin(9600);
  // Start up the DS18B20 sensor
  sensors.begin();
  // Initialize SDI-12 communication - 5TM.
  mySDI12.begin();
  // Initialize the timers
  timer1.setInterval(timer_period, timer_isr);
}
/*-------------------------- ( END SETUP FUNC ) ----------------------------*/
/*-------------------------- ( LOOP FUNC ) ---------------------------------*/
void loop()
{
  timer1.run();
}
/*-------------------------- ( END LOOP FUNC ) -----------------------------*/
/*--------------------------( Declare User-written Functions )--------------*/
void timer_isr()
{
  // Rain
  rainSensor_val = analogRead(rainSensor_pin);
  if(rainSensor_val > 400) rainSensor_status = 0;
  else rainSensor_status = 1;
  // Read T&RH
  air_tempC = sht1x.readTemperatureC();
  air_humi = sht1x.readHumidity();
  if (isnan(air_humi) || isnan(air_tempC)) {
    Serial.println("Failed to read from SHT sensor");
    air_humi=0;
    air_tempC=0;
  }
  // Read DS18B20 temperature sensor
  // request to the sensor 
  sensors.requestTemperatures();
  node_tempC = sensors.getTempCByIndex(0);
  // Read 5TM - Soil T&Dielectric has address '0'
  takeMeasurement('0');
  // Read wind speed and direction
  // No wind sensor
  wind_speed = 0;
  wind_direction = 0;
  // GSM/GPRS: enable
  digitalWrite(12,HIGH);
  digitalWrite(13,HIGH);
  // wait for GPRS getting stable
  delay(20000);

  Serial.println("Checking if GPRS is ready");
  altSerial.println("AT");
  delay(1000);
  gprsReady = isGPRSReady();
  if (gprsReady == true)
  {
    Serial.println("GPRS Ready");
    String json = buildJson();
    char jsonStr[300];
    json.toCharArray(jsonStr,300);
    Serial.println(json);
        
    // Change the IP and Topic.
    /* The arguments here are: clientID, IP, Port, Topic, Message */
    sendMQTTMessage("agrinode", "agriconnect.vn", "1883", "bt01",jsonStr);
  } 
  
  // GSM/GPRS: sleep mode
  digitalWrite(12,LOW);
  digitalWrite(13,LOW);
}
/*------------------------------ ( SUB-FUNC ) --------------------------------*/
boolean isGPRSReady(){
  altSerial.println("AT");
  altSerial.println("AT");
  altSerial.println("AT+CGATT?");
  index = 0;
  while (altSerial.available()){
    data1 = (char)altSerial.read();
    Serial.write(data1);
    gprsStr[index++] = data1;
  }
  Serial.println("Check OK");
  Serial.print("gprs str = ");
  Serial.println(data1);
  if (data1 > -1){
    Serial.println("GPRS OK");
    return true;
  }
  else 
  {
    Serial.println("GPRS NOT OK");
    return false;
  }
}

void sendMQTTMessage(char* clientId, char* brokerUrl, char* brokerPort, char* topic, char* message){
  altSerial.println("AT"); // Sends AT command to wake up cell phone
  Serial.println("send AT to wake up GPRS");
  delay(1000); // Wait a second
  digitalWrite(13, HIGH);
  // Puts phone into GPRS mode
  altSerial.println("AT+CSTT=\"m-wap\",\"mms\",\"mms\"");
  Serial.println("AT+CSTT=\"m-wap\",\"mms\",\"mms\"");
  delay(2000); // Wait a second
  altSerial.println("AT+CIICR");
  Serial.println("AT+CIICR");
  delay(2000);
  altSerial.println("AT+CIFSR");
  Serial.println("AT+CIFSR");
  delay(2000);
  strcpy(atCommand, "AT+CIPSTART=\"TCP\",\"");
  strcat(atCommand, brokerUrl);
  strcat(atCommand, "\",\"");
  strcat(atCommand, brokerPort);
  strcat(atCommand, "\"");
  altSerial.println(atCommand);
  Serial.println(atCommand);
  // Serial.println("AT+CIPSTART=\"TCP\",\"mqttdashboard.com\",\"1883\"");
  delay(2000);
  altSerial.println("AT+CIPSEND");
  Serial.println("AT+CIPSEND");
  delay(2000);
  mqttMessageLength = 16 + strlen(clientId);
  Serial.println(mqttMessageLength);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++) {
  // Message contents
  altSerial.write(mqttMessage[j]);
  Serial.write(mqttMessage[j]);
  Serial.println("");
 }
 altSerial.write(byte(26)); // (signals end of message)
 Serial.println("Sent");
 delay(10000);
 altSerial.println("AT+CIPSEND");
 Serial.println("AT+CIPSEND");
 delay(2000);
 mqttMessageLength = 4 + strlen(topic) + strlen(message);
 Serial.println(mqttMessageLength);
 mqtt_publish_message(mqttMessage, topic, message);
 for (int k = 0; k < mqttMessageLength; k++) {
 altSerial.write(mqttMessage[k]);
 Serial.write((byte)mqttMessage[k]);
 }
 // (signals end of message)
 altSerial.write(byte(26));
 // Message contents
 Serial.println("-------------Sent-------------");
 delay(5000);
 altSerial.println("AT+CIPCLOSE");
 Serial.println("AT+CIPCLOSE");
 delay(2000);
}

String buildJson() {
  String data = "{";
  data += "\n";
  data += "\"d\": {";
  data += "\n";
  data += "\"myName\": \"agrinode_bt01\",";
  data += "\n";
  data += "\"Air_Temperature\": ";
  data += (float)air_tempC;
  data += ",";
  data += "\n";
  
  data += "\"Air_Humidity\": ";
  data += (float)air_humi;
  data += ",";
  data += "\n";

  data += "\"Node_Temperature\": ";
  data += (float)node_tempC;
  data += ",";
  data += "\n";

  data += "\"Rain\": ";
  data += (int)rainSensor_status;
  data += ",";
  data += "\n";

  data += "\"Wind_Speed\": ";
  data += (int)wind_speed;
  data += ",";
  data += "\n";
  
  data += "\"Wind_Direction\": ";
  data += (int)wind_direction;
  data += "\n";
  data += "}";
  data += "\n";
  data += "}";
  return data;
}

void takeMeasurement(char i)
{
  String command = "";
  command += i;
  // SDI-12 measurement command format  [address]['M'][!]
  command += "M!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of measurments available, 0-9]
  while(!mySDI12.available()>5);
  delay(100);
  
  mySDI12.read();//consume address 
  // find out how long we have to wait (in seconds).
  int wait = 0; 
  wait += 100 * mySDI12.read()-'0';
  wait += 10 * mySDI12.read()-'0';
  wait += 1 * mySDI12.read()-'0';
  // ignore # measurements, for this simple examlpe
  mySDI12.read();
  // ignore carriage return
  mySDI12.read();
  // ignore line feed
  mySDI12.read();
  
  long timerStart = millis();
  while((millis() - timerStart) > (1000 * wait))
  {
    //sensor can interrupt us to let us know it is done early
    if(mySDI12.available()) break;
  }
  // in this example we will only take the 'DO' measurement  
  mySDI12.flush(); 
  command = "";
  command += i;
  // SDI-12 command to get data [address][D][dataOption][!]
  command += "D0!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement
  while(!mySDI12.available()>1);
  // let the data transfer
  delay(300);
  printBufferToScreen();
  mySDI12.flush();
}
void printBufferToScreen()
{
  String buffer = "";
  String buffer1 = "";
  String buffer2 = "";
  // consume address
  mySDI12.read();
  while(mySDI12.available())
  {
    char c = mySDI12.read();
    if(c == '+' || c == '-')
    {
      buffer += '/';
      if(c == '-') buffer += '-';
    } 
    else
    {
      buffer += c;
    }
    delay(100);
  }
 buffer1 = buffer.substring(buffer.indexOf("/") + 1, buffer.lastIndexOf("/"));
 buffer2 = buffer.substring(buffer.lastIndexOf("/") + 1, buffer.lastIndexOf("/") + 5);

 soil_dielctric = buffer1.toFloat();
 soil_temp = buffer2.toFloat();
}

// gets identification information from a sensor, and prints it to the serial port
// expects a character between '0'-'9', 'a'-'z', or 'A'-'Z'.
char printInfo(char i)
{
  int j; 
  String command = "";
  command += (char) i;
  command += "I!";
  for(j = 0; j < 1; j++)
  {
    mySDI12.sendCommand(command);
    delay(30);
    if(mySDI12.available()>1) break;
    if(mySDI12.available()) mySDI12.read();
  }

  while(mySDI12.available())
  {
    char c = mySDI12.read();
    if((c!='\n') && (c!='\r')) Serial.write(c);
    delay(5);
  } 
}

// converts allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
// to a decimal number between 0 and 61 (inclusive) to cover the 62 possible addresses
byte charToDec(char i)
{
  if((i >= '0') && (i <= '9')) return i - '0';
  if((i >= 'a') && (i <= 'z')) return i - 'a' + 10;
  if((i >= 'A') && (i <= 'Z')) return i - 'A' + 37;
}

// THIS METHOD IS UNUSED IN THIS EXAMPLE, BUT IT MAY BE HELPFUL.
// maps a decimal number between 0 and 61 (inclusive) to
// allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
char decToChar(byte i)
{
  if((i >= 0) && (i <= 9)) return i + '0';
  if((i >= 10) && (i <= 36)) return i + 'a' - 10;
  if((i >= 37) && (i <= 62)) return i + 'A' - 37;
}

/*
uint32_t parsedecimal(char *str) {
 uint32_t d = 0;
 while (str[0] != 0) {
 if ((str[0] > '9') || (str[0] < '0'))
 return d;
 d *= 10;
 d += str[0] - '0';
 str++;
 }
 return d;
}*/

void readline() {
 /*char c;
 buffidx = 0; // start at begninning
 while (1) {
 c = gpss.read();
 if (c == -1)
 continue;
 Serial.print(c);
 if (c == '\n')
 continue;
 if ((buffidx == BUFFSIZ-1) || (c == '\r')) {
 buffer[buffidx] = 0;
 return;
 }
 buffer[buffidx++]= c;
 }*/
}
