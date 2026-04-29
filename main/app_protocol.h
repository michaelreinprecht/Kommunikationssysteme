#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>

#include "data_structures.h"

// Function to handle received message
void app_protocol_handle_message(struct sockaddr_storage *source_addr, message_t *msg);

// Functions to handle specific message types
void handle_color_command(const color_command_t *cmd);
void handle_pressed_command(const pressed_command_t *cmd);

#endif // APP_PROTOCOL_H
