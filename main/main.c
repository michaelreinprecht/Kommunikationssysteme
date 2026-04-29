#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/sockets.h"

#include "wifi_handler.h"
#include "udp_communication.h"
#include "app_protocol.h"
#include "data_structures.h"

#include "led.h"
#include <stdlib.h>
#include <time.h>

#include "esp_ieee802154.h"

// UDP Setup:
#define STARTUDP 0

#define PORT 4444

#define USEPROTO 1
#define USEJSON 0
#define USETLV 0

// IEEE802154 Setup:
#define STARTIEEE802154 1

#define IEEE802154_SENDER 1
#define IEEE802154_RECEIVER 1

typedef struct
{
        uint8_t payload[128];
        uint8_t length;
        int8_t rssi;
} ieee_packet_t;

// Create a queue handle
QueueHandle_t radio_rx_queue = NULL;

static TaskHandle_t receiver_task_handle = NULL;

void udp_sender_task(void *pvParameters)
{
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr("10.153.106.223");
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);

        uint8_t seqNum = 0;

        while (1)
        {
                message_t msg = {0};
                msg.header.version_number = 1;
                msg.header.message_type = COLOR_COMMAND;
                msg.header.sequence_number = seqNum++;
                msg.header.length = sizeof(color_command_t);
                msg.payload.color.red = rand() % 256;   // 0–255
                msg.payload.color.green = rand() % 256; // 0–255
                msg.payload.color.blue = rand() % 256;  // 0–255
                msg.payload.color.alpha = 255;

#if USEPROTO
                udp_send_packet_proto(&msg, (struct sockaddr_storage *)&dest_addr);
                ESP_LOGI("SENDER", "Sent proto packet seq=%d", msg.header.sequence_number);
#elif USEJSON
                udp_send_packet_json(&msg, (struct sockaddr_storage *)&dest_addr);
                ESP_LOGI("SENDER", "Sent json packet seq=%d", msg.header.sequence_number);
#elif USETLV
                udp_send_packet_tlv(&msg, (struct sockaddr_storage *)&dest_addr);
                ESP_LOGI("SENDER", "Sent tlv packet seq=%d", msg.header.sequence_number);
#endif

                vTaskDelay(pdMS_TO_TICKS(2000));
        }
}

void udp_receiver_task(void *pvParameters)
{
        struct sockaddr_storage source_addr;

        if (udp_init_socket() < 0)
        {
                ESP_LOGE("UDP", "Failed to init socket");
                vTaskDelete(NULL);
                return;
        }

        while (1)
        {
                message_t msg = {0};

#if USEPROTO
                int len = udp_receive_packet_proto(&msg, &source_addr);
#elif USEJSON
                int len = udp_receive_packet_json(&msg, &source_addr);
#elif USETLV
                int len = udp_receive_packet_tlv(&msg, &source_addr);
#endif

                if (len > 0)
                {
                        app_protocol_handle_message(&source_addr, &msg);
                }
        }
}

void ieee802154_receiver_task(void *pvParameters)
{
        ieee_packet_t rx_packet;

        esp_ieee802154_set_rx_when_idle(true);
        esp_ieee802154_receive();

        while (1)
        {
                if (xQueueReceive(radio_rx_queue, &rx_packet, portMAX_DELAY) == pdPASS)
                {
                        ESP_LOGI("RX_TASK", "Received %d bytes, RSSI: %d dBm", rx_packet.length, rx_packet.rssi);
                        esp_log_buffer_hex("PAYLOAD", rx_packet.payload, rx_packet.length);
                }
        }
}

// This is the ISR callback
void esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
        ieee_packet_t packet;

        packet.length = frame[0];
        packet.rssi = frame_info->rssi;

        uint8_t copy_len = (packet.length > 127) ? 127 : packet.length;
        memcpy(packet.payload, &frame[1], copy_len);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(radio_rx_queue, &packet, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken)
        {
                portYIELD_FROM_ISR();
        }

        esp_ieee802154_receive_handle_done(frame);
}

void ieee802154_sender_task(void *pvParameters)
{
        uint8_t frame[128];
        static uint8_t seq_num = 0;

        // payload
        const char *payload = "Hello";
        uint8_t payload_len = strlen(payload);
        frame[1] = 0x41;      // FCF Low Byte
        frame[2] = 0x88;      // FCF High Byte
        frame[3] = seq_num++; // Sequence Number
        // Destination PAN ID (42 -> 0x002A)
        frame[4] = 0x2A;
        frame[5] = 0x00;
        // Destination Address (Broadcast -> 0xFFFF)
        frame[6] = 0xFF;
        frame[7] = 0xFF;
        // Source Address (Short Address 42 -> 0x002A)
        frame[8] = 0x2A;
        frame[9] = 0x00;
        // Add Payload
        memcpy(&frame[10], payload, payload_len);
        // Set the Length
        frame[0] = 9 + payload_len + 2;

        esp_ieee802154_transmit(frame, true);
        vTaskDelay(pdMS_TO_TICKS(500));
        while (1)
        {
                ESP_LOGI("SENDER", "Sent frame.");
                esp_ieee802154_transmit(frame, true);
                vTaskDelay(pdMS_TO_TICKS(2000));
        }
}

void start_udp_setup()
{
        srand(time(NULL));
        led_init();

        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        wifi_init_static_ip();
        wifi_wait_connected();

        esp_ieee802154_enable();

        xTaskCreate(udp_receiver_task, "udp_receiver", 4096, NULL, 5, NULL);
        xTaskCreate(udp_sender_task, "udp_sender", 4096, NULL, 5, NULL);
}

void start_ieee802154_setup()
{
        ESP_ERROR_CHECK(nvs_flash_init());
        esp_ieee802154_enable();
        esp_ieee802154_set_channel(15);
        esp_ieee802154_set_panid(42);
        uint16_t short_address = 42;
        esp_ieee802154_set_short_address(short_address);
        esp_ieee802154_set_txpower(10);
        esp_ieee802154_set_promiscuous(false);

        radio_rx_queue = xQueueCreate(10, sizeof(ieee_packet_t));

#if IEEE802154_SENDER
        // xTaskCreate(ieee802154_sender_task, "ieee802154_sender", 4096, NULL, 5, NULL);
#endif
#if IEEE802154_RECEIVER
        xTaskCreate(ieee802154_receiver_task, "ieee802154_receiver", 4096, NULL, 5, NULL);
#endif
}

void app_main(void)
{
#if STARTUDP
        start_udp_setup();
#elif STARTIEEE802154
        start_ieee802154_setup();
#endif

        while (1)
        {
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
}
