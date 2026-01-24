/**
 * esp-knx-ip library for KNX/IP communication on an ESP8266
 * Author: Nico Weichbrodt <envy>
 * License: MIT
 */

#include "esp-knx-ip.h"

/**
 * Conversion functions
 */

bool ESPKNXIP::data_to_bool(uint8_t *data)
{
	return (data[0] & 0x01) == 1 ? true : false;
}

int8_t ESPKNXIP::data_to_1byte_int(uint8_t *data)
{
	return (int8_t)data[1];
}

uint8_t ESPKNXIP::data_to_1byte_uint(uint8_t *data)
{
	return data[1];
}

int16_t ESPKNXIP::data_to_2byte_int(uint8_t *data)
{
	return (int16_t)((data[1] << 8) | data[2]);
}

uint16_t ESPKNXIP::data_to_2byte_uint(uint8_t *data)
{
	return (uint16_t)((data[1] << 8) | data[2]);
}

float ESPKNXIP::data_to_2byte_float(uint8_t *data)
{
	//uint8_t  sign = (data[1] & 0b10000000) >> 7;
	uint8_t  expo = (data[1] & 0b01111000) >> 3;
	int16_t mant = ((data[1] & 0b10000111) << 8) | data[2];
	return 0.01f * mant * pow(2, expo);
    // KNX DPT9: data[0] = MSB, data[1] = LSB
    uint8_t msb = data[0];
    uint8_t lsb = data[1];

    // Sign-Bit (Bit 7 MSB)
    int sign = (msb & 0x80) ? -1 : 1;

    // Exponent (4 Bit, Bits 3–6 MSB)
    int exponent = (msb >> 3) & 0x0F;

    // Mantisse (11 Bit, Bits 0–2 MSB + LSB)
    int16_t mantissa = ((msb & 0x07) << 8) | lsb;

    // Negative Mantisse im 2er-Komplement korrigieren
    if (mantissa & 0x0400) // Bit 10 = Vorzeichen in Mantisse
        mantissa |= 0xF800; // Vorzeichen erweitern

    // Wert berechnen (0,01 * Mantisse * 2^Exponent) und Sign anwenden
    return sign * 0.01f * mantissa * powf(2, exponent);
}

time_of_day_t ESPKNXIP::data_to_3byte_time(uint8_t *data)
{
	time_of_day_t time;
	time.weekday = (weekday_t)((data[1] & 0b11100000) >> 5);
	time.hours = (data[1] & 0b00011111);
	time.minutes = (data[2] & 0b00111111);
	time.seconds = (data[3] & 0b00111111);
	return time;
}

date_t ESPKNXIP::data_to_3byte_data(uint8_t *data)
{
	date_t date;
	date.day = (data[1] & 0b00011111);
	date.month = (data[2] & 0b00001111);
	date.year = (data[3] & 0b01111111);
	return date;
}

color_t ESPKNXIP::data_to_3byte_color(uint8_t *data)
{
	color_t color;
	color.red = data[1];
	color.green = data[2];
	color.blue = data[3];
	return color;
}

int32_t ESPKNXIP::data_to_4byte_int(uint8_t *data)
{
	return (int32_t)((data[1] << 24) | (data[2] << 16) | (data[3] << 8) | (data[4] << 0));
}

uint32_t ESPKNXIP::data_to_4byte_uint(uint8_t *data)
{
	return (uint32_t)((data[1] << 24) | (data[2] << 16) | (data[3] << 8) | (data[4] << 0));
}

float ESPKNXIP::data_to_4byte_float(uint8_t *data)
{
	return (float)((data[1] << 24) | (data[2] << 16) | (data[3] << 8) |data[4]);
}