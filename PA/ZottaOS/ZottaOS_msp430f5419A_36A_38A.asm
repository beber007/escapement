; Copyright (c) 2006-2011 MIS Institute of the HEIG affiliated to the University of
; Applied Sciences of Western Switzerland. All rights reserved.
; IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED SCIENCES
; OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDEN-
; TAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMEN-
; TATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED SCIENCES OF
; WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
; THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZER-
; LAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVI-
; DED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG AND NOR THE
; UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION TO PROVIDE
; MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
;
; File ZottaOS_msp430f5419A_36A_38A.asm: Holds msp430f5419A_36A_38A dependent assembler
;                                        implemented for ZottaOSHard functions.
; Created on June 22, 2011, by ZottaOS MSP430 Configurator Tool.
; Authors: MIS-TIC

; Include device definitions and ZottaOS configuration files
  .cdecls C,LIST,"msp430.h"
  .cdecls C,LIST,"ZottaOS_msp430f5419A_36A_38A.h"
  .cdecls C,LIST,"ZottaOS.h"


; ******************************************
; Global Variables Shared with ZottaOSHard.c
; ******************************************
  .ref _OSNoSaveContext
  .ref _OSTime
  .ref _OSActiveTask
  .ref _OSQueueHead
  .ref _OSStackBasePointer
  .ref _OSLLReserveBit
  .ref _OSIdleSP


; ***************************************
; Global Variables Shared with TI library
; ***************************************
  .ref __cinit__
  .ref __STACK_END


; **********************************
; Imported Functions From TI library
; **********************************
  .ref _args_main
  .ref _auto_init
  .ref _system_pre_init


; *************************************
; Imported Functions From ZottaOSHard.c
; *************************************
  .ref _OSTimerInterruptHandler


; **********************************
; Implemented and Exported Functions
; **********************************
  .global _c_int00
  .global _OSEndInterruptClrNoSaveCtx
  .global _OSStartNextReadyTask
  .global OSMalloc
  .global OSInitializeSystemClocks
  .global _OSEnableSoftTimerInterrupt
  .global OSSetProcessorSpeed
  .global OSGetProcessorSpeed
  
; **********************************
; Constants declaration
; **********************************

; Claude Beber Es-qu'il faut tjs être 0,5 MHz en dessous de la valeur voulue ?

  ; MSP430 with PMM have 4 operating frequency dynamic settings. Although it is pre-
  ; ferable to have the highest frequency operating with the lowest core voltage, these
  ; setting are at the limit of a malfunction as a slight frequency variation may entail
  ; a higher voltage. Through experimentation, we found that the frequency jitter is
  ; approximately +/- 0.5 MHz. Hence the chosen frequencies are the highest minus this
  ; jitter. */
  .sect ".const"
FreqTabUCSCTL1:
  .word DCORSEL_6     ; ~7.5 MHz, 1.4V
  .word DCORSEL_7     ; ~11.5 MHz, 1.6V
  .word DCORSEL_7     ; ~19.5 MHz, 1.8V
  .word DCORSEL_7     ; ~24.5 MHz, 1.9V
FreqTabUCSCTL2:
  .word 488+FLLD_5    ; ~7.5 MHz, 1.4V
  .word 731+FLLD_5    ; ~11.5 MHz, 1.6V
  .word 609+FLLD_5    ; ~19.5 MHz, 1.8V  
  .word 762+FLLD_5    ; ~24.5 MHz, 1.9V 
FreqTabUCSCTL2Cal:
  .word 30+FLLD_5     ; ~7.5 MHz, 1.4V
  .word 45+FLLD_5     ; ~11.5 MHz, 1.6V
  .word 38+FLLD_5     ; ~19.5 MHz, 1.8V  
  .word 47+FLLD_5     ; ~24.5 MHz, 1.9V 
FreqTabDIVM:
  .byte DIVM_3 & ~DIVM__4 ; ~7.5 MHz, 1.4V
  .byte DIVM_3 & ~DIVM__4 ; ~11.5 MHz, 1.6V
  .byte DIVM_3 & ~DIVM__2 ; ~19.5 MHz, 1.8V
  .byte DIVM_3 & ~DIVM__2 ; ~24.5 MHz, 1.9V

   ; Table of speed slowdowns * 256 for all frequency settings except for the maximum
   ; one. These values are computed using the relation Speed/Max_Speed = A/256, where
   ; A is the number in the table. To compensate for frequency jitter A is chosen to
   ; be 7,11 and 19 instead of 11.5,11.5 and 19.5. */
  .if POWER_MANAGEMENT != NONE
     .sect ".const"
     .global _OSSlowdownRatios
_OSSlowdownRatios:
     .byte 74     ;  7MHz / 24MHz * 256
     .byte 117    ; 11MHz / 24MHz * 256
     .byte 202    ; 19MHz / 24MHz * 256
  .endif
  
  
; **********************************
; Variables declaration
; **********************************

  .bss FreqTabUCSCTL0,8,2   
  .bss ptFreqTabUCSCTL2,2,2
  .bss TargetSpeed,1,1 ; Speed to bring the processor
  .bss AdjustFreq,1,1
  .bss CurrentSpeed,1,1
  
  
; *****************
; Macro Definitions
; *****************

; SaveCtx: Saves the context of the interrupted task or interrupt routine onto the stack
; and sets the new current stack base pointer of this interrupt.
SaveCtx .macro
  push &_OSStackBasePointer         ; Save base stack pointer and registers onto the stack
  .if $defined(SAVE_20_BIT_REGISTERS)
     pushx.a r15
     pushx.a r12
     pushx.a r11
     pushx.a r10
     pushx.a r9
     pushx.a r8
     pushx.a r7
     pushx.a r6
     pushx.a r5
     pushx.a r4
  .else
     push r15
     .if $isdefed("__LARGE_CODE_MODEL__")
        pushx.a r12
     .else
        push r12
     .endif
     push r11
     push r10
     push r9
     push r8
     push r7
     push r6
     push r5
     push r4
  .endif
  mov sp,&_OSStackBasePointer
  .endm
; end of SaveCtx

; default_handler: Default entry point of interrupt handlers with a single source vector.
; This macro sets up the common code of the interrupt handler before before it disables
; its source register.
default_handler .macro IntName
  .sect ".:IntName:"
  .short IntName
  .sect ".text:_isr"
IntName:
  .if $defined(SAVE_20_BIT_REGISTERS)
     pushx.a r13                    ; Save working register R13
     pushx.a r14                    ; Save working register R14
  .else
     push r13                       ; Save working register R13
     push r14                       ; Save working register R14
  .endif
  cmp &_OSIdleSP,sp                 ; Claude IdleTask befor ?
  jne :IntName:_do_not_prempt_IdleTask
  .if $defined(SAVE_20_BIT_REGISTERS)
     mov 8(sp),r14
  .else
     mov 4(sp),r14
  .endif
  and #0xD0,r14                     ; Claude IdleTask en LPM3 ?
  cmp #0xD0,r14
  jne :IntName:_IdleTask_not_in_LPM3
  dec &UCSCTL5                      ; Claude Pour s'assuer que lors du réveil la frèq. ne dépasse la fréq. max du CPU
                                    ; Selon bug PMM11 décrit dans le document SLAZ057F
:IntName:_IdleTask_not_in_LPM3:
  .if $defined(SAVE_20_BIT_REGISTERS)
     bic #0xD0,8(sp)                ; Disable LPM3/LPM1 in IdleTask
  .else
     bic #0xD0,4(sp)                ; Disable LPM3/LPM1 in IdleTask
  .endif
:IntName:_do_not_prempt_IdleTask:
  .endm
; end of default_handler


; ***********************************************************************************
; Table of devices used by the interrupt handler and the peripheral devices to obtain
; the appropriate ISR. For MSP430 there are at most OS_IO_MAX different devices to 
; handle.
; ***********************************************************************************
  .if OS_IO_MAX != 0
     .global _OSTabDevice
     .bss _OSTabDevice,OS_IO_MAX,2
  .endif

; ************************************************************************************
; Interrupt Handlers (one for each source and ordered from highest to lowest priority)
; ************************************************************************************

; _c_int00: Reset interrupt handler (int63), which replaces the version provided with TI
; Code Composer library.
  .sect ".reset"
  .short _c_int00
  .sect ".text:_isr"
_c_int00:
  mov.w #__STACK_END,r15
  sub.w #OSMALLOC_INTERNAL_HEAP_SIZE,r15
  mov.w r15,SP
  .if $isdefed("__LARGE_CODE_MODEL__")
     calla #_system_pre_init
  .else
     call #_system_pre_init
  .endif
  tst.w r12
  jeq _c_int00SkipAutoInit
  mov.w #__cinit__,r12
  .if $isdefed("__LARGE_CODE_MODEL__")
     calla #_auto_init
  .else
     call #_auto_init
  .endif
_c_int00SkipAutoInit:
  mov.w #__STACK_END,&_OSStackBasePointer
  .if $isdefed("__LARGE_CODE_MODEL__")
     calla #_args_main
  .else
     call #_args_main
  .endif
; end of c_int00 (reset interrupt handler int63)

; System non-maskable interrupt handler (int62).
;Claude Beber SYSNMI_SVSMH utilisé uniquement en mode debug car que faire si Vcc est trop bas ?
  .sect ".int62"
  .short int62
  .sect ".text:_isr"
int62:
  add &SYSSNIV,pc                   ; Add offset to jump table
  .if $defined(DEBUG_MODE)
     jmp UndefHandlerSYSNMI         ; Vector 0: no interrupt
     jmp UndefHandlerSYSNMI         ; Vector 2: SVM low-side
     jmp SYSNMI_SVSMH               ; Vector 4: SVM high-side
     jmp SYSNMI_SVSMLDLY            ; Vector 6: SVS and SVM low-side delay expired
     jmp UndefHandlerSYSNMI         ; Vector 8: SVS and SVM high-side delay expired
     jmp UndefHandlerSYSNMI         ; Vector 10: Vacant memory access
     jmp UndefHandlerSYSNMI         ; Vector 12: JTAG mailbox input
     jmp UndefHandlerSYSNMI         ; Vector 14: JTAG mailbox output
     jmp UndefHandlerSYSNMI         ; Vector 16: Low-side voltage level reached
     jmp UndefHandlerSYSNMI         ; Vector 18: High-side voltage level reached
     jmp UndefHandlerSYSNMI         ; Vector 20: Reserved for future extensions
SYSNMI_SVSMLDLY:     
  .else
     nop                            ; Vector 0: no interrupt
     nop                            ; Vector 2: SVM low-side
     nop                            ; Vector 4: SVM high-side
     ; Fall into handler for low-side delay expired (no JMP required).
  .endif
  bic #SVSMLDLYIE,&PMMRIE           ; disable SVSMLDLYIFG interrupt
  ; System NMI: Sets the operating frequency once the core voltage level reaches its tar-
  ; get setting. Because the voltage setting takes time to reach its target level, and
  ; busy waiting in this non maskable service routine is very inefficient, processing of
  ; the interrupt is relegated to a low-priority handler, I/O port P2 pin 6.
  bis.b #0x40,&P2IFG                ; Generate a port 2 pin 6 interrupt
  reti
  .if $defined(DEBUG_MODE)
SYSNMI_SVSMH:
     dint
     nop
     jmp SYSNMI_SVSMH               ; ; Vcc is too low for a Vcore increase
UndefHandlerSYSNMI:
     dint
     nop
     jmp UndefHandlerSYSNMI         ; Error: loop indefinitely
  .endif
; end of system non-maskable interrupt handler (int62)

; User non-maskable interrupt handler (int61).
  .if $defined(DEBUG_MODE)
  .sect ".int61"
  .short UndefHandler61
  .sect ".text:_isr"
UndefHandler61:
     jmp UndefHandler61             ; Loop indefinitely
  .endif
; end of user non-maskable interrupt handler (int61)

; Timer0 B capture/compare CC0 interrupt handler (int60).
  .if $defined(DEBUG_MODE)
  .sect ".int60"
  .short UndefHandler60
  .sect ".text:_isr"
UndefHandler60:
     jmp UndefHandler60             ; Loop indefinitely
  .endif
; end of timer0 B CC0 interrupt handler (int60)

; Timer0 B CC1-6: Capture/compare registers CC1 through CC6 and overflow interrupt
; handler (int59).
  .if $defined(DEBUG_MODE)
  .sect ".int59"
  .short UndefHandler59
  .sect ".text:_isr"
UndefHandler59:
     jmp UndefHandler59             ; Loop indefinitely
  .endif
; end of Timer0 B CC1-6 interrupt handler (int59)

; Watchdog timer interrupt handler (int58).
  .if $defined(DEBUG_MODE)
  .sect ".int58"
  .short UndefHandler58
  .sect ".text:_isr"
UndefHandler58:
     jmp UndefHandler58             ; Loop indefinitely
  .endif
; end of watchdog timer interrupt handler (int58)

; Universal serial communication interface A0 (USCI_A0) receive/transmit interrupt
; handlers (int57) with UART or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int57"
  .short UndefHandler57
  .sect ".text:_isr"
UndefHandler57:
     jmp UndefHandler57             ; Loop indefinitely
  .endif
; end of USCI A0 receive/transmit interrupt handlers (int57)

; Universal serial communication interface (USCI) B0 interrupt handlers (int56) with I2C
; state changes or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int56"
  .short UndefHandler56
  .sect ".text:_isr"
UndefHandler56:
     jmp UndefHandler56             ; Loop indefinitely
  .endif
; end of USCI B0 receive/transmit interrupt handlers (int56)

; 12-bit analog-to-digital converter (ADC12) interrupt handler (int55) with up to 16
; independent ADC samples.
  .if $defined(DEBUG_MODE)
  .sect ".int55"
  .short UndefHandler55
  .sect ".text:_isr"
UndefHandler55:
     jmp UndefHandler55             ; Loop indefinitely
  .endif
; end of ADC12 interrupt handler (int55)

; Timer0 A (int53): Interval timer0 A interrupt handler used by ZottaOS to propagate
; an emulated software interrupt defined as I/O port 2 pin number 7.
  .sect ".int53"
  .short Timer0A
  .sect ".text:_isr"
Timer0A:
  push r13                          ; Save a register to do timer operations
  mov.w &TA0CCR0,r13                ; Backup comparator to update wall clock later
  mov.w #0xFFFE,&TA0CCR0            ; Set comparator to INFINITY16
  bit #0x00,&TA0IV                  ; Clear timer interrupt flag
  eint                              ; Reenable other interrupts
  inc r13                           ; Elapsed time is actually TA0CCR0 + 1
  add r13,&_OSTime                  ; Update the current wall clock
  adc &_OSTime + 2                  ; Take care of carry because _OSTime is on 32 bits
  pop r13                           ; Restore saved working register
  bis.b #BIT7,&P2IFG                ; Generate a port 2 interrupt to complete the interrupt
  reti                              ; Return to previous interrupt or continue to port P2
; end of timer0 A interrupt handler (int53)

; Universal serial communication interface A2 (USCI_A2) receive/transmit interrupt
; handlers (int52) with UART or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int52"
  .short UndefHandler52
  .sect ".text:_isr"
UndefHandler52:
     jmp UndefHandler52             ; Loop indefinitely
  .endif
; end of USCI A2 receive/transmit interrupt handlers (int52)

; Universal serial communication interface (USCI) B2 interrupt handlers (int51) with I2C
; state changes or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int51"
  .short UndefHandler51
  .sect ".text:_isr"
UndefHandler51:
     jmp UndefHandler51             ; Loop indefinitely
  .endif
; end of USCI B2 receive/transmit interrupt handlers (int51)

; Three-channel DMA interrupt handlers (int50).
  .if $defined(DEBUG_MODE)
  .sect ".int50"
  .short UndefHandler50
  .sect ".text:_isr"
UndefHandler50:
     jmp UndefHandler50             ; Loop indefinitely
  .endif
; end of DMA interrupt handlers (int50)

; Timer1 A capture/compare CC0 interrupt handler (int49).
  .if $defined(DEBUG_MODE)
  .sect ".int49"
  .short UndefHandler49
  .sect ".text:_isr"
UndefHandler49:
     jmp UndefHandler49             ; Loop indefinitely
  .endif
; end of timer1 A CC0 interrupt handler (int49)

; Timer1 A CC1-2: Timer1 A capture/compare registers CC1 through CC2 and overflow
; interrupt handler (int48).
  .if $defined(DEBUG_MODE)
  .sect ".int48"
  .short UndefHandler48
  .sect ".text:_isr"
UndefHandler48:
     jmp UndefHandler48             ; Loop indefinitely
  .endif
; end of timer1 A CC1-2 interrupt handler (int48)

; I/O Port P1 interrupt handler (int47).
  .if $defined(DEBUG_MODE)
  .sect ".int47"
  .short UndefHandler47
  .sect ".text:_isr"
UndefHandler47:
     jmp UndefHandler47             ; Loop indefinitely
  .endif
; end of I/O Port P1 interrupt handler (int47)

; Universal serial communication interface A1 (USCI_A1) receive/transmit interrupt
; handlers (int46) with UART or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int46"
  .short UndefHandler46
  .sect ".text:_isr"
UndefHandler46:
     jmp UndefHandler46             ; Loop indefinitely
  .endif
; end of USCI A1 receive/transmit interrupt handlers (int46)

; Universal serial communication interface (USCI) B1 interrupt handlers (int45) with I2C
; state changes or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int45"
  .short UndefHandler45
  .sect ".text:_isr"
UndefHandler45:
     jmp UndefHandler45             ; Loop indefinitely
  .endif
; end of USCI B1 receive/transmit interrupt handlers (int45)

; Universal serial communication interface A3 (USCI_A3) receive/transmit interrupt
; handlers (int44) with UART or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int44"
  .short UndefHandler44
  .sect ".text:_isr"
UndefHandler44:
     jmp UndefHandler44             ; Loop indefinitely
  .endif
; end of USCI A3 receive/transmit interrupt handlers (int44)

; Universal serial communication interface (USCI) B3 interrupt handlers (int43) with I2C
; state changes or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int43"
  .short UndefHandler43
  .sect ".text:_isr"
UndefHandler43:
     jmp UndefHandler43             ; Loop indefinitely
  .endif
; end of USCI B3 receive/transmit interrupt handlers (int43)

; I/O Port P2: Digital I/O interrupt handler for port 2. Because software interrupt can
; be propagated by the interval timer (pin 7), we need to check the source of the inter-
; ruption before jumping to the interrupt routine for the other (user) sources of this
; interrupt.
; Note: To emulate a prioritized interruption with I/O port P2 (int42) being at the low-
; est level, we proceed as follows: When an interruption other than int42 occurs, int42
; are masked when entering the ISR. Finally when exiting the ISR, int42 are unmasked. To
; allow for nested ISRs, the current state of int42 is stored on the stack when entering
; the ISR and restored when exiting the ISR.
  default_handler int42
  push.b P2IE                       ; Save mask Port 2
  add &P2IV,pc                      ; Add offset to jump table
  .if $defined(DEBUG_MODE)
     jmp UndefHandlerPort2          ; Vector 0: No interrupt
     jmp Port2_0                    ; Vector 2: Port 2 pin 0
     jmp UndefHandlerPort2          ; Vector 4: Port 2 pin 1
     jmp UndefHandlerPort2          ; Vector 6: Port 2 pin 2
     jmp UndefHandlerPort2          ; Vector 8: Port 2 pin 3
     jmp UndefHandlerPort2          ; Vector 10: Port 2 pin 4
     jmp UndefHandlerPort2          ; Vector 12: Port 2 pin 5
     jmp PMMHandler                 ; Vector 14: Port 2 pin 6
  .else
     nop                            ; Vector 0: No interrupt
     jmp Port2_0                    ; Vector 2: Port 2 pin 0
     nop                            ; Vector 4: Port 2 pin 1
     nop                            ; Vector 6: Port 2 pin 2
     nop                            ; Vector 8: Port 2 pin 3
     nop                            ; Vector 10: Port 2 pin 4
     nop                            ; Vector 12: Port 2 pin 5
     jmp PMMHandler                 ; Vector 14: Port 2 pin 6
  .endif
; Vector 16 (Port 2 pin 7): ZottaOS system clock timer.
  bic.b #BIT7,&P2IE                 ; Disable I/O Port 2 pin 7
  eint
  ; Before jumping to the service routine of the timer, we need to assert that an appli-
  ; cation task is not in the middle of preparing to do a context switch. If this is the
  ; case, we need to supersede its actions and never return to that task instance.
  mov.w &_OSQueueHead,r13           ; R13 <- _OSQueueHead
  mov.w @r13,&_OSActiveTask         ; _OSActiveTask <- _OSQueueHead->Next[READYQ]
  ; A task is in the middle of removing itself from the ready queue if its current state
  ; is set to STATE_ZOMBIE. If this is the case, the ready queue may be in an incoherent
  ; state and needs to be adjusted. The context of this task should also not be saved.
  mov.w &_OSActiveTask,r14          ; R14 <- _OSActiveTask
  bit.b #2,4(r14)           ; set Z (SR) bit if (_OSActiveTask->TaskState & STATE_ZOMBIE)
  jeq DonotUpdateReadyQueue ; Is an application task removing itself from the ready q.?
  ; Make the ready queue coherent, i.e. finish the work started by the interrupted task
  ; and don't save its context since it is terminated.
  ; _OSActiveTask = _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
  mov.w @r14,0(r13)           ; _OSQueueHead->Next[READYQ] <- _OSActiveTask->Next[READYQ]
  mov.w @r13,&_OSActiveTask         ; _OSActiveTask <- _OSQueueHead->Next[READYQ]
  jmp DontSaveCtx                   ; Don't save the context of the interrupted
DonotUpdateReadyQueue:
  ; The application task has either completely removed itself from the ready queue or has
  ; simply not finished.
  ; _OSNoSaveContext is TRUE in the former case and FALSE in the latter
  cmp.b #TRUE,&_OSNoSaveContext     ; Save the context of the application task?
  jeq DontSaveCtx                   ; Jump if _OSNoSaveContext = TRUE
  SaveCtx                           ; Save context of the application task
  .if $isdefed("__LARGE_CODE_MODEL__")
     bra #_OSTimerInterruptHandler  ; Continue to the timer service routine
  .else
     br #_OSTimerInterruptHandler   ; Continue to the timer service routine
  .endif
DontSaveCtx:
  mov &_OSStackBasePointer,sp       ; Wipe-out the stack of the task
  .if $isdefed("__LARGE_CODE_MODEL__")
     bra #_OSTimerInterruptHandler  ; Continue to the timer service routine
  .else
     br #_OSTimerInterruptHandler   ; Continue to the timer service routine
  .endif
PMMHandler:
; Interrupt service routine to adjust core voltage and operating frequency once the core
; voltage has reached its designated target. Frequency and core voltage settings are done
; in steps to keep track of the current and to allow fast switching between target speeds.
; Also by proceeding in this fashion, we are guaranteed to apply an operating frequency
; only when the core voltage can sustain the frequency. Note that when increasing the
; operating frequency, the core voltage first needs to attain its target level before the
; operating frequency can be set. This is not the case when decreasing the operating
; frequency.
  bic.b #BIT6|BIT7,&P2IE            ; Disable I/O Port 2 pin 6 interrupt
  eint
  .if $defined(SAVE_20_BIT_REGISTERS)
     pushx.a r12                    ; Save working register R12
  .else
     push r12                       ; Save working register R12
  .endif
  tst.b &AdjustFreq
  jn PMMInterrupt_SkipAdjustFreq  
  mov.b &AdjustFreq,r12
  ; Claude calcul l'index des FreqTabs
  clr r13
  mov.b r12,r13
  rla r13
  ; Claude retrouve la valeur désirée de FreqTabUCSCTL2   
  mov &ptFreqTabUCSCTL2,r14
  add r13,r14 
  dint
  nop
  bis #SCG0,sr                      ; Disable the FLL control loop
  mov @r14,&UCSCTL2                 ; Set FLLN
  mov FreqTabUCSCTL0(R13),&UCSCTL0  ; Set DCO and MOD
  mov FreqTabUCSCTL1(R13),&UCSCTL1  ; Set RSEL
  bis.b #DIVM_3,&UCSCTL5            ; Set DIVM
  bic.b FreqTabDIVM(R12),&UCSCTL5   
  bic #SCG0,sr                      ; Enable the FLL control loop
  eint
  mov.b r12,&CurrentSpeed           ; Update CurrentSpeed variable
  mov.b #-1,&AdjustFreq              
PMMInterrupt_SkipAdjustFreq:
  cmp.b &TargetSpeed,&PMMCTL0_L     ; Test if the voltage level is already set
  jeq PMMInterrupt_Equal
  mov.b &PMMCTL0_L,r12
  jl PMMInterrupt_HIGHER
  dec.b r12
  mov.b r12,&CurrentSpeed           ; Update CurrentSpeed variable
  clr r13
  mov.b r12,r13
  rla r13
  mov &ptFreqTabUCSCTL2,r14
  add r13,r14 
  dint
  nop
  bis #SCG0,sr                      ; Disable the FLL control loop
  bis.b #DIVM_3,&UCSCTL5            ; Set DIVM
  bic.b FreqTabDIVM(R12),&UCSCTL5   
  mov @r14,&UCSCTL2                 ; Set FLLN
  clr &UCSCTL0
  mov FreqTabUCSCTL1(R13),&UCSCTL1 ; Set RSEL
  mov FreqTabUCSCTL0(R13),&UCSCTL0 ; Set DCO and MOD
  bic #SCG0,sr                      ; Enable the FLL control loop
  eint
  mov.b r12,&PMMCTL0_L              ; Set VCore to new level
  bic #SVSMLDLYIFG,&PMMIFG          ; Clear already set flags
  .if $defined(DEBUG_MODE)
     mov.b r12,&SVSMHCTL_L          ; Set SVM high side to new level
  .endif
  mov.b r12,&SVSMLCTL_L             ; Set SVM low side to new level
  bis #SVSMLDLYIE,&PMMRIE           ; Enabel PMM low side delay expired interrupt
  jmp PMMInterrupt_END
PMMInterrupt_HIGHER:
  inc.b r12
  .if $defined(DEBUG_MODE)
     bic #SVSMHDLYIFG,&PMMIFG       ; Clear already set flags
     bis.b #SVMHFP_H,&SVSMHCTL_H    ; Enable full-performance mode
     mov.b r12,&SVSMHCTL_L          ; Set SVM highside to new level
PMMInterrupt_Wait:                  ; Wait until SVM highside is settled
     bit #SVSMHDLYIFG,&PMMIFG
     jz PMMInterrupt_Wait
     bic.b #SVMHFP_H,&SVSMHCTL_H    ; Disable full-performance mode
  .endif
  mov.b r12,&AdjustFreq
  mov.b r12,&PMMCTL0_L              ; Set VCore to new level
  bic #SVSMLDLYIFG,&PMMIFG          ; Clear already set flags
  mov.b r12,&SVSMLCTL_L             ; Set SVM low side to new level
  bis #SVSMLDLYIE,&PMMRIE           ; Enabel PMM low side delay expired interrupt
  jmp PMMInterrupt_END
PMMInterrupt_Equal:
  mov sp,r12                        ; Claude IdleTask befor ?
  add #0x4,r12
  .if $defined(SAVE_20_BIT_REGISTERS)
     incd r12
  .endif
  cmp &_OSIdleSP,r12
  jne PMMInterrupt_do_not_set_LPM3
  cmp.b #OS_25MHZ_SPEED,&CurrentSpeed
  jne PMMInterrupt_do_not_set_LPM3
  .if $defined(SAVE_20_BIT_REGISTERS)
     bis #0xD0,14(sp)               ; Enable LPM3 in IdleTask
  .else
     bis #0xD0,8(sp)                ; Enable LPM3 in IdleTask
  .endif
  inc &UCSCTL5                      ; Claude Pour s'assuer que lors du réveil la frèq. ne dépasse la fréq. max du CPU
                                    ; Selon bug PMM11 décrit dans le document SLAZ057F
PMMInterrupt_do_not_set_LPM3:
  bis #SVSMLDLYIFG,&PMMIFG          ; Set delay interrupt flag for next speed modification
PMMInterrupt_END:
  .if $defined(SAVE_20_BIT_REGISTERS)
     popx.a r12                     ; Save working register R12
  .else
     pop r12                        ; Save working register R12
  .endif
  dint
  nop
  bis.b #BIT6,&P2IE                 ; Enable I/O Port 2 pin 6 interrupt
  pop.b r13                         ; Restore Port 2 pin 7 interrupt mask
  and.b #0x80,r13
  bis.b r13,&P2IE
  .if $defined(SAVE_20_BIT_REGISTERS)
     popx.a r14                     ; Save working register R14
     popx.a r13                     ; Save working register R13
  .else
     pop r14                        ; Save working register R14
     pop r13                        ; Save working register R13
  .endif
  clr.b &_OSLLReserveBit            ; Make all pending SC() fail
  reti
Port2_0:
  bic.b #BIT0|BIT7,&P2IE            ; Disable I/O Port 2 pin 0 and 7 interrupt
  eint                              ; Enable all interrupts
  mov #OS_IO_PORT2_0,r13            ; Load interrupt entry to ZottaOS device handler table in R13
  SaveCtx                           ; Save context of the application task
  mov #_OSTabDevice,r12             ; Load ZottaOS device handler table in R12
  add r13,r12                       ; Add table offset to get the interrupt entry
  mov @r12,r12                      ; Get entry to the interrupt
  .if $isdefed("__LARGE_CODE_MODEL__")
     calla @r12                     ; Continue with the interrupt routine
     bra #_OSEndInterruptDontClrNoSaveCtx            ; Restore context
  .else
     call @r12                      ; Continue with the interrupt routine
     br #             ; Restore context
  .endif
  .if $defined(DEBUG_MODE)
UndefHandlerPort2:
     dint
     jmp UndefHandlerPort2          ; Error: loop indefinitely
  .endif
; end of I/O Port P2 interrupt handler (int42)

; Real-time clock (RTC_A) interrupt handler (int41).
  .if $defined(DEBUG_MODE)
  .sect ".int41"
  .short UndefHandler41
  .sect ".text:_isr"
UndefHandler41:
     jmp UndefHandler41             ; Loop indefinitely
  .endif
; end of real-time clock interrupt handler (int41)


; ***************************
; Context Switching Functions
; ***************************

; OSEndInterrupt and _OSEndInterrupt: Continue execution of the highest priority task.
; These functions are called when a task or an interrupt terminates its execution and
; resumes a nested interrupt or the task that is at the head of the ready queue. The
; difference between these two functions is that OSEndInterrupt never modifies the
; _OSNoSaveContext flag but _OSEndInterrupt always clears it. _OSEndInterrupt is only
; called from the timer service routine, OSEndTask or OSSuspendSynchronousTask.
; The stack contents (from top to bottom) are:
;   current stack base, i.e. the previous top or R1 (i.e. stack pointer SP)
;   R4 - R12, R15, _OSStackBasePointer
;   Port 2 interrupt mask
;   R14
;   R13
;   R2 (SR or status register)
;   R0 (PC or program counter)
; Note that R3 is neither saved nor restored.
  .text
_OSEndInterruptDontClrNoSaveCtx:
  setz                              ; Set zero bit (Z) of SR
  jmp RestoreCtx;
_OSEndInterruptClrNoSaveCtx:
  clrz                              ; Clear zero bit (Z) of SR
RestoreCtx:
  mov &_OSStackBasePointer,r13      ; The current stack base becomes the new stack top
  .if $defined(SAVE_20_BIT_REGISTERS)
     movx.a @r13+,r4                ; Restore R4-R15; R1 and R3 are never pushed
     movx.a @r13+,r5                ; and R0 and R2 are pushed by the interrupt call
     movx.a @r13+,r6
     movx.a @r13+,r7
     movx.a @r13+,r8
     movx.a @r13+,r9
     movx.a @r13+,r10
     movx.a @r13+,r11
     movx.a @r13+,r12
     movx.a @r13+,r15
  .else
     mov @r13+,r4                   ; Restore R4-R15; R1 and R3 are never pushed
     mov @r13+,r5                   ; and R0 and R2 are pushed by the interrupt call
     mov @r13+,r6
     mov @r13+,r7
     mov @r13+,r8
     mov @r13+,r9
     mov @r13+,r10
     mov @r13+,r11
     .if $isdefed("__LARGE_CODE_MODEL__")
        movx.a @r13+,r12
     .else
        mov @r13+,r12
     .endif
     mov @r13+,r15
  .endif
  dint                              ; Disable interrupts to finalize return
  nop                               ; DINT takes effect after executing an instruction
  mov r13,sp                        ; Set the new stack pointer
  jeq DontClearNoSaveCtx            ; Called from OSEndInterrupt?
                                    ; (Skip next instruction if zero bit is set)
  clr.b &_OSNoSaveContext           ; Set _OSNoSaveContext to FALSE
DontClearNoSaveCtx:
  pop &_OSStackBasePointer          ; Restore the previous stack base address
  pop.b r13                         ; Restore Port 2 pin 7 interrupt mask
  and.b #0x80,r13
  bis.b r13,&P2IE
  .if $defined(SAVE_20_BIT_REGISTERS)
     popx.a r14                     ; Restore working register R14
     popx.a r13                     ; Restore working register R13
  .else
     pop r14                        ; Restore working register R14
     pop r13                        ; Restore working register R13
  .endif
  clr.b &_OSLLReserveBit            ; Make all pending SC() fail
  reti
; end of OSEndInterrupt and _OSEndInterrupt

; _OSStartNextReadyTask: Starts a new instance of the first task in the ready queue.
; This task has the highest priority amongst all the ready tasks in the queue but it has
; never been executed or it is starting afresh. This task therefore has no previous
; context to be restored. This function can be called when returning from an interrupt
; or when terminating a task instance. In both cases, the actual content of the stack
; must be wiped out and a new context restored (program counter PC, status register SR
; and stack pointer register SP).
  .text
_OSStartNextReadyTask:
  ; At this point, _OSStackBasePointer holds the base address of the currently active
  ; task or interrupt. This address also corresponds to the previous top-stack address.
  ; Since we simulate a RETI, PC and SR must be pushed and the new top-stack address
  ; (SR) must be adjusted.
  mov &_OSStackBasePointer,r15
  mov &_OSActiveTask,r14
  decd r15
  .if POWER_MANAGEMENT = DRA | POWER_MANAGEMENT = DR_OTE
     mov.a 12(r14),0(r15)           ; Push PC, i.e. the starting of the task
  .else
     mov 10(r14),0(r15)             ; ditto
  .endif
  ; Push a new SR (bit 3 (0x08) which corresponds to bit GIE (General Interrupt Enable)
  ; and enables all maskable interrupts; this also clears previous power-off.
  .if $isdefed("__LARGE_CODE_MODEL__")
     .if POWER_MANAGEMENT = DRA | POWER_MANAGEMENT = DR_OTE
        mov.w 14(r14),r13
     .else
        mov.w 12(r14),r13
     .endif
     swpb r13
     rlam.w #4,r13
     andx.a #0xF000,r13
     bisx.a #0x8,r13
     decd r15
     mov r13,0(r15)                 ; Push SR
  .else
     decd r15
     mov #8h,0(r15)                 ; Push SR
  .endif
  ; Copy task argument into R12 so that the task instance (which is C function) can
  ; recover it.
  .if SCHEDULER_REAL_TIME_MODE = DEADLINE_MONOTONIC_SCHEDULING | POWER_MANAGEMENT = DRA | POWER_MANAGEMENT = DR_OTE 
     .if $isdefed("__LARGE_CODE_MODEL__")
        mov 16(r14),r12
     .else
        mov 14(r14),r12
     .endif
  .else
     .if $isdefed("__LARGE_CODE_MODEL__")
        mov 14(r14),r12
     .else
        mov 12(r14),r12
     .endif
  .endif
  dint                              ; Disable all interrupts
  nop                               ; DINT takes effect after executing an instruction
  mov.b #0,&_OSNoSaveContext        ; Set _OSNoSaveContext to FALSE
  ; Set the state of the new task to RUNNING so that the next time it gets interrupted a
  ; full context switching restoring all its registers is performed.
  .if POWER_MANAGEMENT = DRA | POWER_MANAGEMENT = DR_OTE
     bis.b #1h,6(r14)               ; _OSActiveTask->TaskState |= STATE_RUNNING;
  .else
     bis.b #1h,4(r14)               ; _OSActiveTask->TaskState |= STATE_RUNNING;
  .endif
  mov r15,SP                        ; Adjust SP to the new top-stack address
  clr.b &_OSLLReserveBit            ; Make all pending SC() fail
  reti                              ; Emulate a return from interrupt
; end of _OSStartNextReadyTask


; ***************************
; Memory Allocation Functions
; ***************************

; Since most applications do all their dynamic memory allocations at initialization time
; and these allocations are never reclaimed, the malloc function provided by libc is
; overkill because there is no need to manage freed blocks. To correct this state of af-
; fairs, we use the following scheme: During initialization time, i.e., when main is ac-
; tive and the first task has not yet executed, the run-time stack starts at a specific
; address and all functions that are called to prepare the kernel are stacked below main.
; During this time, any dynamic memory block allocated to the application is done at the
; opposite end starting with the highest possible address. Once the kernel starts its
; execution, the run-time stack is adjusted so that it begins immediately below the al-
; lotted blocks.
; As well as providing a useful malloc function for the user application, this approach
; has several interesting advantages:
;  (1) Code size is definitively smaller and faster compared to the usual thread-safe
;      malloc/free pair;
;  (2) There is no memory corruption in case of a stack overflow;
;  (3) The run-time stack occupies its largest possible size.

; _stack must be defined so that the compiler can define __STACK_END
_stack .usect ".stack",1

; OSMalloc: This function can only be used while the main function is active. Allocation
; is done above the stack in a RAM memory size of OSMALLOC_INTERNAL_HEAP_SIZE bytes; the
; run-time stack of main starts immediately below the reserved memory for the allocated
; memory blocks. By proceeding in this way, all global memory blocks are kept at one end
; of RAM memory while leaving the remaining memory for the run-time stack.
; Parameter: (UINT16) requested memory block size in bytes.
; Returned value: (void *) Pointer to the base of the newly allocated block, and NULL in
;    case of an error, i.e., when more than OSMALLOC_INTERNAL_HEAP_SIZE bytes of memory
;    is consumed by dynamic allocations done in main.
; Note: On the MSP430 line of microcontrollers, this is the only memory allocation func-
; tion that is provided. Because the MSP430 does not provide a frame pointer, other mem-
; ory allocation functions such as the standard C alloca() cannot be supplied.
; OSMalloc should NEVER be called from an application task or by a user defined ISR.
  .text
OSMalloc:
  tst &_OSActiveTask                ; Check if OSStartMultitasking was called
  jeq OSMalloc_CallFromMain         ; No, proceed with OSMalloc()
OSMalloc_ReturnNULL:
  mov #0,r12                        ; Return NULL
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
OSMalloc_CallFromMain:
  ; Memory blocks are allocated on 16 bit boundaries. For arbitrary boundary values, use
  ;     if (size % BOUNDARY != 0)
  ;        size += BOUNDARY - size % BOUNDARY;
  bit #1,r12
  jeq OSMalloc_BoundaryOK
  add #1,r12
OSMalloc_BoundaryOK:
  sub r12,&_OSStackBasePointer
  ; Check that requested memory block doesn't overlap main's activation record.
  mov #__STACK_END,r13              ; Find the start address of the run-time stack (limit)
  sub #OSMALLOC_INTERNAL_HEAP_SIZE,r13
  cmp &_OSStackBasePointer,r13      ; Got overflow?
  jlo OSMalloc_End                  ; No, finalize and return
  add r12,&_OSStackBasePointer      ; Yes, restore previous top-of-stack value
  .if $defined(DEBUG_MODE)
     dint
     nop
OSMalloc_InfiniteLoop:
     jmp OSMalloc_InfiniteLoop      ; Memory overflow: increase the value of OSMALLOC_IN-
                                    ; TERNAL_HEAP_SIZE defined in ZottaOS_msp430f5419A_36A_38A.h
  .else
     jmp OSMalloc_ReturnNULL        ; Return NULL
  .endif
OSMalloc_End:
  mov &_OSStackBasePointer,r12      ; Finalize and return
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of OSMalloc


; **********************************
; Internal Functions Used By ZottaOS
; **********************************

; _OSEnableSoftTimerInterrupt: Enables I/O Port 2 pin 7 interrupts. This function is
; used by ZottaOS after it finishes handling a timer interrupt.
  .text
_OSEnableSoftTimerInterrupt:
  bis.b #BIT7,&P2IE
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of _OSEnableSoftTimerInterrupt



; ****************************************************
; Clock Module Initialization for msp430f5419A_36A_38A
; ****************************************************
; OSInitializeSystemClocks: System clock initialization routine with:
;    MCLK sourced from the prescaler of the DCO (DCOCLKDIV) divided by 1
;    ACLK sourced from XT1 oscillator divided by 1
;    SMCLK sourced from the prescaler of the DCO (DCOCLKDIV) divided by 1
;    DCO controlled by FLL to achieve an internal DCOCLK of 8.00 MHz and outputting
;        8.00 MHz; The DCO uses XT1/1 as reference source.
;    XT1 set to 32.768 kHz watch crystal with effective capacitance of ~2 pF
;    PMM core voltage supply level set to 0
  .text
OSInitializeSystemClocks:
  ; Configure XT1
  ; Leave default XT1 oscillator frequency mode (low)
  bic #0x000C,&UCSCTL6              ; Set XCAP to 2 pF
  bis.b #0x03,&P7SEL                ; Set analog function for XT1 pins
  bic #XT1OFF,&UCSCTL6              ; Enable XT1
Wait_XT1:                           ; Wait until XT1CLK is stabilized
  bic #XT1LFOFFG,&UCSCTL7           ; Clear XT1LFOFFG flag
  bic #OFIFG,&SFRIFG1               ; Clear OFIFG flag
  mov #0xFF,r15                     ; Delay loop for OFIFG flag to be set again
Wait_Flag_XT1:
  dec r15
  jnz Wait_Flag_XT1
  bit #XT1LFOFFG,&UCSCTL7           ; Test XT1LFOFFG for stabilization
  jnz Wait_XT1                      ; Repeat while not stabilized
  ; Leave default XT1DRIVE configuration (maximum drive capability)
  ; Configure XT2
  ; Leave default setting for XT2 (disabled)
  ; Configure PMM
  clr &FreqTabUCSCTL0               
  clr &FreqTabUCSCTL0+2
  clr &FreqTabUCSCTL0+4
  clr &FreqTabUCSCTL0+6
  mov.b #0xA5,&PMMCTL0_H            ; Open PMM module for write access
  bic.b #SVSHE_H,&SVSMHCTL_H        ; Disable SVS highside
  bic.b #SVSLE_H,&SVSMLCTL_H        ; Disable SVS lowside
  .if $defined(DEBUG_MODE)
     bic #SVMHIFG,&PMMIFG           ; Clear SVM highside interrupt flag
     bis #SVMHIE,&PMMRIE            ; Enable SVM highside interrupt
  .else
     bic.b #SVMHE_H,&SVSMHCTL_H     ; Disable SVM highside
  .endif
  mov #SVSMLDLYIFG,&PMMIFG          ; 
  bis.b #BIT6,&P2IE                 ; Enable I/O Port 2 pin 6 interrupts
  bic #0x07,&UCSCTL4                ; Clear SELM bits
  bis #0x03,&UCSCTL4                ; Set SELM to DCOCLK
  ; Claude phase d'étalonage (ecriture de la table FreqTabUCSCTL0)
  ; FreqTabUCSCTL2Cal doit être utilisée quand FLL reference divider est 1 (valeur par default).
  mov #FreqTabUCSCTL2Cal,&ptFreqTabUCSCTL2
  mov.b #OS_8MHZ_SPEED,r13
  mov.b &PMMCTL0_L,&CurrentSpeed
  mov.b &PMMCTL0_L,&AdjustFreq      ; Force la configuration de DCO/FLL
  eint
Loop_SetProcessorSpeed:
  mov.b r13,r12
  calla #OSSetProcessorSpeed
Wait_SetProcessorSpeed:
  calla  #OSGetProcessorSpeed
  cmp.b r13,r12
  jne Wait_SetProcessorSpeed
  ; Wait for FLL stabilization 
  mov #0xFFFF,r15
Wait_FLL:  
  dec r15
  jnz Wait_FLL
  ; Save FreqTabUCSCTL0 entrie
  bis #SCG0,sr                      ; Disable the FLL control loop
  rla r12
  mov &UCSCTL0,FreqTabUCSCTL0(r12)
  bic #SCG0,sr                      ; Enable the FLL control loop
  cmp.b #OS_25MHZ_SPEED,r13
  jeq Exit_SetProcessorSpeed
  inc.b r13
  jmp Loop_SetProcessorSpeed
Exit_SetProcessorSpeed:
  bis #FLLREFDIV__16,&UCSCTL3              ; Set FLL reference divider (by 16)
  mov #FreqTabUCSCTL2,&ptFreqTabUCSCTL2
  ; Claude on doit appeler OSSetProcessorSpeed à nouveau pour fixer FLLN et FLLD
  ; avec les valeurs contenue dans la table FreqTabUCSCTL2 (FLL reference divider by 16). 
  mov.b #OS_25MHZ_SPEED,r12
  mov.b #OS_25MHZ_SPEED,&AdjustFreq ; Claude force la configuration du DCO/FLL
  calla #OSSetProcessorSpeed
  dint
  nop
  ; Configure auxiliary clock (ACLK) and its external pin (ACLK/n)
  ; Leave default DIVPA and DIVA settings (each dividing by 1)
  ; Leave default setting for ACLK source (SELA = XT1CLK)
  ; Configure sub-main clock (SMCLK)
  ; Leave default DIVS (dividing by 1) 
  ; Leave default setting for SMCLK source (SELS = DCOCLKDIV)
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of OSInitializeSystemClocks


; OSGetProcessorSpeed: return PMMCOREV value.
  .text
OSGetProcessorSpeed:
  mov.b &CurrentSpeed,r12
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of OSGetProcessorSpeed


; OSSetProcessorSpeed: Sets the speed of the processor to the given value passed as a
; parameter. Note that there is no verification that the value is in the range delimited
; by the values in table FreqTab. This function should be called prior to all
; context switches after determining the target value. Note that processing is done by
; provoking an interrupt.
; Parameter: (UINT8) index in the FreqTab table with the speed settings. This
;            value also corresponds to the voltage setting that should be applied for
;            the frequency entry in the table. */
  .text
OSSetProcessorSpeed:
  mov.b r12,&TargetSpeed            ; Set the new target speed for the PMM ISR
  bis #SVSMLDLYIE,&PMMRIE           ; Set PMM low side delay expired interrupt enable bit    
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of OSSetProcessorSpeed


; PMMSetSVMLow: Set SVM lowside to new level.
; Parameter: level
  .text
PMMSetSVMLow:
  bic #SVSMLDLYIFG,&PMMIFG          ; Clear already set flags
  bis.b #SVMLFP_H,&SVSMLCTL_H       ; Enable full-performance mode
  mov.b r12,&SVSMLCTL_L;
  ; Wait until SVS low side is settled
PMMSetSVMLow_WAIT:
  bit #SVSMLDLYIFG,&PMMIFG
  jz PMMSetSVMLow_WAIT
  bic.b #SVMLFP_H,&SVSMLCTL_H       ; Disable full-performance mode
  .if $isdefed("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of PMMSetSVSLow

