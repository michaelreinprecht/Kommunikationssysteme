#pragma once

void start_udp_setup();
void udp_sender_task(void *pvParameters);
void udp_receiver_task(void *pvParameters);