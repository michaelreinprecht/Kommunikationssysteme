#include "udp_communication.h"
#include <string.h>
#include <sys/param.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "data_structures.h"

// Protobuf includes
#include "message.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

// JSON includes
#include "cJSON.h"

// TLV Type definitions
#define TLV_VERSION 1
#define TLV_SEQ 2
#define TLV_TYPE 3
#define TLV_R 10
#define TLV_G 11
#define TLV_B 12
#define TLV_A 13
#define TLV_PRESSED 20

#define PORT 4444

static const char *TAG = "udp_layer";
static int udp_socket = -1; // Static variable for the UDP socket

// Initialize the UDP socket and bind to the specified port
int udp_init_socket(void)
{
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0)
    {
        ESP_LOGE("UDP", "Unable to create socket: errno %d", errno);
        return -1;
    }

    ESP_LOGI("UDP", "Socket created");

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int err = bind(udp_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0)
    {
        ESP_LOGE("UDP", "Socket unable to bind: errno %d", errno);
        close(udp_socket);
        udp_socket = -1;
        return -1;
    }

    ESP_LOGI("UDP", "Socket bound, port %d", PORT);
    return udp_socket;
}

// Receive a UDP packet and return its size
int udp_receive_packet_binary(message_t *out, struct sockaddr_storage *src)
{
    uint8_t raw[sizeof(message_t)];
    socklen_t socklen = sizeof(*src);

    int len = recvfrom(udp_socket, raw, sizeof(raw), 0, (struct sockaddr *)src, &socklen);
    if (len < 0)
    {
        ESP_LOGE(TAG, "recvfrom failed: %d", errno);
        return -1;
    }

    memcpy(out, raw, len);
    return len;
}

int udp_receive_packet_json(message_t *out, struct sockaddr_storage *src)
{
    char buffer[256];
    socklen_t socklen = sizeof(*src);

    int len = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0,
                       (struct sockaddr *)src, &socklen);

    if (len < 0)
        return -1;

    buffer[len] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root)
        return -1;

    out->header.version_number = cJSON_GetObjectItem(root, "version")->valueint;
    out->header.sequence_number = cJSON_GetObjectItem(root, "seq")->valueint;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *payload = cJSON_GetObjectItem(root, "payload");

    if (strcmp(type->valuestring, "color") == 0)
    {
        out->header.message_type = COLOR_COMMAND;
        out->payload.color.red = cJSON_GetObjectItem(payload, "r")->valueint;
        out->payload.color.green = cJSON_GetObjectItem(payload, "g")->valueint;
        out->payload.color.blue = cJSON_GetObjectItem(payload, "b")->valueint;
        out->payload.color.alpha = cJSON_GetObjectItem(payload, "a")->valueint;
    }
    else if (strcmp(type->valuestring, "pressed") == 0)
    {
        out->header.message_type = PRESSED_COMMAND;
        out->payload.pressed.pressed =
            cJSON_GetObjectItem(payload, "pressed")->valueint;
    }

    cJSON_Delete(root);
    return len;
}

int udp_receive_packet_tlv(message_t *out, struct sockaddr_storage *src)
{
    uint8_t buffer[64];
    socklen_t socklen = sizeof(*src);

    int len = recvfrom(udp_socket, buffer, sizeof(buffer), 0,
                       (struct sockaddr *)src, &socklen);

    if (len < 0)
        return -1;

    int i = 0;
    while (i < len)
    {
        uint8_t type = buffer[i++];
        uint8_t length = buffer[i++];

        switch (type)
        {
        case TLV_VERSION:
            out->header.version_number = buffer[i];
            break;
        case TLV_SEQ:
            out->header.sequence_number = buffer[i];
            break;
        case TLV_TYPE:
            out->header.message_type = buffer[i];
            break;
        case TLV_R:
            out->payload.color.red = buffer[i];
            break;
        case TLV_G:
            out->payload.color.green = buffer[i];
            break;
        case TLV_B:
            out->payload.color.blue = buffer[i];
            break;
        case TLV_A:
            out->payload.color.alpha = buffer[i];
            break;
        case TLV_PRESSED:
            out->payload.pressed.pressed = buffer[i];
            break;
        }

        i += length;
    }

    return len;
}

int udp_receive_packet_proto(message_t *out, struct sockaddr_storage *src)
{
    uint8_t raw[256];
    socklen_t socklen = sizeof(*src);

    int len = recvfrom(udp_socket, raw, sizeof(raw), 0, (struct sockaddr *)src, &socklen);
    if (len < 0)
    {
        ESP_LOGE(TAG, "recvfrom failed: %d", errno);
        return -1;
    }

    ProtoMessage msg = ProtoMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(raw, len);
    if (!pb_decode(&stream, ProtoMessage_fields, &msg))
    {
        ESP_LOGE(TAG, "Decode failed: %s", PB_GET_ERROR(&stream));
        return -1;
    }

    out->header.version_number = (uint8_t)msg.version;
    out->header.sequence_number = (uint8_t)msg.sequence_number;

    // Refill my local struct using the proto data (allows me to use common protocol logic
    // for different types of serialization ...)
    if (msg.which_payload == ProtoMessage_color_tag)
    {
        out->header.message_type = COLOR_COMMAND;
        out->header.length = sizeof(color_command_t);
        out->payload.color.red = (uint8_t)msg.payload.color.red;
        out->payload.color.green = (uint8_t)msg.payload.color.green;
        out->payload.color.blue = (uint8_t)msg.payload.color.blue;
        out->payload.color.alpha = (uint8_t)msg.payload.color.alpha;
    }
    else if (msg.which_payload == ProtoMessage_pressed_tag)
    {
        out->header.message_type = PRESSED_COMMAND;
        out->header.length = sizeof(pressed_command_t);
        out->payload.pressed.pressed = (uint8_t)msg.payload.pressed.pressed;
    }
    else
    {
        ESP_LOGE(TAG, "Unknown payload");
        return -1;
    }

    return len;
}

// Send a UDP packet to a specified destination address
int udp_send_packet_binary(const message_t *msg, const struct sockaddr_storage *dest)
{
    int data_size = sizeof(header_t) + msg->header.length;
    int err = sendto(udp_socket, msg, data_size, 0, (struct sockaddr *)dest, sizeof(*dest));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return -1;
    }
    return err;
}

int udp_send_packet_json(const message_t *msg, const struct sockaddr_storage *dest)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "version", msg->header.version_number);
    cJSON_AddNumberToObject(root, "seq", msg->header.sequence_number);

    if (msg->header.message_type == COLOR_COMMAND)
    {
        cJSON_AddStringToObject(root, "type", "color");

        cJSON *payload = cJSON_CreateObject();
        cJSON_AddNumberToObject(payload, "r", msg->payload.color.red);
        cJSON_AddNumberToObject(payload, "g", msg->payload.color.green);
        cJSON_AddNumberToObject(payload, "b", msg->payload.color.blue);
        cJSON_AddNumberToObject(payload, "a", msg->payload.color.alpha);

        cJSON_AddItemToObject(root, "payload", payload);
    }
    else if (msg->header.message_type == PRESSED_COMMAND)
    {
        cJSON_AddStringToObject(root, "type", "pressed");

        cJSON *payload = cJSON_CreateObject();
        cJSON_AddBoolToObject(payload, "pressed", msg->payload.pressed.pressed);

        cJSON_AddItemToObject(root, "payload", payload);
    }

    char *json_str = cJSON_PrintUnformatted(root);

    int err = sendto(udp_socket, json_str, strlen(json_str), 0,
                     (struct sockaddr *)dest, sizeof(*dest));

    cJSON_Delete(root);
    free(json_str);

    return err;
}

int udp_send_packet_tlv(const message_t *msg, const struct sockaddr_storage *dest)
{
    uint8_t buffer[64];
    int idx = 0;

    // version
    buffer[idx++] = TLV_VERSION;
    buffer[idx++] = 1;
    buffer[idx++] = msg->header.version_number;

    // seq
    buffer[idx++] = TLV_SEQ;
    buffer[idx++] = 1;
    buffer[idx++] = msg->header.sequence_number;

    // type
    buffer[idx++] = TLV_TYPE;
    buffer[idx++] = 1;
    buffer[idx++] = msg->header.message_type;

    if (msg->header.message_type == COLOR_COMMAND)
    {
        buffer[idx++] = TLV_R;
        buffer[idx++] = 1;
        buffer[idx++] = msg->payload.color.red;
        buffer[idx++] = TLV_G;
        buffer[idx++] = 1;
        buffer[idx++] = msg->payload.color.green;
        buffer[idx++] = TLV_B;
        buffer[idx++] = 1;
        buffer[idx++] = msg->payload.color.blue;
        buffer[idx++] = TLV_A;
        buffer[idx++] = 1;
        buffer[idx++] = msg->payload.color.alpha;
    }
    else if (msg->header.message_type == PRESSED_COMMAND)
    {
        buffer[idx++] = TLV_PRESSED;
        buffer[idx++] = 1;
        buffer[idx++] = msg->payload.pressed.pressed;
    }

    int err = sendto(udp_socket, buffer, idx, 0,
                     (struct sockaddr *)dest, sizeof(*dest));

    return err;
}

int udp_send_packet_proto(const message_t *msg, const struct sockaddr_storage *dest)
{
    ProtoMessage pb_msg = ProtoMessage_init_zero;
    pb_msg.version = msg->header.version_number;
    pb_msg.sequence_number = msg->header.sequence_number;

    if (msg->header.message_type == COLOR_COMMAND)
    {
        pb_msg.which_payload = ProtoMessage_color_tag;
        pb_msg.payload.color.red = msg->payload.color.red;
        pb_msg.payload.color.green = msg->payload.color.green;
        pb_msg.payload.color.blue = msg->payload.color.blue;
        pb_msg.payload.color.alpha = msg->payload.color.alpha;
    }
    else if (msg->header.message_type == PRESSED_COMMAND)
    {
        pb_msg.which_payload = ProtoMessage_pressed_tag;
        pb_msg.payload.pressed.pressed = msg->payload.pressed.pressed;
    }

    uint8_t buffer[ProtoMessage_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, ProtoMessage_fields, &pb_msg))
    {
        ESP_LOGE(TAG, "Encode failed: %s", PB_GET_ERROR(&stream));
        return -1;
    }

    int err = sendto(udp_socket, buffer, stream.bytes_written, 0, (struct sockaddr *)dest, sizeof(*dest));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
    }
    return err;
}

// Close the UDP socket
void udp_close_socket(void)
{
    if (udp_socket != -1)
    {
        close(udp_socket);
        udp_socket = -1;
        ESP_LOGI("UDP", "Socket closed");
    }
}