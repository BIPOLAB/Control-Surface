#pragma once

#include "MIDI_Interface.hpp"
#include <AH/Arduino-Wrapper.h> // Stream
#include <AH/STL/utility>
#include <AH/Teensy/TeensyUSBTypes.hpp>
#include <MIDI_Parsers/SerialMIDI_Parser.hpp>
#include <Settings/SettingsWrapper.hpp>

#if defined(ESP32) || !defined(ARDUINO)
#include <mutex>
#endif

BEGIN_CS_NAMESPACE

/**
 * @brief   A class for MIDI interfaces sending and receiving MIDI messages
 *          over a Stream.
 * 
 * @ingroup MIDIInterfaces
 */
class StreamMIDI_Interface : public Parsing_MIDI_Interface {
  public:
    /**
     * @brief   Construct a StreamMIDI_Interface on the given Stream.
     *
     * @param   stream
     *          The Stream interface.
     */
    StreamMIDI_Interface(Stream &stream)
        : Parsing_MIDI_Interface(parser), stream(stream) {}

    StreamMIDI_Interface(StreamMIDI_Interface &&other)
        : Parsing_MIDI_Interface(std::move(other)), stream(other.stream) {}
    // TODO: should I move the mutex too?

    MIDIReadEvent read() override {
        while (stream.available() > 0) {
            uint8_t midiByte = stream.read();
            MIDIReadEvent parseResult = parser.parse(midiByte);
            if (parseResult != MIDIReadEvent::NO_MESSAGE)
                return parseResult;
        }
        return MIDIReadEvent::NO_MESSAGE;
    }

  protected:
    SerialMIDI_Parser parser;

    void sendImpl(uint8_t header, uint8_t d1, uint8_t d2,
                  uint8_t cn) override {
#if defined(ESP32) || !defined(ARDUINO)
        std::lock_guard<std::mutex> lock(mutex);
#endif
        (void)cn;
        stream.write(header); // Send the MIDI message over the stream
        stream.write(d1);
        stream.write(d2);
        // stream.flush(); // TODO
    }

    void sendImpl(uint8_t header, uint8_t d1, uint8_t cn) override {
#if defined(ESP32) || !defined(ARDUINO)
        std::lock_guard<std::mutex> lock(mutex);
#endif
        (void)cn;
        stream.write(header); // Send the MIDI message over the stream
        stream.write(d1);
        // stream.flush(); // TODO
    }

    void sendImpl(const uint8_t *data, size_t length, uint8_t cn) override {
#if defined(ESP32) || !defined(ARDUINO)
        std::lock_guard<std::mutex> lock(mutex);
#endif
        (void)cn;
        stream.write(data, length);
        // stream.flush(); // TODO
    }

    void sendImpl(uint8_t rt, uint8_t cn) override {
#if defined(ESP32) || !defined(ARDUINO)
        std::lock_guard<std::mutex> lock(mutex);
#endif
        (void)cn;
        stream.write(rt); // Send the MIDI message over the stream
        // stream.flush(); // TODO
    }

  protected:
    Stream &stream;
#if defined(ESP32) || !defined(ARDUINO)
    std::mutex mutex;
#endif
};

/**
 * @brief   A wrapper class for MIDI interfaces sending and receiving
 *          MIDI messages over a Serial port of generic class T.
 *
 * @note    This is a template class because the type of the Serial object
 *          is completely different on different architectures, and they
 *          do not share a common super-class that has a `begin` method.
 * 
 * @ingroup MIDIInterfaces
 */
template <class T>
class SerialMIDI_Interface : public StreamMIDI_Interface {
  public:
    /**
     * @brief   Create a new MIDI Interface on the given Serial interface
     *          with the given baud rate.
     *
     * @param   serial
     *          The Serial interface.
     * @param   baud
     *          The baud rate for the Serial interface.
     */
    SerialMIDI_Interface(T &serial, unsigned long baud = MIDI_BAUD)
        : StreamMIDI_Interface(serial), baud(baud) {}

    /**
     * @brief   Start the Serial interface at the predefined baud rate.
     */
    void begin() override { static_cast<T &>(stream).begin(baud); }

  private:
    const unsigned long baud;
};

/**
 * @brief   A class for MIDI interfaces sending and receiving
 *          MIDI messages over a Hardware Serial port.
 * 
 * @ingroup MIDIInterfaces
 */
class HardwareSerialMIDI_Interface
    : public SerialMIDI_Interface<HardwareSerial> {
  public:
    /**
     * @brief   Construct a new MIDI Interface on the given HardwareSerial
     *          interface with the given baud rate.
     *
     * @param   serial
     *          The HardwareSerial interface.
     * @param   baud
     *          The baud rate for the serial interface.
     */
    HardwareSerialMIDI_Interface(HardwareSerial &serial,
                                 unsigned long baud = MIDI_BAUD)
        : SerialMIDI_Interface(serial, baud) {}
};

/**
 * @brief   A class for MIDI interfaces sending and receiving
 *          MIDI messages over the Serial port of the USB connection.
 *
 * @ingroup MIDIInterfaces
 */
class USBSerialMIDI_Interface : public SerialMIDI_Interface<decltype(Serial)> {
  public:
    /**
     * @brief   Construct a USBSerialMIDI_Interface with the given baud rate.
     *
     * @param   baud
     *          The baud rate to start the USB Serial connection with.
     */
    USBSerialMIDI_Interface(unsigned long baud)
        : SerialMIDI_Interface(Serial, baud) {}
};

#if !defined(TEENSYDUINO) ||                                                   \
    (defined(TEENSYDUINO) && defined(TEENSY_SERIALUSB_ENABLED))
/**
 * @brief   A class for MIDI Interfaces sending and receiving
 *          data over the USB Serial CDC connection for the use
 *          with the [Hairless MIDI<->Serial Bridge]
 *          (http://projectgus.github.io/hairless-midiserial/).
 * 
 * @ingroup MIDIInterfaces
 */
class HairlessMIDI_Interface : public USBSerialMIDI_Interface {
  public:
    /**
     * @brief   Construct a HairlessMIDI_Interface.
     *
     * The default Hairless baud rate of 115200 baud is used.
     * This can be changed in the Settings.hpp file.
     */
    HairlessMIDI_Interface() : USBSerialMIDI_Interface(HAIRLESS_BAUD){};
};
#endif

END_CS_NAMESPACE

// TODO: Teensy 4.0 SoftwareSerial bug
#if defined(__AVR__) || (defined(TEENSYDUINO) && TEENSYDUINO != 147) ||        \
    (defined(TEENSYDUINO) && !defined(__IMXRT1052__) &&                        \
     !defined(__IMXRT1062__))

#include <SoftwareSerial.h>

BEGIN_CS_NAMESPACE

/**
 * @brief   A class for MIDI interfaces sending and receiving
 *          MIDI messages over a SoftwareSerial interface.
 * 
 * @ingroup MIDIInterfaces
 */
class SoftwareSerialMIDI_Interface
    : public SerialMIDI_Interface<SoftwareSerial> {
  public:
    /**
     * @brief   Create a SoftwareSerialMIDI_Interface on the given
     *          SoftwareSerial interface with the given baud rate.
     *
     * @param   serial
     *          The SoftwareSerial interface.
     * @param   baud
     *          The baud rate for the serial interface.
     */
    SoftwareSerialMIDI_Interface(SoftwareSerial &serial, unsigned long baud)
        : SerialMIDI_Interface(serial, baud) {}
};

END_CS_NAMESPACE

#endif