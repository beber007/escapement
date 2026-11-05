/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_RISCV.c: The reset and the dispatch of the external interrupts of the
** RISC-V layer (issue #77), the counterpart of Escapement_CortexMx.c, from which they are
** carried over. The soft timer interrupt's handler and the dynamic memory allocation are
** those of Escapement_Port.c, shared with the Cortex-M.
** Platform version: Hazard3 (RV32IMAC), RP2350.
*/

#include "Escapement.h"

#ifdef ESCAPEMENT_TRACE
   #error "the scheduling trace (make TRACE=1) is not ported to RISC-V yet"
#endif
#ifdef ESCAPEMENT_MEASURE_WCET
   #error "the execution times (make WCET=1) are not measured on RISC-V yet"
#endif

extern void _OSVectorTable(void);   // defined in Escapement_RISCV_a.S

void _OSResetHandler(void); // Called by _OSRamEntry, with sp set
extern void _OSSoftTimerInterrupt(void); // defined in Escapement_Port.c

#define OS_CSR_WRITE(csr,value) \
   __asm volatile ("csrw " OS_CSR_XSTR(csr) ", %0" :: "r"((UINT32)(value)) : "memory")

/* The fault of the last exception, which ExceptionHandler (Escapement_RISCV_a.S) stores
** before it stops: mcause, mepc and mtval, for the debugger to read. */
volatile UINT32 _OSFault[3];

/* _OSResetHandler: The globals set, then the processor: the stack's guard, the trap
** vector, the levels of the interrupts, and main called with interrupts masked. */
void _OSResetHandler(void)
{
  /* Segment start and end address defined in linker file */
  extern UINT32 _sidata, _data, _edata, _bss, _ebss;
  extern UINT32 _OSStackGuard;   // 1 KB, aligned on its size, between _ebss and the stack
  UINT32 *pulSrc, *pulDest;
  UINT32 i;
  _OSDisableInterrupts();
  pulSrc = &_sidata;
  // cppcheck-suppress comparePointers
  for (pulDest = &_data; pulDest < &_edata; )
     *(pulDest++) = *(pulSrc++);
  // cppcheck-suppress comparePointers
  for (pulDest = &_bss; pulDest < &_ebss; )
     *(pulDest++) = 0;
  /* The stack's guard, as the MPU makes one on the RP2040: PMP region 0, NAPOT over the
  ** 1 KB the linker script keeps between the globals and the stack, no access. A PMP
  ** region binds machine mode only if locked, which only a reset undoes; the Hazard3's
  ** PMPCFGM0 binds it without the lock (RP2350 datasheet, Hazard3, PMPCFGM0). The other
  ** regions are left as the bootrom set them. pmpaddr0 reads back its two low bits
  ** cleared, yet the region is the whole 1 KB: on the Pico 2 a store to its first and
  ** last words faulted, one on each side of it did not (2026-11-05). */
  __asm volatile ("csrw pmpaddr0, %0" ::
                  "r"(((UINT32)&_OSStackGuard | (1024u / 2 - 1)) >> 2) : "memory");
  __asm volatile ("csrc pmpcfg0, %0" :: "r"(0xFFu) : "memory");
  __asm volatile ("csrs pmpcfg0, %0" :: "r"(0x18u) : "memory");   // A = NAPOT, no RWX
  _OSCSRSet(OS_CSR_PMPCFGM0, 1u);
  /* Vectored traps; mcycle counting, for the measurements to come. */
  __asm volatile ("csrw mtvec, %0" :: "r"((UINT32)_OSVectorTable | 1u) : "memory");
  __asm volatile ("csrci mcountinhibit, 1" ::: "memory");
  /* Every external interrupt disabled and at the highest level, as the NVIC leaves them
  ** at 0 on the Cortex-M; the soft timer's at the lowest, 0, and enabled. 13 windows of
  ** 4 levels. */
  for (i = 0; i < (OS_IRQ_COUNT + 15) / 16; i++) {
     OS_CSR_WRITE(OS_CSR_MEIEA, i);
     OS_CSR_WRITE(OS_CSR_MEIFA, i);
  }
  for (i = 0; i < OS_IRQ_COUNT / 4; i++)
     OS_CSR_WRITE(OS_CSR_MEIPRA, i | 0xFFFF0000u);
  _OSCSRClear(OS_CSR_MEIPRA, (OS_IRQ_SOFT_TIMER / 4) |
                             ((0xFu << (4 * (OS_IRQ_SOFT_TIMER % 4))) << 16));
  _OSIRQEnable(_OS_IRQ_WORD(OS_IRQ_SOFT_TIMER), _OS_IRQ_BIT(OS_IRQ_SOFT_TIMER));
  /* The external interrupts and the software one, which is the context switch; the
  ** timer of the core (mtip) is not used. */
  __asm volatile ("csrw mie, %0" :: "r"(0x808u) : "memory");
  __asm volatile ("call main");
} /* end of _OSResetHandler */


/* _OSIOHandler: Called by the external interrupt handler of Escapement_RISCV_a.S with the
** number of the interrupt meinext gave it. The soft timer's runs the kernel, any other
** the handler of its descriptor in _OSTabDevice. No reservation of an LL needs clearing:
** the Hazard3 clears it on the way into and out of the trap. */
void _OSIOHandler(UINT32 irq)
{
  typedef struct MinimalIODescriptor {
     void (*PeripheralInterruptHandler)(struct MinimalIODescriptor *);
  } MinimalIODescriptor;
  extern void *_OSTabDevice[];
  MinimalIODescriptor *peripheralIODescriptor;
  if (irq == OS_IRQ_SOFT_TIMER) {
     _OSSoftTimerInterrupt();
     return;
  }
  peripheralIODescriptor = _OSTabDevice[irq];
  #ifdef DEBUG_MODE
     if (peripheralIODescriptor == NULL) {
        _OSDisableInterrupts();
        while (TRUE); // referring to an inexistent ISR descriptor.
     }
  #endif
  // cppcheck-suppress nullPointerRedundantCheck
  peripheralIODescriptor->PeripheralInterruptHandler(peripheralIODescriptor);
} /* end of _OSIOHandler */
