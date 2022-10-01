/******* INCLUDES *******/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <EEPROM.h>

#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"

#include <homekit/types.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <arduino_homekit_server.h>

#include "bsec.h"

#include "creds.h"


/******* SERVER *******/
AsyncWebServer server(80);
String log_content = "";
uint8_t log_count = 0;

/******* HOMEKIT VARIABLES *******/
extern "C" homekit_server_config_t homekit_config;
extern "C" homekit_characteristic_t temperature_chara;
extern "C" homekit_characteristic_t humidity_chara;
extern "C" homekit_characteristic_t iaq_chara;
extern "C" homekit_characteristic_t co2_chara;
extern "C" homekit_characteristic_t voc_chara;


/******* SENSOR *******/
#define STATE_SAVE_PERIOD UINT32_C(360 * 60 * 1000)
Bsec sensor;
const uint8_t bsec_config_iaq[] = {
    #include "config.txt"
};
uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t state_update_counter = 0;
void check_IAQ_sensor_status(void);
void load_state(void);
void update_state(void);
void err_leds(void);


void log(String log_msg) {
    Serial.print(log_msg);

    if (log_count == 200) {
        log_content = "";
        log_count = 0;
    }

    log_content = log_content + log_msg;
    log_count++;
}


void check_IAQ_sensor_status(void) {
    if (sensor.status != BSEC_OK) {
        if (sensor.status < BSEC_OK) {
            log("#### BSEC error: " + String(sensor.status) + "\n");
            for (;;)
                err_leds();
        } else {
            log("#### BSEC warning: " + String(sensor.status) + "\n");
        }
    }

    if (sensor.bme680Status != BME680_OK) {
        if (sensor.bme680Status < BME680_OK) {
            log("#### BME680 error: " + String(sensor.bme680Status) + "\n");
            for (;;)
                err_leds();
        } else {
            log("#### BME680 warning: " + String(sensor.bme680Status) + "\n");
        }
    }
}


void load_state(void) {
    if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
        // Existing state in EEPROM
        log("Reading state from EEPROM");

        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
            bsec_state[i] = EEPROM.read(i + 1);
        }

        sensor.setState(bsec_state);
        check_IAQ_sensor_status();
    } else {
        // Erase the EEPROM with zeroes
        log("Erasing EEPROM");

        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++) {
            EEPROM.write(i, 0);
        }

        EEPROM.commit();
    }
}


void update_state(void) {
    bool update = false;
    if (state_update_counter == 0) {
        /* First state update when IAQ accuracy is >= 3 */
        if (sensor.iaqAccuracy >= 3) {
            update = true;
            state_update_counter++;
        }
    } else {
        /* Update every STATE_SAVE_PERIOD minutes */
        if ((state_update_counter * STATE_SAVE_PERIOD) < millis()) {
            update = true;
            state_update_counter++;
        }
    }

    if (update) {
        sensor.getState(bsec_state);
        check_IAQ_sensor_status();

        log("Writing state to EEPROM");

        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
            EEPROM.write(i + 1, bsec_state[i]);
        }

        EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
        EEPROM.commit();
    }
}


void err_leds(void) {
    log("Error...\n");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
}

void setup() {
    Serial.begin(115200);


    pinMode(LED_BUILTIN, OUTPUT);


    Serial.print("#### Connecting to WiFi");
    WiFi.begin(SSID, PSK);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA ,"HomeAtmo");


    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message = "No message sent";
        request->send(200, "text/plain", log_content);
    });

    server.begin();

    log("#### WiFi connected.\n");
    log("#### IP address: ");
    log(WiFi.localIP().toString());
    log("\n#### Hostname: ");
    log(WiFi.getHostname());
    log("\n#### Wi-Fi Connected.\n");


    log("#### Setting up sensor.\n");


    EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);


    Wire.begin();
    sensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
    check_IAQ_sensor_status();

    sensor.setConfig(bsec_config_iaq);
    check_IAQ_sensor_status();

    load_state();

    bsec_virtual_sensor_t sensor_list[5] = {
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
    };

    sensor.updateSubscription(sensor_list, 5, BSEC_SAMPLE_RATE_ULP);
    check_IAQ_sensor_status();

    log("#### Setting up HomeKit.\n");
    arduino_homekit_setup(&homekit_config);

    log("#### Everything set up\n");
}


void loop() {
    if (sensor.run()) {
        float tmp_temp = sensor.temperature;
        float tmp_humi = sensor.humidity;
        float tmp_co2 = sensor.co2Equivalent;
        float tmp_voc = sensor.breathVocEquivalent;
        float tmp_acc = sensor.iaqAccuracy;
        float tmp_iaq = sensor.iaq;

        temperature_chara.value.float_value = tmp_temp;
        homekit_characteristic_notify(&temperature_chara, temperature_chara.value);

        humidity_chara.value.float_value = tmp_humi;
        homekit_characteristic_notify(&humidity_chara, humidity_chara.value);

        co2_chara.value.float_value = tmp_co2;
        homekit_characteristic_notify(&co2_chara, co2_chara.value);

        voc_chara.value.float_value = tmp_voc;
        homekit_characteristic_notify(&voc_chara, voc_chara.value);

        iaq_chara.value.float_value = tmp_iaq;
        homekit_characteristic_notify(&iaq_chara, iaq_chara.value);


        log("\nTemperature: ");
        log(String(tmp_temp));

        log("\nHumidity: ");
        log(String(tmp_humi));

        log("\nCO2: ");
        log(String(tmp_co2));

        log("\nVOC: ");
        log(String(tmp_voc));

        log("\nIAQ RAW: ");
        log(String(tmp_iaq));

        log("\nIAQ: ");
        log(String(tmp_iaq));

        log("\nIAQ Acuuracy: ");
        log(String(tmp_acc));

        update_state();
    } else {
        check_IAQ_sensor_status();
    }
}
