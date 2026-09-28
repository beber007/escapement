/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
**
** Run by tools/pico2_check.py from SRAM before an image, in place of the step of the
** bootrom a debugger skips. After a reset the debugger stops core 0 at the entry of the
** bootrom, before it seeds the salt of the redundancy coprocessor (RCP) of both cores;
** core 1's bootrom waits for its salt before it answers the launch handshake, and
** OSLaunchCore1 would wait for it forever (pico-bootrom-rp2350 c6cdb17: varm_boot_path.c,
** step 4; arm8_bootrom_rt0.S, wait_for_rcp_salt). The stub does what step 4 does: CP7
** enabled in CPACR, as the bootrom does first, both salts set unless they already are,
** and an event for core 1. The values need not be random: the images do not use the RCP.
** It stops on a BKPT for the debugger to load the image.
*/
    .syntax unified
    .cpu cortex-m33
    .thumb
    .global _start
_start:
    ldr  r0, =0xE000ED88                  /* CPACR */
    ldr  r1, [r0]
    orr  r1, r1, #0xC000                  /* CP7, full access */
    str  r1, [r0]
    dsb
    isb
    ldr  r0, =0x9E3779B9
    ldr  r1, =0x7F4A7C15
    ldr  r2, =0x85EBCA6B
    ldr  r3, =0xC2B2AE35
    mrc  p7, #1, apsr_nzcv, c0, c0, #0    /* canary status: N set once seeded */
    bmi  1f
    mcrr p7, #8, r0, r1, c0               /* salt of core 0 */
    mcrr p7, #8, r2, r3, c1               /* salt of core 1 */
    sev                                   /* core 1 waits for it in WFE */
1:  bkpt #0
    .ltorg
