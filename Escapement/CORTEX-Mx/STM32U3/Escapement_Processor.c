/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock set-up of the STM32U385 (RM0487 rev. 3, the reference
** manual of the STM32U3: RCC 10, PWR 9, FLASH 7, ICACHE 8; addresses and bits checked
** against STMicroelectronics, cmsis-device-u3, stm32u385xx.h). Written before the board
** came: the sequence follows the manual, and only Renode has run it (docs/stm32u3.md).
**
** Reset leaves the core on the MSIS at 12 MHz, from MSIRC1, the slower of the MSI's two
** RCs, divided by 2; the regulator in voltage range 2; the flash at 1 wait state (10.2.3,
** p. 401 and 405; 9.3.3, p. 333; 7.3.3, p. 228). The chip has no PLL for its system
** clock, which comes from the MSIS, HSI16 or the HSE (p. 405): 96 MHz, its maximum, is
** MSIRC0, the faster RC, divided by 1 (table 101, p. 401).
**
** Left to run free the MSI is within about 1 % of its frequency, which will not do for
** the kernel's microsecond (on the U5 the second was 0.48 % short, docs/stm32u5.md). In
** its PLL mode the MSI locks on the LSE, the 32.768 kHz crystal: MSIRC0 then runs at 2930
** periods of it, 96.010 MHz, 107 ppm fast, and so does the kernel's clock, TIM2 being
** clocked from HCLK (table 102, p. 403; "Timer clock", p. 408). Locked on an HSE of 16 MHz
** it would run at 96.0 MHz exactly, but the NUCLEO-U385RG-Q comes without that crystal
** (X2, UM3062), and the MSI takes no other: an 8 MHz clock from the ST-LINK will not do,
** its input must be 16 MHz (RCC_ICSCR1.MSIHSINDIV, p. 423). Not written until a board
** has one.
**
** The sequence, from the manual:
**  1. the LSE: its drive, then LSEON, LSERDY; then LSESYSEN and LSESYSRDY, without which
**     the LSE reaches only the RTC, not the MSI (p. 404-405, RCC_BDCR p. 471-474);
**  2. the MSIS onto MSIRC0 divided by 8, 12 MHz, the same frequency, which range 2 runs
**     (table 104, p. 406), and the MSIK with it, so that MSIRC1 may stop: ST recommends
**     one RC for both (p. 401). The PLL mode of an RC needs an output on it, ready (p. 402);
**  3. its PLL mode on the LSE (MSIPLL0SEL = 0, as reset leaves it), then MSIPLL0RDY;
**  4. above 24 MHz the EPOD booster, fed by the MSIS, in either range, and above 48 MHz
**     range 1 before it, as 9.3.3 orders them (p. 333-334): BOOSTSEL, R1EN, R1RDY,
**     BOOSTEN, BOOSTRDY, then the flash's wait states, then the frequency;
**  5. the MSIS's divider for OS_SYSTEM_CLOCK_HZ.
**
** The PLL mode leaves the MSI if the LSE stops or is disturbed; an interrupt of the RCC
** then puts it back, as the manual says to (p. 402). That is no erratum on this chip
** (ES0626 rev. 3 has none on the MSI), but erratum 2.2.1 disturbs the LSE when PC13
** toggles, and B1, the user button of the NUCLEO, is on PC13.
**
** Steps 4 and 5 are _OSRaiseSystemClock, which the idle task runs again on waking from
** Stop 2 (Escapement_Stop2.c). The chip wakes on the MSIS in range 2, its divider kept
** unless it gave more than 48 MHz, in which case the hardware sets it to 48 (9.3.5,
** p. 351; 10.3, p. 415; RCC_ICSCR1, p. 421); the booster, left on through Stop 2 as a
** wake-up at 48 MHz requires (p. 405), is kept. So the steps start from what the
** registers read, PWR_VOSR, RCC_ICSCR1 and RCC_CFGR4, not from what reset leaves: RM0487
** does not say whether R1EN reads 1 or 0 after a Stop 2 entered in range 1, and either
** is taken. The MSI's PLL mode is cleared with the MSI in Stop 2 (MSIPLL0RDY, p. 418) and
** locks again after it, which takes tSTAB unless MSIPLL0FAST is set (p. 415, 419):
** _OSWaitMSILock waits for it, bounded.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#include "Escapement.h"

#define RCC_BASE             0x40030C00
#define RCC_CR               *((volatile UINT32 *)(RCC_BASE + 0x000))
#define RCC_ICSCR1           *((volatile UINT32 *)(RCC_BASE + 0x008))
#define RCC_CFGR4            *((volatile UINT32 *)(RCC_BASE + 0x028))
#define RCC_CIER             *((volatile UINT32 *)(RCC_BASE + 0x050))
#define RCC_CICR             *((volatile UINT32 *)(RCC_BASE + 0x058))
#define RCC_AHB1ENR2         *((volatile UINT32 *)(RCC_BASE + 0x094))
#define RCC_BDCR             *((volatile UINT32 *)(RCC_BASE + 0x110))

/* RCC_CR (p. 417-420). */
#define RCC_CR_MSISRDY       (1u << 2)
#define RCC_CR_MSIKRDY       (1u << 4)
#define RCC_CR_MSIPLL0EN     (1u << 6)
#define RCC_CR_MSIPLL0FAST   (1u << 8)
#define RCC_CR_MSIPLL0RDY    (1u << 10)
/* RCC_ICSCR1 (p. 420-423): MSISSEL 0 takes MSIRC0, MSISDIV divides it by 1, 2, 4 or 8;
** the same for the MSIK; MSIRGSEL makes them count, rather than RCC_CSR's. */
#define RCC_ICSCR1_MSISSEL   (1u << 31)
#define RCC_ICSCR1_MSISDIV(d) ((UINT32)(d) << 29)
#define RCC_ICSCR1_MSISDIV_MASK (3u << 29)
#define RCC_ICSCR1_MSIKSEL   (1u << 28)
#define RCC_ICSCR1_MSIKDIV(d) ((UINT32)(d) << 26)
#define RCC_ICSCR1_MSIKDIV_MASK (3u << 26)
#define RCC_ICSCR1_MSIRGSEL  (1u << 23)
#define RCC_ICSCR1_MSIPLL0SEL (1u << 21)   /* 0: the LSE */
#define MSIDIV_96MHZ         0u
#define MSIDIV_48MHZ         1u
#define MSIDIV_24MHZ         2u
#define MSIDIV_12MHZ         3u
/* RCC_CFGR4 (p. 429): the booster's clock, the MSIS, which the hardware divides itself. */
#define RCC_CFGR4_BOOSTSEL_MASK 3u
#define RCC_CFGR4_BOOSTSEL_MSIS 1u
/* RCC_CIER and RCC_CICR (p. 430-433): the MSI leaving its PLL mode on the LSE. */
#define RCC_CIER_MSIPLLUIE   (1u << 8)
#define RCC_CICR_MSIPLLUC    (1u << 8)
#define RCC_AHB1ENR2_PWREN   (1u << 2)    /* p. 444-445 */
/* RCC_BDCR (p. 471-474). */
#define RCC_BDCR_LSEON       (1u << 0)
#define RCC_BDCR_LSERDY      (1u << 1)
#define RCC_BDCR_LSEDRV_MASK (3u << 3)
#define RCC_BDCR_LSEDRV_MEDHIGH (2u << 3)
#define RCC_BDCR_LSESYSEN    (1u << 7)
#define RCC_BDCR_LSESYSRDY   (1u << 11)

#define PWR_BASE             0x40030800
#define PWR_VOSR             *((volatile UINT32 *)(PWR_BASE + 0x0C))
#define PWR_DBPR             *((volatile UINT32 *)(PWR_BASE + 0x28))
/* PWR_VOSR (p. 368-369): R1EN and R2EN at opposite values, R1RDY and R2RDY, the booster. */
#define PWR_VOSR_R1EN        (1u << 0)
#define PWR_VOSR_R2EN        (1u << 1)
#define PWR_VOSR_BOOSTEN     (1u << 8)
#define PWR_VOSR_R1RDY       (1u << 16)
#define PWR_VOSR_R2RDY       (1u << 17)
#define PWR_VOSR_BOOSTRDY    (1u << 24)
#define PWR_DBPR_DBP         (1u << 0)    /* p. 373 */

#define FLASH_ACR            *((volatile UINT32 *)(0x40022000 + 0x00))
#define FLASH_ACR_LATENCY_MASK 0xFu
#define FLASH_ACR_PRFTEN     (1u << 8)

#define ICACHE_CR            *((volatile UINT32 *)(0x40030400 + 0x00))
#define ICACHE_CR_EN         (1u << 0)

#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

/* Some seconds at 12 MHz, a few cycles a turn, for the LSE to start; then for the MSI to
** lock on it, tSTAB, some 0.8 ms in the datasheet (DS14830), which no board has measured
** yet: the image goes on unlocked past it rather than hang. */
#define LSE_START_TURNS      2000000u
#define LOCK_TURNS           200000u

/* The voltage range, the booster, the flash's wait states and the MSIS's divider of each
** system clock: range 1 above 48 MHz, which range 2 does not run (9.3.3, p. 333; table
** 104, p. 406); the booster above 24 MHz in either range (table 103, p. 406); the wait
** states of table 43 (p. 227) for the STM32U375/385, 0 up to 32 MHz in range 1, 16 in
** range 2, one more for each step of that size. */
#if OS_SYSTEM_CLOCK_HZ == 96000000u
   #define CLOCK_RANGE       1u
   #define FLASH_WAIT_STATES 2u
   #define MSIS_DIV          MSIDIV_96MHZ
#elif OS_SYSTEM_CLOCK_HZ == 48000000u
   #define CLOCK_RANGE       2u
   #define FLASH_WAIT_STATES 2u
   #define MSIS_DIV          MSIDIV_48MHZ
#elif OS_SYSTEM_CLOCK_HZ == 24000000u
   #define CLOCK_RANGE       2u
   #define FLASH_WAIT_STATES 1u
   #define MSIS_DIV          MSIDIV_24MHZ
#elif OS_SYSTEM_CLOCK_HZ == 12000000u
   #define CLOCK_RANGE       2u
   #define FLASH_WAIT_STATES 0u
   #define MSIS_DIV          MSIDIV_12MHZ
#else
   #error "OS_SYSTEM_CLOCK_HZ: 96, 48, 24 or 12 MHz on the STM32U3"
#endif
#define CLOCK_BOOST          (OS_SYSTEM_CLOCK_HZ > 24000000u)


typedef struct UNLOCK_ISR_DATA {
  void (*UnlockHandler)(struct UNLOCK_ISR_DATA *);
} UNLOCK_ISR_DATA;

void (*_OSIdleHook)(void) = NULL;

static UNLOCK_ISR_DATA UnlockDescriptor;
static volatile UINT32 MSIRelocks;
static BOOL Locked;

/* With OS_CLOCK_TIMES, the cycle counter of the DWT, which the image starts, read at each
** step of the clock set-up, for ClockU3 to report how long each took on the board: the
** LSE's start, LSESYSRDY, the lock, R1RDY, BOOSTRDY and the MSIS at its frequency
** (docs/stm32u3.md). Without it, nothing. */
#ifdef OS_CLOCK_TIMES
   UINT32 _OSClockTimes[8];
   #define CLOCK_MARK(step) (_OSClockTimes[step] = *((volatile UINT32 *)0xE0001004))
#else
   #define CLOCK_MARK(step) ((void)0)
#endif


/* UnlockHandler: The MSI left its PLL mode; cleared and set again, as RM0487 says to put it
** back (p. 402; RCC_CIFR, p. 432). The flag is cleared first, so that an unlock that comes
** while the mode is set again raises the interrupt anew. */
static void UnlockHandler(UNLOCK_ISR_DATA *descriptor)
{
  (void)descriptor;
  RCC_CICR = RCC_CICR_MSIPLLUC;
  RCC_CR &= ~RCC_CR_MSIPLL0EN;
  RCC_CR |= RCC_CR_MSIPLL0EN;
  MSIRelocks += 1;
} /* end of UnlockHandler */


/* OSGetMSIRelocks: The times UnlockHandler put the MSI back in its PLL mode. */
UINT32 OSGetMSIRelocks(void)
{
  return MSIRelocks;
} /* end of OSGetMSIRelocks */


/* OSMSILocked: Whether LockMSI found the LSE and put the MSI in its PLL mode on it. */
BOOL OSMSILocked(void)
{
  return Locked;
} /* end of OSMSILocked */


/* SetMSIDividers: The MSIS and the MSIK from MSIRC0, each divided as given, written while
** both are ready: neither may change while it is on and not ready (RCC_ICSCR1, p. 421-422).
** Then until the MSIS is ready at its new frequency. */
static void SetMSIDividers(UINT32 msis, UINT32 msik)
{
  while ((RCC_CR & (RCC_CR_MSISRDY | RCC_CR_MSIKRDY)) != (RCC_CR_MSISRDY | RCC_CR_MSIKRDY));
  RCC_ICSCR1 = (RCC_ICSCR1 & ~(RCC_ICSCR1_MSISSEL | RCC_ICSCR1_MSISDIV_MASK |
                               RCC_ICSCR1_MSIKSEL | RCC_ICSCR1_MSIKDIV_MASK)) |
               RCC_ICSCR1_MSIRGSEL | RCC_ICSCR1_MSISDIV(msis) | RCC_ICSCR1_MSIKDIV(msik);
  while ((RCC_CR & RCC_CR_MSISRDY) == 0);
} /* end of SetMSIDividers */


/* LockMSI: The LSE started if it is not, then MSIRC0 locked on it. Returns with MSIRC0
** running free if the LSE does not start. */
static void LockMSI(void)
{
  UINT32 turns;
  PWR_DBPR |= PWR_DBPR_DBP;                 // RCC_BDCR, in the backup domain, may be written
  while ((PWR_DBPR & PWR_DBPR_DBP) == 0);
  if ((RCC_BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) == 0) {
     /* The drive is written while the oscillator is off, before LSEON (p. 404, 473):
     ** medium-high, as on the U5, where the two lowest failed (ES0499 2.2.3, 2.2.16);
     ** ES0626 has no such erratum, and the crystal of the board decides. */
     RCC_BDCR = (RCC_BDCR & ~RCC_BDCR_LSEDRV_MASK) | RCC_BDCR_LSEDRV_MEDHIGH;
     RCC_BDCR |= RCC_BDCR_LSEON;
  }
  CLOCK_MARK(0);
  for (turns = 0; (RCC_BDCR & RCC_BDCR_LSERDY) == 0 && turns < LSE_START_TURNS; turns += 1);
  CLOCK_MARK(1);
  if ((RCC_BDCR & RCC_BDCR_LSERDY) != 0) {
     /* The LSE to the RCC's functions, the MSI's PLL mode among them, ready after two of
     ** its cycles (p. 405, 473). */
     RCC_BDCR |= RCC_BDCR_LSESYSEN;
     while ((RCC_BDCR & RCC_BDCR_LSESYSRDY) == 0);
     CLOCK_MARK(2);
     /* MSIPLL0SEL is 0, the LSE, as reset leaves it; written once the LSE is ready
     ** (p. 422). The hardware refuses MSIPLL0EN before LSERDY (p. 419). */
     RCC_ICSCR1 &= ~RCC_ICSCR1_MSIPLL0SEL;
     RCC_CR |= RCC_CR_MSIPLL0EN;
     for (turns = 0; (RCC_CR & RCC_CR_MSIPLL0RDY) == 0 && turns < LOCK_TURNS; turns += 1);
     CLOCK_MARK(3);
     Locked = (RCC_CR & RCC_CR_MSIPLL0EN) != 0;
     #ifdef OS_MSIPLL_FAST
        /* The PLL mode kept through Stop 2, the MSI powered and gated there, so that it
        ** wakes locked (p. 403, 415): what that costs in Stop 2 against the wait for the
        ** lock at each wake-up, only the board can tell (make FAST=1). It takes effect
        ** from the first return from switch-off, not this first lock (p. 419). */
        RCC_CR |= RCC_CR_MSIPLL0FAST;
     #endif
     /* An unlock to the RCC's interrupt, 9 (table 106, p. 415-416; table 134, p. 627). */
     UnlockDescriptor.UnlockHandler = UnlockHandler;
     OSSetISRDescriptor(OS_IO_RCC,&UnlockDescriptor);
     RCC_CICR = RCC_CICR_MSIPLLUC;
     RCC_CIER |= RCC_CIER_MSIPLLUIE;
     NVIC_ISER(OS_IO_RCC) = NVIC_BIT(OS_IO_RCC);
  }
  PWR_DBPR &= ~PWR_DBPR_DBP;
} /* end of LockMSI */


void OSInitializeSystemClocks(void)
{
  /* The image runs from SRAM (STM32U3_SRAM.ld): the core took its stack and first
  ** instruction from the loader, and the vector table must be named before the first
  ** interrupt, VTOR pointing at the flash after reset. */
  extern void (* const CortexMxVectorTable[])(void);
  *((volatile UINT32 *)0xE000ED08) = (UINT32)CortexMxVectorTable;   // SCB->VTOR
  /* PWR's clock, then 2 cycles of its bus before PWR is written (p. 410): the read back
  ** of the RCC's register takes them. */
  RCC_AHB1ENR2 |= RCC_AHB1ENR2_PWREN;
  (void)RCC_AHB1ENR2;
  /* MSIRC0 at 12 MHz for both outputs, its PLL mode on the LSE. */
  SetMSIDividers(MSIDIV_12MHZ,MSIDIV_12MHZ);
  LockMSI();
  _OSRaiseSystemClock();
  ICACHE_CR |= ICACHE_CR_EN;                // for an image in the flash (8.7.1, p. 320)
} /* end of OSInitializeSystemClocks */


/* _OSRaiseSystemClock: From the MSIS on MSIRC0 in range 2, at 12 MHz after reset or at
** what Stop 2 left, to OS_SYSTEM_CLOCK_HZ, each step taken only if the registers do not
** already read it done. */
void _OSRaiseSystemClock(void)
{
  #if CLOCK_BOOST
     /* The booster's clock before the booster (p. 405, 429): the MSIS, which stays on
     ** as the system clock, as it must while the booster is (caution, p. 334). Stop 2
     ** keeps it, as it keeps every register (9.3.5, p. 350). */
     if ((RCC_CFGR4 & RCC_CFGR4_BOOSTSEL_MASK) != RCC_CFGR4_BOOSTSEL_MSIS)
        RCC_CFGR4 = (RCC_CFGR4 & ~RCC_CFGR4_BOOSTSEL_MASK) | RCC_CFGR4_BOOSTSEL_MSIS;
  #endif
  CLOCK_MARK(4);
  #if CLOCK_RANGE == 1
     /* Range 1, from range 2 once it is ready: R1EN and R2EN change only then, and
     ** together, a write of both to the same value being ignored (p. 369). An R1EN that
     ** reads 1 already, as Stop 2 may leave it, is only waited for: written again it would
     ** change nothing. */
     if ((PWR_VOSR & PWR_VOSR_R1EN) == 0) {
        while ((PWR_VOSR & (PWR_VOSR_R1RDY | PWR_VOSR_R2RDY)) != PWR_VOSR_R2RDY);
        PWR_VOSR = (PWR_VOSR & ~(PWR_VOSR_R1EN | PWR_VOSR_R2EN)) | PWR_VOSR_R1EN;
     }
     while ((PWR_VOSR & PWR_VOSR_R1RDY) == 0);
  #endif
  CLOCK_MARK(5);
  #if CLOCK_BOOST
     /* Set again, it changes nothing after Stop 2, which keeps it on. */
     PWR_VOSR |= PWR_VOSR_BOOSTEN;
     while ((PWR_VOSR & PWR_VOSR_BOOSTRDY) == 0);
  #endif
  CLOCK_MARK(6);
  /* The wait states before the clock rises, read back until they hold (7.3.3, p. 228);
  ** at 12 MHz the one reset sets is taken off, the clock not changing. The prefetch stays
  ** off: the images run from SRAM, and it is only for 1 wait state or more (p. 228). The
  ** 48 MHz of a wake-up from 96 need the same 2 in range 2 as 96 in range 1 (table 43). */
  FLASH_ACR = (FLASH_ACR & ~(FLASH_ACR_LATENCY_MASK | FLASH_ACR_PRFTEN)) | FLASH_WAIT_STATES;
  while ((FLASH_ACR & FLASH_ACR_LATENCY_MASK) != FLASH_WAIT_STATES);
  /* The MSIS at its frequency; the MSIK stays at 12 MHz, which no part of the port uses. */
  if ((RCC_ICSCR1 & (RCC_ICSCR1_MSISSEL | RCC_ICSCR1_MSISDIV_MASK | RCC_ICSCR1_MSIRGSEL)) !=
      (RCC_ICSCR1_MSISDIV(MSIS_DIV) | RCC_ICSCR1_MSIRGSEL))
     SetMSIDividers(MSIS_DIV,MSIDIV_12MHZ);
  CLOCK_MARK(7);
} /* end of _OSRaiseSystemClock */


/* _OSWaitMSILock: After a wake-up from Stop 2, until the MSI's PLL mode is locked again,
** MSIPLL0RDY, or ticks of the caller's clock, LPTIM1, have passed; TRUE if they did, the
** MSIS then running free, within about 1 %, until it locks. Nothing to wait for if the
** PLL mode is off, the LSE never having started. */
BOOL _OSWaitMSILock(UINT16 (*clock)(void), UINT16 ticks)
{
  UINT16 start;
  if ((RCC_CR & RCC_CR_MSIPLL0EN) == 0)
     return FALSE;
  start = clock();
  while ((RCC_CR & RCC_CR_MSIPLL0RDY) == 0 && (UINT16)(clock() - start) < ticks);
  return (RCC_CR & RCC_CR_MSIPLL0RDY) == 0;
} /* end of _OSWaitMSILock */
