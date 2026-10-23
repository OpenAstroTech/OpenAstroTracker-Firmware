#include "adapters/TcpConnectionPort.hpp"

namespace adapters
{

TcpConnectionPort::TcpConnectionPort(hal::ITcpConnection &connection) : _connection(connection)
{
}

int TcpConnectionPort::available()
{
    return _connection.available();
}

int TcpConnectionPort::read()
{
    return _connection.read();
}

int TcpConnectionPort::peek()
{
    return _connection.peek();
}

size_t TcpConnectionPort::write(uint8_t value)
{
    return _connection.write(&value, 1);
}

size_t TcpConnectionPort::write(const uint8_t *data, size_t length)
{
    return _connection.write(data, length);
}

void TcpConnectionPort::flush()
{
}

}  // namespace adapters
