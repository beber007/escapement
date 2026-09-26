/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock set-up of the STM32U575 (RM0456, the reference
** manual of the STM32U5: RCC, PWR, FLASH and ICACHE; registers and bits as in
** STMicroelectronics, cmsis-device-u5, stm32u575xx.h).
**
** Reset leaves the core on the MSIS at 4 MHz, in voltage range 4. The system clock goes
** to 160 MHz, the maximum of the chip, from PLL1 fed by the MSIS: 4 MHz x 80 = 320 MHz at
** the VCO, divided by 2. That needs voltage range 1 with the EPOD booster, whose clock is
** the input of PLL1 and must be selected before the booster is enabled, then 4 wait
** states on the flash, and a first step through an AHB prescaler of 2, which ST's
** library takes above 80 MHz to soften the jump in current. The instruction cache hides
** the wait states.
**
** Left to run free, the MSIS is only within about 1 % of its frequency: on the UNO Q the
** kernel's second was 0.48 % too short against Linux's clock, kept by NTP
** (2026-09-26). It is therefore locked on the 32.768 kHz crystal of the board, the LSE
** (MSIPLLEN, MSI PLL mode), as Arduino's Zephyr does (msi-pll-mode in the board's device
** tree; the sequence of its clock_stm32_ll_u5.c). The LSE lives in the backup domain,
** which a reset leaves running: Arduino's firmware has most often started it already.
** Otherwise a crystal takes some time to start; should it not start, the MSIS runs free.
**
** Errata of the chip (ES0499, rev. 12, June 2026; the UNO Q's is revision U): the LSE may
** not start or may stop at the two lowest drives (2.2.3, 2.2.16), hence the medium-high
** one. And the MSI may leave its PLL mode on a failure of the LSE it wrongly detects,
** more likely cold and at a low core voltage (2.2.27): the MSIS then runs free again.
** ST's workaround is taken: the unlock raises line 23 of the EXTI, shared with the CSS
** of the LSE, which the port does not enable, and interrupt 125 (RM0456 rev. 7, tables
** 118, 186 and 189; not on revision X), whose handler turns the PLL mode off and on
** again and counts it; the lock is back within about 1 ms, says the erratum.
**
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#include "Escapement.h"

#define RCC_BASE             0x46020C00
#define RCC_CR               *((volatile UINT32 *)(RCC_BASE + 0x00))
#define RCC_CFGR1            *((volatile UINT32 *)(RCC_BASE + 0x1C))
#define RCC_CFGR2            *((volatile UINT32 *)(RCC_BASE + 0x20))
#define RCC_PLL1CFGR         *((volatile UINT32 *)(RCC_BASE + 0x28))
#define RCC_PLL1DIVR         *((volatile UINT32 *)(RCC_BASE + 0x34))
#define RCC_AHB3ENR          *((volatile UINT32 *)(RCC_BASE + 0x94))
#define RCC_BDCR             *((volatile UINT32 *)(RCC_BASE + 0xF0))

#define RCC_CR_MSIPLLEN      (1u << 3)
#define RCC_CR_MSIPLLSEL     (1u << 6)    /* the PLL mode applies to the MSIS, not the MSIK */
#define RCC_BDCR_LSEON       (1u << 0)
#define RCC_BDCR_LSERDY      (1u << 1)
#define RCC_BDCR_LSEDRV_MASK (3u << 3)
#define RCC_BDCR_LSEDRV_MEDHIGH (2u << 3) /* as Zephyr; the lower drives fail */
#define RCC_BDCR_LSESYSEN    (1u << 7)
#define RCC_BDCR_LSESYSRDY   (1u << 11)

#define EXTI_BASE            0x46022000
#define EXTI_RTSR1           *((volatile UINT32 *)(EXTI_BASE + 0x00))
#define EXTI_RPR1            *((volatile UINT32 *)(EXTI_BASE + 0x0C))
#define EXTI_IMR1            *((volatile UINT32 *)(EXTI_BASE + 0x80))
#define EXTI_MSI_PLL_UNLOCK  (1u << 23)   /* LSECSS or MSI_PLL_UNLOCK */
#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

#define RCC_CR_PLL1ON        (1u << 24)
#define RCC_CR_PLL1RDY       (1u << 25)
#define RCC_CFGR1_SW_PLL1    3u
#define RCC_CFGR1_SWS_MASK   (3u << 2)
#define RCC_CFGR1_SWS_PLL1   (3u << 2)
#define RCC_CFGR2_HPRE_MASK  0xFu
#define RCC_CFGR2_HPRE_DIV2  0x8u
#define RCC_AHB3ENR_PWREN    (1u << 2)

/* PLL1CFGR: source MSIS, input range 4 to 8 MHz, M = 1, booster prescaler 1, output R
** enabled. */
#define PLL1SRC_MSIS         (1u << 0)
#define PLL1RGE_4_8MHZ       (0u << 2)
#define PLL1M(m)             (((m) - 1u) << 8)
#define PLL1MBOOST_DIV1      (0u << 12)
#define PLL1REN              (1u << 18)
#define PLL1CFGR_FIELDS      (0x3u | (0x3u << 2) | (0xFu << 8) | (0xFu << 12) | (1u << 18))

/* PLL1DIVR: N and R as their value less one; P and Q are left as reset sets them. */
#define PLL1N(n)             ((n) - 1u)
#define PLL1R(r)             (((r) - 1u) << 24)
#define PLL1DIVR_FIELDS      (0x1FFu | (0x7Fu << 24))

#define PWR_BASE             0x46020800
#define PWR_VOSR             *((volatile UINT32 *)(PWR_BASE + 0x0C))
#define PWR_VOSR_VOS_RANGE1  (3u << 16)
#define PWR_VOSR_BOOSTEN     (1u << 18)
#define PWR_VOSR_VOSRDY      (1u << 15)
#define PWR_VOSR_BOOSTRDY    (1u << 14)
#define PWR_DBPR             *((volatile UINT32 *)(PWR_BASE + 0x28))
#define PWR_DBPR_DBP         (1u << 0)

/* Some seconds at 4 MHz, a few cycles a turn, for the LSE to start. */
#define LSE_START_TURNS      2000000u

#define FLASH_ACR            *((volatile UINT32 *)(0x40022000 + 0x00))
#define FLASH_ACR_LATENCY_MASK 0xFu
#define FLASH_ACR_PRFTEN     (1u << 8)
#define FLASH_WAIT_STATES    4u   /* range 1, 128 to 160 MHz */

#define ICACHE_CR            *((volatile UINT32 *)(0x40030400 + 0x00))
#define ICACHE_CR_EN         (1u << 0)


typedef struct UNLOCK_ISR_DATA {
  void (*UnlockHandler)(struct UNLOCK_ISR_DATA *);
} UNLOCK_ISR_DATA;

static UNLOCK_ISR_DATA UnlockDescriptor;
static volatile UINT32 MSIRelocks;


/* UnlockHandler: The MSI left its PLL mode (erratum 2.2.27): the mode off and on again. */
static void UnlockHandler(UNLOCK_ISR_DATA *descriptor)
{
  (void)descriptor;
  EXTI_RPR1 = EXTI_MSI_PLL_UNLOCK;
  RCC_CR &= ~RCC_CR_MSIPLLEN;
  RCC_CR |= RCC_CR_MSIPLLEN;
  MSIRelocks += 1;
} /* end of UnlockHandler */


/* OSGetMSIRelocks: The times UnlockHandler locked the MSIS again. */
UINT32 OSGetMSIRelocks(void)
{
  return MSIRelocks;
} /* end of OSGetMSIRelocks */


/* LockMSIS: the LSE started if it is not, then the MSIS locked on it. Returns with the
** MSIS left as it was if the LSE does not start. */
static void LockMSIS(void)
{
  UINT32 turns;
  PWR_DBPR |= PWR_DBPR_DBP;                 // the backup domain, RCC_BDCR, may be written
  while ((PWR_DBPR & PWR_DBPR_DBP) == 0);
  if ((RCC_BDCR & RCC_BDCR_LSERDY) == 0) {
     /* The drive before the oscillator starts, as Zephyr sets it. */
     RCC_BDCR = (RCC_BDCR & ~RCC_BDCR_LSEDRV_MASK) | RCC_BDCR_LSEDRV_MEDHIGH;
     RCC_BDCR |= RCC_BDCR_LSEON;
  }
  for (turns = 0; (RCC_BDCR & RCC_BDCR_LSERDY) == 0 && turns < LSE_START_TURNS; turns += 1);
  if ((RCC_BDCR & RCC_BDCR_LSERDY) != 0) {
     /* The LSE to the clocks beyond the RTC, the MSI among them. */
     RCC_BDCR |= RCC_BDCR_LSESYSEN;
     while ((RCC_BDCR & RCC_BDCR_LSESYSRDY) == 0);
     /* MSIPLLSEL is written while MSIPLLEN is 0, as a reset leaves it. */
     RCC_CR |= RCC_CR_MSIPLLSEL;
     RCC_CR |= RCC_CR_MSIPLLEN;
     /* An unlock to interrupt 125, on its rising edge. */
     UnlockDescriptor.UnlockHandler = UnlockHandler;
     OSSetISRDescriptor(OS_IO_LSECSSD,&UnlockDescriptor);
     EXTI_RTSR1 |= EXTI_MSI_PLL_UNLOCK;
     EXTI_IMR1 |= EXTI_MSI_PLL_UNLOCK;
     NVIC_ISER(OS_IO_LSECSSD) = NVIC_BIT(OS_IO_LSECSSD);
  }
  PWR_DBPR &= ~PWR_DBPR_DBP;
} /* end of LockMSIS */


void OSInitializeSystemClocks(void)
{
  volatile UINT32 i;
  /* The image runs from SRAM (STM32U5_SRAM.ld): the core took its stack and first
  ** instruction from the loader, and the vector table must be named before the first
  ** interrupt, VTOR pointing at the flash after reset. */
  extern void (* const CortexMxVectorTable[])(void);
  *((volatile UINT32 *)0xE000ED08) = (UINT32)CortexMxVectorTable;   // SCB->VTOR
  RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;
  (void)RCC_AHB3ENR;                       // the enable takes effect before PWR is written
  /* The MSIS locked before PLL1 multiplies it. */
  LockMSIS();
  /* The input of PLL1, which is also the booster's clock, before the booster. */
  RCC_PLL1CFGR = (RCC_PLL1CFGR & ~PLL1CFGR_FIELDS) |
                 PLL1SRC_MSIS | PLL1RGE_4_8MHZ | PLL1M(1) | PLL1MBOOST_DIV1 | PLL1REN;
  /* Range 1 and the booster, then wait for both. */
  PWR_VOSR = (PWR_VOSR & ~(3u << 16)) | PWR_VOSR_VOS_RANGE1 | PWR_VOSR_BOOSTEN;
  while ((PWR_VOSR & (PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTRDY)) !=
         (PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTRDY));
  /* Wait states before the clock rises; read back until they hold. */
  FLASH_ACR = (FLASH_ACR & ~FLASH_ACR_LATENCY_MASK) | FLASH_WAIT_STATES | FLASH_ACR_PRFTEN;
  while ((FLASH_ACR & FLASH_ACR_LATENCY_MASK) != FLASH_WAIT_STATES);
  /* 4 MHz x 80 / 2 = 160 MHz. */
  RCC_PLL1DIVR = (RCC_PLL1DIVR & ~PLL1DIVR_FIELDS) | PLL1N(80) | PLL1R(2);
  RCC_CR |= RCC_CR_PLL1ON;
  while ((RCC_CR & RCC_CR_PLL1RDY) == 0);
  /* Onto PLL1 through an AHB prescaler of 2 first, then 1. */
  RCC_CFGR2 = (RCC_CFGR2 & ~RCC_CFGR2_HPRE_MASK) | RCC_CFGR2_HPRE_DIV2;
  RCC_CFGR1 = (RCC_CFGR1 & ~3u) | RCC_CFGR1_SW_PLL1;
  while ((RCC_CFGR1 & RCC_CFGR1_SWS_MASK) != RCC_CFGR1_SWS_PLL1);
  for (i = 0; i < 100; i += 1);            // some microseconds at 80 MHz
  RCC_CFGR2 &= ~RCC_CFGR2_HPRE_MASK;
  ICACHE_CR |= ICACHE_CR_EN;
} /* end of OSInitializeSystemClocks */
