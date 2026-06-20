#include "SerialPortInternal.h"

namespace SerialPortInternal
{
    UCHAR InPortByte(_In_ USHORT Port)
    {
        return __inbyte(Port) & 0xff;
    }

    void OutPortByte(_In_ USHORT Port, _In_ UCHAR Data)
    {
        __outbyte(Port, Data);
    }
}

void InitializeSerialPort()
{
    constexpr USHORT Com1Base = 0x3F8;

    SerialPortInternal::OutPortByte(Com1Base + 1, 0x00);
    SerialPortInternal::OutPortByte(Com1Base + 3, 0x80);
    SerialPortInternal::OutPortByte(Com1Base + 0, 0x01);
    SerialPortInternal::OutPortByte(Com1Base + 1, 0x00);
    SerialPortInternal::OutPortByte(Com1Base + 3, 0x03);
    SerialPortInternal::OutPortByte(Com1Base + 2, 0xC7);
    SerialPortInternal::OutPortByte(Com1Base + 4, 0x0B);

    (void)SerialPortInternal::InPortByte(Com1Base + 5);
}

namespace SerialPortInternal
{
    void SendChar(char c)
    {
        char x = 0;

        if (c == '\r')
        {
            return;
        }

        x = InPortByte(0x3f8 + 5);
        while ((x & 0x40) != 0x40)
        {
            x = InPortByte(0x3f8 + 5);
        }

        OutPortByte(0x3f8, c);

        if (c == '\n')
        {
            x = InPortByte(0x3f8 + 5);
            while ((x & 0x20) != 0x20)
            {
                x = InPortByte(0x3f8 + 5);
            }

            OutPortByte(0x3f8, '\r');
        }
    }
}
