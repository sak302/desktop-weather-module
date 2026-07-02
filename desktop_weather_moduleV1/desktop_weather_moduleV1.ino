/*
what to work on:
- fixing name capitalization, can add with city.replace() where the " " is replaced with "%20"
- better scrolling & more api data displayed
- more visually appealing html pages, also html page for success instead of just printing

*/


#include <LiquidCrystal.h>
#include <DHT.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>

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

const char* ap_ssid = "Weather-Module";
const char* ap_password = "12345678!";
WebServer server(80);
String header;

//preferences library to save wifi, location data, and api key to flash memory
Preferences preferences;

String ssid = "";
String password = "";
String apikey = "";
String cityName = "";
String countrycode = "";

String city = ""; //so it can be altered later

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

bool indoorActive = false;

bool scroll = false;

bool toggle_previous;
bool scroll_previous;

//storing as raw cuz its easier and also in flash memory to save space
const char portal_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html>
<form action="/save_success" method="POST">
  <label for="ssid">WiFi Name:</label><br>
  <input type="text" id="ssid" name="ssid" required><br>
  <label for="pass">WiFi Password:</label><br>
  <input type="text" id="pass" name="pass" required><br>
  <label for="api">API Key:</label><br>
  <input type="text" id="api" name="api" required><br>
  <label for="city">City Name:</label><br>
  <input type="text" id="city" name="city" required><br>
  <label for="country">Country Code (ISO 3166, ie. CA, FR, UK)</label><br>
  <input type="text" id="country" name="country" required><br><br>
  <input type="submit" value="Submit & Restart">
</form>
)rawliteral";

void handleRoot(){//handles the "root" or base html page
  server.send(200,"text/html",portal_html);
}

void handleSubmit(){//handles the submit button response
  preferences.begin("weather-config",false);

  if (server.hasArg("ssid") && server.hasArg("pass") && server.hasArg("api")
  && server.hasArg("city") && server.hasArg("country")){//finding if server returns values

    preferences.putString("wifi_ssid",server.arg("ssid"));//putting values in preferences
    preferences.putString("wifi_pass",server.arg("pass"));
    preferences.putString("weather_api_key",server.arg("api"));
    preferences.putString("city_name",server.arg("city"));
    preferences.putString("country_code",server.arg("country"));

    preferences.end();
    server.send(200,"text/plain","Submission Success! Restarting ESP...");
    delay(1000);

    ESP.restart();//restarting esp so regular program can run
  }
}

void startPortal() {

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(ap_ssid, ap_password)) {//loop if access point creation fails so nothing else runs
      log_e("Soft AP creation failed.");
      while (1);
  }

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address:");
  Serial.println(IP);

  server.on("/",HTTP_GET, handleRoot);//setting http_get and post requests from client
  server.on("/save_success",HTTP_POST,handleSubmit);
  server.begin();

  lcd.begin(16,2);//displaying connect to wifi on lcd 
  lcd.setCursor(0, 0);
  lcd.print("Connect to WiFi:");
  lcd.setCursor(0,1);
  lcd.print(ap_ssid);

  while (true){
    server.handleClient();//handling client requests
  }

  
}

void setup() {
  Serial.begin(115200);

  preferences.begin("weather-config",false);//read write mode
 
  ssid = preferences.getString("wifi_ssid","");//setting variables to preference values
  password = preferences.getString("wifi_pass","");
  apikey = preferences.getString("weather_api_key","");
  cityName = preferences.getString("city_name","");
  countrycode = preferences.getString("country_code","");

  preferences.end();

//checking preferences
if (ssid == "" ||password == "" ||apikey == "" ||cityName == "" ||countrycode == ""){
  Serial.printf("\nssid: %s\npassword: %s\napikey: %s\ncityName: %s\ncountrycode: %s",ssid.c_str(),password.c_str(),apikey.c_str(),cityName.c_str(),countrycode.c_str());
  startPortal();//shouldnt return so stays in wifi loop then restart
}

//initializing lcd, dht, buttons
  lcd.begin(16, 2);
  dht.begin();
  pinMode(screen_toggle_button, INPUT_PULLUP);
  pinMode(screen_scroll_button, INPUT_PULLUP);

//making city name spaces API compatible
  city = cityName;
  city.replace(" ","%20");


//connecting to home wifi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(),password.c_str());

  Serial.print("Connecting to WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout <30){
    Serial.print(".");
    delay(500);
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected to %s\n",ssid.c_str());
  } else {
    Serial.println("WIFI STILL NOT CONNECTING :(\n");
  }

  delay(1000);
}

void displayDHT() {
  //reading temp and humidity values
  float sensorHumid  = dht.readHumidity();
  
  float sensorTemp = dht.readTemperature(false);
  
  if (!isnan(sensorTemp) && !isnan(sensorHumid)){//if both temp and humid are valid
    //Serial.println("LCD Print started,");
    
    //updating lcd
    char buffer1[17];//to store formatted text
    snprintf(buffer1,17,"Room Temp:%.1f\337C%-16s",sensorTemp, " ");//snprintf to maintain array size and overwrite previous characters

    lcd.setCursor(0, 0);
    lcd.print(buffer1);
   
    char buffer2[17];//store humidity formatted
    snprintf(buffer2, 17, "Humidity:%.0f%%%-16s", sensorHumid, " ");

    lcd.setCursor(0, 1);
    lcd.print(buffer2);

    
    
    

  } else {
    Serial.println("Failed to read DHT");
  }
}

weatherData WeatherFetch(){
  //timer to make sure the api is only fetched at a certain interval 
  if ((millis() - lastTime < timerDelay) && lastTime != 0) {
    //Serial.printf("Timer Interval: %i\n",millis() - lastTime);
    return currentWeatherData;
  }

  
  
  Serial.printf("Last time is: %i\n",lastTime);
  Serial.printf("Current time is: %i\n",millis());
  weatherData data = currentWeatherData;
    
  if (WiFi.status() == WL_CONNECTED){
    Serial.printf("Weather fetch started\n-----------------------------\n");
    String weatherPath = "http://api.openweathermap.org/data/2.5/weather?q="+city+","+countrycode+"&APPID="+apikey+"&units=metric";
    HTTPClient http;
    http.begin(weatherPath);

    int weatherResponseCode = http.GET();//response code

    if (weatherResponseCode > 0){
      String weatherPayload = http.getString();//stores json of text in weatherPayload

      JsonDocument doc;//for holding json values
      DeserializationError error = deserializeJson(doc, weatherPayload);//puts weatherpayload in doc and stores error
      
      if (!error){ //if no deserialization error store json values in their respective property of data
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
            lastTime = millis();//updating last time api was fetched
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
    Serial.printf("WiFi disconnected D:%i\n",WiFi.status());
  }
    
  return currentWeatherData;
}

void displayWeather(){
  //weatherData weatherInfo = WeatherFetch();
  /*if (currentWeatherData.desc.length()==0){
    lcd.clear();
    return;
  }*/
  
  char buffer1[17];//to store formatted text
  snprintf(buffer1,17,"Temp:%.1f\337C%-16s",currentWeatherData.temp," ");
  lcd.setCursor(0, 1);
  lcd.print(buffer1);

  char buffer2[17];
  snprintf(buffer2,17,"%s,%s%-16s",cityName,countrycode," ");
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

byte* icons_L[] { //pointer list of icons on left side
  clearsky_L, fewclouds_L, scattclouds_L, brokenclouds_L, rain_L, rain_L, thunder_L, snow_L,
};
byte* icons_R[] { //pointer list of right icons
  clearsky_R, fewclouds_R, scattclouds_R, brokenclouds_R, rain_R, rain_R, thunder_R, snow_R,
};

void displayWeatherScroll(){//implement real scrolling system to move down and scroll through all climat
  
  if (currentWeatherData.icon.length()==0){
    lcd.clear();
    return;
  }

  char buffer1[17];
  snprintf(buffer1,17,"Humidity:%.0f%%%-16s",currentWeatherData.humid," ");
  lcd.setCursor(0, 0);
  lcd.print(buffer1);

  char buffer2[17];//to store formatted text
  snprintf(buffer2,17,"%s%-16s",currentWeatherData.desc.c_str()," ");
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
  if (digitalRead(screen_toggle_button) == LOW && toggle_previous == HIGH && indoorActive){//switching to indoor reading or weather only on button click
    displayWeather();
    indoorActive = false;
    scroll = true;
    
  } else if (digitalRead(screen_toggle_button) == LOW && toggle_previous == HIGH && !indoorActive) {
    displayDHT();
    indoorActive = true;
    
  } else if (digitalRead(screen_scroll_button) == LOW && scroll_previous == HIGH && !indoorActive && scroll){//scrolling on weather screen
    displayWeatherScroll();
    scroll = false;
    
  } else if (digitalRead(screen_scroll_button) == LOW && scroll_previous == HIGH && !indoorActive && !scroll){//scrolling on weather screen
    displayWeather();
    scroll = true;
    
  }
  toggle_previous = digitalRead(screen_toggle_button);
  scroll_previous = digitalRead(screen_scroll_button);
  delay(50);
  
    
}