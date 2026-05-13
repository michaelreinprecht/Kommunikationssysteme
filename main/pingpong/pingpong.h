#pragma once

void ieee802154_receiver_task(void *pvParameters);
void ieee802154_sender_task(void *pvParameters);
void pingpong_setup();
void start_ieee802154_setup();