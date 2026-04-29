#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <lwip/ip_addr.h>
#include "protocol_examples_common.h"

static const char *TAG = "wifi_handler";
static EventGroupHandle_t wifi_event_group;

typedef enum
{
    WIFI_CONNECTED_BIT = BIT0,
} wifi_event_bits_t;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa((ip4_addr_t *)&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_static_ip(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    assert(netif);

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 10, 153, 106, 80);      // ESP Address
    IP4_ADDR(&ip_info.gw, 10, 153, 106, 52);      // Gateway (labs@fhv.at) address
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Netmask

    esp_netif_dhcpc_stop(netif);
    esp_netif_set_ip_info(netif, &ip_info);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = "labs@fhv.at",
    //         .password = "vZDjRViutq9lsJ",
    //         .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    //     },
    // };

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "esprmi",
            .password = "esprmi20128",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialized.");
}

void wifi_wait_connected(void)
{
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected, continuing...");
}
