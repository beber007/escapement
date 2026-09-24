//
// Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
// LICENSE at the root of this repository.
//
// Escapement_RP2350_SIO: the part of the SIO of the RP2350 the Escapement examples use, for
// the platform of escapement_pico2.repl. Offsets from the pico-sdk (hardware/regs/sio.h).
// The SIO has no atomic aliases: it lies on the core's own bus, not on APB.
//
// The GPIO outputs, driven through GPIO_OUT and its set, clear and XOR registers, move the
// lines LEDs are connected to.
//
// Each core sees the SIO as its own: CPUID reads its number, and the inter-core FIFO
// registers are its end of two queues, one each way, 4 words deep here, which the
// examples never fill. The core an access comes from is asked of the system bus.
//
// Core 1 starts in the bootrom, which this platform does not run; the model plays its
// part of the launch instead, as the pico-sdk describes it (multicore_reset_core1 and
// multicore_launch_core1_raw in pico_multicore/multicore.c): it has announced itself with
// a 0 in the FIFO to core 0, echoes every word core 0 writes, and once it has received 0,
// 0, 1, the vector table, the stack pointer and the entry point, it starts core 1 there,
// halted until then. A wrong word sends the sequence back to its start. What the model
// does not do is follow a reset of core 1 through the PSM: the handshake happens once.
//
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.CPU;
using Antmicro.Renode.Peripherals.GPIOPort;

namespace Antmicro.Renode.Peripherals.GPIOPort
{
    public class EscapementRP2350SIO : BaseGPIOPort, IDoubleWordPeripheral, IKnownSize
    {
        private const int NumberOfPins = 48;
        private const int FifoDepth = 4;
        private const long CPUID = 0x000, GPIO_OUT = 0x010, GPIO_OUT_SET = 0x018,
                           GPIO_OUT_CLR = 0x020, GPIO_OUT_XOR = 0x028, GPIO_OE = 0x030,
                           GPIO_OE_SET = 0x038, GPIO_OE_CLR = 0x040, GPIO_OE_XOR = 0x048,
                           FIFO_ST = 0x050, FIFO_WR = 0x054, FIFO_RD = 0x058;
        private const uint FIFO_ST_VLD = 0x1, FIFO_ST_RDY = 0x2;

        public EscapementRP2350SIO(IMachine machine, CortexM core1) : base(machine, NumberOfPins)
        {
            this.machine = machine;
            this.core1 = core1;
            Reset();
        }

        public override void Reset()
        {
            base.Reset();
            output = 0;
            outputEnable = 0;
            Drive();
            toCore0.Clear();
            toCore1.Clear();
            toCore0.Enqueue(0);   /* the bootrom of core 1 has announced itself */
            launchStep = 0;
            launched = false;
        }

        public long Size => 0x200;

        public uint ReadDoubleWord(long offset)
        {
            bool onCore1 = FromCore1();
            var incoming = onCore1 ? toCore1 : toCore0;
            var outgoing = onCore1 ? toCore0 : toCore1;
            switch (offset)
            {
                case CPUID: return onCore1 ? 1u : 0u;
                case GPIO_OUT: return (uint)output;
                case GPIO_OE: return (uint)outputEnable;
                case FIFO_ST:
                    return (incoming.Count > 0 ? FIFO_ST_VLD : 0) | (outgoing.Count < FifoDepth ? FIFO_ST_RDY : 0);
                case FIFO_RD: return incoming.Count > 0 ? incoming.Dequeue() : 0;
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
                case FIFO_WR: Push(value); return;
                default: return;
            }
            Drive();
        }

        private bool FromCore1()
        {
            return machine.SystemBus.TryGetCurrentCPU(out var cpu) && cpu == core1;
        }

        private void Push(uint value)
        {
            if (FromCore1())
            {
                if (toCore0.Count < FifoDepth) toCore0.Enqueue(value);
                return;
            }
            if (!launched)
            {
                Bootrom(value);
                return;
            }
            if (toCore1.Count < FifoDepth) toCore1.Enqueue(value);
        }

        /* The bootrom of core 1: echo, and launch at the end of 0, 0, 1, VTOR, SP, entry. */
        private void Bootrom(uint value)
        {
            if (toCore0.Count < FifoDepth) toCore0.Enqueue(value);
            if (launchStep < 3)
            {
                uint expected = launchStep < 2 ? 0u : 1u;
                launchStep = value == expected ? launchStep + 1 : (value == 0 ? 1 : 0);
                return;
            }
            launchWords[launchStep - 3] = value;
            if (++launchStep < 6) return;
            launched = true;
            core1.VectorTableOffset = launchWords[0];
            core1.SetRegister((int)CortexMRegisters.SP, launchWords[1]);
            core1.PC = launchWords[2] & ~1u;
            core1.IsHalted = false;
        }

        /* A pin shows its output level once the SIO drives it. */
        private void Drive()
        {
            for (int i = 0; i < 32; ++i)
            {
                Connections[i].Set((output & outputEnable & (1ul << i)) != 0);
            }
        }

        private readonly IMachine machine;
        private readonly CortexM core1;
        private readonly Queue<uint> toCore0 = new Queue<uint>();
        private readonly Queue<uint> toCore1 = new Queue<uint>();
        private readonly uint[] launchWords = new uint[3];
        private int launchStep;
        private bool launched;
        private ulong output;
        private ulong outputEnable;
    }
}
