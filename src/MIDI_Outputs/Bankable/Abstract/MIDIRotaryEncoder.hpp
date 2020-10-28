#pragma once

#if not defined(Encoder_h_) && not defined(IDE)
#error                                                                         \
    "The PJRC Encoder library should be included before the Control-Surface "  \
    "library. (#include <Encoder.h>)"
#endif

#include <Banks/BankableAddresses.hpp>
#include <Def/Def.hpp>
#include <Encoder.h>
#include <MIDI_Outputs/Abstract/MIDIOutputElement.hpp>

BEGIN_CS_NAMESPACE

namespace Bankable {

/**
 * @brief   An abstract class for rotary encoders that send MIDI events and that
 *          can be added to a Bank.
 */
template <class BankAddress, class Sender>
class MIDIRotaryEncoder : public MIDIOutputElement {
  protected:
    /**
     * @brief   Construct a new MIDIRotaryEncoder.
     *
     * @todo    Documentation
     */
    MIDIRotaryEncoder(BankAddress bankAddress, const EncoderPinList &pins,
                      uint8_t speedMultiply, uint8_t pulsesPerStep,
                      const Sender &sender)
        : address(bankAddress), encoder{pins.A, pins.B},
          speedMultiply(speedMultiply), pulsesPerStep(pulsesPerStep),
          sender(sender) {}

// For tests only
#ifndef ARDUINO
    MIDIRotaryEncoder(BankAddress bankAddress, const Encoder &encoder,
                      uint8_t speedMultiply, uint8_t pulsesPerStep,
                      const Sender &sender)
        : address(bankAddress), encoder(encoder), speedMultiply(speedMultiply),
          pulsesPerStep(pulsesPerStep), sender(sender) {}
#endif

  public:
    void begin() final override {}
    void update() final override {
        long currentPosition = encoder.read();
        long difference = (currentPosition - previousPosition) / pulsesPerStep;
        // I could do the division inside of the if statement for performance
        if (difference) {
            sender.send(difference * speedMultiply, address.getActiveAddress());
            previousPosition += difference * pulsesPerStep;
        }
    }

  private:
    BankAddress address;
    Encoder encoder;
    const uint8_t speedMultiply;
    const uint8_t pulsesPerStep;
    long previousPosition = 0;

  public:
    Sender sender;
};

} // namespace Bankable

END_CS_NAMESPACE