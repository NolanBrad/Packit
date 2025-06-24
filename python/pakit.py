import enum
import struct
from typing import Optional, Tuple, List, Union, BinaryIO


class PacketStatus(enum.Enum):
    """Status codes for packet processing"""
    SUCCESS = 0         # Packet successfully completed
    IN_PROGRESS = 1     # Packet reception in progress, more data needed
    ERROR_INVALID_ID = 2  # Error: Invalid SOP identifier
    ERROR_SIZE_LARGE = 3  # Error: Payload size too large
    ERROR_OVERFLOW = 4   # Error: Buffer overflow
    ERROR_NULL_PARAM = 5  # Error: NULL parameter provided


class Packet:
    """Class representing a packet structure"""

    # Constants for packet format
    SOP_SIZE = 2
    TYPE_SIZE = 2
    COUNT_SIZE = 2
    SIZE_SIZE = 2
    HEADER_SIZE = SOP_SIZE + TYPE_SIZE + COUNT_SIZE + SIZE_SIZE
    MAX_PAYLOAD_SIZE = 1024
    EXPECTED_SOP = bytes([0xB0, 0xB2])

    def __init__(self,
             packet_type: int = 0,  # Changed to accept only int
             count: int = 0,
             payload: bytes = None):
        """
        Initialize a new packet

        Args:
            packet_type: Packet type as 16-bit integer
            count: Packet sequence number
            payload: Payload data bytes
        """
        self.sop = self.EXPECTED_SOP

        # Use the property setters for validation
        self._type = bytes([0, 0])
        self._count = 0
        self._payload = bytes()

        # Set the values through properties for validation
        self.packet_type = packet_type
        self.count = count
        if payload is not None:
            self.payload = payload

    @property
    def packet_type(self) -> int:
        """Get the packet type as an integer"""
        return (self._type[0] << 8) | self._type[1]

    @packet_type.setter
    def packet_type(self, value: int):
        """Set the packet type with validation"""
        if not isinstance(value, int):
            raise TypeError("Packet type must be an integer")

        # Handle 16-bit overflow
        value = value & 0xFFFF

        # Convert to 2 bytes (big-endian)
        self._type = bytes([(value >> 8) & 0xFF, value & 0xFF])

    @property
    def count(self) -> int:
        """Get the packet count"""
        return self._count

    @count.setter
    def count(self, value: int):
        """Set the packet count with validation"""
        if not isinstance(value, int):
            raise TypeError("Count must be an integer")

        # Handle 16-bit overflow
        self._count = value & 0xFFFF

    @property
    def payload(self) -> bytes:
        """Get the packet payload"""
        return self._payload

    @payload.setter
    def payload(self, value: bytes):
        """Set the packet payload with validation"""
        if not isinstance(value, (bytes, bytearray)):
            raise TypeError("Payload must be bytes")

        if len(value) > self.MAX_PAYLOAD_SIZE:
            raise ValueError(f"Payload size {len(value)} exceeds maximum {self.MAX_PAYLOAD_SIZE}")

        self._payload = bytes(value)

    @property
    def size(self) -> int:
        """Get the size of the payload"""
        return len(self._payload)

    def __str__(self) -> str:
        """String representation of the packet"""
        payload_str = ""
        if self._payload:
            # Check if payload is printable ASCII
            if all(32 <= b <= 126 for b in self._payload):
                payload_str = f"'{self._payload.decode('ascii')}'"
            else:
                payload_str = " ".join(f"{b:02X}" for b in self._payload)

        return (f"Packet(SOP={self.sop.hex().upper()}, "
                f"Type={self._type.hex().upper()}, "
                f"Count={self._count}, "
                f"Size={self.size}, "
                f"Payload={payload_str})")


class PacketDecoder:
    """Class for decoding packets from byte streams"""

    class State(enum.Enum):
        """Internal state machine states"""
        SOP = 0
        PACKET_TYPE = 1
        COUNT = 2
        SIZE = 3
        PAYLOAD = 4
        COMPLETE = 5

    def __init__(self):
        """Initialize the packet decoder"""
        self.reset()

    def reset(self):
        """Reset the decoder state to initial values"""
        self.state = self.State.SOP
        self.buffer = bytearray()
        self.sop = bytearray(2)
        self.packet_type = bytearray(2)
        self.count_bytes = bytearray(2)
        self.size_bytes = bytearray(2)
        self.payload = bytearray()
        self.expected_payload_size = 0
        self.header_complete = False

    def receive_byte(self, byte: int) -> PacketStatus:
        """
        Process a single byte of incoming data using a state machine

        Args:
            byte: The byte to process (0-255)

        Returns:
            PacketStatus indicating processing result
        """
        # Validate input
        if not isinstance(byte, int) or byte < 0 or byte > 255:
            return PacketStatus.ERROR_NULL_PARAM

        # Add byte to buffer
        self.buffer.append(byte)

        # Process according to current state using match statement
        match self.state:
            case self.State.SOP:
                # Process SOP bytes
                if len(self.buffer) <= Packet.SOP_SIZE:
                    self.sop[len(self.buffer) - 1] = byte

                    if len(self.buffer) == Packet.SOP_SIZE:
                        # Validate SOP
                        if bytes(self.sop) != Packet.EXPECTED_SOP:
                            self.reset()
                            return PacketStatus.ERROR_INVALID_ID
                        # Transition to next state
                        self.state = self.State.PACKET_TYPE

            case self.State.PACKET_TYPE:
                # Process packet type bytes
                idx = len(self.buffer) - Packet.SOP_SIZE - 1
                self.packet_type[idx] = byte

                if len(self.buffer) == Packet.SOP_SIZE + Packet.TYPE_SIZE:
                    # Transition to next state
                    self.state = self.State.COUNT

            case self.State.COUNT:
                # Process count bytes
                idx = len(self.buffer) - Packet.SOP_SIZE - Packet.TYPE_SIZE - 1
                self.count_bytes[idx] = byte

                if len(self.buffer) == Packet.SOP_SIZE + Packet.TYPE_SIZE + Packet.COUNT_SIZE:
                    # Transition to next state
                    self.state = self.State.SIZE

            case self.State.SIZE:
                # Process size bytes
                idx = len(self.buffer) - Packet.SOP_SIZE - Packet.TYPE_SIZE - Packet.COUNT_SIZE - 1
                self.size_bytes[idx] = byte

                if len(self.buffer) == Packet.HEADER_SIZE:
                    # Calculate payload size (MSB first)
                    self.expected_payload_size = (self.size_bytes[0] << 8) | self.size_bytes[1]

                    # Validate payload size
                    if self.expected_payload_size > Packet.MAX_PAYLOAD_SIZE:
                        self.reset()
                        return PacketStatus.ERROR_SIZE_LARGE

                    # Header is complete
                    self.header_complete = True

                    # Transition to payload state
                    self.state = self.State.PAYLOAD

                    # Special case: zero-length payload
                    if self.expected_payload_size == 0:
                        self.state = self.State.COMPLETE
                        return PacketStatus.SUCCESS

            case self.State.PAYLOAD:
                # Process payload bytes
                self.payload.append(byte)

                # Check if we have received all expected payload bytes
                if len(self.payload) >= self.expected_payload_size:
                    self.state = self.State.COMPLETE
                    return PacketStatus.SUCCESS

            case self.State.COMPLETE:
                # This byte is part of a new packet
                # Reset and process this byte as the start of a new packet
                self.reset()
                return self.receive_byte(byte)

            case _:
                # Should never reach here with our enum, but just in case
                self.reset()
                return PacketStatus.ERROR_NULL_PARAM

        return PacketStatus.IN_PROGRESS

    def receive_buffer(self,
                      buffer: bytes,
                      start_pos: int = 0) -> Tuple[PacketStatus, int]:
        """
        Process a buffer of incoming data

        Args:
            buffer: Bytes to process
            start_pos: Starting position in buffer

        Returns:
            Tuple of (status, end_position)
        """
        if not buffer:
            return PacketStatus.ERROR_NULL_PARAM, start_pos

        pos = start_pos
        status = PacketStatus.IN_PROGRESS

        # Process bytes until end, error, or complete packet
        while pos < len(buffer):
            status = self.receive_byte(buffer[pos])
            pos += 1

            # Stop processing if we get an error or complete packet
            if status != PacketStatus.IN_PROGRESS:
                break

        return status, pos

    def get_packet(self) -> Optional[Packet]:
        """
        Get the completed packet if available

        Returns:
            Packet object or None if no complete packet
        """
        if not self.header_complete or self.state != self.State.COMPLETE:
            return None

        # Create and populate a new packet
        packet = Packet()
        packet.sop = bytes(self.sop)
        packet.packet_type = self.packet_type.to_bytes(2, 'big')
        packet.count = (self.count_bytes[0] << 8) | self.count_bytes[1]
        packet.payload = bytes(self.payload)

        return packet


class PacketEncoder:
    """Class for encoding packets to bytes"""

    def __init__(self):
        """Initialize the packet encoder"""
        pass

    def encode(self, packet: Packet) -> bytes:
        """
        Encode a packet into bytes

        Args:
            packet: The packet to encode

        Returns:
            Encoded bytes
        """
        # Validate packet
        if not packet:
            return bytes()

        # Prepare payload
        payload = packet.payload  # Now using the payload property getter
        payload_size = len(payload)

        if payload_size > Packet.MAX_PAYLOAD_SIZE:
            raise ValueError(f"Payload size {payload_size} exceeds maximum {Packet.MAX_PAYLOAD_SIZE}")

        # Build packet bytes
        result = bytearray()

        # SOP (Start of Packet)
        result.extend(Packet.EXPECTED_SOP)

        # Packet type
        result.extend(packet.packet_type)

        # Count (16-bit, big endian)
        result.append((packet.count >> 8) & 0xFF)  # MSB
        result.append(packet.count & 0xFF)         # LSB

        # Size (16-bit, big endian)
        result.append((payload_size >> 8) & 0xFF)  # MSB
        result.append(payload_size & 0xFF)         # LSB

        # Payload
        if payload_size > 0:
            result.extend(payload)

        return bytes(result)

    def encode_packet(self,
                 packet_type: int,  # Changed to int
                 count: int,
                 payload: bytes = None) -> bytes:
        """
        Create and encode a packet in one step

        Args:
            packet_type: Packet type as 16-bit integer
            count: Packet sequence number
            payload: Payload data bytes

        Returns:
            Encoded bytes
        """
        packet = Packet(packet_type, count, payload)
        return self.encode(packet)


# Example usage
if __name__ == "__main__":
    # Create an encoder
    encoder = PacketEncoder()

    # Create a packet with text payload
    packet1 = Packet(0x0103, 1, b"Hello")  # Use integer for packet type
    encoded1 = encoder.encode(packet1)
    print(f"Encoded packet 1: {' '.join(f'{b:02X}' for b in encoded1)}")

    # Create a packet with binary payload using direct method
    encoded2 = encoder.encode_packet(0x0201, 10, bytes([0xDE, 0xAD, 0xBE, 0xEF]))  # Use integer
    print(f"Encoded packet 2: {' '.join(f'{b:02X}' for b in encoded2)}")

    # Create a decoder
    decoder = PacketDecoder()

    # Decode the first packet
    status, pos = decoder.receive_buffer(encoded1)
    if status == PacketStatus.SUCCESS:
        decoded_packet = decoder.get_packet()
        print(f"Decoded packet 1: {decoded_packet}")

    # Reset decoder and decode the second packet
    decoder.reset()
    status, pos = decoder.receive_buffer(encoded2)
    if status == PacketStatus.SUCCESS:
        decoded_packet = decoder.get_packet()
        print(f"Decoded packet 2: {decoded_packet}")

    # Example of processing multiple packets from a single buffer
    multi_packet_buffer = encoded1 + encoded2  # Concatenate the two encoded packets

    print("\nProcessing multiple packets from buffer:")
    decoder.reset()
    pos = 0
    packet_count = 0

    while pos < len(multi_packet_buffer):
        status, pos = decoder.receive_buffer(multi_packet_buffer, pos)

        if status == PacketStatus.SUCCESS:
            packet_count += 1
            decoded = decoder.get_packet()
            print(f"  Packet #{packet_count}: {decoded}")
            decoder.reset()
        elif status != PacketStatus.IN_PROGRESS:
            print(f"  Error processing buffer at position {pos}: {status}")
            break