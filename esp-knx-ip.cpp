/**
 * esp-knx-ip library for KNX/IP communication on an ESP8266/ESP32
 * Author: Nico Weichbrodt <envy>
 * License: MIT
 */

#include "esp-knx-ip.h"

ESPKNXIP::ESPKNXIP() : registered_callback_assignments(0), registered_callbacks(0), registered_configs(0), registered_feedbacks(0)
{
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("ESPKNXIP starting up");

    // Default physical address 1.1.0
    physaddr.bytes.high = (1 << 4) | 1; // area 1, line 1
    physaddr.bytes.low = 0;             // member 0

    memset(callback_assignments, 0, MAX_CALLBACK_ASSIGNMENTS * sizeof(callback_assignment_t));
    memset(callbacks, 0, MAX_CALLBACKS * sizeof(callback_t));
    memset(custom_config_data, 0, MAX_CONFIG_SPACE * sizeof(uint8_t));
    memset(custom_config_default_data, 0, MAX_CONFIG_SPACE * sizeof(uint8_t));
    memset(custom_configs, 0, MAX_CONFIGS * sizeof(config_t));
}

void ESPKNXIP::load()
{
    memcpy(custom_config_default_data, custom_config_data, MAX_CONFIG_SPACE);
    EEPROM.begin(EEPROM_SIZE);
    restore_from_eeprom();
}

void ESPKNXIP::start()
{
    __start();
}

void ESPKNXIP::__start()
{
    // Webserver komplett entfernt

    #ifdef ESP32
        udp.beginMulticast(MULTICAST_IP, MULTICAST_PORT);
    #else
        udp.beginMulticast(WiFi.localIP(), MULTICAST_IP, MULTICAST_PORT);
    #endif
}

void ESPKNXIP::save_to_eeprom()
{
    uint32_t address = 0;
    uint64_t magic = EEPROM_MAGIC;
    EEPROM.put(address, magic);
    address += sizeof(uint64_t);

    EEPROM.put(address++, registered_callback_assignments);

    for (uint8_t i = 0; i < MAX_CALLBACK_ASSIGNMENTS; ++i)
    {
        EEPROM.put(address, callback_assignments[i].address);
        address += sizeof(address_t);
    }

    for (uint8_t i = 0; i < MAX_CALLBACK_ASSIGNMENTS; ++i)
    {
        EEPROM.put(address, callback_assignments[i].callback_id);
        address += sizeof(callback_id_t);
    }

    EEPROM.put(address, physaddr);
    address += sizeof(address_t);

    EEPROM.put(address, custom_config_data);
    address += sizeof(custom_config_data);

    EEPROM.commit();
    DEBUG_PRINT("Wrote to EEPROM: 0x");
    DEBUG_PRINTLN(address, HEX);
}

void ESPKNXIP::restore_from_eeprom()
{
    uint32_t address = 0;
    uint64_t magic = 0;
    EEPROM.get(address, magic);
    if (magic != EEPROM_MAGIC)
    {
        DEBUG_PRINTLN("No valid magic in EEPROM, aborting restore.");
        return;
    }

    address += sizeof(uint64_t);
    EEPROM.get(address++, registered_callback_assignments);

    for (uint8_t i = 0; i < MAX_CALLBACK_ASSIGNMENTS; ++i)
    {
        EEPROM.get(address, callback_assignments[i].address);
        address += sizeof(address_t);
    }

    for (uint8_t i = 0; i < MAX_CALLBACK_ASSIGNMENTS; ++i)
    {
        EEPROM.get(address, callback_assignments[i].callback_id);
        address += sizeof(callback_id_t);
    }

    EEPROM.get(address, physaddr);
    address += sizeof(address_t);

    uint32_t conf_offset = address;
    for (uint8_t i = 0; i < registered_configs; ++i)
    {
        config_flags_t flags = (config_flags_t)EEPROM.read(address);
        DEBUG_PRINT("Flag in EEPROM @ ");
        DEBUG_PRINT(address - conf_offset);
        DEBUG_PRINT(": ");
        DEBUG_PRINTLN(flags, BIN);

        custom_config_data[custom_configs[i].offset] = flags;
        if (flags & CONFIG_FLAGS_VALUE_SET)
        {
            DEBUG_PRINTLN("Non-default value");
            for (int j = 0; j < custom_configs[i].len - sizeof(uint8_t); ++j)
            {
                custom_config_data[custom_configs[i].offset + sizeof(uint8_t) + j] =
                    EEPROM.read(address + sizeof(uint8_t) + j);
            }
        }

        address += custom_configs[i].len;
    }

    DEBUG_PRINT("Restored from EEPROM: 0x");
    DEBUG_PRINTLN(address, HEX);
}

uint16_t ESPKNXIP::__ntohs(uint16_t n)
{
    return (uint16_t)((((uint8_t*)&n)[0] << 8) | (((uint8_t*)&n)[1]));
}

/**
 * Callback-Methoden
 */
callback_id_t ESPKNXIP::callback_register(String name, callback_fptr_t cb, void *arg, enable_condition_t cond)
{
    if (registered_callbacks >= MAX_CALLBACKS)
        return -1;

    callback_id_t id = registered_callbacks;

    callbacks[id].name = name;
    callbacks[id].fkt = cb;
    callbacks[id].cond = cond;
    callbacks[id].arg = arg;
    registered_callbacks++;
    return id;
}

void ESPKNXIP::callback_assign(callback_id_t id, address_t val)
{
    if (id >= registered_callbacks)
        return;

    __callback_register_assignment(val, id);
}

callback_assignment_id_t ESPKNXIP::__callback_register_assignment(address_t address, callback_id_t id)
{
    if (registered_callback_assignments >= MAX_CALLBACK_ASSIGNMENTS)
        return -1;

    callback_assignment_id_t aid = registered_callback_assignments;
    callback_assignments[aid].address = address;
    callback_assignments[aid].callback_id = id;
    registered_callback_assignments++;
    return aid;
}

void ESPKNXIP::__callback_delete_assignment(callback_assignment_id_t id)
{
    if (id >= registered_callback_assignments)
        return;

    uint32_t dest_offset = 0;
    uint32_t src_offset = 0;
    uint32_t len = 0;

    if (id == 0)
    {
        src_offset = 1;
        len = (registered_callback_assignments - 1);
    }
    else if (id == registered_callback_assignments - 1)
    {
        // last element, just decrement
    }
    else
    {
        dest_offset = id;
        src_offset = dest_offset + 1;
        len = (registered_callback_assignments - 1 - id);
    }

    if (len > 0)
        memmove(callback_assignments + dest_offset, callback_assignments + src_offset, len * sizeof(callback_assignment_t));

    registered_callback_assignments--;
}

/**
 * Feedback-Methoden
 */
feedback_id_t ESPKNXIP::feedback_register_int(String name, int32_t *value, enable_condition_t cond)
{
    if (registered_feedbacks >= MAX_FEEDBACKS)
        return -1;

    feedback_id_t id = registered_feedbacks;
    feedbacks[id].type = FEEDBACK_TYPE_INT;
    feedbacks[id].name = name;
    feedbacks[id].cond = cond;
    feedbacks[id].data = (void *)value;

    registered_feedbacks++;
    return id;
}

feedback_id_t ESPKNXIP::feedback_register_float(String name, float *value, uint8_t precision, enable_condition_t cond)
{
    if (registered_feedbacks >= MAX_FEEDBACKS)
        return -1;

    feedback_id_t id = registered_feedbacks;
    feedbacks[id].type = FEEDBACK_TYPE_FLOAT;
    feedbacks[id].name = name;
    feedbacks[id].cond = cond;
    feedbacks[id].data = (void *)value;
    feedbacks[id].options.float_options.precision = precision;

    registered_feedbacks++;
    return id;
}

feedback_id_t ESPKNXIP::feedback_register_bool(String name, bool *value, enable_condition_t cond)
{
    if (registered_feedbacks >= MAX_FEEDBACKS)
        return -1;

    feedback_id_t id = registered_feedbacks;
    feedbacks[id].type = FEEDBACK_TYPE_BOOL;
    feedbacks[id].name = name;
    feedbacks[id].cond = cond;
    feedbacks[id].data = (void *)value;

    registered_feedbacks++;
    return id;
}

feedback_id_t ESPKNXIP::feedback_register_action(String name, feedback_action_fptr_t value, void *arg, enable_condition_t cond)
{
    if (registered_feedbacks >= MAX_FEEDBACKS)
        return -1;

    feedback_id_t id = registered_feedbacks;
    feedbacks[id].type = FEEDBACK_TYPE_ACTION;
    feedbacks[id].name = name;
    feedbacks[id].cond = cond;
    feedbacks[id].data = (void *)value;
    feedbacks[id].options.action_options.arg = arg;

    registered_feedbacks++;
    return id;
}

/**
 * KNX-Loop
 */
void ESPKNXIP::loop()
{
    __loop_knx();
}

void ESPKNXIP::__loop_knx()
{
    int read = udp.parsePacket();
    if (!read)
        return;

    uint8_t buf[read];
    udp.read(buf, read);
    udp.flush();

    knx_ip_pkt_t *knx_pkt = (knx_ip_pkt_t *)buf;

    if (knx_pkt->header_len != 0x06 && knx_pkt->protocol_version != 0x10 && knx_pkt->service_type != KNX_ST_ROUTING_INDICATION)
        return;

    cemi_msg_t *cemi_msg = (cemi_msg_t *)knx_pkt->pkt_data;
    if (cemi_msg->message_code != KNX_MT_L_DATA_IND)
        return;

    cemi_service_t *cemi_data = &cemi_msg->data.service_information;
    if (cemi_msg->additional_info_len > 0)
        cemi_data = (cemi_service_t *)(((uint8_t *)cemi_data) + cemi_msg->additional_info_len);

    if (cemi_data->control_2.bits.dest_addr_type != 0x01)
        return;

    knx_command_type_t ct = (knx_command_type_t)(((cemi_data->data[0] & 0xC0) >> 6) | ((cemi_data->pci.apci & 0x03) << 2));

    // Call callbacks
    for (int i = 0; i < registered_callback_assignments; ++i)
    {
        if (cemi_data->destination.value == callback_assignments[i].address.value)
        {
            if (callbacks[callback_assignments[i].callback_id].cond &&
                !callbacks[callback_assignments[i].callback_id].cond())
            {
#if ALLOW_MULTIPLE_CALLBACKS_PER_ADDRESS
                continue;
#else
                return;
#endif
            }
            uint8_t data[cemi_data->data_len];
            memcpy(data, cemi_data->data, cemi_data->data_len);
            data[0] = data[0] & 0x3F;

            message_t msg = {};
            msg.ct = ct;
            msg.received_on = cemi_data->destination;
            msg.data_len = cemi_data->data_len;
            msg.data = data;

            callbacks[callback_assignments[i].callback_id].fkt(msg,
                                                              callbacks[callback_assignments[i].callback_id].arg);
#if ALLOW_MULTIPLE_CALLBACKS_PER_ADDRESS
            continue;
#else
            return;
#endif
        }
    }
}

// Global "singleton" object
ESPKNXIP knx;
