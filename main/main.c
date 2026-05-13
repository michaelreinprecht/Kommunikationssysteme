#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pretty_good_protocol/pretty_good_protocol.h"
#include "pingpong/pingpong.h"
#include "zigbee_router/zigbee_router.h"

// UDP Setup:
#define STARTUDP 0

// IEEE802154 Setup:
#define STARTIEEE802154 0

// ZigBee-Router Setup
#define STARTZIGBEE 1

void app_main(void)
{
#if STARTUDP
        start_udp_setup();
#elif STARTIEEE802154
        start_ieee802154_setup();
#elif STARTZIGBEE
        start_zigbee_setup();
#endif

        while (1)
        {
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
}