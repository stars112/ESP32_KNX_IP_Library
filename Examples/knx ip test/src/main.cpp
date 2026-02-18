#include <Arduino.h>
#include <WiFi.h>
#include <esp-knx-ip.h>

// -------------------- WLAN --------------------
const char* ssid = "xxxxx";
const char* pass = "xxxxx";

// -------------------- Hardware --------------------
#define LED_PIN 2
#define UPDATE_INTERVAL 10000

unsigned long next_change = 0;
float last_temp = 21.5;

// -------------------- Dynamische GA Struktur --------------------
struct DynGA {
    address_t ga;
    String name;
};

#define MAX_DYNAMIC_GAS 10
DynGA dynamic_gas[MAX_DYNAMIC_GAS];
uint8_t dynamic_ga_count = 0;

// ---------------------------------------------------------------
// Hilfsfunktion: GA in 5/5/10 Format (korrekt)
// ---------------------------------------------------------------
String ga_to_string(address_t ga)
{
    return String(ga.ga.area) + "/" + String(ga.ga.line) + "/" + String(ga.ga.member);
}

// ---------------------------------------------------------------
// GA suchen
// ---------------------------------------------------------------
int8_t find_ga_index(String name)
{
    for (uint8_t i = 0; i < dynamic_ga_count; i++)
        if (dynamic_gas[i].name == name) return i;
    return -1;
}

address_t* get_ga(String name)
{
    int8_t idx = find_ga_index(name);
    if (idx >= 0) return &dynamic_gas[idx].ga;
    return nullptr;
}

// ---------------------------------------------------------------
// GA hinzufügen oder aktualisieren
// ---------------------------------------------------------------
void register_or_update_ga(String name, address_t new_ga)
{
    int8_t idx = find_ga_index(name);

    if (idx >= 0)
    {
        dynamic_gas[idx].ga = new_ga;
        knx.callback_assign(0, new_ga); // überschreibt alte GA
        Serial.print("GA aktualisiert: ");
        Serial.print(name);
        Serial.print(" -> ");
        Serial.println(ga_to_string(new_ga));
        return;
    }

    if (dynamic_ga_count < MAX_DYNAMIC_GAS)
    {
        dynamic_gas[dynamic_ga_count].name = name;
        dynamic_gas[dynamic_ga_count].ga = new_ga;

        knx.callback_assign(0, new_ga);

        Serial.print("GA hinzugefügt: ");
        Serial.print(name);
        Serial.print(" -> ");
        Serial.println(ga_to_string(new_ga));

        dynamic_ga_count++;
    }
    else
        Serial.println("Maximale Anzahl dynamischer GAs erreicht!");
}

// ---------------------------------------------------------------
// GA entfernen (nur aus Array)
void remove_ga(String name)
{
    int8_t idx = find_ga_index(name);
    if (idx < 0)
    {
        Serial.println("GA nicht gefunden.");
        return;
    }

    Serial.print("GA entfernt: ");
    Serial.print(name);
    Serial.print(" (");
    Serial.print(ga_to_string(dynamic_gas[idx].ga));
    Serial.println(")");

    for (uint8_t j = idx; j < dynamic_ga_count - 1; j++)
        dynamic_gas[j] = dynamic_gas[j + 1];

    dynamic_ga_count--;
}

// ---------------------------------------------------------------
// Einheitlicher KNX Callback
// ---------------------------------------------------------------
void knx_callback(message_t const &msg, void *arg)
{
    String ga_str = ga_to_string(msg.received_on);
    Serial.print("\n[KNX] Telegramm auf GA: ");
    Serial.print(ga_str);
    Serial.print(" | CT=");
    Serial.println(msg.ct);

    for (uint8_t i = 0; i < dynamic_ga_count; i++)
    {
        if (dynamic_gas[i].ga.value == msg.received_on.value)
        {
            String name = dynamic_gas[i].name;

            // -------- TEMP --------
            if (name == "temp")
            {
                if (msg.ct == KNX_CT_READ)
                    knx.answer_2byte_float(msg.received_on, last_temp);
                else if (msg.ct == KNX_CT_WRITE)
                {
                    last_temp = knx.data_to_2byte_float(msg.data);
                    Serial.print("-> WRITE Temp: ");
                    Serial.println(last_temp);
                }
            }
            // -------- SWITCH --------
            else if (name == "switch")
            {
                if (msg.ct == KNX_CT_WRITE)
                {
                    bool state = knx.data_to_bool(msg.data);
                    digitalWrite(LED_PIN, state ? HIGH : LOW);
                    Serial.print("-> WRITE Switch: ");
                    Serial.println(state);
                }
            }
            // -------- EXTERNE ABFRAGE --------
            else if (name == "ext_abfrage")
            {
                if (msg.ct == KNX_CT_ANSWER)
                {
                    float val = knx.data_to_2byte_float(msg.data);
                    Serial.print("-> Antwort ext_abfrage: ");
                    Serial.println(val);
                }
            }

            break; // gefunden → Callback erledigt
        }
    }
}

void send_read_2byte_float(address_t ga)
{
    // L-Data READ ist 0 Byte Payload
    knx.send(ga, KNX_CT_READ, 0, nullptr);
    Serial.print("Ext. Abfrage (READ) gesendet an GA: ");
    Serial.println(ga_to_string(ga));
}

// ---------------------------------------------------------------
// Setup
// ---------------------------------------------------------------
void setup()
{
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);

    // KNX Callback registrieren
    knx.callback_register("Unified Callback", knx_callback);

    // Physical Address
    knx.physical_address_set(knx.PA_to_address(1,0,101));

    // WLAN Verbindung
    WiFi.hostname("ESP_KNX_TEST");
    WiFi.begin(ssid, pass);

    Serial.println("Verbinde WLAN...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        Serial.print(".");
    }
    Serial.println("\nWiFi verbunden!");
    Serial.println(WiFi.localIP());

    knx.start();

    // Dynamische GAs registrieren
    register_or_update_ga("temp", knx.GA_to_address(5,5,10));
    register_or_update_ga("switch", knx.GA_to_address(5,5,11));
    register_or_update_ga("ext_abfrage", knx.GA_to_address(5,1,16));
}

// ---------------------------------------------------------------
// Loop
// ---------------------------------------------------------------
void loop()
{
    knx.loop();
    unsigned long now = millis();

    // -------- Temp regelmäßig senden --------
    if (now > next_change)
    {
        next_change = now + UPDATE_INTERVAL;
        address_t* temp_ga = get_ga("temp");
        if (temp_ga != nullptr)
        {
            knx.write_2byte_float(*temp_ga, last_temp);
            Serial.print("Sende Temp auf ");
            Serial.print(ga_to_string(*temp_ga));
            Serial.print(" = ");
            Serial.println(last_temp);
        }
    }

    // -------- Test: nach 30s GA ändern --------
    static bool changed = false;
    if (!changed && now > 30000)
    {
        register_or_update_ga("temp", knx.GA_to_address(5,5,20));
        changed = true;
    }

    // -------- Test: nach 60s GA entfernen --------
    static bool removed = false;
    if (!removed && now > 60000)
    {
        remove_ga("temp");
        removed = true;
    }

    // -------- Test: extern abfragen alle 15s --------
    static unsigned long next_ext_read = 0;
    if (now > next_ext_read)
    {
        next_ext_read = now + 15000; // alle 15 Sekunden
        address_t* ext_ga = get_ga("ext_abfrage");
        if (ext_ga != nullptr)
        {
            knx.send_ext(*ext_ga); // nur die Adresse, kein CT oder Daten nötig
        }

    }

    delay(50);
}
