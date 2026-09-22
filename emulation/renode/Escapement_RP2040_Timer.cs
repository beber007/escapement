//
// Escapement_RP2040_Timer: a fixed copy of Timers.RP2040Timer from matgla/Renode_RP2040, at
// the commit the CI pins (5aca847c9f57ed96603e55d28e8ceeefa09e55f6). escapement_pico.repl
// puts it in place of the original, whose alarms interfere with one another as soon as
// more than one is in use — which the kernel does with two, and the timer events with a
// third:
//   1. writing INTR lowered the interrupt of all four alarms, not of those written with a
//      one;
//   2. writing INTF set the forced state of all four, so that forcing one alarm cleared a
//      forced or pending interrupt of another;
//   3. writing INTE enabled all four and could disable none;
//   4. writing ARMED disarmed the alarms written with a one and re-armed the others;
//   5. an alarm fired when its clock reached the value written, counted from the whole
//      64-bit counter, where the chip compares the lower 32 bits.
// Here each alarm keeps a raw, a forced and an enabled bit, and its interrupt line is
// (raw | forced) & enabled, as the datasheet describes; an alarm fires when the lower 32
// bits of the counter reach its value, so a value already past waits for the next wrap
// of those bits, as on the chip.
//
// And the frequency of the counter, fixed there at the 1 MHz of the chip, is a property:
// the wrap test raises it so that the 2^30 boundary of the kernel clock comes within a
// test.
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
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Peripherals.Miscellaneous;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.Timers
{
    public class EscapementRP2040Timer : RP2040PeripheralBase, IKnownSize
    {
        private enum Registers
        {
            ALARM0 = 0x10,
            ARMED = 0x20,
            TIMERAWH = 0x24,
            TIMERAWL = 0x28,
            INTR = 0x34,
            INTE = 0x38,
            INTF = 0x3c,
            INTS = 0x40
        }

        private const int NumberOfAlarms = 4;

        public EscapementRP2040Timer(Machine machine, ulong address) : base(machine, address)
        {
            IRQs = new GPIO[NumberOfAlarms];
            alarms = new LimitTimer[NumberOfAlarms];
            raw = new bool[NumberOfAlarms];
            forced = new bool[NumberOfAlarms];
            enabled = new bool[NumberOfAlarms];
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
            DefineRegisters();
            Reset();
        }

        public override void Reset()
        {
            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                alarms[i].Enabled = false;
                raw[i] = forced[i] = enabled[i] = false;
                IRQs[i].Unset();
            }
        }

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

        private void DefineRegisters()
        {
            Registers.TIMERAWH.Define(this)
                .WithValueField(0, 32, FieldMode.Read, valueProviderCallback: _ => counter.Value >> 32, name: "TIMERAWH");
            Registers.TIMERAWL.Define(this)
                .WithValueField(0, 32, FieldMode.Read, valueProviderCallback: _ => counter.Value & 0xffffffff, name: "TIMERAWL");

            for (int i = 0; i < NumberOfAlarms; ++i)
            {
                int id = i;
                ((Registers)((long)Registers.ALARM0 + 4 * i)).Define(this)
                    .WithValueField(0, 32, valueProviderCallback: _ => alarmValue[id],
                        writeCallback: (_, value) => Arm(id, (uint)value), name: "ALARM" + i);
            }

            /* Write 1 to disarm; the others are left as they are. */
            Registers.ARMED.Define(this)
                .WithFlags(0, NumberOfAlarms, valueProviderCallback: (i, _) => alarms[i].Enabled,
                    writeCallback: (i, _, value) => { if (value) alarms[i].Enabled = false; }, name: "ARMED")
                .WithReservedBits(NumberOfAlarms, 32 - NumberOfAlarms);

            /* Raw interrupts, write 1 to clear. */
            Registers.INTR.Define(this)
                .WithFlags(0, NumberOfAlarms, valueProviderCallback: (i, _) => raw[i],
                    writeCallback: (i, _, value) => { if (value) { raw[i] = false; Update(i); } }, name: "INTR")
                .WithReservedBits(NumberOfAlarms, 32 - NumberOfAlarms);

            Registers.INTE.Define(this)
                .WithFlags(0, NumberOfAlarms, valueProviderCallback: (i, _) => enabled[i],
                    writeCallback: (i, _, value) => { enabled[i] = value; Update(i); }, name: "INTE")
                .WithReservedBits(NumberOfAlarms, 32 - NumberOfAlarms);

            Registers.INTF.Define(this)
                .WithFlags(0, NumberOfAlarms, valueProviderCallback: (i, _) => forced[i],
                    writeCallback: (i, _, value) => { forced[i] = value; Update(i); }, name: "INTF")
                .WithReservedBits(NumberOfAlarms, 32 - NumberOfAlarms);

            Registers.INTS.Define(this)
                .WithFlags(0, NumberOfAlarms, FieldMode.Read, valueProviderCallback: (i, _) => Pending(i), name: "INTS")
                .WithReservedBits(NumberOfAlarms, 32 - NumberOfAlarms);
        }

        /* Arms an alarm: it fires when the lower 32 bits of the counter next equal the
        ** value, after as many ticks as separate them modulo 2^32. */
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

        private const ulong DefaultFrequency = 1000000;
        private readonly LimitTimer counter;
        private readonly LimitTimer[] alarms;
        private readonly uint[] alarmValue = new uint[NumberOfAlarms];
        private readonly bool[] raw;
        private readonly bool[] forced;
        private readonly bool[] enabled;
    }
}
