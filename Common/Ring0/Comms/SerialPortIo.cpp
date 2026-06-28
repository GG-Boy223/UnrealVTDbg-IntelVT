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

// 1. VMware 的“串行端口 -> 输出文件”本质上是在模拟标准串口设备；
// 2. 当前工程之前只有直接写 0x3F8 的发送逻辑，没有看到显式初始化；
// 3. 如果串口参数没有初始化，宿主机文件可能一直为空，即使驱动已经在调用 Log()；
// 4. 这里使用经典 UART 初始化顺序，固定为 115200 8N1，足够用于调试验证。
// 初始化标准 COM1（0x3F8）。
//
// 设计说明：
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
        //while ((x & 0x20) != 0x20)
        while ((x & 0x40) != 0x40) //bit6 为1表示发送数据完全移出，发送器完全空闲。
        {
            x = InPortByte(0x3f8 + 5);
        }

        OutPortByte(0x3f8, c);

        if (c == '\n') //检查字符是否是换行符
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
