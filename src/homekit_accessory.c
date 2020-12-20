#include <Arduino.h>
#include <homekit/types.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "creds.h"

#define TMP_IDENTIFIER ("Temperatur")
#define PRS_IDENTIFIER ("Luftdruck")
#define HUM_IDENTIFIER ("Luftfeuchtigkeit")
#define IAQ_IDENTIFIER ("Luftqualität")
#define CO2_IDENTIFIER ("CO2-Äquivalent")

homekit_characteristic_t temperature_chara = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0.0);
homekit_characteristic_t humidity_chara    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0.0);
homekit_characteristic_t iaq_chara    = HOMEKIT_CHARACTERISTIC_(AIR_QUALITY, 0);
homekit_characteristic_t co2_chara    = HOMEKIT_CHARACTERISTIC_(CARBON_DIOXIDE_LEVEL, 0.0);
homekit_characteristic_t voc_chara    = HOMEKIT_CHARACTERISTIC_(VOC_DENSITY, 0.0);

void sensor_identify(homekit_value_t _value) {
	printf("accessory identify");
}

// Accessory list to expose via HomeKit
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, ACCESSORY_MANUFACTURER),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, ACCESSORY_SN),
            HOMEKIT_CHARACTERISTIC(MODEL, ACCESSORY_MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, TMP_IDENTIFIER),
            &temperature_chara,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, HUM_IDENTIFIER),
            &humidity_chara,
            NULL
        }),
        HOMEKIT_SERVICE(AIR_QUALITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, IAQ_IDENTIFIER),
            &iaq_chara,
            &co2_chara,
            &voc_chara,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t homekit_config = {
    .accessories = accessories,
    .password = HOMEKIT_PASSWORD
};