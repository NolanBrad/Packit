#ifndef pakit_H
#define pakit_H

#include <stdint.h>
#include <stdbool.h>

#define PACKET_SOP_SIZE 2
#define PACKET_TYPE_SIZE 2
#define PACKET_COUNT_SIZE 2
#define PACKET_SIZE_SIZE 2

typedef enum {
    PAKIT_STATUS_SUCCESS,          // Packet successfully completed
    PAKIT_STATUS_IN_PROGRESS,      // Packet reception in progress, more data needed
    PAKIT_STATUS_ERROR_INVALID_SOP, // Error: Invalid unique identifier
    PAKIT_STATUS_ERROR_SIZE_LARGE, // Error: Payload size too large
    PAKIT_STATUS_ERROR_OVERFLOW,   // Error: Buffer overflow
    PAKIT_STATUS_ERROR_NULL_PARAM  // Error: NULL parameter provided
} PakitStatus;

typedef struct {
    uint8_t sop[PACKET_SOP_SIZE];
    uint8_t type[PACKET_TYPE_SIZE];
    uint16_t count;
    uint16_t size;
    uint8_t *payload;
} Packet;

// Define states for the packet receiver state machine
typedef enum {
    STATE_UNIQUE_SOP,
    STATE_PACKET_TYPE,
    STATE_COUNT,
    STATE_SIZE,
    STATE_PAYLOAD,
    STATE_COMPLETE
} ReceiverState;

#define HEADER_SIZE (PACKET_SOP_SIZE + PACKET_TYPE_SIZE + PACKET_COUNT_SIZE + PACKET_SIZE_SIZE)
#define MAX_PACKET_SIZE (255 + HEADER_SIZE)
#define EXPECTED_SOP_0 0xB0
#define EXPECTED_SOP_1 0xB2

// Header structure that overlays the beginning of the packet buffer
typedef struct {
    uint8_t sop[PACKET_SOP_SIZE];
    uint8_t type[PACKET_TYPE_SIZE];
    uint8_t count_bytes[PACKET_COUNT_SIZE];
    uint8_t size_bytes[PACKET_SIZE_SIZE];  // Raw size bytes to handle endianness
} PacketHeader;

// Union to access packet data either as a byte buffer or structured fields
typedef union {
    struct {
        PacketHeader header;
        uint8_t payload[MAX_PACKET_SIZE];
    } fields;
    uint8_t buffer[HEADER_SIZE + MAX_PACKET_SIZE];
} PacketBuffer;

typedef struct {
    PacketBuffer packet;
    size_t received_bytes;
    bool header_complete;
    ReceiverState state;
    uint16_t expected_payload_size;
} PakitReceiver;

// Initializes a packet receiver instance
// Parameters:
//   receiver - Pointer to the PakitReceiver to initialize
void pakit_create(PakitReceiver* receiver);

// Releases resources associated with a packet receiver
// Parameters:
//   receiver - Pointer to the PakitReceiver to be destroyed
void pakit_destroy(PakitReceiver* receiver);

// Initializes or resets a packet receiver to its initial state
// Parameters:
//   receiver - Pointer to the PakitReceiver to initialize
void pakit_init(PakitReceiver* receiver);

// Processes a single byte of incoming data
// Parameters:
//   receiver - Pointer to the PakitReceiver
//   byte - The byte to process
// Returns:
//   PAKIT_STATUS_SUCCESS - A complete packet has been received
//   PAKIT_STATUS_IN_PROGRESS - More bytes needed to complete the packet
//   PAKIT_STATUS_ERROR_* - An error occurred during processing
PakitStatus pakit_receive_byte(PakitReceiver* receiver, uint8_t byte);

// Processes a buffer of incoming data starting from the given position
// Parameters:
//   receiver - Pointer to the PakitReceiver
//   buffer - Pointer to the buffer containing bytes to process
//   buffer_length - Number of bytes in the buffer
//   position - Pointer to position variable: input is starting position, output is ending position
//              Can be NULL, in which case processing starts at position 0
// Returns:
//   PAKIT_STATUS_SUCCESS - A complete packet has been received
//   PAKIT_STATUS_IN_PROGRESS - More bytes needed to complete the packet
//   PAKIT_STATUS_ERROR_* - An error occurred during processing
PakitStatus pakit_receive_buffer(PakitReceiver* receiver,
                                const uint8_t* buffer,
                                size_t buffer_length,
                                size_t* position);

// Checks if a complete packet has been received and copies it to the provided structure
// Parameters:
//   receiver - Pointer to the PakitReceiver
//   packet - Pointer to a Packet structure where the received packet will be copied
// Returns:
//   true - A complete packet was available and copied to the packet parameter
//   false - No complete packet is available
bool pakit_is_packet_complete(PakitReceiver* receiver, Packet* packet);

// Creates and initializes a packet structure with the provided values
// Parameters:
//   packet - Pointer to the Packet structure to initialize
//   packet_type - 16-bit packet type identifier
//   count - Packet sequence number
//   payload - Pointer to the payload data (can be NULL for empty payload)
//   payload_size - Size of the payload in bytes
// Returns:
//   true if initialization was successful, false if parameters were invalid
bool pakit_packet_create(Packet* packet, uint16_t packet_type,
                       uint16_t count, const uint8_t* payload, uint16_t payload_size);

#endif // pakit_H