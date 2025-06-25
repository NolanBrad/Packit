#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pakit.h"

/* Simple testing framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(message, test) do { \
    tests_run++; \
    if (test) { \
        tests_passed++; \
        printf("[PASS] %s\n", message); \
    } else { \
        tests_failed++; \
        printf("[FAIL] %s at line %d\n", message, __LINE__); \
    } \
} while (0)

#define RUN_TEST(test_function) do { \
    printf("\nRunning %s...\n", #test_function); \
    test_function(); \
} while (0)

/* Helper functions */
void print_test_summary() {
    printf("\n----- TEST SUMMARY -----\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (tests_run > 0) ? (100.0 * tests_passed / tests_run) : 0);
    printf("-----------------------\n");
}

/* Utility functions for testing */
bool compare_packets(const Packet* p1, const Packet* p2) {
    if (p1 == NULL || p2 == NULL)
        return false;

    if (memcmp(p1->sop, p2->sop, PACKET_SOP_SIZE) != 0)
        return false;

    if (memcmp(p1->type, p2->type, PACKET_TYPE_SIZE) != 0)
        return false;

    if (p1->count != p2->count)  // Check the count field
        return false;

    if (p1->size != p2->size)
        return false;

    // For payload comparison, we just verify they're not NULL since pointers might differ
    if ((p1->payload == NULL) != (p2->payload == NULL))
        return false;

    return true;
}

/* Test Cases */

void test_initialization() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Test state after initialization
    Packet packet;
    bool is_complete = pakit_is_packet_complete(&receiver, &packet);
    TEST_ASSERT("Initial state is not complete", is_complete == false);

    // Test reinitialization
    pakit_init(&receiver);
    is_complete = pakit_is_packet_complete(&receiver, &packet);
    TEST_ASSERT("State after reinitialization is not complete", is_complete == false);

    pakit_destroy(&receiver);
}

void test_valid_packet_byte_by_byte() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Create test packet with pakit_packet_create
    Packet expected_packet;
    uint8_t payload[] = {'A', 'B', 'C'};
    bool created = pakit_packet_create(&expected_packet, 0x0103, 1, payload, 3);
    TEST_ASSERT("Packet creation", created == true);

    // Valid packet: 0xB0B2 + type 0x0103 + count 0x0001 + size 0x0003 + payload "ABC"
    uint8_t test_data[] = {
        0xB0, 0xB2,     // SOP/Unique ID
        0x01, 0x03,     // Packet type
        0x00, 0x01,     // Packet count (1)
        0x00, 0x03,     // Payload size (3 bytes)
        'A', 'B', 'C'   // Payload
    };

    // Process byte by byte
    for (size_t i = 0; i < sizeof(test_data) - 1; i++) {
        PakitStatus status = pakit_receive_byte(&receiver, test_data[i]);
        TEST_ASSERT("Incomplete packet status", status == PAKIT_STATUS_IN_PROGRESS);
    }

    // Last byte should complete the packet
    PakitStatus status = pakit_receive_byte(&receiver, test_data[sizeof(test_data) - 1]);
    TEST_ASSERT("Complete packet status", status == PAKIT_STATUS_SUCCESS);

    // Verify packet contents
    Packet packet;
    bool is_complete = pakit_is_packet_complete(&receiver, &packet);
    TEST_ASSERT("Packet completion check", is_complete == true);
    TEST_ASSERT("Packet SOP[0]", packet.sop[0] == 0xB0);
    TEST_ASSERT("Packet SOP[1]", packet.sop[1] == 0xB2);
    TEST_ASSERT("Packet type[0]", packet.type[0] == 0x01);
    TEST_ASSERT("Packet type[1]", packet.type[1] == 0x03);
    TEST_ASSERT("Packet count", packet.count == 1);  // Check count field
    TEST_ASSERT("Packet size", packet.size == 3);
    TEST_ASSERT("Payload content", packet.payload[0] == 'A' &&
                                  packet.payload[1] == 'B' &&
                                  packet.payload[2] == 'C');

    pakit_destroy(&receiver);
}

void test_packet_create() {
    // Test creating a packet with payload
    Packet packet1;
    uint8_t payload[] = "Test";
    bool result = pakit_packet_create(&packet1, 0x0102, 5, payload, 4);

    TEST_ASSERT("Create packet with payload", result == true);
    TEST_ASSERT("Packet SOP check", packet1.sop[0] == 0xB0 && packet1.sop[1] == 0xB2);
    TEST_ASSERT("Packet type check", packet1.type[0] == 0x01 && packet1.type[1] == 0x02);
    TEST_ASSERT("Packet count check", packet1.count == 5);
    TEST_ASSERT("Packet size check", packet1.size == 4);
    TEST_ASSERT("Packet payload check", memcmp(packet1.payload, "Test", 4) == 0);

    // Test creating a packet without payload
    Packet packet2;
    result = pakit_packet_create(&packet2, 0x0304, 10, NULL, 0);

    TEST_ASSERT("Create packet without payload", result == true);
    TEST_ASSERT("Empty packet type check", packet2.type[0] == 0x03 && packet2.type[1] == 0x04);
    TEST_ASSERT("Empty packet count check", packet2.count == 10);
    TEST_ASSERT("Empty packet size check", packet2.size == 0);

    // Test invalid parameters
    result = pakit_packet_create(NULL, 0x0506, 15, payload, 4);
    TEST_ASSERT("NULL packet pointer", result == false);

    result = pakit_packet_create(&packet1, 0x0708, 20, NULL, 5);
    TEST_ASSERT("NULL payload with non-zero size", result == false);

    result = pakit_packet_create(&packet1, 0x090A, 25, payload, 0);
    TEST_ASSERT("Non-NULL payload with zero size", result == false);
}

void test_invalid_packet_handling() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Test invalid SOP (not 0xB0B2)
    uint8_t invalid_sop[] = {
        0xA0, 0xA2,     // Invalid SOP
        0x01, 0x03,     // Type
        0x00, 0x01,     // Count
        0x00, 0x02,     // Size
        0x41, 0x42      // Payload
    };

    for (size_t i = 0; i < sizeof(invalid_sop); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, invalid_sop[i]);
        if (i <= 1) {
            TEST_ASSERT("Invalid SOP detection", status == PAKIT_STATUS_ERROR_INVALID_SOP);
            // Reset receiver for next byte
            pakit_init(&receiver);
        }
    }

    // Test recovery after error
    pakit_init(&receiver);
    uint8_t valid_packet[] = {
        0xB0, 0xB2,     // Valid SOP
        0x01, 0x02,     // Type
        0x00, 0x05,     // Count
        0x00, 0x02,     // Size
        0x41, 0x42      // Payload
    };

    for (size_t i = 0; i < sizeof(valid_packet); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, valid_packet[i]);
        if (i < sizeof(valid_packet) - 1) {
            TEST_ASSERT("Valid byte after reset", status == PAKIT_STATUS_IN_PROGRESS);
        } else {
            TEST_ASSERT("Valid packet completion after reset", status == PAKIT_STATUS_SUCCESS);
        }
    }

    pakit_destroy(&receiver);
}

void test_empty_payload() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Valid packet with empty payload
    uint8_t empty_packet[] = {
        0xB0, 0xB2,     // SOP
        0x02, 0x01,     // Type
        0x00, 0x0A,     // Count (10)
        0x00, 0x00      // Size (0) - No payload
    };

    for (size_t i = 0; i < sizeof(empty_packet) - 1; i++) {
        PakitStatus status = pakit_receive_byte(&receiver, empty_packet[i]);
        TEST_ASSERT("Empty payload packet progress", status == PAKIT_STATUS_IN_PROGRESS);
    }

    PakitStatus status = pakit_receive_byte(&receiver, empty_packet[sizeof(empty_packet) - 1]);
    TEST_ASSERT("Empty payload packet completion", status == PAKIT_STATUS_SUCCESS);

    Packet packet;
    bool is_complete = pakit_is_packet_complete(&receiver, &packet);
    TEST_ASSERT("Empty payload packet check", is_complete == true);
    TEST_ASSERT("Empty payload size check", packet.size == 0);
    TEST_ASSERT("Empty payload pointer check", packet.payload != NULL);

    pakit_destroy(&receiver);
}

void test_large_payload() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Create a large payload (100 bytes)
    const size_t payload_size = 100;
    uint8_t large_payload[payload_size];
    for (size_t i = 0; i < payload_size; i++) {
        large_payload[i] = (uint8_t)(i & 0xFF);
    }

    // Create packet with large payload
    Packet large_packet;
    bool result = pakit_packet_create(&large_packet, 0x0505, 1, large_payload, payload_size);
    TEST_ASSERT("Create large payload packet", result == true);
    TEST_ASSERT("Large payload size check", large_packet.size == payload_size);

    // Test receiving large packet byte by byte
    uint8_t header[] = {
        0xB0, 0xB2,             // SOP
        0x05, 0x05,             // Type
        0x00, 0x01,             // Count (1)
        (uint8_t)(payload_size >> 8), (uint8_t)(payload_size & 0xFF) // Size (100)
    };

    for (size_t i = 0; i < sizeof(header); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, header[i]);
        TEST_ASSERT("Large packet header progress", status == PAKIT_STATUS_IN_PROGRESS);
    }

    for (size_t i = 0; i < payload_size - 1; i++) {
        PakitStatus status = pakit_receive_byte(&receiver, large_payload[i]);
        TEST_ASSERT("Large payload progress", status == PAKIT_STATUS_IN_PROGRESS);
    }

    PakitStatus status = pakit_receive_byte(&receiver, large_payload[payload_size - 1]);
    TEST_ASSERT("Large payload completion", status == PAKIT_STATUS_SUCCESS);

    Packet received_packet;
    bool is_complete = pakit_is_packet_complete(&receiver, &received_packet);
    TEST_ASSERT("Large packet complete", is_complete == true);
    TEST_ASSERT("Large packet size match", received_packet.size == payload_size);

    // Check first, middle and last bytes of payload
    TEST_ASSERT("Large packet first byte", received_packet.payload[0] == 0);
    TEST_ASSERT("Large packet middle byte", received_packet.payload[50] == 50);
    TEST_ASSERT("Large packet last byte", received_packet.payload[99] == 99);

    pakit_destroy(&receiver);
}

void test_packet_malformed() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Malformed packet: correct SOP but invalid size
    uint8_t invalid_size[] = {
        0xB0, 0xB2,     // Valid SOP
        0x01, 0x03,     // Type
        0x00, 0x01,     // Count
        0xFF, 0xFF      // Size (too large)
    };

    for (size_t i = 0; i < sizeof(invalid_size); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, invalid_size[i]);
        if (i < sizeof(invalid_size) - 1) {
            TEST_ASSERT("Invalid size packet progress", status == PAKIT_STATUS_IN_PROGRESS);
        } else {
            // Assuming the library validates size limits
            TEST_ASSERT("Invalid size detection", status == PAKIT_STATUS_ERROR_SIZE_LARGE);
        }
    }

    pakit_init(&receiver);

    // Packet with incorrect byte count
    uint8_t short_packet[] = {
        0xB0, 0xB2,     // Valid SOP
        0x02, 0x04,     // Type
        0x00, 0x01,     // Count
        0x00, 0x05,     // Size (5 bytes)
        0x41, 0x42, 0x43 // Only 3 bytes of payload (should be 5)
    };

    for (size_t i = 0; i < sizeof(short_packet); i++) {
        pakit_receive_byte(&receiver, short_packet[i]);
    }

    Packet packet;
    bool is_complete = pakit_is_packet_complete(&receiver, &packet);
    TEST_ASSERT("Incomplete payload detection", is_complete == false);

    pakit_destroy(&receiver);
}

void test_multiple_packets() {
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Two packets back to back
    uint8_t dual_packets[] = {
        // First packet
        0xB0, 0xB2,     // SOP
        0x01, 0x01,     // Type
        0x00, 0x01,     // Count
        0x00, 0x02,     // Size
        0x41, 0x42,     // Payload "AB"

        // Second packet
        0xB0, 0xB2,     // SOP
        0x02, 0x02,     // Type
        0x00, 0x02,     // Count
        0x00, 0x03,     // Size
        0x43, 0x44, 0x45 // Payload "CDE"
    };

    // Process first packet
    size_t i;
    for (i = 0; i < 10; i++) {
        PakitStatus status = pakit_receive_byte(&receiver, dual_packets[i]);
        if (i < 9) {
            TEST_ASSERT("First packet progress", status == PAKIT_STATUS_IN_PROGRESS);
        } else {
            TEST_ASSERT("First packet completion", status == PAKIT_STATUS_SUCCESS);
        }
    }

    Packet packet1;
    bool is_complete = pakit_is_packet_complete(&receiver, &packet1);
    TEST_ASSERT("First packet check", is_complete == true);
    TEST_ASSERT("First packet type check", packet1.type[0] == 0x01 && packet1.type[1] == 0x01);
    TEST_ASSERT("First packet size check", packet1.size == 2);
    TEST_ASSERT("First packet payload check", packet1.payload[0] == 'A' && packet1.payload[1] == 'B');

    // Reset receiver for next packet
    pakit_init(&receiver);

    // Process second packet
    for (; i < sizeof(dual_packets); i++) {
        PakitStatus status = pakit_receive_byte(&receiver, dual_packets[i]);
        if (i < sizeof(dual_packets) - 1) {
            TEST_ASSERT("Second packet progress", status == PAKIT_STATUS_IN_PROGRESS);
        } else {
            TEST_ASSERT("Second packet completion", status == PAKIT_STATUS_SUCCESS);
        }
    }

    Packet packet2;
    is_complete = pakit_is_packet_complete(&receiver, &packet2);
    TEST_ASSERT("Second packet check", is_complete == true);
    TEST_ASSERT("Second packet type check", packet2.type[0] == 0x02 && packet2.type[1] == 0x02);
    TEST_ASSERT("Second packet count check", packet2.count == 2);
    TEST_ASSERT("Second packet size check", packet2.size == 3);
    TEST_ASSERT("Second packet payload check", memcmp(packet2.payload, "CDE", 3) == 0);

    pakit_destroy(&receiver);
}

int main() {
    printf("Starting Pakit tests...\n");

    // Basic functionality tests
    RUN_TEST(test_initialization);
    RUN_TEST(test_valid_packet_byte_by_byte);
    RUN_TEST(test_packet_create);

    // Edge case tests
    RUN_TEST(test_invalid_packet_handling);
    RUN_TEST(test_empty_payload);
    RUN_TEST(test_large_payload);

    // Error handling tests
    RUN_TEST(test_packet_malformed);

    // Multiple packet tests
    RUN_TEST(test_multiple_packets);

    print_test_summary();

    // Return non-zero exit code if any tests failed
    return (tests_failed > 0) ? 1 : 0;
}