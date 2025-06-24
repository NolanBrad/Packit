#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pakit.h"


// Helper function to print a packet's contents
void print_packet(const Packet* packet) {
    printf("Packet received:\n");
    printf("  SOP (Start of Packet): 0x%02X%02X\n", packet->sop[0], packet->sop[1]);
    printf("  Type: 0x%02X%02X\n", packet->type[0], packet->type[1]);
    printf("  Count: %u\n", packet->count);  // Added count field display
    printf("  Size: %u bytes\n", packet->size);

    printf("  Payload: ");
    // Print payload as hex if not printable ASCII
    bool printable = true;
    for (int i = 0; i < packet->size; i++) {
        if (packet->payload[i] < 32 || packet->payload[i] > 126) {
            printable = false;
            break;
        }
    }

    if (printable) {
        printf("'");
        for (int i = 0; i < packet->size; i++) {
            printf("%c", packet->payload[i]);
        }
        printf("'\n");
    } else {
        for (int i = 0; i < packet->size; i++) {
            printf("%02X ", packet->payload[i]);
        }
        printf("\n");
    }
}

void process_byte_stream() {
    printf("\nExample 1: Processing a byte stream\n");
    printf("-----------------------------------\n");

    // Create and initialize the packet receiver
    PakitReceiver receiver;

    pakit_create(&receiver);

    // Example packet: 0xB0B2 + type 0x0103 + count 0x0001 + size 0x0005 + payload "Hello"
    uint8_t packet_bytes[] = {
        0xB0, 0xB2,             // SOP
        0x01, 0x03,             // Packet type
        0x00, 0x01,             // Packet count (1)
        0x00, 0x05,             // Payload size (5 bytes)
        'H', 'e', 'l', 'l', 'o' // Payload
    };

    // Process byte-by-byte
    printf("Feeding bytes one by one...\n");
    for (size_t i = 0; i < sizeof(packet_bytes); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, packet_bytes[i]);

        printf("Byte %zu (0x%02X) - Status: ", i, packet_bytes[i]);

        switch (status) {
            case PAKIT_STATUS_SUCCESS:
                printf("SUCCESS (Complete packet received)\n");

                // Retrieve and print the packet
                Packet packet;
                if (pakit_is_packet_complete(&receiver, &packet)) {
                    print_packet(&packet);
                }
                break;

            case PAKIT_STATUS_IN_PROGRESS:
                printf("IN_PROGRESS (Waiting for more bytes)\n");
                break;

            case PAKIT_STATUS_ERROR_INVALID_SOP:
                printf("ERROR: Invalid packet ID\n");
                break;

            case PAKIT_STATUS_ERROR_SIZE_LARGE:
                printf("ERROR: Payload size too large\n");
                break;

            case PAKIT_STATUS_ERROR_OVERFLOW:
                printf("ERROR: Buffer overflow\n");
                break;

            case PAKIT_STATUS_ERROR_NULL_PARAM:
                printf("ERROR: Null parameter\n");
                break;

            default:
                printf("UNKNOWN STATUS\n");
                break;
        }
    }

    // Create a packet using pakit_packet_create and print it
    printf("\nCreating a packet directly:\n");
    Packet created_packet;
    uint8_t hello_payload[] = "Hello World";
    if (pakit_packet_create(&created_packet, 0x0202, 2, hello_payload, 11)) {
        print_packet(&created_packet);
    } else {
        printf("Failed to create packet\n");
    }

    // Cleanup
    pakit_destroy(&receiver);
}

void process_buffer() {
    printf("\nExample 2: Processing a complete buffer\n");
    printf("--------------------------------------\n");

    // Create the packet receiver
    PakitReceiver receiver_inst;
    PakitReceiver *receiver = &receiver_inst;

    pakit_create(receiver);

    // Example packet with binary payload and count field
    uint8_t binary_packet[] = {
        0xB0, 0xB2,             // SOP
        0x02, 0x01,             // Packet type (binary data)
        0x00, 0x0A,             // Packet count (10)
        0x00, 0x04,             // Payload size (4 bytes)
        0xDE, 0xAD, 0xBE, 0xEF  // Binary payload
    };

    // Process the whole buffer
    printf("Processing complete buffer...\n");
    PakitStatus status = pakit_receive_buffer(receiver,
                                           binary_packet,
                                           sizeof(binary_packet),
                                           NULL); // Start from beginning

    printf("Buffer processing status: ");
    switch (status) {
        case PAKIT_STATUS_SUCCESS:
            printf("SUCCESS\n");

            // Retrieve and print the packet
            Packet packet;
            if (pakit_is_packet_complete(receiver, &packet)) {
                print_packet(&packet);
            }
            break;

        case PAKIT_STATUS_IN_PROGRESS:
            printf("IN_PROGRESS (Incomplete packet)\n");
            break;

        default:
            printf("ERROR (Status code: %d)\n", status);
            break;
    }

    // Cleanup
    pakit_destroy(receiver);
}

void process_multiple_packets_in_buffer() {
    printf("\nExample 3: Processing multiple packets from a single buffer\n");
    printf("--------------------------------------------------------\n");

    // Create the packet receiver
    PakitReceiver receiver_inst;
    PakitReceiver *receiver = &receiver_inst;

    pakit_create(receiver);

    // Create packets using pakit_packet_create
    Packet packet1, packet2, packet3;
    uint8_t payload1[] = {'A', 'B', 'C'};
    uint8_t payload2[] = {'X', 'Y', 'Z'};
    uint8_t payload3[] = {'1', '2', '3'};

    pakit_packet_create(&packet1, 0x0101, 1, payload1, 3);
    pakit_packet_create(&packet2, 0x0202, 2, payload2, 3);
    pakit_packet_create(&packet3, 0x0303, 4, payload3, 3); // Count=4 to simulate dropped packet

    // Buffer with multiple packets with sequential count values
    uint8_t multi_packet[] = {
        // First packet: type 0x0101, count 0x0001, payload "ABC"
        0xB0, 0xB2, 0x01, 0x01, 0x00, 0x01, 0x00, 0x03, 'A', 'B', 'C',
        // Second packet: type 0x0202, count 0x0002, payload "XYZ"
        0xB0, 0xB2, 0x02, 0x02, 0x00, 0x02, 0x00, 0x03, 'X', 'Y', 'Z',
        // Third packet: type 0x0303, count 0x0004, payload "123" (notice count skips 3 - simulating dropped packet)
        0xB0, 0xB2, 0x03, 0x03, 0x00, 0x04, 0x00, 0x03, '1', '2', '3'
    };

    size_t position = 0;
    int packet_count = 0;
    uint16_t last_count = 0;
    bool first_packet = true;

    printf("Processing buffer with multiple packets...\n");

    // Process until we reach the end of the buffer
    while (position < sizeof(multi_packet)) {
        PakitStatus status = pakit_receive_buffer(receiver,
                                               multi_packet,
                                               sizeof(multi_packet),
                                               &position);

        if (status == PAKIT_STATUS_SUCCESS) {
            // Got a complete packet
            packet_count++;
            Packet packet;
            if (pakit_is_packet_complete(receiver, &packet)) {
                printf("\nPacket #%d at position %zu:\n", packet_count, position);
                print_packet(&packet);

                // Check for dropped packets using the count field
                if (!first_packet && packet.count > last_count + 1) {
                    printf("  WARNING: Possible dropped packet(s) detected!\n");
                    printf("  Expected count %d but received %d\n", last_count + 1, packet.count);
                }

                last_count = packet.count;
                first_packet = false;
            }

            // Reset for next packet
            pakit_init(receiver);
        }
        else if (status != PAKIT_STATUS_IN_PROGRESS) {
            // Error occurred
            printf("Error processing buffer at position %zu: %d\n", position, status);
            break;
        }
        else if (position >= sizeof(multi_packet)) {
            // Reached end of buffer with incomplete packet
            printf("Incomplete packet at end of buffer\n");
            break;
        }
    }

    printf("\nFound %d packets in the buffer\n", packet_count);

    // Cleanup
    pakit_destroy(receiver);
}

void process_invalid_data() {
    printf("\nExample 4: Handling invalid data\n");
    printf("--------------------------------\n");

    // Create the packet receiver
    PakitReceiver receiver_inst;
    PakitReceiver *receiver = &receiver_inst;

    pakit_create(receiver);

    // Invalid packet (wrong SOP)
    uint8_t invalid_packet[] = {
        0xA1, 0xA2,             // Wrong SOP (should be 0xB0B2)
        0x01, 0x02,             // Packet type
        0x00, 0x01,             // Count
        0x00, 0x02,             // Payload size
        0xAA, 0xBB              // Payload
    };

    printf("Processing invalid packet...\n");
    PakitStatus status = pakit_receive_buffer(receiver,
                                           invalid_packet,
                                           sizeof(invalid_packet),
                                           NULL);

    printf("Result: ");
    if (status == PAKIT_STATUS_ERROR_INVALID_SOP) {
        printf("Properly detected invalid packet ID\n");
    } else {
        printf("Unexpected status: %d\n", status);
    }

    // Reset for the next test
    pakit_init(receiver);

    // Packet with too large payload size
    uint8_t oversized_packet[] = {
        0xB0, 0xB2,             // Correct SOP
        0x01, 0x02,             // Packet type
        0x00, 0x01,             // Count
        0xFF, 0xFF,             // Payload size (too large)
        0xAA, 0xBB              // Payload start
    };

    printf("Processing packet with oversized payload...\n");
    status = pakit_receive_buffer(receiver,
                                oversized_packet,
                                sizeof(oversized_packet),
                                NULL);

    printf("Result: ");
    if (status == PAKIT_STATUS_ERROR_SIZE_LARGE) {
        printf("Properly detected oversized payload\n");
    } else {
        printf("Unexpected status: %d\n", status);
    }

    // Cleanup
    pakit_destroy(receiver);
}

int main() {
    printf("Packet Receiver Library Demo\n");
    printf("===========================\n");

    process_byte_stream();
    process_buffer();
    process_multiple_packets_in_buffer();
    process_invalid_data();

    printf("\nDemo completed successfully.\n");
    return 0;
}