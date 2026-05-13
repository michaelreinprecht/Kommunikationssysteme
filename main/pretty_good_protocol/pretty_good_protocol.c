#pragma once

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
#include <math.h>

#define PORT 4444

#define USEPROTO 1
#define USEJSON 0
#define USETLV 0

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

void start_udp_setup()
{
        srand(time(NULL));
        led_init();

        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        wifi_init_static_ip();
        wifi_wait_connected();

        xTaskCreate(udp_receiver_task, "udp_receiver", 4096, NULL, 5, NULL);
        xTaskCreate(udp_sender_task, "udp_sender", 4096, NULL, 5, NULL);
}