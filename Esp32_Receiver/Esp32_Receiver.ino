#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_adc_cal.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "icons.h"
#include <HTTPClient.h>

#define LM35_Sensor1 35
#define BUTTON_PIN 23

#define RXD2 16 //RX2
#define TXD2 17 //TX2
#define HC12 Serial2  //Hardware Serial 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid     = "YOUR SSID";
const char* password = "YOUR WIFI PASSWORD";

int LM35_Raw_Sensor1 = 0;
float LM35_TempC_Sensor1 = 0.0;
float LM35_TempF_Sensor1 = 0.0;
float Voltage = 0.0;

int contRic = 0;
int sunset = 0, sunrise = 0, dt = 0;
bool giorno = true, spento = false, spentoTemp = false, onlyPrev = false;
bool abilitaBottone = true;
int ritenta = 0;
String desc = "default", wid = "default", type = "nointernet", rec = "", lastTempEsterna = "";
String tempEsterna = "--.-C", tempInterna = "--.-'C", umiditaEsterna = "--";
float arrayTI[3]; int posATI = 0;

String forecast_id [9];
float forecast_temp [9];
String forecast_main [9];
String forecast_datetime [9];
String forecast_dt [9];

void task2(void * parameters){
  int cont = 0;
  for(;;){
    if (abilitaBottone == true){
      if(digitalRead(BUTTON_PIN) != HIGH){
        cont++;
        Serial.println("holding button...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
      } else {
        if (cont == 1){
          Serial.println("previsioni");
          onlyPrev = true;
        } else if (cont > 6 && cont != 1){
          Serial.println("turning off / on screen");
          if (spento == false)
            spento = true;
          else{
            spento = false; 
            onlyPrev = false;
          }
        }
        cont = 0;
      }
    } else {
      Serial.println("waiting");
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}

void task1(void * parameters){
  bool nuovo = false;
  for(;;){
    while (HC12.available()) {
      rec += char(HC12.read());
      nuovo = true;
    }

    if (rec != " " && rec != ""){    
      Serial.println("what received: " + rec);
      String temp = "", temp2 = "";
      int c = 0;
      tempEsterna = splitString(rec, '@', 0);
      umiditaEsterna = splitString(rec, '@', 1);
      Serial.print(tempEsterna + ", " + umiditaEsterna);
      rec = "";
    } 
    vTaskDelay(110 / portTICK_PERIOD_MS);
  }
}

String splitString(String str, char sep, int index){ //non ho idea di cosa faccia ma funziona
 int found = 0;
 int strIdx[] = { 0, -1 };
 int maxIdx = str.length() - 1;

 for (int i = 0; i <= maxIdx && found <= index; i++)
 {
    if (str.charAt(i) == sep || i == maxIdx)
    {
      found++;
      strIdx[0] = strIdx[1] + 1;
      strIdx[1] = (i == maxIdx) ? i+1 : i;
    }
 }
 return found > index ? str.substring(strIdx[0], strIdx[1]) : "";
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Impossibile eseguire SSD1306"));
    for(;;);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  display.clearDisplay(); //Cancella il buffer
  display.setTextSize(1);
  display.setTextColor(WHITE);
  WiFi.begin(ssid, password);
  int cont = 5;
  while (WiFi.status() != WL_CONNECTED && cont >= 0) {
    display.setCursor(25, 45);
    display.clearDisplay();
    display.drawBitmap(44, 7, logo, 40, 34, WHITE);
    display.println("Connessione " + String(cont));
    display.display();
    cont--;
    delay(1000);
  }
  if (cont == -1){
    display.setCursor(50, 55);
    display.println("FAILED");
  } else {
    display.setCursor(60, 55);
    display.println("OK");
  }
  display.display();
  delay(1000);

  display.clearDisplay();
  xTaskCreate(
    task1,
    "Task 1",
    1000,
    NULL,
    1,
    NULL  
  );

  xTaskCreate(
    task2,
    "Task 2",
    1000,
    NULL,
    1,
    NULL  
  );
}

void loop() {
  if (spento == false && onlyPrev == false){
  UpdateFromAPI();
  if (dt > sunrise && dt < sunset){
    giorno = true;
  } else {
    giorno = false;
  }
  
  display.fillRect(0, 49, 128, 11, BLACK);
  display.setCursor(5, 50);
  display.print(desc);
  display.fillRect(0, 0, 46, 48, BLACK);
  
  setIcon(type, 10, 10, wid);
  
  display.drawFastHLine(4, 60, 120, WHITE);
  display.drawFastHLine(4, 63, 120, WHITE);
  display.drawFastVLine(4, 60, 4, WHITE);
  display.drawFastVLine(124, 60, 4, WHITE);
  
  updateLM35();

  display.fillRect(60, 5, 68, 44, BLACK);
  
  String tmp = tempEsterna;
  tmp.remove(tempEsterna.length() - 1);
  if(tempEsterna.length() >= 3){
    if (lastTempEsterna == tmp)
      contRic++;
    else {
      contRic = 0;
      display.fillRect(110, 5, 28, 40, BLACK);
    }
      
    if(contRic >= 3){
      display.drawBitmap(110, 8, alert, 9, 8, WHITE);
      display.drawBitmap(110, 38, alert, 9, 8, WHITE);
    }
    
    display.setCursor(64, 8);
    display.print(tmp + "'C");
    display.setCursor(64, 23);
    display.print(tempInterna + "'C");
    display.setCursor(64, 38);
    display.print(String(umiditaEsterna[0]) + String(umiditaEsterna[1]) + String(umiditaEsterna[2]) + String(umiditaEsterna[3]) + "%");
    lastTempEsterna = tmp;
    display.display();    
  } else {
    display.print("--.-'C");
    display.setCursor(64, 23);
    display.print(tempInterna + "'C");
    display.setCursor(64, 38);
    display.print("--%");
    display.display();
  }

  spentoTemp = false;
  startBar();
  
  } else {
    if(spentoTemp == false){
      display.fillRect(0, 0, 126, 64, BLACK);
      display.display();
      display.setCursor(30, 30);
      display.print("Sospensione");
      display.display();
      spentoTemp = true;
    }
    delay(1000);
    display.fillRect(0, 0, 126, 64, BLACK);
    display.display();
  }
}

void mostraPrevisioni(){
  onlyPrev = false;
  if (!(WiFi.status() == WL_CONNECTED)){
    display.clearDisplay();
    display.setCursor(30, 30);
    display.print("No Internet");
    display.display();
    delay(1500);
    display.clearDisplay();
    return;
  } else {
    display.clearDisplay();
    display.setCursor(14, 30);
    display.print("Scaricamento dati");
    display.display();
    display.clearDisplay();
  }
  forecast24h();
  display.clearDisplay();
  display.setCursor(32, 25);
  display.print("Previsioni");
  display.setCursor(27, 35);
  display.print("prossime 24h");
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();

  int tempsunset = sunset, tempsunrise = sunrise;
  for(int i = 1; i < 9; i = i + 2){ 
    //-7200, is for my location in italy
    if(forecast_datetime[i] == "00:00" || forecast_datetime[i+1] == "00:00"){
      tempsunset += 86400;
      tempsunrise += 86400;
    }
    
    if((forecast_dt[i]).toInt() - 7200 > tempsunrise && (forecast_dt[i]).toInt() - 7200 < tempsunset){
      giorno = true;
      setIcon(forecast_main[i], 15, 15, forecast_id[i]);
    } else {
      giorno = false;
      setIcon(forecast_main[i], 15, 15, forecast_id[i]);
    }

    if((forecast_dt[i+1]).toInt() - 7200 > tempsunrise && (forecast_dt[i+1]).toInt() - 7200 < tempsunset){
      giorno = true;
      setIcon(forecast_main[i+1], 76, 15, forecast_id[i+1]);
    } else {
      giorno = false;
      setIcon(forecast_main[i+1], 76, 15, forecast_id[i+1]);
    }

    display.setCursor(17, 7);
    display.print(forecast_datetime[i]);
    display.setCursor(77, 7);
    display.print(forecast_datetime[i+1]);

    String temp1 = String(forecast_temp[i]) + String("'C");
    String temp2 = String(forecast_temp[i+1]) + String("'C");
    temp1.remove(temp1.length()-3, 1);
    temp2.remove(temp2.length()-3, 1);  

    display.setCursor(15, 48);
    display.print(temp1);
    display.setCursor(75, 48);
    display.print(temp2);
    
    if(i == 1 || i == 2){
      display.drawCircle(35, 60, 2, WHITE);
      display.drawPixel(53, 60, WHITE);
      display.drawPixel(68, 60, WHITE);
      display.drawPixel(85, 60, WHITE);      
    } else if (i == 3 || i == 4){
      display.drawPixel(35, 60, WHITE);
      display.drawCircle(53, 60, 2, WHITE);
      display.drawPixel(68, 60, WHITE);
      display.drawPixel(85, 60, WHITE);  
    } else if (i == 5 || i == 6){
      display.drawPixel(35, 60, WHITE);
      display.drawPixel(53, 60, WHITE);
      display.drawCircle(68, 60, 2, WHITE);
      display.drawPixel(85, 60, WHITE);  
    } else if (i == 7 || i == 8){
      display.drawPixel(35, 60, WHITE);
      display.drawPixel(53, 60, WHITE);
      display.drawPixel(68, 60, WHITE);
      display.drawCircle(85, 60, 2, WHITE);
    }
    
    display.display();
    delay(4000);
    display.clearDisplay();
  }

  display.clearDisplay();
  int lastX = 0, lastY = 31, firstTemp = forecast_temp[0], nextTemp;
  display.setCursor(37, 0);
  display.print("andamento");
  display.setCursor(35, 8);
  display.print("meteo 24h");
  display.setCursor(0, 7);
  display.print("ora");
  display.setCursor(110, 7);
  display.print("24h");
  for(int i = 0; i < 9; i++){
    display.drawPixel(i*15, 55 - int(forecast_temp[i]), WHITE);
    display.drawFastVLine(i*15, 55 - int(forecast_temp[i]), 100, WHITE);
    if(i != 0){
      display.drawLine(lastX, lastY, i*15, 55 - int(forecast_temp[i]), WHITE);
    }
    lastY = 55 - int(forecast_temp[i]);
    lastX = i*15;
    
    display.display();
  }
    
  delay(5000);
  display.clearDisplay();
  
  for(int i = 0; i < 9; i++){
    forecast_temp[i] = 0;
    forecast_main[i] = "";
    forecast_datetime[i] = "";
    forecast_id[i] = "";
    forecast_dt[i] = "";
  }
}

void startBar(){
  for (int i = 1; i < 120; i+=2){
    if (spento == true){
      return;
    } else if (onlyPrev == true){
      abilitaBottone = false;
      mostraPrevisioni();
      abilitaBottone = true;
      return;
    }
    display.fillRect(5, 61, i, 2, WHITE);
    display.display();  
    delay(1000);
  }
  display.fillRect(5, 61, 118, 2, BLACK);
  display.display();
}

uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

void updateLM35(){
  LM35_Raw_Sensor1 = analogRead(LM35_Sensor1);
  Voltage = readADC_Cal(LM35_Raw_Sensor1);
  tempInterna = (Voltage / 10) - 3;
  String tmp = String(tempInterna[0]) + "" + String(tempInterna[1]) + "" + String(tempInterna[2]) + "" + String(tempInterna[3]);
  if(posATI == 0){
    arrayTI[posATI] = tmp.toFloat();
    posATI = 1;
  } else if (posATI == 1){
    arrayTI[posATI] = tmp.toFloat();
    posATI = 2;
  } else if (posATI == 2){
    arrayTI[posATI] = tmp.toFloat();
    posATI = 0;
  }
  float temp = (arrayTI[0] + arrayTI[1] + arrayTI[2]) / 3;
  String tempDiNuovo = String(temp);
  tempInterna = String(tempDiNuovo[0]) + "" + String(tempDiNuovo[1]) + "" + String(tempDiNuovo[2]) + "" + String(tempDiNuovo[3]);
}

void setIcon(String meteo, int x, int y, String idd){
  if (meteo == "Thunderstorm"){
      display.drawBitmap(x, y, fulmini, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Clouds" && idd == "801" && giorno == false){
      display.drawBitmap(x, y, notte_nuvolosa, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Clear" && giorno == false){
      display.drawBitmap(x, y, notte, 36, 33, WHITE);
      display.display();
  } else if (meteo == "Clouds" && idd == "801" && giorno == true){
      display.drawBitmap(x, y, sole_nuvoloso, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Drizzle" || meteo == "Rain"){
      display.drawBitmap(x, y, pioggia, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Clouds"){
      display.drawBitmap(x, y, nuvoloso, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Clear" && giorno == true){
      display.drawBitmap(x, y, sole, 36, 33, WHITE);
      display.display();  
  } else if (meteo == "Snow"){
      display.drawBitmap(x, y, neve, 36, 33, WHITE);
      display.display();
  } else {
      display.drawBitmap(x, y, nointernet, 36, 33, WHITE);
      display.display();
  }
}

void forecast24h(){
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    DynamicJsonDocument doc(1024);
    http.begin("http://api.openweathermap.org/data/2.5/forecast?q=Porcia&appid=e703cc05c5ab1ba3e0906bcdb542a143");
    int httpCode = http.GET(); 

    String payload;
    if (httpCode > 0) { 
      String payload = http.getString();
      Serial.println(httpCode);
  
      int temp = 0;
      for (int i = 0; temp < 9; i++){
        int wtf0 = payload.indexOf("dt", i);
        int wtf = payload.indexOf("temp", i);
        forecast_temp[temp] = (String(payload[wtf+6]) + "" + String(payload[wtf+7]) + "" + String(payload[wtf+8]) + "" + String(payload[wtf+9]) + "" + String(payload[wtf+10])).toFloat();
        int wtf2 = payload.indexOf("weather", i);
        int wtf22 = payload.indexOf("id", wtf2);
        for(int k = wtf0+4; payload[k] != ','; k++){
          forecast_dt[temp] += payload[k];
        }
        for(int k = wtf22+4; payload[k] != ','; k++){
          forecast_id[temp] += payload[k];
        }
        int wtf3 = payload.indexOf("main", wtf2);
        for(int k = wtf3+7; payload[k] != '"'; k++){
          forecast_main[temp] += payload[k];
        }
        int wtf4 = payload.indexOf("dt_txt", i);
        for(int k = wtf4+20; payload[k] != '"'; k++){
          forecast_datetime[temp] += payload[k];
        }
        i = wtf4;
        temp++;
      }
      for(int i = 0; i < 9; i++){
        forecast_temp[i] = forecast_temp[i] - 273.15;
        forecast_datetime[i].remove(5, 3);  
        Serial.println(forecast_datetime[i]);
      }
    }
    http.end();
 }
}

void UpdateFromAPI(){
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    DynamicJsonDocument doc(1024);
 
    http.begin("http://api.openweathermap.org/data/2.5/weather?lat=45.96&lon=12.60&appid=e703cc05c5ab1ba3e0906bcdb542a143&lang=it"); 
    int httpCode = http.GET();

    String payload;
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        delay(120000);
        return;
      }
      JsonObject root = doc.as<JsonObject>();
      sunrise = root["sys"]["sunrise"];
      sunset = root["sys"]["sunset"];
      dt = root["dt"];
      
      bool cicle = true;
      for(int i = 0; cicle == true; i++){
        if(payload[i] == 'i' && payload[i+1] == 'd'){
          cicle = false;
          wid = String(payload[i+4]) + "" + String(payload[i+5]) + "" + String(payload[i+6]);
        }
      }
      cicle = true;
      desc = "";
      for(int i = 0; cicle == true; i++) {
        if(payload[i] == 'd' && payload[i+10] == 'n'){
          cicle = false;
          int j = i;
          while(payload[j+14] != ','){
            if (payload[j+14] != '"')
              desc += payload[j+14];
            j++;
          }
        }
      }
      if(desc.length() >= 17){
        int temp = 0;
        String tmp = "";
        for (int h = 0; h <= 17; h++){
          tmp += desc[h];
        }
        desc = tmp + "...";
      }
      
      cicle = true;
      type = "";
      for(int i = 0; cicle == true; i++){
        if(payload[i] == 'm' && payload[i+3] == 'n'){
          cicle = false;
          int j = i;
          while(payload[j+7] != ','){
            if (payload[j+7] != '"')
              type += payload[j+7];
            j++;
          }
        }
      }
    }
    http.end();
  } else {
    WiFi.begin(ssid, password);
    setIcon("nointernet", 10, 10, wid);
    type = "nointernet";
    desc = "no internet";
  }
}
