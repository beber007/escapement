//
// Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
// LICENSE at the root of this repository.
//
// Escapement_RP2350_Timer: TIMER0 of the RP2350 for the platform of escapement_pico2.repl,
// of which no model exists. The logic is that of Escapement_RP2040_Timer.cs, the fixed copy
// of the RP2040 timer of matgla/Renode_RP2040 (MIT, Mateusz Stadnik): each of the four
// alarms keeps a raw, a forced and an enabled bit, its interrupt line is
// (raw | forced) & enabled, and it fires when the lower 32 bits of the 64-bit counter reach
// its value. What differs is the chip: INTR, INTE, INTF and INTS lie two words further
// than on the RP2040, after LOCKED and SOURCE (pico-sdk, hardware/regs/timer.h), and the
// model stands on its own, decoding the atomic aliases of the register block itself —
// +0x1000 XOR, +0x2000 set, +0x3000 clear — which the firmware uses on INTE and INTF.
//
// The counter runs at 1 MHz, the tick the TICKS block makes of the 12 MHz crystal; the
// model does not follow TICKS, whose programming it ignores. Frequency is a property so
// that a test can bring the 2^30 boundary of the kernel clock within reach.
//
// Original source: emulation/peripherals/timer/rp2040_timer.cs,
// Copyright (c) 2025 Mateusz Stadnik, MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
using System;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.Timers
{
    public class EscapementRP2350Timer : IDoubleWordPeripheral, IKnownSize
    {
        private const int NumberOfAlarms = 4;
        private const long ALARM0 = 0x10, ARMED = 0x20, TIMERAWH = 0x24, TIMERAWL = 0x28,
                           DBGPAUSE = 0x2C, LOCKED = 0x34, SOURCE = 0x38, INTR = 0x3C,
                           INTE = 0x40, INTF = 0x44, INTS = 0x48;
        private const ulong DefaultFrequency = 1000000;

        public EscapementRP2350Timer(IMachine machine)
        {
            IRQs = new GPIO[NumberOfAlarms];
            alarms = new LimitTimer[NumberOfAlarms];
            counter = new LimitTimer(machine.ClockSource, DefaultFrequency, this, "Counter", limit: ulong.MaxValue,
                direction: Direction.Ascending, eventEnabled: false, enabled: true, workMode: WorkMode.Periodic);
            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                int id = i;
                IRQs[i] = new GPIO();
                alarms[i] = new LimitTimer(machine.ClockSource, DefaultFrequency, this, "Alarm" + i,
                    direction: Direction.Ascending, enabled: false, workMode: WorkMode.OneShot, eventEnabled: true);
                alarms[i].LimitReached += () => OnAlarm(id);
            }
            Reset();
        }

        public void Reset()
        {
            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                alarms[i].Enabled = false;
                raw[i] = forced[i] = enabled[i] = false;
                alarmValue[i] = 0;
                IRQs[i].Unset();
            }
            dbgpause = 0x7;
            source = 0;
        }

        public long Size => 0x4000;

        /* Counts per second of the counter and of the alarms, 1 MHz on the chip. */
        public ulong Frequency
        {
            get { return counter.Frequency; }
            set
            {
                counter.Frequency = value;
                foreach (var alarm in alarms)
                {
                    alarm.Frequency = value;
                }
            }
        }

        public GPIO[] IRQs { get; private set; }
        public GPIO IRQ0 => IRQs[0];
        public GPIO IRQ1 => IRQs[1];
        public GPIO IRQ2 => IRQs[2];
        public GPIO IRQ3 => IRQs[3];

        public uint ReadDoubleWord(long offset)
        {
            return Read(offset & 0xFFF);
        }

        /* The aliases combine the value written with the register as it reads, then write
        ** the result through the register's own semantics. Faithful for the plain
        ** read-write registers, INTE and INTF, the only ones the firmware writes through
        ** an alias; not for INTR and ARMED, whose ones clear and disarm, which it never
        ** does. */
        public void WriteDoubleWord(long offset, uint value)
        {
            long register = offset & 0xFFF;
            switch ((offset >> 12) & 0x3)
            {
                case 0: Write(register, value); break;
                case 1: Write(register, Read(register) ^ value); break;
                case 2: Write(register, Read(register) | value); break;
                case 3: Write(register, Read(register) & ~value); break;
            }
        }

        private uint Read(long register)
        {
            if (register >= ALARM0 && register < ALARM0 + 4 * NumberOfAlarms)
            {
                return alarmValue[(register - ALARM0) / 4];
            }
            switch (register)
            {
                case ARMED: return Bits(i => alarms[i].Enabled);
                case TIMERAWH: return (uint)(counter.Value >> 32);
                case TIMERAWL: return (uint)counter.Value;
                case DBGPAUSE: return dbgpause;
                case SOURCE: return source;
                case INTR: return Bits(i => raw[i]);
                case INTE: return Bits(i => enabled[i]);
                case INTF: return Bits(i => forced[i]);
                case INTS: return Bits(Pending);
                default: return 0;
            }
        }

        private void Write(long register, uint value)
        {
            if (register >= ALARM0 && register < ALARM0 + 4 * NumberOfAlarms)
            {
                Arm((int)((register - ALARM0) / 4), value);
                return;
            }
            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                bool bit = (value & (1u << i)) != 0;
                switch (register)
                {
                    case ARMED: if (bit) alarms[i].Enabled = false; break;        /* 1 disarms */
                    case INTR: if (bit) { raw[i] = false; Update(i); } break;       /* 1 clears */
                    case INTE: enabled[i] = bit; Update(i); break;
                    case INTF: forced[i] = bit; Update(i); break;
                }
            }
            if (register == DBGPAUSE) dbgpause = value & 0x7;
            if (register == SOURCE) source = value & 0x1;
        }

        private uint Bits(Func<int, bool> bit)
        {
            uint value = 0;
            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                if (bit(i)) value |= 1u << i;
            }
            return value;
        }

        /* Arms an alarm: it fires when the lower 32 bits of the counter next equal the
        ** value, after as many ticks as separate them modulo 2^32 (a value equal to the
        ** counter fires on the next tick). */
        private void Arm(int id, uint value)
        {
            uint ticks = value - (uint)counter.Value;
            alarmValue[id] = value;
            alarms[id].Enabled = false;
            alarms[id].Value = 0;
            alarms[id].Limit = ticks == 0 ? 1 : ticks;
            alarms[id].Enabled = true;
        }

        private void OnAlarm(int id)
        {
            alarms[id].Enabled = false;
            raw[id] = true;
            Update(id);
        }

        private bool Pending(int id)
        {
            return (raw[id] || forced[id]) && enabled[id];
        }

        private void Update(int id)
        {
            IRQs[id].Set(Pending(id));
        }

        private readonly LimitTimer counter;
        private readonly LimitTimer[] alarms;
        private readonly uint[] alarmValue = new uint[NumberOfAlarms];
        private readonly bool[] raw = new bool[NumberOfAlarms];
        private readonly bool[] forced = new bool[NumberOfAlarms];
        private readonly bool[] enabled = new bool[NumberOfAlarms];
        private uint dbgpause;
        private uint source;
    }
}
