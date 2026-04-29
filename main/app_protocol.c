#include "app_protocol.h"
#include "udp_communication.h"
#include "data_structures.h"
#include "esp_log.h"
#include <string.h>

#include "led.h"

static const char *TAG = "app_protocol";
static uint8_t last_received_sequence_number = 255; // Global variable to store last received sequence number
static bool is_first_packet = true;

bool is_sequence_number_valid(uint8_t seq)
{
    if (is_first_packet)
    {
        is_first_packet = false;
        last_received_sequence_number = seq;
        return true;
    }

    // Get distance between seq numbers (works with wraparound)
    uint8_t distance = seq - last_received_sequence_number;

    if (distance == 0)
    {
        ESP_LOGI(TAG, "Duplicate packet seq=%d, dropping", seq);
        return false;
    }

    if (distance < 128)
    {
        last_received_sequence_number = seq;
        return true;
    }

    return false;
}

void handle_pressed_command(const pressed_command_t *cmd)
{
    ESP_LOGI(TAG, "Button: %s", cmd->pressed ? "PRESSED" : "RELEASED");
}

void handle_color_command(const color_command_t *cmd)
{
    ESP_LOGI(TAG, "Color -> R:%d G:%d B:%d A:%d", cmd->red, cmd->green, cmd->blue, cmd->alpha);
    led_set_color(cmd->red, cmd->green, cmd->blue);
}

void app_protocol_handle_message(struct sockaddr_storage *source_addr, message_t *msg)
{
    ESP_LOGI(TAG, "Received: Version=%d Type=%d Seq=%d Len=%d",
             msg->header.version_number,
             msg->header.message_type,
             msg->header.sequence_number,
             msg->header.length);

    if (!is_sequence_number_valid(msg->header.sequence_number))
    {
        ESP_LOGI(TAG, "Dropping out-of-order packet seq=%d (last accepted=%d)", msg->header.sequence_number, last_received_sequence_number);
        return;
    }

    switch (msg->header.message_type)
    {
    case COLOR_COMMAND:
        handle_color_command(&msg->payload.color);
        break;
    case PRESSED_COMMAND:
        handle_pressed_command(&msg->payload.pressed);
        break;
    default:
        ESP_LOGE(TAG, "Unknown message type %d", msg->header.message_type);
        break;
    }
}