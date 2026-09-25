# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The exclusive monitor of the RP2350, played in place of Renode's for the byte LL/SC pair
# of the kernel. Renode 1.17 lets a STREX succeed while its own core's reservation stands
# and the location still holds what the LDREX read, and a plain store by the other core
# leaves that reservation alone; where both cores touch a reserved location it also slows
# down a thousandfold or stalls (docs/emulation.md). The RP2350, with ACTLR.EXTEXCLALL set
# on both cores, does otherwise (datasheet, 2.1.6; Armv8-M, B9):
#
#   - each core holds one reservation, on a granule of 16 bytes;
#   - any write by the other core to that granule clears it, whatever the value written;
#   - the core's own plain writes leave it;
#   - an exception taken by the core clears it, entry and return alike;
#   - the SC succeeds only with the reservation in place, and clears it.
#
# escapement_pico2.robot hooks the entry of OSUINT8_LL and OSUINT8_SC on both cores to
# ll() and sc(), which do the access themselves and return to the caller, so that no
# LDREXB or STREXB runs; the start of every exception to exception(); and the first LL on
# a granule installs write hooks on it that call write(). The state is kept in the
# AppDomain, which the hooks of both cores share.
#
# Two settings, for the checks that must fail: MONITOR = "local" lets the other core's
# writes leave the reservation, as monitors local to each core do without EXTEXCLALL; and
# SPURIOUS = n makes every n-th SC of core 1 fail for no reason, as an interrupt between
# its LL and its SC would.

import System

STORE = System.AppDomain.CurrentDomain
GRANULE = ~0xF


def get(key, default=None):
    value = STORE.GetData("rp2350." + key)
    return default if value is None else value


def put(key, value):
    STORE.SetData("rp2350." + key, value)


def count(key):
    put(key, get(key, 0) + 1)


def setup(monitor="global", spurious=0):
    put("monitor", monitor)
    put("every", spurious)


def core(cpu):
    return int(cpu.MultiprocessingId)


def register(cpu, n):
    return int(str(cpu.GetRegisterUnsafe(n)), 0)


def ret(cpu, value):
    """Return from the hooked function with value in r0."""
    from Antmicro.Renode.Peripherals.CPU import RegisterValue
    cpu.SetRegisterUnsafe(0, RegisterValue.Create(value, 32))
    cpu.PC = cpu.LR


def ll(cpu):
    k, address = core(cpu), register(cpu, 0)
    granule = address & GRANULE
    put("reservation%d" % k, granule)
    watch(cpu.Bus, granule)
    count("ll%d" % k)
    ret(cpu, cpu.Bus.ReadByte(address))


def sc(cpu):
    k, address, value = core(cpu), register(cpu, 0), register(cpu, 1) & 0xFF
    held = get("reservation%d" % k) == address & GRANULE
    put("reservation%d" % k, None)
    spurious = get("every", 0)
    if held and k == 1 and spurious:
        count("sc1.attempts")
        if get("sc1.attempts") % spurious == 0:
            held = False
            count("sc1.spurious")
    if held:
        put("writer", k)
        cpu.Bus.WriteByte(address, value)     # clears the other core's, through write()
        put("writer", None)
        count("sc%d.ok" % k)
    else:
        count("sc%d.failed" % k)
    ret(cpu, 1 if held else 0)


def exception(cpu):
    k = core(cpu)
    if get("reservation%d" % k) is not None:
        count("cleared.exception%d" % k)
    put("reservation%d" % k, None)


def write(cpu, address):
    """A write reached the granule: the other core loses its reservation there."""
    k = get("writer")
    if k is None:
        k = core(cpu) if cpu is not None else None
    if k is None:
        return
    other = 1 - k
    if get("reservation%d" % other) == address & GRANULE:
        if get("monitor") == "local":
            count("kept.write%d" % other)     # what a global monitor would have cleared
            return
        put("reservation%d" % other, None)
        count("cleared.write%d" % other)


def watch(bus, granule):
    watched = get("watched", "")
    if ("%x," % granule) in watched:
        return
    put("watched", watched + "%x," % granule)
    from Antmicro.Renode.Peripherals.Bus import SysbusAccessWidth, Access, BusHookDelegate

    def hook(cpu, address, width, value):
        write(cpu, address)
    for address in range(granule, granule + 16):
        for width, size in ((SysbusAccessWidth.Byte, 1), (SysbusAccessWidth.Word, 2),
                            (SysbusAccessWidth.DoubleWord, 4)):
            if address % size == 0:
                bus.AddWatchpointHook(address, width, Access.Write, BusHookDelegate(hook))


def report():
    keys = ("ll0", "ll1", "sc0.ok", "sc0.failed", "sc1.ok", "sc1.failed", "sc1.spurious",
            "cleared.write0", "cleared.write1", "kept.write0", "kept.write1",
            "cleared.exception0", "cleared.exception1")
    return ", ".join("%s %d" % (key, get(key, 0)) for key in keys)
