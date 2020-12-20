/******* INCLUDES *******/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include <homekit/types.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <arduino_homekit_server.h>

#include "bsec.h"
#include "creds.h"


/******* TIMER *******/
uint64_t second = 1000;


/******* SENSOR *******/
Bsec sensor;
void check_IAQ_sensor_status(void);


/******* HOMEKIT VARIABLES *******/
extern "C" homekit_server_config_t homekit_config;
extern "C" homekit_characteristic_t temperature_chara;
extern "C" homekit_characteristic_t humidity_chara;
extern "C" homekit_characteristic_t iaq_chara;
extern "C" homekit_characteristic_t co2_chara;
extern "C" homekit_characteristic_t voc_chara;


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

    Wire.begin();
    sensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
    check_IAQ_sensor_status();

    bsec_virtual_sensor_t sensor_list[5] = {
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
    };

    sensor.updateSubscription(sensor_list, 5, BSEC_SAMPLE_RATE_LP);
    check_IAQ_sensor_status();

    Serial.print("#### Connecting to WiFi");
    WiFi.begin(SSID, PSK);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA ,"HomeAtmo");
    Serial.println("#### WiFi connected.");
    Serial.print("#### IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("#### Hostname: ");
    Serial.println(WiFi.getHostname());
    Serial.println("#### Wi-Fi Connected.");

    

    Serial.println("#### Setting up HomeKit.");
    arduino_homekit_setup(&homekit_config);
    Serial.println("#### Everything set up");
}


void loop() {
    if (sensor.run()) {
        Serial.print("Temperature: ");
        Serial.println(sensor.temperature);
        temperature_chara.value.float_value = sensor.temperature;
        homekit_characteristic_notify(&temperature_chara, temperature_chara.value);

        Serial.print("Humidity: ");
        Serial.println(sensor.humidity);
        humidity_chara.value.float_value = sensor.humidity;
        homekit_characteristic_notify(&humidity_chara, humidity_chara.value);

        Serial.print("CO2: ");
        Serial.println(sensor.co2Equivalent);
        co2_chara.value.float_value = sensor.co2Equivalent;
        homekit_characteristic_notify(&co2_chara, co2_chara.value);

        Serial.print("VOC: ");
        Serial.println(sensor.breathVocEquivalent);
        voc_chara.value.float_value = sensor.breathVocEquivalent;
        homekit_characteristic_notify(&voc_chara, voc_chara.value);

        Serial.print("IAQ: ");
        Serial.println(sensor.staticIaq);
        float air_quality = 1 + (5 - 1) * ((sensor.staticIaq - 0)/(500 - 0));
        iaq_chara.value.int_value = (int)air_quality;
        homekit_characteristic_notify(&iaq_chara, iaq_chara.value);
    } else {
        check_IAQ_sensor_status();
    }

    vTaskDelay((second * 5) / portTICK_PERIOD_MS);
}
