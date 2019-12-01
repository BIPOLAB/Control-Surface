#pragma once

#include "MIDI_Interface.hpp"
#include <AH/Error/Error.hpp>
#include <AH/Teensy/TeensyUSBTypes.hpp>
#include <MIDI_Parsers/USBMIDI_Parser.hpp>

#ifdef TEENSY_MIDIUSB_ENABLED
#ifndef __AVR_AT90USB1286__ // Probably Teensy 3.x
#include <usb_dev.h>
#else // Teensy++ 2.0
#include <usb_private.h>
#endif
#elif defined TEENSYDUINO
#warning                                                                       \
    "Teensy: USB MIDI not enabled. Enable it from the Tools > USB Type menu."
#endif

// If the main MCU has a USB connection but is not a Teensy
#if defined(USBCON) && !defined(TEENSYDUINO)
#include "MIDIUSB.h"
#endif

#ifndef ARDUINO
#include <gmock-wrapper.h>
#endif

// If the main MCU has a USB connection or is a Teensy with MIDI USB type
#if defined(USBCON) || defined(TEENSY_MIDIUSB_ENABLED) || !defined(ARDUINO)

BEGIN_CS_NAMESPACE

/**
 * @brief   A class for MIDI interfaces sending MIDI messages over a USB MIDI
 *          connection.
 * 
 * @note    See the [MIDI over USB Wiki]
 *          (https://github.com/tttapa/MIDI_controller/wiki/MIDI-over-USB)
 *          for more information.
 * 
 * @ingroup MIDIInterfaces
 */
class USBMIDI_Interface : public Parsing_MIDI_Interface {
  public:
    /**
     * @brief   Construct a new USBMIDI_Interface.
     */
    USBMIDI_Interface() : Parsing_MIDI_Interface(parser) {}

  private:
    USBMIDI_Parser parser;

// If this is a test on PC
#ifndef ARDUINO

  public:
    W_SUGGEST_OVERRIDE_OFF

    MOCK_METHOD5(writeUSBPacket,
                 void(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t));
    using MIDIUSBPacket_t = Array<uint8_t, 4>;
    MOCK_METHOD0(readUSBPacket, MIDIUSBPacket_t(void));
    void flush() {}

    W_SUGGEST_OVERRIDE_ON

    MIDI_read_t read() override {
        for (uint8_t i = 0; i < (SYSEX_BUFFER_SIZE + 2) / 3; ++i) {
            MIDIUSBPacket_t rx_packet = readUSBPacket();
            if (!rx_packet[0])
                return NO_MESSAGE;
            MIDI_read_t parseResult = parser.parse(rx_packet.data);
            if (parseResult != NO_MESSAGE)
                return parseResult;
        }
        return NO_MESSAGE;
    }

// If it's a Teensy board
#elif defined(TEENSYDUINO)
#ifndef __AVR_AT90USB1286__ // Probably Teensy 3.x
    void writeUSBPacket(uint8_t cn, uint8_t cin, uint8_t d0, uint8_t d1,
                        uint8_t d2) {
        usb_midi_write_packed((cn << 4) | cin | // CN|CIN
                              (d0 << 8) |       // status
                              (d1 << 16) |      // data 1
                              (d2 << 24));      // data 2
    }
#else                       // Teensy++ 2.0
    void writeUSBPacket(uint8_t cn, uint8_t cin, uint8_t d0, uint8_t d1,
                        uint8_t d2) {
        uint8_t intr_state, timeout;

        if (!usb_configuration)
            return;
        intr_state = SREG;
        cli();
        UENUM = MIDI_TX_ENDPOINT;
        timeout = UDFNUML + 2;
        while (1) {
            // are we ready to transmit?
            if (UEINTX & (1 << RWAL))
                break;
            SREG = intr_state;
            if (UDFNUML == timeout)
                return;
            if (!usb_configuration)
                return;
            intr_state = SREG;
            cli();
            UENUM = MIDI_TX_ENDPOINT;
        }
        UEDATX = (cn << 4) | cin;
        UEDATX = d0;
        UEDATX = d1;
        UEDATX = d2;
        if (!(UEINTX & (1 << RWAL)))
            UEINTX = 0x3A;
        SREG = intr_state;
    }
#endif                      // Teensy++ 2.0

    void flush() {}

// If the main MCU has a USB connection but is not a Teensy
#elif defined(USBCON)

    void writeUSBPacket(uint8_t cn, uint8_t cin, uint8_t d0, uint8_t d1,
                        uint8_t d2) {
        midiEventPacket_t msg = {
            uint8_t((cn << 4) | cin), // CN|CIN
            d0,                       // status
            d1,                       // data 1
            d2,                       // data 2
        };
        MidiUSB.sendMIDI(msg);
    }

    void flush() { MidiUSB.flush(); }

#endif

    void sendImpl(uint8_t m, uint8_t c, uint8_t d1, uint8_t d2,
                  uint8_t cn) override {
        writeUSBPacket(cn, m >> 4, // CN|CIN
                       (m | c),    // status
                       d1,         // data 1
                       d2);        // data 2
        flush();
    }

    void sendImpl(uint8_t m, uint8_t c, uint8_t d1, uint8_t cn) override {
        sendImpl(m, c, d1, 0, cn);
    }

    void sendImpl(const uint8_t *data, size_t length, uint8_t cn) override {
        while (length > 3) {
            writeUSBPacket(cn, 0x4, data[0], data[1], data[2]);
            data += 3;
            length -= 3;
        }
        switch (length) {
            case 3: writeUSBPacket(cn, 0x7, data[0], data[1], data[2]); break;
            case 2: writeUSBPacket(cn, 0x6, data[0], data[1], 0); break;
            case 1: writeUSBPacket(cn, 0x5, data[0], 0, 0); break;
            default: break;
        }
        flush();
    }

    void sendImpl(uint8_t rt, uint8_t cn) override {
        writeUSBPacket(cn, 0xF, // CN|CIN
                       rt,      // single byte
                       0,       // no data
                       0);      // no data
        flush();
    }

  public:
// If it's a Teensy board
#if defined(TEENSYDUINO)
#ifndef __AVR_AT90USB1286__ // Probably Teensy 3.x
    MIDI_read_t read() override {
        for (uint8_t i = 0; i < (SYSEX_BUFFER_SIZE + 2) / 3; ++i) {
            if (rx_packet == nullptr) { // If there's no previous packet
                if (!usb_configuration) // Check USB configuration
                    return NO_MESSAGE;
                rx_packet = usb_rx(
                    MIDI_RX_ENDPOINT); // Read a new packet from the USB buffer
                if (rx_packet == nullptr) { // If there's no new packet, return
                    return NO_MESSAGE;
                }
                if (rx_packet->len < 4) {
                    // If the lenght is less than 4, it's not a valid MIDI USB
                    // packet
                    usb_free(rx_packet); // Free the packet
                    rx_packet = nullptr; // Read new packet next time around
                    return NO_MESSAGE;
                }
            }

            size_t index = rx_packet->index;
            uint8_t *data = rx_packet->buf + index; // A pointer to this packet

            MIDI_read_t parseResult = parser.parse(data);

            index += 4;
            if (index < rx_packet->len) {
                // If the packet is longer than 4 bytes
                rx_packet->index = index; // Change the index and read the next
                                          // four bytes on next read
            } else {
                // If the packet was only 4 bytes long
                usb_free(rx_packet);                  // Free the packet
                rx_packet = usb_rx(MIDI_RX_ENDPOINT); // Read the next packet
            }
            if (parseResult != NO_MESSAGE)
                return parseResult;
        }
        return NO_MESSAGE;
    }
#else  // Teensy++ 2.0
    MIDI_read_t read() override {
        for (uint8_t i = 0; i < (SYSEX_BUFFER_SIZE + 2) / 3; ++i) {
            uint8_t data[4];

            // https://github.com/PaulStoffregen/cores/blob/73ea157600a7082686d9cc48786a73caa7567da9/usb_midi/usb_api.cpp#L195
            uint8_t c, intr_state;

            intr_state = SREG;
            cli();
            if (!usb_configuration) {
                SREG = intr_state;
                return NO_MESSAGE;
            }
            UENUM = MIDI_RX_ENDPOINT;
        retry:
            c = UEINTX;
            if (!(c & (1 << RWAL))) {
                if (c & (1 << RXOUTI)) {
                    UEINTX = 0x6B;
                    goto retry;
                }
                SREG = intr_state;
                return NO_MESSAGE;
            }
            data[0] = UEDATX;
            data[1] = UEDATX;
            data[2] = UEDATX;
            data[3] = UEDATX;
            if (!(UEINTX & (1 << RWAL)))
                UEINTX = 0x6B;
            SREG = intr_state;

            MIDI_read_t parseResult = parser.parse(data);

            if (parseResult != NO_MESSAGE)
                return parseResult;
        }
        return NO_MESSAGE;
    }
#endif // Teensy++ 2.0

// If the main MCU has a USB connection but is not a Teensy → MIDIUSB library
#elif defined(USBCON)
    MIDI_read_t read() override {
        for (uint8_t i = 0; i < (SYSEX_BUFFER_SIZE + 2) / 3; ++i) {
            midiEventPacket_t midipacket = MidiUSB.read();
            uint8_t rx_packet[4] = {midipacket.header, midipacket.byte1,
                                    midipacket.byte2, midipacket.byte3};
            if (!midipacket.header)
                return NO_MESSAGE;
            MIDI_read_t parseResult = parser.parse(rx_packet);
            if (parseResult != NO_MESSAGE)
                return parseResult;
        }
        return NO_MESSAGE;
    }
#endif

  private:
// If it's a Teensy board
#if defined(TEENSYDUINO) && !defined(__AVR_AT90USB1286__)
    usb_packet_t *rx_packet = nullptr;
#endif
};

END_CS_NAMESPACE

// If the main MCU doesn't have a USB connection:
// Fall back on Serial connection at the hardware MIDI baud rate.
// (Can be used with HIDUINO or USBMidiKliK.)
#elif !defined(USBCON) && !defined(TEENSYDUINO)

#include "SerialMIDI_Interface.hpp"

BEGIN_CS_NAMESPACE

/**
 * @brief   A class for MIDI interfaces sending MIDI messages over a USB MIDI
 *          connection.
 * 
 * @note    See the [MIDI over USB Wiki]
 *          (https://github.com/tttapa/MIDI_controller/wiki/MIDI-over-USB)
 *          for more information.
 * 
 * @ingroup MIDIInterfaces
 */
class USBMIDI_Interface : public USBSerialMIDI_Interface {
  public:
    /**
     * @brief   Construct a new USBMIDI_Interface.
     */
    USBMIDI_Interface() : USBSerialMIDI_Interface(MIDI_BAUD){};
};

END_CS_NAMESPACE

#endif