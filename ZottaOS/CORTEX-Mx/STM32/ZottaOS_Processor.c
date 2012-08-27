#include "ZottaOS_CortexMx.h"
#include "ZottaOS.h"

#ifdef ZOTTAOS_VERSION_HARD_PA

static UINT8 TargetSpeed   = OS_32MHZ_SPEED;
static UINT8 CurrentSpeed  = OS_32MHZ_SPEED;
static UINT8 CoreVoltageSpeed = OS_32MHZ_SPEED;

const UINT8 _OSSlowdownRatios[] = {32,   //  4 / 32 * 256
                                   128}; // 16 / 32 * 256

typedef struct myRCCDescriptorDef {
     void (*InterruptHandler)(struct myRCCDescriptorDef *);
} myRCCDescriptorDef;

static void RCCInterruptHandler(myRCCDescriptorDef *descriptor);
static void SetCoreVoltage(UINT8 newSpeed);

myRCCDescriptorDef *RCCDescriptor;

#define IRQ_PRIORITY_REGISTER   0xE000E400 // 0xE000E400 to 0xE000E41F (see CortexM3_TRM)

#define IRQ_SET_ENABLE_REGISTER  ((UINT32 *)0xE000E100)
#define IRQ_CLR_ENABLE_REGISTER  ((UINT32 *)0xE000E180)
#define IRQ_SET_PENDING_REGISTER ((UINT32 *)0xE000E200)
#define IRQ_CLR_PENDING_REGISTER ((UINT32 *)0xE000E280)

#define RCC_IRQ_PRIORITY_REGISTER ((UINT8 *)0xE000E405)
#define RCC_BASE          ((UINT32)0x40023800)
#define RCC_CIR_OFFSET    ((UINT32)0x0000000C)
#define RCC_CFGR_OFFSET   ((UINT32)0x00000008)
#define RCC_AHBENR_OFFSET ((UINT32)0x0000001C)
#define RCC_CSR_OFFSET    ((UINT32)0x00000034)

#define GPIOA_BASE           ((UINT32)0x40020000)
#define GPIOA_OTYPER_OFFSET  ((UINT32)0x00000004)
#define GPIOA_OSPEEDR_OFFSET ((UINT32)0x00000008)
#define GPIOA_PUPDR_OFFSET   ((UINT32)0x0000000C)


void OSInitProcessorSpeed(void)
{
  #define PRIORITY     0
  #define SUB_PRIORITY 0
  /* GPIO port A clock enable */
  *(UINT32 *)(RCC_BASE + RCC_AHBENR_OFFSET) |= 0x00000001;
  /* Output the system clock on MCO pin (PA.08) */
  *(UINT32 *)GPIOA_BASE &= 0xFFFCFFFF; // GPIO Alternate function mode
  *(UINT32 *)GPIOA_BASE |= 0x00020000;
  *(UINT32 *)(GPIOA_BASE + GPIOA_OSPEEDR_OFFSET) |= 0x00030000; // Speed mode configuration 40 MHz
  *(UINT32 *)(GPIOA_BASE + GPIOA_OTYPER_OFFSET)  &= 0xFFFFFEFF; // Output mode configuration GPIO_OType_PP
  *(UINT32 *)(GPIOA_BASE + GPIOA_PUPDR_OFFSET) &= 0xFFFCFFFF;   // Pull-up resistor configuration
  *(UINT32 *)(GPIOA_BASE + GPIOA_PUPDR_OFFSET) |= 0x00010000;
  /* HSE selected to output on MCO pin (PA.08)*/
  *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) &= 0x88FFFFFF;
  *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) |= 0x01000000; //0x04000000;
  /* Initialize kernel internal interrupt structure. */
  RCCDescriptor = (myRCCDescriptorDef *)OSMalloc(sizeof(myRCCDescriptorDef));
  RCCDescriptor->InterruptHandler = RCCInterruptHandler;
  OSSetISRDescriptor(OS_IO_RCC,RCCDescriptor);
  /* Enable PLL ready interrupt */
  *(UINT32 *)(RCC_BASE + RCC_CIR_OFFSET) = ((UINT32)0x00001000);
  /* Set RCC interrupt priority */
  *RCC_IRQ_PRIORITY_REGISTER = ((PRIORITY << (PRIGROUP - 3)) | (SUB_PRIORITY & (0x0F >> (7 - PRIGROUP)))) << 4;
  /* Set RCC interrupt pending flag */
  *IRQ_SET_PENDING_REGISTER = 0x20;
}


/* OSGetCurrentSpeed: */
UINT8 OSGetProcessorSpeed(void)
{
   return CurrentSpeed;
}

/* OSGetCurrentSpeed: */
void OSSetProcessorSpeed(UINT8 speed)
{
  if (speed != CurrentSpeed) {
     TargetSpeed = speed;
     *IRQ_SET_ENABLE_REGISTER = 0x20;
  }
}

#define PWR_BASE ((UINT32)0x40007000)
#define PWR_CSR_OFFSET ((UINT32)0x4)


void RCCInterruptHandler(myRCCDescriptorDef *descriptor)
{
  UINT8 localTargetSpeed = TargetSpeed;
  *(UINT32 *)(RCC_BASE + RCC_CIR_OFFSET)  |= ((UINT32)0x00100000); // Clear PLL ready interrupt
  if (CurrentSpeed < localTargetSpeed) { // Increase voltage before increase frequency
     SetCoreVoltage(localTargetSpeed);
     CoreVoltageSpeed = localTargetSpeed;
  }
  else if (CurrentSpeed == localTargetSpeed) {
     *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) |= (UINT32)0x1; // Set SYSCLK to PLL
     if (CoreVoltageSpeed != CurrentSpeed) { // decrease voltage after decrease frequency
        SetCoreVoltage(CurrentSpeed);
        CoreVoltageSpeed = CurrentSpeed;
     }
     /* Set RCC interrupt flag to be ready for next frequency/voltage modification */
     *IRQ_CLR_ENABLE_REGISTER  = 0x20;
     *IRQ_SET_PENDING_REGISTER = 0x20;
     return;
  }
  //else if (CurrentSpeed > localTargetSpeed) -> decrease frequency before decrease voltage
  /*** Modify PLL configuration ***/
  *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) &= (UINT32)0xFFFFFFFE; // Set SYSCLK to HSE
  while (((*(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET)) & (UINT32)0xC) != (UINT32)0x8);
  *(UINT32 *)RCC_BASE &= (UINT32)0xFEFFFFFF; // Disable PLL
  *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) &= (UINT32)0xFF03FFFF;
  switch (localTargetSpeed) {
  case OS_32MHZ_SPEED: // RCC_CFGR_PLLMUL16 and RCC_CFGR_PLLDIV2
     *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) |= (UINT32)0x00540000;
     break;
  case OS_16MHZ_SPEED: // RCC_CFGR_PLLMUL8 and RCC_CFGR_PLLDIV2
     *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) |= (UINT32)0x004C0000;
     break;
  case OS_4MHZ_SPEED: // RCC_CFGR_PLLMUL4 and RCC_CFGR_PLLDIV4
     *(UINT32 *)(RCC_BASE + RCC_CFGR_OFFSET) |= (UINT32)0x00C40000;
     break;
  default:
     break;
  }
  CurrentSpeed = localTargetSpeed;
  *(UINT32 *)RCC_BASE |= (UINT32)0x01000000; // Enable PLL
}


/* SetCoreVoltage: . */
void SetCoreVoltage(UINT8 newSpeed)
{
  UINT16 localPWR_CR;
  localPWR_CR = *(UINT16 *)PWR_BASE;
  localPWR_CR &= (UINT16)0xE7FF;
  switch (newSpeed) {
  case OS_4MHZ_SPEED:
     localPWR_CR |= ((UINT16)0x1800);
     break;
  case OS_16MHZ_SPEED:
     localPWR_CR |= ((UINT16)0x1000);
     break;
  case OS_32MHZ_SPEED:
     localPWR_CR |= ((UINT16)0x0800);
     break;
  default:
     break;
  }
  *(UINT16 *)PWR_BASE = localPWR_CR & 0xFFFFFFF3;
  while(((*(UINT16 *)(PWR_BASE + PWR_CSR_OFFSET)) & (UINT16)0x0010) != 0);
} /* end of SetCoreVoltage */


#endif /* ZOTTAOS_VERSION_HARD_PA */
