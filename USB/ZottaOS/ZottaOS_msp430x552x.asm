; Copyright (c) 2006-2009 MIS Institute of the HEIG affiliated to the University of
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
; File ZottaOS_msp430x552x.asm: Holds msp430x552x dependent assembler implemented
;                               functions.
; Created on March 12, 2010, by ZottaOS MSP430 Configuration Tool.
; Authors: MIS-TIC
; Include device definition file and ZottaOS configuration file
  .cdecls C,LIST,"ZottaOS_msp430x552x.h"


; ******************************************************
; Global Variables Shared with ZottaOS.c and ZottaOS.asm
; ******************************************************
  .ref _OSNoSaveContext
  .ref _OSBackupCCR
  .ref _OSActiveTask
  .ref _OSQueueHead
  .ref _OSStackBasePointer
  .ref _OSLLReserveBit


; ******************************************************
; Global Variables Shared with USB.c
; ******************************************************
  .if $defined ("USB_DRIVER")
  .global USBKernelSleep
  .endif


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


; *********************************
; Imported Functions From ZottaOS.c
; *********************************
  .ref _OSTimerInterruptHandler


; **********************************
; Implemented and Exported Functions
; **********************************
  .global _c_int00
  .global OSEndInterrupt
  .global _OSEndInterrupt
  .global _OSStartNextReadyTask
  .global _OSSleep
  

; *****************
; Macro Definitions
; *****************

; SaveCtx: Saves the context of the interrupted task or interrupt routine onto the stack
; and sets the new current stack base pointer of this interrupt.
SaveCtx .macro
  push &_OSStackBasePointer         ; Save base stack pointer and registers onto the stack
  .if $defined("SAVE_20_BIT_REGISTERS")
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
     .if $defined("__LARGE_CODE_MODEL__")
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
  .if $defined("SAVE_20_BIT_REGISTERS")
     pushx.a r13                    ; Save working register R13
     pushx.a r14                    ; Save working register R14
  .else
     push r13                       ; Save working register R13
     push r14                       ; Save working register R14
  .endif
  .endm
; end of default_handler


; *************************************************************************************
; Table of devices used by the interrupt handler and the peripheral devices to obtain
; the appropriate I/O routine. For MSP430 there are at most OS_IO_MAX different devices
; to handle.
; *************************************************************************************
  .if OS_IO_MAX != 0
     .global _OSTabDevice
     .bss _OSTabDevice,OS_IO_MAX;
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
  sub.w #OSALLOCA_INTERNAL_HEAP_SIZE,r15
  mov.w r15,SP
  .if $defined("__LARGE_CODE_MODEL__")
     calla #_system_pre_init
  .else
     call #_system_pre_init
  .endif
  tst.w r12
  jeq _c_int00SkipAutoInit
  mov.w #__cinit__,r12
  .if $defined("__LARGE_CODE_MODEL__")
     calla #_auto_init
  .else
     call #_auto_init
  .endif
_c_int00SkipAutoInit:
  mov.w #__STACK_END,&_OSStackBasePointer
  .if $defined("__LARGE_CODE_MODEL__")
     calla #_args_main
  .else
     call #_args_main
  .endif
; end of c_int00 (reset interrupt handler int63)

; System non-maskable interrupt handler (int62).
  .if $defined(DEBUG_MODE)
  .sect ".int62"
  .short UndefHandler62
  .sect ".text:_isr"
UndefHandler62:
     jmp UndefHandler62             ; Loop indefinitely
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

; Comparator B interrupt handler (int60).
  .if $defined(DEBUG_MODE)
  .sect ".int60"
  .short UndefHandler60
  .sect ".text:_isr"
UndefHandler60:
     jmp UndefHandler60             ; Loop indefinitely
  .endif
; end of comparator B interrupt handler (int60)

; Timer0 B capture/compare CC0 interrupt handler (int59).
  .if $defined(DEBUG_MODE)
  .sect ".int59"
  .short UndefHandler59
  .sect ".text:_isr"
UndefHandler59:
     jmp UndefHandler59             ; Loop indefinitely
  .endif
; end of timer0 B CC0 interrupt handler (int59)

; Timer0 B CC1-6: Timer1 A capture/compare registers CC1 through CC6 and overflow
; interrupt handler (int58).
  .if $defined(DEBUG_MODE)
  .sect ".int58"
  .short UndefHandler58
  .sect ".text:_isr"
UndefHandler58:
     jmp UndefHandler58             ; Loop indefinitely
  .endif
; end of Timer0 B CC1-6 interrupt handler (int58)

; Watchdog timer interrupt handler (int57).
  .if $defined(DEBUG_MODE)
  .sect ".int57"
  .short UndefHandler57
  .sect ".text:_isr"
UndefHandler57:
     jmp UndefHandler57             ; Loop indefinitely
  .endif
; end of watchdog timer interrupt handler (int57)

; Universal serial communication interface A0 (USCI_A0) receive/transmit interrupt
; handlers (int56) with UART or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int56"
  .short UndefHandler56
  .sect ".text:_isr"
UndefHandler56:
     jmp UndefHandler56             ; Loop indefinitely
  .endif
; end of USCI A0 receive/transmit interrupt handlers (int56)

; Universal serial communication interface (USCI) B0 interrupt handlers (int55) with I2C
; state changes or SPI modes.
  .if $defined(DEBUG_MODE)
  .sect ".int55"
  .short UndefHandler55
  .sect ".text:_isr"
UndefHandler55:
     jmp UndefHandler55             ; Loop indefinitely
  .endif
; end of USCI B0 receive/transmit interrupt handlers (int55)

; 12-bit analog-to-digital converter (ADC12) interrupt handler (int54) with up to 16
; independent ADC samples.
  .if $defined(DEBUG_MODE)
  .sect ".int54"
  .short UndefHandler54
  .sect ".text:_isr"
UndefHandler54:
     jmp UndefHandler54             ; Loop indefinitely
  .endif
; end of ADC12 interrupt handler (int54)

; Timer0_A (int52): Interval timer0 A interrupt handler used by ZottaOS to propagate
; an emulated software interrupt defined as I/O port 2 pin number 7.
  .sect ".int52"
  .short Timer0A
  .sect ".text:_isr"
Timer0A:
  mov.w &TACCR0,&_OSBackupCCR       ; Backup comparator to update wall clock later
  mov.w #0xFFFE,&TACCR0             ; Set comparator to INFINITY16
  bit #0x00,&TAIV                   ; Clear timer interrupt flag
  bis.b #BIT7,&P2IFG                ; Generate a port 2 interrupt to finish the interrupt
  reti                              ; Return to previous interrupt or continue to port P2
; end of timer0 A interrupt handler (int52)


; USB interrupt handlers (int51).
  default_handler int51
  push.b &P2IE                      ; Save mask Port 2 (see int42)
  bic.b #BIT7,&P2IE                 ; Disable I/O Port 2 pin 7 interrupts (see int42)
  ; Check if the setup interrupt is pending. 
  ; We need to check it before other interrupts, 
  ; to work around that the Setup Int has lower priority then Input Endpoint 0
  bit.b #SETUPIE,&USBIE
  jz USB_CONTINUE
  bit.b #SETUPIFG,&USBIFG
  jz USB_CONTINUE
  bic.b #SETUPIE,&USBIE               ; Disable interrupt
  bic.b #SETUPIFG,&USBIFG             ; clear the interrupt bit
  eint                              ; Enable all interrupts
  mov #OS_IO_USB_SETUP_PACKET_RECEIVED,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_CONTINUE:
  add &USBVECINT,pc                 ; Add offset to jump table
  .if $defined(DEBUG_MODE)
     jmp USB_NO_INTERRUPT           ; Vector  0: No interrupt
     jmp USB_PWR_DROP               ; Vector  2: drop ind.
     jmp USB_PLL_LOCK               ; Vector  4: USB-PLL lock error
     jmp USB_PLL_SIGNAL             ; Vector  6: USB-PLL signal error
     jmp USB_PLL_RANGE              ; Vector  8: USB-PLL range error
     jmp USB_PWR_VBUSOn             ; Vector 10: USB-PWR VBUS-on
     jmp USB_PWR_VBUSOff            ; Vector 12: USB-PWR VBUS-off
     jmp UndefHandlerUSB            ; Vector 14: reserved
     jmp USB_USB_TIMESTAMP          ; Vector 16: USB timestamp event
     jmp USB_INPUT_ENDPOINT0        ; Vector 18: Input Endpoint-0
     jmp USB_OUTPUT_ENDPOINT0       ; Vector 20: Output Endpoint-0
     jmp USB_RSTR                   ; Vector 22: RSTR interrupt
     jmp USB_SUSR                   ; Vector 24: SUSR interrupt
     jmp USB_RESR                   ; Vector 26: RESR interrupt
     jmp USB_SETUP_PACKET_RECEIVED  ; Vector 28: 
     jmp USB_STPOW_PACKET_RECEIVED  ; Vector 30: 
     jmp UndefHandlerUSB            ; Vector 32: reserved
     jmp UndefHandlerUSB            ; Vector 34: reserved
     jmp USB_INPUT_ENDPOINT1        ; Vector 36: Input Endpoint-1
     jmp USB_INPUT_ENDPOINT2        ; Vector 38: Input Endpoint-2
     jmp USB_INPUT_ENDPOINT3        ; Vector 40: Input Endpoint-3
     jmp USB_INPUT_ENDPOINT4        ; Vector 42: Input Endpoint-4
     jmp USB_INPUT_ENDPOINT5        ; Vector 44: Input Endpoint-5
     jmp USB_INPUT_ENDPOINT6        ; Vector 46: Input Endpoint-6
     jmp USB_INPUT_ENDPOINT7        ; Vector 48: Input Endpoint-7
     jmp USB_OUTPUT_ENDPOINT1       ; Vector 50: Output Endpoint-1
     jmp USB_OUTPUT_ENDPOINT2       ; Vector 52: Output Endpoint-2
     jmp USB_OUTPUT_ENDPOINT3       ; Vector 54: Output Endpoint-3
     jmp USB_OUTPUT_ENDPOINT4       ; Vector 56: Output Endpoint-4
     jmp USB_OUTPUT_ENDPOINT5       ; Vector 58: Output Endpoint-5
     jmp USB_OUTPUT_ENDPOINT6       ; Vector 60: Output Endpoint-6
  .else
     jmp USB_NO_INTERRUPT           ; Vector  0: No interrupt
     jmp UndefHandlerUSB            ; Vector  0: No interrupt
     jmp USB_PWR_DROP               ; Vector  2: drop ind.
     jmp USB_PLL_LOCK               ; Vector  4: USB-PLL lock error
     jmp USB_PLL_SIGNAL             ; Vector  6: USB-PLL signal error
     jmp USB_PLL_RANGE              ; Vector  8: USB-PLL range error
     jmp USB_PWR_VBUSOn             ; Vector 10: USB-PWR VBUS-on
     jmp USB_PWR_VBUSOff            ; Vector 12: USB-PWR VBUS-off
     jmp UndefHandlerUSB            ; Vector 14: reserved
     jmp USB_USB_TIMESTAMP          ; Vector 16: USB timestamp event
     jmp USB_INPUT_ENDPOINT0        ; Vector 18: Input Endpoint-0
     jmp USB_OUTPUT_ENDPOINT0       ; Vector 20: Output Endpoint-0
     jmp USB_RSTR                   ; Vector 22: RSTR interrupt
     jmp USB_SUSR                   ; Vector 24: SUSR interrupt
     jmp USB_RESR                   ; Vector 26: RESR interrupt
     jmp USB_SETUP_PACKET_RECEIVED  ; Vector 28: 
     jmp USB_STPOW_PACKET_RECEIVED  ; Vector 30: 
     jmp USB_INPUT_ENDPOINT1        ; Vector 32: Input Endpoint-1
     jmp USB_INPUT_ENDPOINT2        ; Vector 34: Input Endpoint-2
     jmp USB_INPUT_ENDPOINT3        ; Vector 36: Input Endpoint-3
     jmp USB_INPUT_ENDPOINT4        ; Vector 38: Input Endpoint-4
     jmp USB_INPUT_ENDPOINT5        ; Vector 40: Input Endpoint-5
     jmp USB_INPUT_ENDPOINT6        ; Vector 42: Input Endpoint-6
     jmp USB_INPUT_ENDPOINT7        ; Vector 44: Input Endpoint-7
     jmp USB_OUTPUT_ENDPOINT1       ; Vector 46: Output Endpoint-1
     jmp USB_OUTPUT_ENDPOINT2       ; Vector 48: Output Endpoint-2
     jmp USB_OUTPUT_ENDPOINT3       ; Vector 50: Output Endpoint-3
     jmp USB_OUTPUT_ENDPOINT4       ; Vector 52: Output Endpoint-4
     jmp USB_OUTPUT_ENDPOINT5       ; Vector 54: Output Endpoint-5
     jmp USB_OUTPUT_ENDPOINT6       ; Vector 56: Output Endpoint-6
  .endif
  ; Fall into handler for Output Endpoint-7 interrupt (no JMP required).
  bic.b #BIT7,&USBOEPIE               ; Disable Output Endpoint-7 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT7,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT6:
  bic.b #BIT6,&USBOEPIE               ; Disable Output Endpoint-6 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT6,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT5:
  bic.b #BIT5,&USBOEPIE               ; Disable Output Endpoint-5 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT5,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT4:
  bic.b #BIT4,&USBOEPIE               ; Disable Output Endpoint-4 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT4,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT3:
  bic.b #BIT3,&USBOEPIE               ; Disable Output Endpoint-3 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT3,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT2:
  bic.b #BIT2,&USBOEPIE               ; Disable Output Endpoint-2 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT2,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT1:
  bic.b #BIT1,&USBOEPIE               ; Disable Output Endpoint-1 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT1,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT7:
  bic.b #BIT7,&USBIEPIE               ; Disable Input Endpoint-7 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT7,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT6:
  bic.b #BIT6,&USBIEPIE               ; Disable Input Endpoint-6 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT6,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT5:
  bic.b #BIT5,&USBIEPIE               ; Disable Input Endpoint-5 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT5,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT4:
  bic.b #BIT4,&USBIEPIE               ; Disable Input Endpoint-4 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT4,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT3:
  bic.b #BIT3,&USBIEPIE               ; Disable Input Endpoint-3 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT3,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT2:
  bic.b #BIT2,&USBIEPIE               ; Disable Input Endpoint-2 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT2,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT1:
  bic.b #BIT1,&USBIEPIE               ; Disable Input Endpoint-1 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT1,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_STPOW_PACKET_RECEIVED: 
  bic.b #STPOWIE,&USBIE               ; Disable interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_STPOW_PACKET_RECEIVED,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_SETUP_PACKET_RECEIVED: 
  bic.b #SETUPIE,&USBIE               ; Disable interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_SETUP_PACKET_RECEIVED,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_RESR:
  bic.b #RESRIE,&USBIE                ; Disable resume interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_RESR,r13             ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_SUSR:
  bic.b #SUSRIE,&USBIE                ; Disable suspend interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_SUSR,r13             ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_RSTR:
  bic.b #RSTRIE,&USBIE                ; Disable reset interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_RSTR,r13             ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_OUTPUT_ENDPOINT0:
  bic.b #BIT0,&USBOEPIE               ; Disable Output Endpoint-0 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_OUTPUT_ENDPOINT0,r13 ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_INPUT_ENDPOINT0:
  bic.b #BIT0,&USBIEPIE               ; Disable Input Endpoint-0 interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_INPUT_ENDPOINT0,r13  ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_USB_TIMESTAMP:
  bic #UTIE,&USBMAINT                 ; Disable USB timestamp event interrupt
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_USB_TIMESTAMP,r13    ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PWR_VBUSOff:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #VBOFFIE,&USBPWRCTL             ; Disable USB-PWR VBUS-off interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PWR_VBUSOff,r13      ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PWR_VBUSOn:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #VBONIE,&USBPWRCTL              ; Disable USB-PWR VBUS-off interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PWR_VBUSOn,r13       ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PLL_RANGE:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #USBOORIE,&USBPLLIR             ; Disable USB-PLL range error interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PLL_RANGE,r13        ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PLL_SIGNAL:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #USBLOSIE,&USBPLLIR             ; Disable USB-PLL signal error interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PLL_SIGNAL,r13       ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PLL_LOCK:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #USBOOLIE,&USBPLLIR             ; Disable USB-PLL lock error interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PLL_LOCK,r13         ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_PWR_DROP:
  mov #0x9628,&USBKEYPID              ; set KEY and PID to 0x9628 -> access to configuration registers enabled
  bic #VUOVLIE,&USBPWRCTL             ; Disable drop ind. interrupt
  mov #0x9600,&USBKEYPID              ; access to configuration registers disabled
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_PWR_DROP,r13         ; Load interrupt entry to ZottaOS device handler table in R13
  jmp EndUSB
USB_NO_INTERRUPT:
  eint                                ; Enable all interrupts
  mov #OS_IO_USB_NO_INTERRUPT,r13     ; Load interrupt entry to ZottaOS device handler table in R13
EndUSB:
  SaveCtx                             ; Save context of the application task
  mov #_OSTabDevice,r12               ; Load ZottaOS device handler table in R12
  add r13,r12                         ; Add table offset to get the interrupt entry
  mov @r12,r12                        ; Get entry to the interrupt
  .if $isdefed("__LARGE_CODE_MODEL__")
     calla @r12                       ; Continue with the interrupt routine
     bra #OSEndInterrupt              ; Restore context
  .else 
     call @r12                        ; Continue with the interrupt routine
     br #OSEndInterrupt               ; Restore context
  .endif
  .if $defined(DEBUG_MODE)
UndefHandlerUSB:
     dint
     jmp UndefHandlerUSB       ; Error: loop indefinitely
  .endif
; end of USB interrupt handlers (int51)

; DMA interrupt handlers (int50).
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

; Timer9 A capture/compare CC0 interrupt handler (int44).
  .if $defined(DEBUG_MODE)
  .sect ".int44"
  .short UndefHandler44
  .sect ".text:_isr"
UndefHandler44:
     jmp UndefHandler44             ; Loop indefinitely
  .endif
; end of timer9 A CC0 interrupt handler (int44)

; Timer2 A CC1-2: Timer2 A capture/compare registers CC1 through CC2 and overflow
; interrupt handler (int43).
  .if $defined(DEBUG_MODE)
  .sect ".int43"
  .short UndefHandler43
  .sect ".text:_isr"
UndefHandler43:
     jmp UndefHandler43             ; Loop indefinitely
  .endif
; end of timer2 A CC1-2 interrupt handler (int43)

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
     jmp UndefHandlerPort2          ; Vector 2: Port 2 pin 0
     jmp UndefHandlerPort2          ; Vector 4: Port 2 pin 1
     jmp UndefHandlerPort2          ; Vector 6: Port 2 pin 2
     jmp UndefHandlerPort2          ; Vector 8: Port 2 pin 3
     jmp UndefHandlerPort2          ; Vector 10: Port 2 pin 4
     jmp UndefHandlerPort2          ; Vector 12: Port 2 pin 5
     jmp UndefHandlerPort2          ; Vector 14: Port 2 pin 6
  .else
     nop                            ; Vector 0: No interrupt
     nop                            ; Vector 2: Port 2 pin 0
     nop                            ; Vector 4: Port 2 pin 1
     nop                            ; Vector 6: Port 2 pin 2
     nop                            ; Vector 8: Port 2 pin 3
     nop                            ; Vector 10: Port 2 pin 4
     nop                            ; Vector 12: Port 2 pin 5
     nop                            ; Vector 14: Port 2 pin 6
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
  .if $defined("__LARGE_CODE_MODEL__")
     bra #_OSTimerInterruptHandler  ; Continue to the timer service routine
  .else
     br #_OSTimerInterruptHandler   ; Continue to the timer service routine
  .endif
DontSaveCtx:
  mov &_OSStackBasePointer,sp       ; Wipe-out the stack of the task
  .if $defined("__LARGE_CODE_MODEL__")
     bra #_OSTimerInterruptHandler  ; Continue to the timer service routine
  .else
     br #_OSTimerInterruptHandler   ; Continue to the timer service routine
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
OSEndInterrupt:
  setz                              ; Set zero bit (Z) of SR
  jmp RestoreCtx;
_OSEndInterrupt:
  clrz                              ; Clear zero bit (Z) of SR
RestoreCtx:
  mov &_OSStackBasePointer,r13      ; The current stack base becomes the new stack top
  .if $defined("SAVE_20_BIT_REGISTERS")
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
     .if $defined("__LARGE_CODE_MODEL__")
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
  .if $defined("SAVE_20_BIT_REGISTERS")
     popx.a r14                     ; Restore working register R14
     popx.a r13                     ; Restore working register R13
  .else
     pop r14                        ; Restore working register R14
     pop r13                        ; Restore working register R13
  .endif
  clr.b &_OSLLReserveBit            ; Make all pending SC() fail
  .if $defined(USB_DRIVER)
     bic #CPUOFF+SCG1+SCG0,0(SP)    ; 
  .endif
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
  mov 10(r14),0(r15)                ; Push PC, i.e. the starting of the task
  ; Push a new SR (bit 3 (0x08) which corresponds to bit GIE (General Interrupt Enable)
  ; and enables all maskable interrupts; this also clears previous power-off.
  .if $defined("__LARGE_CODE_MODEL__")
     mov.w 12(r14),r13
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
  .if SCHEDULER_REAL_TIME_MODE = DEADLINE_MONOTONIC_SCHEDULING
     .if $defined("__LARGE_CODE_MODEL__")
        mov 16(r14),r12
     .else
        mov 14(r14),r12
     .endif
  .else
     .if $defined("__LARGE_CODE_MODEL__")
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
  bis.b #1h,4(r14)                  ; _OSActiveTask->TaskState |= STATE_RUNNING;
  mov r15,SP                        ; Adjust SP to the new top-stack address
  clr.b &_OSLLReserveBit            ; Make all pending SC() fail
  reti                              ; Emulate a return from interrupt
; end of _OSStartNextReadyTask



; ***************************
; Memory Allocation Functions
; ***************************

; Since most applications do all their dynamic memory allocations at initialization time
; and these allocations are never reclaimed, the malloc function provided by libc is an
; overkill because there is no need to manage free blocks. To correct this state of af-
; fairs, we use the following scheme: During initialization time, i.e., when main is ac-
; tive and the first task has not yet executed, the run-time stack starts at a specific
; address and all functions that are called to prepare the kernel are stacked above main.
; During this time, any dynamic memory block allocated to the application is done at the
; base of the stack using function OSAlloca(). Once the kernel starts its execution, the
; run-time stack is adjusted so that it evolves immediately above the allocated blocks.
; As well as providing a useful alloca function for the user application, this approach
; has several interesting advantages:
; (1) Code size is definitively smaller and faster compared to the malloc/free pair;
; (2) Code is reentrant and needs no synchronization lock;
; (3) There is no memory corruption in case of a stack overflow.

; _stack must be defined so that the compiler can define __STACK_END
_stack .usect ".stack",1

; OSAlloca: This function can only be used while the main function is active. Allocation
; is done at the bottom of the stack OSALLOCA_INTERNAL_HEAP_SIZE bytes; the run-time stack
; of main starts immediately above the reserved memory for the dynamically allocated mem-
; ory blocks. By proceeding in this way, all global memory blocks are kept at the bottom
; of the stack.
; Parameter: (size_t or UINT16) requested memory block size in bytes.
; Returned value: (void *) Pointer to the base of the newly allocated block, and NULL in
;    case of an error, i.e., when more than OSALLOCA_STACK_SIZE bytes of memory is con-
;    sumed by dynamic allocations done in main.
; Note: On the MSP430 line of microcontrollers, this is the only memory allocation func-
; tion that is provided. On other microcontrollers having a frame pointer, once OSStart-
; Multitasking is called, a call to OSAlloca becomes equivalent to standard C alloca(),
; i.e. memory allocation is done at the top of the run-time stack.
  .text
OSAlloca:
  tst &_OSActiveTask                ; Check if OSStartMultitasking was called
  jeq OSAlloca_CallFromMain         ; No, proceed with OSAlloca()
OSAlloca_ReturnNULL:
  mov #0,r12                        ; Return NULL
  .if $defined("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
OSAlloca_CallFromMain:
  ; Memory blocks are allocated on 16 bit boundaries. For arbitrary boundary values, use
  ;     if (size % BOUNDARY != 0)
  ;        size += BOUNDARY - size % BOUNDARY;
  bit #1,r12
  jeq OSAlloca_BoundaryOK
  add #1,r12
OSAlloca_BoundaryOK:
  sub r12,&_OSStackBasePointer
  ; Check that requested memory block doesn't overlap main's activation record.
  mov #__STACK_END,r13              ; Find the start address of the run-time stack (limit)
  sub #OSALLOCA_INTERNAL_HEAP_SIZE,r13
  cmp &_OSStackBasePointer,r13      ; Got overflow?
  jlo OSAlloca_End                  ; No, finalize and return
  add r12,&_OSStackBasePointer      ; Yes, restore previous top-of-stack value
  .if $defined(DEBUG_MODE)
     dint
     nop
OSAlloca_InfinitLoop:
     jmp OSAlloca_InfinitLoop       ; Memory overflow: increase the value of OSALLOCA_IN-
                                    ; TERNAL_HEAP_SIZE defined in ZottaOS_msp430xxx.h
  .else
     jmp OSAlloca_ReturnNULL        ; Return NULL
  .endif
OSAlloca_End:
  mov &_OSStackBasePointer,r12      ; Finalize and return
  .if $defined("__LARGE_CODE_MODEL__")
     reta
  .else
     ret
  .endif
; end of OSAlloca

; OSSleep: .
  .text
_OSSleep:
  .if $defined ("USB_DRIVER")
     .if $defined("__LARGE_CODE_MODEL__")
        calla #USBKernelSleep
     .else
        call #USBKernelSleep
     .endif
  .else
     ; set bits OSCOFF, SCG0 and SCG1 into the SR register to enter in LPM3 sleep mode
     bis.w #0xD0,sr
  .endif  
;end of _OSSleep