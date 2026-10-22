#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/meade/CommandAssembler.hpp"
#include "hal/ISerialPort.hpp"
#include "ports/ITransport.hpp"

namespace adapters
{

namespace meade = oat::core::meade;

/// `ports::ITransport` over any `hal::ISerialPort`: the serial control path, and the shape the
/// Wi-Fi transport will copy.
///
/// It owns framing (through `core::meade::CommandAssembler`), the ACK reply the LX200 protocol
/// expects for a `0x06` probe, and the reply write. It owns no interpretation: the command goes
/// out as text and the adapter above decides what it means.
///
/// `answerAck` is a constructor flag rather than an `#if`: when the build shares one serial
/// port with the debug log, the firmware has always suppressed the `P` reply so it does not
/// interleave with a log line. The caller owns that decision.
class SerialTransport final : public ports::ITransport
{
  public:
    /// The command buffer, sized to what the serial loop accepted before: 19 characters plus
    /// the NUL. A longer command is discarded whole, exactly as it was.
    static constexpr uint8_t kCommandCapacity = 20;

    SerialTransport(hal::ISerialPort &port, bool answerAck) : _port(port), _answerAck(answerAck)
    {
    }

    bool hasCommand() override
    {
        if (_haveCommand)
        {
            return true;
        }

        while (_port.available() > 0)
        {
            const int value = _port.read();
            if (value < 0)
            {
                break;
            }

            switch (_assembler.push(static_cast<uint8_t>(value)))
            {
                case meade::CommandAssembler<kCommandCapacity>::Event::AckRequested:
                    if (_answerAck)
                    {
                        _port.write(static_cast<uint8_t>('P'));
                    }
                    break;

                case meade::CommandAssembler<kCommandCapacity>::Event::CommandReady:
                    _haveCommand = true;
                    return true;

                case meade::CommandAssembler<kCommandCapacity>::Event::Overflow:
                    _overflowed = true;
                    break;

                case meade::CommandAssembler<kCommandCapacity>::Event::None:
                    break;
            }
        }

        return false;
    }

    size_t readCommand(char *buffer, size_t capacity) override
    {
        if (!_haveCommand)
        {
            return 0;
        }

        const char *command = _assembler.command();
        const size_t length = _assembler.length();

        _haveCommand = false;

        if (length + 1 > capacity)
        {
            _assembler.reset();
            return 0;  // the caller's buffer is too small: drop it, as the interface says
        }

        for (size_t i = 0; i < length; i++)
        {
            buffer[i] = command[i];
        }
        buffer[length] = '\0';

        _assembler.reset();
        return length;
    }

    void writeResponse(const char *response) override
    {
        if ((response == nullptr) || (response[0] == '\0'))
        {
            return;
        }

        size_t length = 0;
        while (response[length] != '\0')
        {
            length++;
        }

        _port.write(reinterpret_cast<const uint8_t *>(response), length);
    }

    /// True once since the last call when a command had to be discarded for being too long.
    /// The caller logs it; the transport has no logger of its own.
    bool takeOverflow()
    {
        const bool overflowed = _overflowed;
        _overflowed           = false;
        return overflowed;
    }

  private:
    hal::ISerialPort &_port;
    const bool _answerAck;
    meade::CommandAssembler<kCommandCapacity> _assembler;
    bool _haveCommand = false;
    bool _overflowed  = false;
};

}  // namespace adapters
