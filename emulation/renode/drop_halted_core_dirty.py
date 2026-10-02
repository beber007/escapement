# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Empties the list of dirty addresses Renode keeps for core 1, which the endurance test
# under the RP2040 models leaves halted (soak_emulated.robot). Renode 1.16.1 appends each
# address the other core writes to the list of every core, for its translated code to be
# flushed, and only a core that runs takes its list: the halted one's grew by some 400,000
# addresses a second of virtual time, until the four instances of 2026-10-01 ran their
# machine out of memory after 41 min to 1 h 47 min, Renode stopping on "Array dimensions
# exceeded" in Machine.AppendDirtyAddresses. Core 1 never runs, so it has no translated
# code to flush, and its list can go. Included after every interval of the run.
machine = monitor.Machine
for cpu in machine.SystemBus.GetCPUs():
    if machine.GetLocalName(cpu) == "cpu1":
        machine.GetNewDirtyAddressesForCore(cpu)
