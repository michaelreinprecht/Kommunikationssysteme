// data_structures.h
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>

typedef enum
{
    COLOR_COMMAND = 0,
    PRESSED_COMMAND = 1,
} message_type_t;

// typedef union
// {
//     uint32_t flagstorage; // The full representation of flags
//     struct
//     {
//         uint32_t ack : 1;       // 1 bit for ACK flag
//         uint32_t reserved : 31; // Remaining bits reserved for future use
//     } bits;
// } flags_t;

typedef struct __attribute__((packed))
{
    uint8_t version_number;  // 1 byte defining version number of the protocol
    uint8_t message_type;    // 1 byte defining message type (e.g. ERR=0, ACK=1, Control_Command=2, MoveTo_Command=3, ...)
    uint8_t sequence_number; // 1 byte for sequence number
    uint8_t length;          // 1 byte specifying the length of the payload in bytes
    // flags_t flags;           // 1 byte for flags
} header_t;

typedef struct __attribute__((packed))
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} color_command_t;

typedef struct __attribute__((packed))
{
    uint8_t pressed;
} pressed_command_t;

typedef struct __attribute__((packed))
{
    header_t header;
    union
    {
        color_command_t color;
        pressed_command_t pressed;
    } payload;
} message_t;

#endif DATA_STRUCTURES_H