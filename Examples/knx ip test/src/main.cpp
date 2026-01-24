#include <Arduino.h>
/*
 * This is an example showing a simple environment sensor based on a BME280 attached via I2C.
 * It shows, how the library can used to statically configure a device without a webserver for config.
 * This sketch was tested on a WeMos D1 mini
 */

#include <esp-knx-ip.h>

// WiFi config here
const char* ssid = "-------------";
const char* pass = "----------------------------";

#define LED_PIN 2
#define UPDATE_INTERVAL 10000

unsigned long next_change = 0;

float last_temp = 12.3;   // entspricht 12.0 °C
float last_hum = 13.4; 
float last_pres = 14.0;

// Group addresses to send to (5/5/10, 5/5/11 and 5/5/12)
address_t temp_ga = knx.GA_to_address(5, 5, 10);
address_t hum_ga = knx.GA_to_address(5, 5, 11);
address_t pres_ga = knx.GA_to_address(5, 5, 12);

void temp_cb(message_t const &msg, void *arg) {
  if (msg.ct == KNX_CT_READ) {
    knx.answer_2byte_float(msg.received_on, last_temp); // Sendet den aktuellen Float als Antwort (2 Byte DPT9)
  }
}

void hum_cb(message_t const &msg, void *arg) {
  if (msg.ct == KNX_CT_READ) {
    knx.answer_2byte_float(msg.received_on, last_hum);
  }
}

void pres_cb(message_t const &msg, void *arg) {
  if (msg.ct == KNX_CT_READ) {
      knx.answer_2byte_float(msg.received_on, last_pres);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);

  callback_id_t temp_cb_id = knx.callback_register("Read Temperature", temp_cb);
  callback_id_t hum_cb_id = knx.callback_register("Read Humidity", hum_cb);
  callback_id_t pres_cb_id = knx.callback_register("Read Pressure", pres_cb);

  // Assign callbacks to group addresses (5/5/20, 5/5/21, 5/5/22)
  knx.callback_assign(temp_cb_id, knx.GA_to_address(5, 5, 20));
  knx.callback_assign(hum_cb_id, knx.GA_to_address(5, 5, 21));
  knx.callback_assign(pres_cb_id, knx.GA_to_address(5, 5, 22));

  // Set physical address (1.1.1)
  knx.physical_address_set(knx.PA_to_address(1, 0, 101));

  // Do not call knx.load() for static config, it will try to load config from EEPROM which we don't have here

  // Init WiFi
  WiFi.hostname("env_knx_test");
  WiFi.begin(ssid, pass);

  Serial.println("");
  Serial.print("[Connecting]");
  Serial.print(ssid);

  digitalWrite(LED_PIN, LOW);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    Serial.print(".");
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
  digitalWrite(LED_PIN, HIGH);

  // Start knx, disable webserver by passing nullptr
  knx.start();

  Serial.println();
  Serial.println("Connected to wifi");
  Serial.println(WiFi.localIP());
}

void loop() {
  knx.loop();
  
  unsigned long now = millis();

  if (next_change < now)
  {
    next_change = now + UPDATE_INTERVAL;

    Serial.print("T: ");
    Serial.print(last_temp);
    Serial.print("°C H: ");
    Serial.print(last_hum);
    Serial.print("% P: ");
    Serial.print(last_pres);
    Serial.println("hPa");

    knx.write_2byte_float(temp_ga, last_temp);
    knx.write_2byte_float(hum_ga, last_hum);
    knx.write_2byte_float(pres_ga, last_pres);
  }

  delay(50);
}

