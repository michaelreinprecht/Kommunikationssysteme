#include "led.h"

#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"

#define LED_RGB_PIN 20
#define RGB_POWER_PIN 19

static const char *TAG = "led";

static led_strip_handle_t led_strip_rgb = NULL;

void led_init(void)
{
    // Enable RGB power
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RGB_POWER_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(RGB_POWER_PIN, 1); // turn on power

    // LED strip config
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_RGB_PIN,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_rgb)
    );

    led_strip_clear(led_strip_rgb);

    ESP_LOGI(TAG, "LED initialized");
}

void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!led_strip_rgb)
        return;

    
    led_strip_set_pixel(led_strip_rgb, 0, r, g, b);

    led_strip_refresh(led_strip_rgb);
}

void led_off(void)
{
    if (!led_strip_rgb)
        return;

    led_strip_clear(led_strip_rgb);
    led_strip_refresh(led_strip_rgb);
}