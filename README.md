# Packet Receiver Library

Human input --> This is a 100% AI (copilot) gernerated lib

A lightweight C library for receiving and parsing binary packet data byte-by-byte or from buffers.

## Overview

The Packet Receiver library provides a simple and efficient way to handle incoming binary data packets. It processes individual bytes or buffers as they arrive and reconstructs complete packets according to a defined structure:

- 2-byte Start of Packet identifier (0xB0B2)
- 2-byte packet type identifier
- 2-byte packet count field
- 2-byte payload size field
- Variable-length payload data (up to 1024 bytes)

The packet count field allows detection of dropped packets during transmission by tracking the sequence of received packets.

## API Reference

### Constants

```c
#define PACKET_SOP_SIZE 2      // Size of the Start of Packet identifier (bytes)
#define PACKET_TYPE_SIZE 2     // Size of the packet type identifier (bytes)
#define PACKET_COUNT_SIZE 2    // Size of the packet count field (bytes)
#define PACKET_SIZE_SIZE 2     // Size of the payload size field (bytes)
```

### Packet Receiver Functions

```c
// Creates a new packet receiver instance and initializes it
// Returns: Pointer to the newly created PakitReceiver, or NULL if allocation failed
PakitReceiver* pakit_create(void);

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
```

## Project Structure

```
packet-receiver
├── src
│   └── packet_receiver.c       # Implementation of the packet receiving functionality
├── include
│   └── packet_receiver.h       # Header file defining the public interface
├── examples
│   └── simple_receiver.c       # Example usage of the packet receiver library
├── test
│   └── pakit_test.c  # Unit tests for the library
├── CMakeLists.txt             # CMake configuration file
├── BUILD                      # Bazel build configuration
└── README.md                  # Project documentation
```

## Building the Library

To build the packet receiver library, you will need CMake installed on your system. Follow these steps:

1. Clone the repository:
   ```
   git clone <repository-url>
   cd packet-receiver
   ```

2. Create a build directory and navigate into it:
   ```
   mkdir build
   cd build
   ```

3. Run CMake to configure the project:
   ```
   cmake ..
   ```

4. Build the project:
   ```
   make
   ```

## Usage Example

The `examples/simple_receiver.c` file demonstrates how to use the packet receiver library. It includes steps to initialize the receiver, receive data, and process complete packets.

## Dependencies

This project does not have any external dependencies beyond the standard C library.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.

typedef enum {
    PAKIT_STATUS_SUCCESS,          // Packet successfully completed
    PAKIT_STATUS_IN_PROGRESS,      // Packet reception in progress, more data needed
    PAKIT_STATUS_ERROR_INVALID_SOP, // Error: Invalid Start of Packet identifier
    PAKIT_STATUS_ERROR_SIZE_LARGE, // Error: Payload size too large
    PAKIT_STATUS_ERROR_OVERFLOW,   // Error: Buffer overflow
    PAKIT_STATUS_ERROR_NULL_PARAM  // Error: NULL parameter provided
} PakitStatus;

typedef struct {
    uint8_t sop[PACKET_SOP_SIZE];     // Start of Packet identifier (should be 0xB0B2)
    uint8_t type[PACKET_TYPE_SIZE];   // Packet type identifier
    uint16_t count;                   // Packet count (sequence number)
    uint16_t size;                    // Size of payload in bytes
    uint8_t *payload;                 // Pointer to payload data
} Packet;

#include "packet_receiver.h"
#include <stdio.h>

int main() {
    // Create a packet receiver
    PakitReceiver* receiver = pakit_create();

    // Sample incoming data with multiple packets
    // First packet: 0xB0B2 + type 0x0001 + count 0x0001 + size 0x0005 + payload "Hello"
    // Second packet: 0xB0B2 + type 0x0002 + count 0x0002 + size 0x0004 + payload "Test"
    uint8_t data[] = {
        0xB0, 0xB2, 0x00, 0x01, 0x00, 0x01, 0x00, 0x05, 'H', 'e', 'l', 'l', 'o',  // First packet
        0xB0, 0xB2, 0x00, 0x02, 0x00, 0x02, 0x00, 0x04, 'T', 'e', 's', 't'        // Second packet
    };

    // Process the buffer, keeping track of position
    size_t position = 0;
    PakitStatus status;

    // Process first packet
    status = pakit_receive_buffer(receiver, data, sizeof(data), &position);

    if (status == PAKIT_STATUS_SUCCESS) {
        // Get the completed packet
        Packet packet;
        if (pakit_is_packet_complete(receiver, &packet)) {
            printf("Received packet of type %02x%02x with %d bytes payload\n",
                   packet.type[0], packet.type[1], packet.size);
        }

        // Reset receiver for next packet
        pakit_init(receiver);

        // Process second packet starting from position
        status = pakit_receive_buffer(receiver, data, sizeof(data), &position);

        if (status == PAKIT_STATUS_SUCCESS) {
            if (pakit_is_packet_complete(receiver, &packet)) {
                printf("Received second packet of type %02x%02x with %d bytes payload\n",
                       packet.type[0], packet.type[1], packet.size);
            }
        }
    }

    // Clean up
    pakit_destroy(receiver);
    return 0;
}

#include "pakit.h"
#include <stdio.h>

int main() {
    // Create a packet receiver
    PakitReceiver receiver;
    pakit_create(&receiver);

    // Sample incoming data with two sequential packets
    uint8_t data[] = {
        0xB0, 0xB2, 0x00, 0x01, 0x00, 0x01, 0x00, 0x05, 'H', 'e', 'l', 'l', 'o',  // First packet
        0xB0, 0xB2, 0x00, 0x02, 0x00, 0x02, 0x00, 0x04, 'T', 'e', 's', 't'         // Second packet
    };

    // Process the buffer, keeping track of position
    size_t position = 0;
    uint16_t last_count = 0;
    bool first_packet = true;

    // Process until we reach the end of the buffer
    while (position < sizeof(data)) {
        PakitStatus status = pakit_receive_buffer(&receiver, data, sizeof(data), &position);

        if (status == PAKIT_STATUS_SUCCESS) {
            // Get the completed packet
            Packet packet;
            if (pakit_is_packet_complete(&receiver, &packet)) {
                printf("Received packet of type %02x%02x with count %d and %d bytes payload\n",
                       packet.type[0], packet.type[1], packet.count, packet.size);

                // Check for dropped packets using the count field
                if (!first_packet && packet.count > last_count + 1) {
                    printf("Warning: Possible dropped packet detected!\n");
                    printf("Expected count %d but received %d\n", last_count + 1, packet.count);
                }

                last_count = packet.count;
                first_packet = false;
            }

            // Reset receiver for next packet
            pakit_init(&receiver);
        } else if (status != PAKIT_STATUS_IN_PROGRESS) {
            // Error handling
            printf("Error: status %d\n", status);
            break;
        }
    }

    // Create a new packet using pakit_packet_create
    Packet outgoing_packet;
    uint8_t payload[] = "Hello world";
    if (pakit_packet_create(&outgoing_packet, 0x0103, 5, payload, sizeof(payload) - 1)) {
        printf("Created packet with type 0x0103, count 5, and payload 'Hello world'\n");
    }

    // Clean up
    pakit_destroy(&receiver);
    return 0;
}
