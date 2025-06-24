#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "packet_receiver.h"

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