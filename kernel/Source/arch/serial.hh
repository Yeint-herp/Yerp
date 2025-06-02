#ifndef SERIAL_HH
#define SERIAL_HH

#include <arch/io.hh>
#include <ktl/string_view>

template<
    u16     PORT_BASE = 0x3F8,
    u32     BAUD      = 115200,
    u8      ILCR      = 0x03
>
class Serial {
public:
    enum SerialRegister {
        DATA = 0,
        IER = 1,
        FCR = 2,
        LCR = 3,
        MCR = 4,
        LSR = 5,
        MSR = 6,
        SCR = 7,
    };

    static void init() {
        static_assert(115200 % BAUD == 0,
                          "Baud rate must divide 115200");
        constexpr u16 divisor = 115200 / BAUD;

        io::out<u8>(PORT_BASE + IER, 0x00);
        io::out<u8>(PORT_BASE + LCR, 0x80);
        io::out<u8>(PORT_BASE + DATA,
                    static_cast<u8>(divisor & 0xFF));
        io::out<u8>(PORT_BASE + IER,
                    static_cast<u8>(divisor >> 8));
        io::out<u8>(PORT_BASE + LCR, ILCR);
        io::out<u8>(PORT_BASE + FCR, 0xC7);
        io::out<u8>(PORT_BASE + MCR, 0x0B);
    }

    [[nodiscard]] static bool tx_ready() {
        return io::in<u8>(PORT_BASE + LSR) & (1 << 5);
    }

    [[nodiscard]] static bool rx_ready() {
        return io::in<u8>(PORT_BASE + LSR) & 0x01;
    }

    static void put(char c) {
        while (!tx_ready()) { /* TODO spin */ }
        io::out<u8>(PORT_BASE + DATA, static_cast<u8>(c));
    }

    static char get() {
        while (!rx_ready()) { /* TODO spin */ }
        return static_cast<char>(io::in<u8>(PORT_BASE + DATA));
    }

    static void flush() {
        while (!tx_ready()) /* todo spin */ ;
    }

    static constexpr ktl::string_view port_name() noexcept {
        if constexpr (PORT_BASE == 0x3F8) return "COM1";
        else if constexpr (PORT_BASE == 0x2F8) return "COM2";
        else if constexpr (PORT_BASE == 0x3E8) return "COM3";
        else if constexpr (PORT_BASE == 0x2E8) return "COM4";
        else return "COM?";
    }
};

using SerialCOM1 = Serial<0x3F8>;
using SerialCOM2 = Serial<0x2F8>;
using SerialCOM3 = Serial<0x3E8>;
using SerialCOM4 = Serial<0x2E8>;

#endif //SERIAL_HH
