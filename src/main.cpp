/******* INCLUDES *******/
#include <Arduino.h>

#include <EEPROM.h>
#include <Wire.h>
#include "bsec.h"

#include "WiFi.h"
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <movingAvg.h>

#include "html.h"
#include "creds.h"

/******* TIMER VARIABLES *******/
uint64_t hour = 1000 * 60 * 60;
uint64_t minute = 1000 * 60;
int last_check = 0;
uint64_t last_ifttt = 0;

/******* WI-FI VARIABLES *******/
AsyncWebServer server(80);
HTTPClient http;

/******* SENSOR VARIABLES *******/
const uint8_t bsec_config_iaq[] = {
    #include "bsec_iaq.txt"
};

#define STATE_SAVE_PERIOD UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day

Bsec sensor;
uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t state_update_counter = 0;

movingAvg iaq_avg(6);

void check_IAQ_sensor_status(void);
void load_state(void);
void update_state(void);

float temperature = 0.0;
float pressure = 0.0;
float humidity = 0.0;
float iaq = 0.0;
float co2Equivalent = 0.0;

void load_state(void) {
    if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
        Serial.println("#### Reading state from EEPROM:");

        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
            bsec_state[i] = EEPROM.read(i + 1);
            Serial.print(bsec_state[i], HEX);
        }
        Serial.println("");

        sensor.setState(bsec_state);
        check_IAQ_sensor_status();
    } else {
        Serial.println("#### Erasing EEPROM");

        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++) {
            EEPROM.write(i, 0);
        }

        EEPROM.commit();
    }
}

void update_state(void) {
    bool update = false;
    if (state_update_counter == 0) {
        if (sensor.iaqAccuracy >= 3) {
            update = true;
            state_update_counter++;
        }
    } else {
        if ((state_update_counter * STATE_SAVE_PERIOD) < millis()) {
            update = true;
            state_update_counter++;
        }
    }

    if (update) {
        sensor.getState(bsec_state);
        check_IAQ_sensor_status();

        Serial.println("#### Writing state to EEPROM:");
        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
            EEPROM.write(i + 1, bsec_state[i]);
            Serial.println(bsec_state[i], HEX);
        }
        Serial.println("");

        EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
        EEPROM.commit();
    }
}

void check_IAQ_sensor_status(void) {
    if (sensor.status != BSEC_OK) {
        if (sensor.status < BSEC_OK) {
            Serial.println("#### BSEC error: " + String(sensor.status));
            for (;;)
                ;
        } else {
            Serial.println("#### BSEC warning: " + String(sensor.status));
        }
    }

    if (sensor.bme680Status != BME680_OK) {
        if (sensor.bme680Status < BME680_OK) {
            Serial.println("#### BME680 error: " + String(sensor.bme680Status));
            for (;;)
                ;
        } else {
            Serial.println("#### BME680 warning: " + String(sensor.bme680Status));
        }
    }
}

void setup() {
    Serial.begin(115200);

    Serial.println("#### Setting up sensor.");
    EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);

    Wire.begin();
    sensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);

    check_IAQ_sensor_status();
    sensor.setConfig(bsec_config_iaq);
    check_IAQ_sensor_status();

    load_state();

    bsec_virtual_sensor_t sensor_list[5] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_RAW_PRESSURE
    };

    sensor.updateSubscription(sensor_list, 5, BSEC_SAMPLE_RATE_LP);
    check_IAQ_sensor_status();

    iaq_avg.begin();

    Serial.print("#### Connecting to WiFi");
    WiFi.begin(SSID, PSK);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA ,"HomeAtmo");
    Serial.println("#### WiFi connected.");
    Serial.println("#### IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("#### Hostname: ");
    Serial.println(WiFi.getHostname());

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", INDEX_HTML);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(temperature).c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(humidity).c_str());
    });
    server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(pressure / 100.0F).c_str());
    });
    server.on("/iaq", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(iaq).c_str());
    });
    server.on("/co2", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/plain", String(co2Equivalent).c_str());
    });

    server.begin();
    Serial.println("#### Server Started.");
}

void loop() {
    if (millis() - last_check > hour) {
        if (iaq > 100) {
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
        last_check = millis();
    }

    if (sensor.run()) {
        temperature = sensor.temperature;
        pressure = sensor.pressure;
        humidity = sensor.humidity;
        co2Equivalent = sensor.co2Equivalent;
        iaq = iaq_avg.reading(sensor.iaq);

        update_state();
    } else {
        check_IAQ_sensor_status();
    }

    vTaskDelay((minute * 5) / portTICK_PERIOD_MS);
}
