#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ieee802154.h"
#include "esp_random.h"

#define DEVICE_SHORT_ADDR 0x002A // Device Addresses, lets just use 002A (start) through 002E (end)
#define DESTINATION_ADDR 0x002E  // The address the SENDER wants to reach
#define PAN_ID 0x002A
#define CHANNEL 15
#define MAX_RADIUS 5
#define CACHE_SIZE 255

#define IS_SENDER 1 // Set to 1 for the esp that is sending, 0 for others

typedef struct
{
    uint16_t origin_src;
    uint16_t dest_addr;
    uint8_t seq_num;
    uint8_t radius;
    char message[32];
} __attribute__((packed)) mesh_header_t;

typedef struct
{
    uint8_t payload[128];
    uint8_t length;
    int8_t rssi;
} ieee_packet_t;

typedef struct
{
    uint16_t src;
    uint8_t seq;
} packet_record_t;

QueueHandle_t radio_rx_queue = NULL;
packet_record_t seen_cache[CACHE_SIZE];
int cache_idx = 0;
static const char *TAG = "ZIGBEE_ROUTER";

bool is_packet_new(uint16_t src, uint8_t seq)
{
    for (int i = 0; i < CACHE_SIZE; i++)
    {
        if (seen_cache[i].src == src && seen_cache[i].seq == seq)
            return false;
    }
    seen_cache[cache_idx].src = src;
    seen_cache[cache_idx].seq = seq;
    cache_idx = (cache_idx + 1) % CACHE_SIZE;
    return true;
}

void esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    ieee_packet_t packet;
    packet.length = frame[0];
    packet.rssi = frame_info->rssi;
    memcpy(packet.payload, &frame[1], (packet.length > 127) ? 127 : packet.length);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(radio_rx_queue, &packet, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
    esp_ieee802154_receive_handle_done(frame);
}

void router_task(void *pvParameters)
{
    ieee_packet_t rx_packet;
    uint8_t tx_frame[128];
    const uint8_t mac_offset = 9;

    esp_ieee802154_set_rx_when_idle(true);
    esp_ieee802154_receive();

    while (1)
    {
        if (xQueueReceive(radio_rx_queue, &rx_packet, portMAX_DELAY) == pdPASS)
        {
            mesh_header_t *mesh = (mesh_header_t *)&rx_packet.payload[mac_offset];

            // Drop package if already received
            if (!is_packet_new(mesh->origin_src, mesh->seq_num))
            {
                continue;
            }

            // Check if package has received dest
            if (mesh->dest_addr == DEVICE_SHORT_ADDR)
            {
                ESP_LOGW(TAG, "SUCCESS! Packet reached destination.");
                ESP_LOGI(TAG, "From: 0x%04X | Msg: %s | RSSI: %d",
                         mesh->origin_src, mesh->message, rx_packet.rssi);
                // We are the destination, so we DO NOT forward.
                continue;
            }

            if (mesh->radius > 1)
            {
                mesh->radius--; // decreade radius/hops

                tx_frame[0] = mac_offset + sizeof(mesh_header_t) + 2;
                tx_frame[1] = 0x41;
                tx_frame[2] = 0x88;
                tx_frame[3] = mesh->seq_num;
                tx_frame[4] = (PAN_ID & 0xFF);
                tx_frame[5] = (PAN_ID >> 8);
                tx_frame[6] = 0xFF;
                tx_frame[7] = 0xFF; // Always broadcast at MAC level
                tx_frame[8] = (DEVICE_SHORT_ADDR & 0xFF);
                tx_frame[9] = (DEVICE_SHORT_ADDR >> 8);

                memcpy(&tx_frame[10], mesh, sizeof(mesh_header_t));

                esp_ieee802154_transmit(tx_frame, false);
                ESP_LOGI(TAG, "Forwarding packet for 0x%04X (Radius: %d)", mesh->dest_addr, mesh->radius);
            }
            else
            {
                ESP_LOGD(TAG, "Radius expired for packet to 0x%04X", mesh->dest_addr);
            }
        }
    }
}

void initial_sender_task(void *pvParameters)
{
    static uint8_t my_seq = 0;
    uint8_t tx_frame[128];
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));

        mesh_header_t mesh = {
            .origin_src = DEVICE_SHORT_ADDR,
            .dest_addr = DESTINATION_ADDR,
            .seq_num = my_seq++,
            .radius = MAX_RADIUS};
        snprintf(mesh.message, sizeof(mesh.message), "Ping to Destination");

        tx_frame[0] = 9 + sizeof(mesh_header_t) + 2;
        tx_frame[1] = 0x41;
        tx_frame[2] = 0x88;
        tx_frame[3] = mesh.seq_num;
        tx_frame[4] = (PAN_ID & 0xFF);
        tx_frame[5] = (PAN_ID >> 8);
        tx_frame[6] = 0xFF;
        tx_frame[7] = 0xFF;
        tx_frame[8] = (DEVICE_SHORT_ADDR & 0xFF);
        tx_frame[9] = (DEVICE_SHORT_ADDR >> 8);
        memcpy(&tx_frame[10], &mesh, sizeof(mesh_header_t));

        is_packet_new(mesh.origin_src, mesh.seq_num);
        esp_ieee802154_transmit(tx_frame, false);
        ESP_LOGI(TAG, ">>> SENDER: Initiating route to 0x%04X <<<", DESTINATION_ADDR);
    }
}

void start_zigbee_setup()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    radio_rx_queue = xQueueCreate(15, sizeof(ieee_packet_t));

    esp_ieee802154_enable();
    esp_ieee802154_set_promiscuous(false);
    esp_ieee802154_set_channel(CHANNEL);
    esp_ieee802154_set_panid(PAN_ID);
    esp_ieee802154_set_short_address(DEVICE_SHORT_ADDR);

    xTaskCreate(router_task, "router_task", 4096, NULL, 5, NULL);
#if IS_SENDER
    xTaskCreate(initial_sender_task, "initial_sender_task", 4096, NULL, 5, NULL);
#endif
}