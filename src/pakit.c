#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pakit.h"


void pakit_create(PakitReceiver* receiver) {
    if (receiver != NULL) {
        pakit_init(receiver);
    }
}

void pakit_destroy(PakitReceiver* receiver) {
    (void)receiver;  // Suppress unused parameter warning
}

void pakit_init(PakitReceiver* receiver) {
    memset(receiver, 0, sizeof(PakitReceiver));
    receiver->received_bytes = 0;
    receiver->header_complete = false;
    receiver->state = STATE_UNIQUE_ID;
    receiver->expected_payload_size = 0;
}

PakitStatus pakit_receive_byte(PakitReceiver* receiver, uint8_t byte) {
    // Check for null parameter
    if (receiver == NULL) {
        return PAKIT_STATUS_ERROR_NULL_PARAM;
    }

    // Check for buffer overflow
    if (receiver->received_bytes >= HEADER_SIZE + MAX_PACKET_SIZE) {
        return PAKIT_STATUS_ERROR_OVERFLOW;
    }

    // Store the byte in the buffer
    receiver->packet.buffer[receiver->received_bytes++] = byte;

    // Process the byte based on current state
    switch (receiver->state) {
        case STATE_UNIQUE_ID:
            // Process bytes for the unique ID
            if (receiver->received_bytes == PACKET_SOP_SIZE) {
                // Validate unique ID
                if (receiver->packet.fields.header.sop[0] != EXPECTED_SOP_0 ||
                    receiver->packet.fields.header.sop[1] != EXPECTED_SOP_1) {
                    pakit_init(receiver);
                    return PAKIT_STATUS_ERROR_INVALID_SOP;
                }
                // Transition to next state
                receiver->state = STATE_PACKET_TYPE;
            }
            break;

        case STATE_PACKET_TYPE:
            // Process bytes for the packet type
            if (receiver->received_bytes == PACKET_SOP_SIZE + PACKET_TYPE_SIZE) {
                // Transition to next state (no validation needed for packet type)
                receiver->state = STATE_COUNT;  // Now go to COUNT state instead
            }
            break;

        case STATE_COUNT:
            // Process bytes for the count field
            if (receiver->received_bytes == (PACKET_SOP_SIZE + PACKET_TYPE_SIZE + PACKET_COUNT_SIZE)) {
                // Transition to next state (no validation needed for count)
                receiver->state = STATE_SIZE;
            }
            break;

        case STATE_SIZE:
            // Process bytes for the size field
            if (receiver->received_bytes == HEADER_SIZE) {  // HEADER_SIZE includes SOP, type, count, and size fields
                // Calculate payload size (MSB first)
                receiver->expected_payload_size = ((uint16_t)receiver->packet.fields.header.size_bytes[0] << 8) |
                                                 receiver->packet.fields.header.size_bytes[1];

                // Validate payload size
                if (receiver->expected_payload_size > MAX_PACKET_SIZE) {
                    pakit_init(receiver);
                    return PAKIT_STATUS_ERROR_SIZE_LARGE;
                }

                // Header is now complete
                receiver->header_complete = true;

                // Transition to payload state
                receiver->state = STATE_PAYLOAD;

                // Special case: zero-length payload
                if (receiver->expected_payload_size == 0) {
                    receiver->state = STATE_COMPLETE;
                    return PAKIT_STATUS_SUCCESS;
                }
            }
            break;

        case STATE_PAYLOAD:
            // Process payload bytes
            // Check if we have received all expected payload bytes
            if (receiver->received_bytes >= HEADER_SIZE + receiver->expected_payload_size) {
                receiver->state = STATE_COMPLETE;
                return PAKIT_STATUS_SUCCESS;
            }
            break;

        case STATE_COMPLETE:
            // This byte is part of a new packet
            // Reset the receiver and process this byte as the start of a new packet
            {
                uint8_t saved_byte = byte;
                pakit_init(receiver);
                return pakit_receive_byte(receiver, saved_byte);
            }
            break;
    }

    return PAKIT_STATUS_IN_PROGRESS;
}

bool pakit_is_packet_complete(PakitReceiver* receiver, Packet* packet) {
    if (!receiver->header_complete) {
        return false;
    }

    // Calculate payload size
    uint16_t size = ((uint16_t)receiver->packet.fields.header.size_bytes[0] << 8) |
                    receiver->packet.fields.header.size_bytes[1];

    if (receiver->received_bytes < HEADER_SIZE + size) {
        return false;
    }

    if (packet != NULL) {
        // Copy data to output packet
        memcpy(packet->sop, receiver->packet.fields.header.sop, PACKET_SOP_SIZE);
        memcpy(packet->type, receiver->packet.fields.header.type, PACKET_TYPE_SIZE);

        // Extract the count field
        packet->count = ((uint16_t)receiver->packet.fields.header.count_bytes[0] << 8) |
                        receiver->packet.fields.header.count_bytes[1];

        packet->size = size;
        packet->payload = receiver->packet.fields.payload;
    }

    return true;
}

PakitStatus pakit_receive_buffer(PakitReceiver* receiver, const uint8_t* buffer, size_t buffer_length, size_t* position) {
    // Check for null parameters (but position can be NULL)
    if (receiver == NULL || buffer == NULL) {
        return PAKIT_STATUS_ERROR_NULL_PARAM;
    }

    // Start from the current position in the buffer
    // If position is NULL, start from beginning (position 0)
    size_t current_pos = (position != NULL) ? *position : 0;
    PakitStatus status = PAKIT_STATUS_IN_PROGRESS;

    // Process bytes until we reach the end, get an error, or complete a packet
    while (current_pos < buffer_length) {
        status = pakit_receive_byte(receiver, buffer[current_pos]);
        current_pos++;

        // Stop processing if we get an error or complete packet
        if (status != PAKIT_STATUS_IN_PROGRESS) {
            break;
        }
    }

    // Update the caller's position if pointer was provided
    if (position != NULL) {
        *position = current_pos;
    }

    return status;
}

bool pakit_packet_create(Packet* packet, uint16_t packet_type,
                       uint16_t count, const uint8_t* payload, uint16_t payload_size) {
    // Validate input parameters
    if (packet == NULL) {
        return false;
    }

    // Check payload parameter consistency
    if ((payload == NULL && payload_size > 0) ||
        (payload != NULL && payload_size == 0)) {
        return false;
    }

    // Initialize SOP (Start of Packet) with the expected value
    packet->sop[0] = EXPECTED_SOP_0;  // 0xB0
    packet->sop[1] = EXPECTED_SOP_1;  // 0xB2

    // Set packet type (convert from uint16_t to 2 bytes - big endian)
    packet->type[0] = (uint8_t)((packet_type >> 8) & 0xFF);  // MSB
    packet->type[1] = (uint8_t)(packet_type & 0xFF);         // LSB

    // Set count and size
    packet->count = count;
    packet->size = payload_size;

    // Set payload
    packet->payload = (uint8_t*)payload;  // Note: this doesn't copy the payload data
                                         // just stores the pointer to it

    return true;
}