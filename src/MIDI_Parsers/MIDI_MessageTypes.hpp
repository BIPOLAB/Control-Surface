#pragma once

#include <AH/STL/cstddef> // size_t
#include <AH/STL/cstdint> // uint8_t
#include <AH/Settings/Warnings.hpp>
#include <Settings/NamespaceSettings.hpp>

#ifndef ARDUINO
#include <vector>
#endif

AH_DIAGNOSTIC_WERROR()

#include <Def/Cable.hpp>
#include <Def/Channel.hpp>
#include <Def/MIDIAddress.hpp>

BEGIN_CS_NAMESPACE

// -------------------------------------------------------------------------- //

enum class MIDIMessageType : uint8_t {
    NOTE_OFF = 0x80,         // 3B
    NOTE_ON = 0x90,          // 3B
    KEY_PRESSURE = 0xA0,     // 3B
    CC = 0xB0,               // 3B
    CONTROL_CHANGE = CC,     // 3B
    PROGRAM_CHANGE = 0xC0,   // 2B
    CHANNEL_PRESSURE = 0xD0, // 2B
    PITCH_BEND = 0xE0,       // 3B

    SYSEX_START = 0xF0,
    SYSEX_END = 0xF7,

    TUNE_REQUEST = 0xF6,

    /* System Real-Time messages */
    TIMING_CLOCK = 0xF8,
    UNDEFINED_REALTIME_1 = 0xF9,
    START = 0xFA,
    CONTINUE = 0xFB,
    STOP = 0xFC,
    UNDEFINED_REALTIME_2 = 0xFD,
    ACTIVE_SENSING = 0xFE,
    RESET = 0xFF,
};

/// See table 4-1 in <https://usb.org/sites/default/files/midi10.pdf>.
enum class MIDICodeIndexNumber : uint8_t {
    MISC_FUNCTION_CODES = 0x0,
    CABLE_EVENTS = 0x1,
    SYSTEM_COMMON_2B = 0x2,
    SYSTEM_COMMON_3B = 0x3,
    SYSEX_START_CONT = 0x4,
    SYSTEM_COMMON_1B = 0x5,
    SYSEX_END_1B = 0x5,
    SYSEX_END_2B = 0x6,
    SYSEX_END_3B = 0x7,

    NOTE_OFF = 0x8,
    NOTE_ON = 0x9,
    KEY_PRESSURE = 0xA,
    CONTROL_CHANGE = 0xB,
    PROGRAM_CHANGE = 0xC,
    CHANNEL_PRESSURE = 0xD,
    PITCH_BEND = 0xE,

    SINGLE_BYTE = 0xF,
};

// -------------------------------------------------------------------------- //

struct ChannelMessage {
    /// Constructor.
    ChannelMessage(uint8_t header, uint8_t data1, uint8_t data2, uint8_t cable)
        : header(header), data1(data1), data2(data2), cable(cable) {}

    /// Constructor.
    ChannelMessage(MIDIMessageType type, Channel channel, uint8_t data1,
                   uint8_t data2 = 0x00, Cable cable = CABLE_1)
        : ChannelMessage(uint8_t(type) | channel.getRaw(), data1, data2,
                         cable.getRaw()) {}

    uint8_t header; ///< MIDI status byte (message type and channel).
    uint8_t data1;  ///< First MIDI data byte
    uint8_t data2;  ///< First MIDI data byte

    uint8_t cable; ///< USB MIDI cable number;

    /// Check for equality.
    bool operator==(ChannelMessage other) const {
        return this->header == other.header && this->data1 == other.data1 &&
               this->data2 == other.data2 && this->cable == other.cable;
    }
    /// Check for inequality.
    bool operator!=(ChannelMessage other) const { return !(*this == other); }

    /// Get the MIDI channel of the message.
    Channel getChannel() const { return Channel(header & 0x0F); }
    /// Set the MIDI channel of the message.
    void setChannel(Channel channel) {
        header &= 0xF0;
        header |= channel.getRaw();
    }

    /// Get the MIDI USB cable number of the message.
    Cable getCable() const { return Cable(cable); }
    /// Set the MIDI USB cable number of the message.
    void setCable(Cable cable) { this->cable = cable.getRaw(); }

    /// Get the MIDI message type.
    MIDIMessageType getMessageType() const {
        return static_cast<MIDIMessageType>(header & 0xF0);
    }
    /// Set the MIDI message type.
    void setMessageType(MIDIMessageType type) {
        header &= 0x0F;
        header |= static_cast<uint8_t>(type) & 0xF0;
    }

    /// Get the first data byte.
    uint8_t getData1() const { return data1; }
    /// Get the second data byte.
    uint8_t getData2() const { return data2; }
    /// Set the first data byte.
    void setData1(uint8_t data) { data1 = data; }
    /// Get the second data byte.
    void getData2(uint8_t data) { data2 = data; }

    /// Get the MIDI address of this message, using `data1` as the address.
    /// @note   Don't use this for Channel Pressure or Pitch Bend messages,
    ///         as `data1` will have a different meaning in those cases.
    MIDIAddress getAddress() const { return {data1, getChannelCable()}; }
    /// Get the MIDI channel and cable number.
    /// @note   Valid for all MIDI Channel messages, including Channel Pressure
    ///         and Pitch Bend.
    MIDIChannelCable getChannelCable() const {
        return {getChannel(), getCable()};
    }

    /// Check whether this message has one or two data bytes.
    ///
    /// - 2 data bytes: Note On/Off, Aftertouch, Control Change or Pitch Bend
    /// - 1 data byte: Program Change or Channel Pressure
    bool hasTwoDataBytes() const {
        auto type = getMessageType();
        return type <= MIDIMessageType::CONTROL_CHANGE ||
               type == MIDIMessageType::PITCH_BEND;
    }

    /// Check whether the header is a valid header for a channel message.
    bool hasValidHeader() const {
        auto type = getMessageType();
        return type >= MIDIMessageType::NOTE_OFF &&
               type <= MIDIMessageType::PITCH_BEND;
    }
};

struct SysExMessage {
    /// Constructor.
    SysExMessage() : data(nullptr), length(0), cable(0) {}

    /// Constructor.
    SysExMessage(const uint8_t *data, size_t length, uint8_t cable)
        : data(data), length(length), cable(cable) {}

    /// Constructor.
    SysExMessage(const uint8_t *data, size_t length, Cable cable = CABLE_1)
        : data(data), length(length), cable(cable.getRaw()) {}

#ifndef ARDUINO
    /// Constructor.
    SysExMessage(const std::vector<uint8_t> &vec, Cable cable = CABLE_1)
        : SysExMessage(vec.data(), vec.size(), cable) {}
#endif

    const uint8_t *data;
    uint8_t length;
    uint8_t cable;

    bool operator==(SysExMessage other) const {
        return this->length == other.length &&
               this->cable == other.cable && 
               memcmp(this->data, other.data, length) == 0;
    }
    bool operator!=(SysExMessage other) const { return !(*this == other); }

    /// Get the MIDI USB cable number of the message.
    Cable getCable() const { return Cable(cable); }
    /// Set the MIDI USB cable number of the message.
    void setCable(Cable cable) { this->cable = cable.getRaw(); }
};

struct RealTimeMessage {
    /// Constructor.
    RealTimeMessage(uint8_t message, uint8_t cable)
        : message(message), cable(cable) {}

    /// Constructor.
    RealTimeMessage(MIDIMessageType message, uint8_t cable)
        : message(uint8_t(message)), cable(cable) {}

    /// Constructor.
    RealTimeMessage(uint8_t message, Cable cable = CABLE_1)
        : message(message), cable(cable.getRaw()) {}

    /// Constructor.
    RealTimeMessage(MIDIMessageType message, Cable cable = CABLE_1)
        : message(uint8_t(message)), cable(cable.getRaw()) {}

    uint8_t message;
    uint8_t cable;

    bool operator==(RealTimeMessage other) const {
        return this->message == other.message && this->cable == other.cable;
    }
    bool operator!=(RealTimeMessage other) const { return !(*this == other); }

    /// Get the MIDI USB cable number of the message.
    Cable getCable() const { return Cable(cable); }
    /// Set the MIDI USB cable number of the message.
    void setCable(Cable cable) { this->cable = cable.getRaw(); }
};

END_CS_NAMESPACE

AH_DIAGNOSTIC_POP()