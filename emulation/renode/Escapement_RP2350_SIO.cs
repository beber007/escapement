//
// Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
// LICENSE at the root of this repository.
//
// Escapement_RP2350_SIO: the part of the SIO of the RP2350 the Escapement examples use, for
// the platform of escapement_pico2.repl: the GPIO outputs, driven through GPIO_OUT and its
// set, clear and XOR registers, which move the lines LEDs are connected to. The other
// registers read 0, except CPUID, 0 for core 0, and FIFO_ST, whose RDY bit says the FIFO to
// core 1 has room; nothing answers from core 1, which this platform does not run. Offsets
// from the pico-sdk (hardware/regs/sio.h). The SIO has no atomic aliases: it lies on the
// core's own bus, not on APB.
//
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.GPIOPort;

namespace Antmicro.Renode.Peripherals.GPIOPort
{
    public class EscapementRP2350SIO : BaseGPIOPort, IDoubleWordPeripheral, IKnownSize
    {
        private const int NumberOfPins = 48;
        private const long CPUID = 0x000, GPIO_OUT = 0x010, GPIO_OUT_SET = 0x018,
                           GPIO_OUT_CLR = 0x020, GPIO_OUT_XOR = 0x028, GPIO_OE = 0x030,
                           GPIO_OE_SET = 0x038, GPIO_OE_CLR = 0x040, GPIO_OE_XOR = 0x048,
                           FIFO_ST = 0x050;
        private const uint FIFO_ST_RDY = 0x2;

        public EscapementRP2350SIO(IMachine machine) : base(machine, NumberOfPins)
        {
            Reset();
        }

        public override void Reset()
        {
            base.Reset();
            output = 0;
            outputEnable = 0;
            Drive();
        }

        public long Size => 0x200;

        public uint ReadDoubleWord(long offset)
        {
            switch (offset)
            {
                case CPUID: return 0;
                case GPIO_OUT: return (uint)output;
                case GPIO_OE: return (uint)outputEnable;
                case FIFO_ST: return FIFO_ST_RDY;
                default: return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch (offset)
            {
                case GPIO_OUT: output = value; break;
                case GPIO_OUT_SET: output |= value; break;
                case GPIO_OUT_CLR: output &= ~(ulong)value; break;
                case GPIO_OUT_XOR: output ^= value; break;
                case GPIO_OE: outputEnable = value; break;
                case GPIO_OE_SET: outputEnable |= value; break;
                case GPIO_OE_CLR: outputEnable &= ~(ulong)value; break;
                case GPIO_OE_XOR: outputEnable ^= value; break;
                default: return;
            }
            Drive();
        }

        /* A pin shows its output level once the SIO drives it. */
        private void Drive()
        {
            for (int i = 0; i < 32; ++i)
            {
                Connections[i].Set((output & outputEnable & (1ul << i)) != 0);
            }
        }

        private ulong output;
        private ulong outputEnable;
    }
}
