#include <LiquidCrystal.h>
#include <DHT.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const int dht_pin = 4;
const int screen_toggle_button = 5;
const int screen_scroll_button = 18;

const int RS = 13;
const int E = 21;
const int D4 = 27;
const int D5 = 26;
const int D6 = 25;
const int D7 = 33;

DHT dht(dht_pin, DHT11);

LiquidCrystal lcd(RS,E,D4,D5,D6,D7);

const char* ssid = "x";
const char* password = "x";

String apikey = "x";

String cityName = "Brampton";
String city = cityName;
String countrycode = "CA";

unsigned long lastTime = 0; //previous time API read occured
unsigned long timerDelay = 600000;

struct weatherData { //storing weather info in structure so its global
  float temp;
  float humid;
  String desc;
  String icon;
  float pop;
  float rain;
};

weatherData currentWeatherData;

//bool startup = false;

bool indoorActive = false;

bool scroll = false;

void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  dht.begin();
  pinMode(screen_toggle_button, INPUT_PULLUP);
  pinMode(screen_scroll_button, INPUT_PULLUP);

  city.replace(" ","%20");
  //startup = true;

  WiFi.begin(ssid,password);

  Serial.print("Connecting to WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout <30){
    Serial.print(".");
    delay(500);
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected to %s",ssid);
  } else {
    Serial.println("\nWIFI STILL NOT CONNECTING :(");
  }

  delay(1000);
}

void displayDHT() {
  lcd.clear();
  float sensorHumid  = dht.readHumidity();
  
  float sensorTemp = dht.readTemperature(false);
  
  if (isnan(sensorTemp) == false && isnan(sensorHumid) ==false){
    //Serial.println("LCD Print started,");
    

    char buffer1[16];//to store formatted text
    sprintf(buffer1,"Temp:%.1f\337C",sensorTemp);

    lcd.setCursor(0, 0);
    lcd.print(buffer1);
   
    char buffer2[16];//store humidity formatted
    sprintf(buffer2, "Humidity:%.0f%%", sensorHumid);

    lcd.setCursor(0, 1);
    lcd.print(buffer2);

    //Serial.printf("LCD Printed %s & %s\n",buffer1,buffer2);
    
    
    delay(2000);
  } else {
    Serial.println("Failed to read DHT");
  }
  
  delay(2000);
}

weatherData WeatherFetch(){

  if ((millis() - lastTime < timerDelay) && lastTime != 0) {
    return currentWeatherData;
  }

  //update timer right away so we don't spam requests if network fails
  lastTime = millis();

  weatherData data = currentWeatherData;
  if ((millis() - lastTime > timerDelay) || lastTime == 0 ) { //if current time minus previous api read timer is the read interval or previous read is at 0ms or first startup of code
    
    if (WiFi.status() == WL_CONNECTED){

      String weatherPath = "http://api.openweathermap.org/data/2.5/weather?q="+city+","+countrycode+"&APPID="+apikey+"&units=metric";
      HTTPClient http;
      http.begin(weatherPath);

      int weatherResponseCode = http.GET();

      if (weatherResponseCode > 0){
        String weatherPayload = http.getString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, weatherPayload);
        
        if (!error){ //if no deserialization error
          data.temp = doc["main"]["temp"];
          data.humid = doc["main"]["humidity"];
          data.desc = doc["weather"][0]["description"].as<String>();
          data.icon = doc["weather"][0]["icon"].as<String>();

          

          http.end();

          String forecastPath = "http://api.openweathermap.org/data/2.5/forecast?q="+city+","+countrycode+"&APPID="+apikey+"&units=metric";
          http.begin(forecastPath);

          int forecastResponseCode = http.GET();

          if (forecastResponseCode > 0){
            String forecastPayload = http.getString();
            JsonDocument doc2;
            DeserializationError error2 = deserializeJson(doc2,forecastPayload);

            if (!error2){ //if no forecast error
              data.pop = doc2["list"][0]["pop"];
              data.pop = data.pop*100;
              if (doc2["rain"]) {
                data.rain = doc2["rain"][0]["3h"];
              } else {
                data.rain = 0.00;
              }

              Serial.printf("temp: %.1f\nhumid: %.0f%%\ndesc: %s\nicon: %s\npop: %.0f%%\nrain: %.2fmm\n\n",data.temp, data.humid, data.desc.c_str(), data.icon,data.pop,data.rain);

              http.end();
              return data;
            } else {
            Serial.printf("JSON Forecast Path parsing failed: %s",error2.c_str());
            } 
          } else {
            Serial.printf("Error on forecast HTTP req: %i", forecastResponseCode);
          }

        } else {
          Serial.printf("JSON Weather Path parsing failed: %s",error.c_str());
        } 
      } else {
        Serial.printf("Error on weather HTTP req: %i", weatherResponseCode);
      } 
    } else {
      Serial.printf("WiFi disconnected D:%s\n",WiFi.status());
    }
    
  }
  return currentWeatherData;
}

void displayWeather(){
  //weatherData weatherInfo = WeatherFetch();
  if (currentWeatherData.desc.length()==0){
    lcd.clear();
    return;
  }
  lcd.clear();
  char buffer1[16];//to store formatted text
  sprintf(buffer1,"Temp:%.1f\337C",currentWeatherData.temp);
  lcd.setCursor(0, 1);
  lcd.print(buffer1);

  char buffer2[16];
  sprintf(buffer2,"%s,%s",cityName,countrycode);
  lcd.setCursor(0, 0);
  lcd.print(buffer2);
  
}

const char* codeList[] = {
  "01","02","03","04","09","10","11","13"
};

byte clearsky_L[] = {
  0x00,
  0x03,
  0x07,
  0x07,
  0x07,
  0x07,
  0x03,
  0x00
};
byte clearsky_R[] = {
  0x00,
  0x18,
  0x1C,
  0x1C,
  0x1C,
  0x1C,
  0x18,
  0x00
};
byte fewclouds_L[] = {
  0x00,
  0x00,
  0x03,
  0x0F,
  0x1F,
  0x1F,
  0x0F,
  0x00
};
byte fewclouds_R[] = {
  0x0E,
  0x11,
  0x11,
  0x1D,
  0x1E,
  0x1F,
  0x1F,
  0x00
};
byte scattclouds_L[] = {
  0x00,
  0x03,
  0x0F,
  0x1F,
  0x1F,
  0x0F,
  0x00,
  0x00
};
byte scattclouds_R[] = {
  0x00,
  0x10,
  0x1C,
  0x1E,
  0x1F,
  0x1F,
  0x00,
  0x00
};
byte brokenclouds_L[] = {
  0x00,
  0x01,
  0x02,
  0x07,
  0x08,
  0x10,
  0x10,
  0x0F
};
byte brokenclouds_R[] = {
  0x00,
  0x1C,
  0x02,
  0x11,
  0x0D,
  0x03,
  0x01,
  0x1E
};
byte rain_L[] = {
  0x03,
  0x0F,
  0x1F,
  0x1F,
  0x0F,
  0x04,
  0x01,
  0x04
};
byte rain_R[] = {
  0x10,
  0x1C,
  0x1E,
  0x1F,
  0x1F,
  0x08,
  0x02,
  0x08
};
byte thunder_L[] = {
  0x03,
  0x0F,
  0x1F,
  0x1F,
  0x0D,
  0x02,
  0x04,
  0x02
};
byte thunder_R[] = {
  0x10,
  0x1C,
  0x1E,
  0x1F,
  0x1B,
  0x04,
  0x08,
  0x04
};
byte snow_L[] = {
  0x09,
  0x06,
  0x05,
  0x09,
  0x09,
  0x05,
  0x06,
  0x09
};
byte snow_R[] = {
  0x12,
  0x0C,
  0x14,
  0x12,
  0x12,
  0x14,
  0x0C,
  0x12
};

byte* icons_L[] { //pointer list
  clearsky_L, fewclouds_L, scattclouds_L, brokenclouds_L, rain_L, rain_L, thunder_L, snow_L,
};
byte* icons_R[] { //pointer list
  clearsky_R, fewclouds_R, scattclouds_R, brokenclouds_R, rain_R, rain_R, thunder_R, snow_R,
};

void displayWeatherScroll(){//implement real scrolling system to move down and scroll through all climat
  //weatherData weatherInfo = WeatherFetch();
  if (currentWeatherData.icon.length()==0){
    lcd.clear();
    return;
  }

  lcd.clear();

  char buffer1[16];
  sprintf(buffer1,"Humidity:%.0f%%",currentWeatherData.humid);
  lcd.setCursor(0, 0);
  lcd.print(buffer1);

  char buffer2[16];//to store formatted text
  sprintf(buffer2,"%s",currentWeatherData.desc.c_str());
  lcd.setCursor(0, 1);
  lcd.print(buffer2);

  for (int i = 0; i < 8; i++) {
    if ((strncmp(currentWeatherData.icon.c_str(),codeList[i],2))==0){//compares first 2 digits to find matching icon
      
      lcd.createChar(0, icons_L[i]);
      lcd.setCursor(14,0);
      lcd.write(byte(0));
      lcd.createChar(1, icons_R[i]);
      lcd.setCursor(15,0);
      lcd.write(byte(1));
    }
  }

}

void loop(){
  currentWeatherData = WeatherFetch();
  /*if (digitalRead(screen_toggle_button) == LOW && indoorActive){//switching to indoor reading or weather on click
    displayWeather();
    indoorActive = false;
    scroll = true;
    delay(200);
  } else if (digitalRead(screen_toggle_button) == LOW && !indoorActive) {
    displayDHT();
    indoorActive = true;
    delay(200);
  } else if (digitalRead(screen_scroll_button) == LOW && !indoorActive && scroll){//scrolling on weather screen
    displayWeatherScroll();
    scroll = false;
    delay(200);
  } else if (digitalRead(screen_scroll_button) == LOW && !indoorActive && !scroll){//scrolling on weather screen
    displayWeather();
    scroll = true;
    delay(200);
  }*/
  displayWeather();
  delay(2000);
  displayWeatherScroll();
  

}