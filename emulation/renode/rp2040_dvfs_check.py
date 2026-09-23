# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Checks, from write hooks on the bus, that the DVFS driver of the RP2040 never lets clk_sys
# run faster than the core voltage allows, and counts what it did. escapement_pico.robot
# installs the hooks; the results go to scratch registers of the VREG model declared in
# escapement_pico.repl, where the test reads them back.
#
# The hooks run before the write reaches the peripheral: the register being written still
# holds its old value, the new one is in `value`.

VREG = 0x40064000
CLK_SYS_CTRL = 0x4000803C
PLL_FBDIV = 0x40028008
PLL_PRIM = 0x4002800C

VOLTAGE_CHANGES = VREG + 0x100   # writes that changed VSEL
LOWEST_VSEL = VREG + 0x104       # lowest VSEL written
VIOLATIONS = VREG + 0x108        # clk_sys faster than the voltage allows
SPEEDS_SEEN = VREG + 0x10C       # bit 1: 50 MHz, bit 2: 125 MHz, from the PLL

# Highest clk_sys, in MHz, each VSEL is used for. From 1.05 V up the datasheet guarantees
# the 133 MHz of the chip; below, these are the pairs ESCAPEMENT_RP2040_UNDERVOLT claims.
UNDERVOLT_MAX_MHZ = {0x7: 12, 0x8: 50}
# The 12 MHz point is left out: clk_sys also goes through the reference, briefly, on every
# change between the other two.
SPEED_BITS = {50: 2, 125: 4}


def vsel(vreg):
    return (vreg >> 4) & 0xF


def max_mhz(v):
    if v >= 0xA:
        return 133
    return UNDERVOLT_MAX_MHZ.get(v, 0)


def sys_mhz(bus, ctrl=None, prim=None):
    """Frequency of clk_sys in MHz, or None when it is not taken from the reference or
    from the system PLL. The reference counts as 12 MHz: on the ring oscillator the bootrom
    starts on, it runs slower still."""
    if ctrl is None:
        ctrl = bus.ReadDoubleWord(CLK_SYS_CTRL)
    if ctrl & 1 == 0:
        return 12
    if (ctrl >> 5) & 7 != 0:
        return None
    if prim is None:
        prim = bus.ReadDoubleWord(PLL_PRIM)
    postdiv = ((prim >> 16) & 7) * ((prim >> 12) & 7)
    if postdiv == 0:
        return None
    return 12 * bus.ReadDoubleWord(PLL_FBDIV) // postdiv


def increment(bus, address):
    bus.WriteDoubleWord(address, bus.ReadDoubleWord(address) + 1)


def check(bus, mhz, v):
    if mhz is not None and mhz > max_mhz(v):
        increment(bus, VIOLATIONS)


def on_vreg(bus, value):
    new = vsel(value)
    if new != vsel(bus.ReadDoubleWord(VREG)):
        increment(bus, VOLTAGE_CHANGES)
    if new < bus.ReadDoubleWord(LOWEST_VSEL):
        bus.WriteDoubleWord(LOWEST_VSEL, new)
    check(bus, sys_mhz(bus), new)


def on_clk_sys_ctrl(bus, value):
    mhz = sys_mhz(bus, ctrl=value)
    if mhz in SPEED_BITS:
        bus.WriteDoubleWord(SPEEDS_SEEN, bus.ReadDoubleWord(SPEEDS_SEEN) | SPEED_BITS[mhz])
    check(bus, mhz, vsel(bus.ReadDoubleWord(VREG)))


def on_pll_prim(bus, value):
    check(bus, sys_mhz(bus, prim=value), vsel(bus.ReadDoubleWord(VREG)))
