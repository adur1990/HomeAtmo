#include <Arduino.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#include "WiFi.h"
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

#include "html.h"
#include "creds.h"

/******* Timer Variables *******/
bool sent_today = false;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
uint64_t day = 1000000 * 60 * 60 * 24;
uint64_t hour = 1000 * 60 * 60;
int last_time = 0;

/******* Sensor Variables *******/
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;

/******* WiFi Variables *******/
AsyncWebServer server(80);
HTTPClient http;

String getTemperature() {
  float t = bme.readTemperature();
  if (isnan(t)) {
    Serial.println("Failed to read from BME680 sensor!");
    return "";
  }
  else {
    Serial.println("Temperature: " + String(t));
    return String(t);
  }
}

String getHumidity() {
  float h = bme.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from BME680 sensor!");
    return "";
  }
  else {
    Serial.println("Humidity: " + String(h));
    return String(h);
  }
}

String getPressure() {
  float p = bme.readPressure() / 100.0F;
  if (isnan(p)) {
    Serial.println("Failed to read from BME680 sensor!");
    return "";
  }
  else {
    Serial.println("Pressure: " + String(p));
    return String(p);
  }
}

void getGasReference(){
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  Serial.println("#### Getting a new gas reference value");
  int readings = 5;
  for (int i = 1; i <= readings; i++){
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

String getIAQ(){
  //Calculate humidity contribution to IAQ index
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42) {
    hum_score = 0.25*100; // Humidity +/-5% around optimum
  } else { //sub-optimal
    if (current_humidity < 38) {
      hum_score = 0.25 / hum_reference * current_humidity * 100;
    } else {
      hum_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }

  //Calculate gas contribution to IAQ index
  float gas_lower_limit = 5000;   // Bad air quality limit
  float gas_upper_limit = 50000;  // Good air quality limit
  float gas_boundary = gas_upper_limit - gas_lower_limit;

  if (gas_reference > gas_upper_limit) {
      gas_reference = gas_upper_limit;
  }

  if (gas_reference < gas_lower_limit) {
      gas_reference = gas_lower_limit;
  }

  gas_score = (0.75 / gas_boundary * gas_reference - (gas_lower_limit * (0.75 / gas_boundary))) * 100;

  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float score = hum_score + gas_score;
  float air_quality_score = (100 - score) * 5;

  if ((++getgasreference_count) % 10 == 0) {
      getGasReference();
  }

  Serial.println("IAQ: " + String(air_quality_score));
  return String(air_quality_score);
}

void IRAM_ATTR resetSent() {
  portENTER_CRITICAL_ISR(&timerMux);
  sent_today = false;
  portEXIT_CRITICAL_ISR(&timerMux);

}

void setup() {
    Serial.begin(115200);

    Serial.println("#### Setting up Timer");
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &resetSent, true);
    timerAlarmWrite(timer, day, true);
    timerAlarmEnable(timer);

    Serial.println("#### Setting up Sensors");
    Wire.begin();
    if (!bme.begin()) {
        Serial.println("#### Could not find a valid BME680 sensor, check wiring!");
        while (1);
    } else {
        Serial.println("#### Found a sensor");
    }

    Serial.println("#### Calibrating Sensors");
    bme.setTemperatureOversampling(BME680_OS_2X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_2X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms
    // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
    getGasReference();


    Serial.print("#### Connecting to WiFi");
    WiFi.begin(SSID, PSK);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("#### WiFi connected.");
    Serial.println("#### IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.macAddress());


    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/html", INDEX_HTML);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", getTemperature().c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", getHumidity().c_str());
    });
    server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", getPressure().c_str());
    });
        server.on("/iaq", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", getIAQ().c_str());
    });

    Serial.println("#### Server Started.");
    server.begin();
}

void loop() {
    if(millis() - last_time > hour && !sent_today) {
      float iaq = getIAQ().toFloat();
      if (iaq > 50) {
        portENTER_CRITICAL(&timerMux);
        sent_today = true;
        portEXIT_CRITICAL(&timerMux);
        Serial.println("BAD QUALITY");

        http.begin(IFTTT);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST("{\"value1\":\"" + String(iaq) + "\"}");

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println(httpResponseCode);
          Serial.println(response);
        } else {
          Serial.print("Error on sending POST: ");
          Serial.println(httpResponseCode);
        }

        http.end();
      }
      last_time = millis();
    }
}
