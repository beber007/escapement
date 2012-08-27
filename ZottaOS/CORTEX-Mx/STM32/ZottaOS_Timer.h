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
*/
/* File ZottaOSHard_Timer.h: Defines the interface between the hardware timer and
**                           ZottaOS-Hard.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef ZOTTAOS_TIMER_H
#define ZOTTAOS_TIMER_H

/* Keeping track of time and performing all its related events is a tricky business. In
** ZottaOS we divide these 2 actions in two. The first only keeps track of the time (the
** wall clock) with a dedicated hardware timer and is implemented by an ISR. The second
** part, which is triggered by the hardware timer ISR, takes care of all the actions that
** need to be done when the wall clock progresses. This includes processing the instance
** arrivals that occur at the current time, preventing temporal variable overflow and
** programming the next hardware timer interrupt. This second part is implemented as a
** software ISR and it may be triggered any time and as often as there is an event to
** process.
** This scheme provides 2 main advantages:
** (1) The latency associated with the time events can be kept to a minimal (18 machine
**     cycles for MSP430) as only the keeping of time executes in a critical section.
** (2) The scheme can be ported to any microcontroller independently of whether it has
**     prioritized interrupts or not. */

/* Must be defined if the platform does not offer the option to disable the software
** interrupt timer. Used only prior to starting task scheduling. */
#define NonMaskableSoftwareTimer

/* _OSInitializeTimer: Sets up the hardware timer to count in up mode without starting
** it. After the call, the timer is ready to produce an interrupt as soon as it is order-
** ed to start counting and at the first interrupt, the current wall clock is set to 0. */
void _OSInitializeTimer(void);

/* _OSStartTimer: Orders the hardware timer to begin counting. */
void _OSStartTimer(void);

/* _OSGetActualTime: Returns the current value of the wall clock.
** Parameters: Node
** Returned value: (INT32) current time. */
INT32 _OSGetActualTime(void);

/* _OSTimerIsOverflow: return true if a timer overflow occurs. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit);

/* _OSSetTimer: Sets the timer to interrupt at a specified value. This function is called
** by the software timer ISR to set the next time event.
** Parameter: (INT32) nextTimeInterval: time duration until the next hardware timer in-
**                    terrupt, i.e. the time of the next event minus the current time.
** Return: claude             */
BOOL _OSSetTimer(INT32 nextArrivalTime);

/* Claude */
/* Lors de l'initialisation, une structure TIMERSELECT est déclarée pour chaque entrée de la table
** _OSTabDevice qui correspond à un vecteur ayant comme source plusieurs timer.
** Ceci permet l'appel à la fonction _OSTimerSelectorHandler afin de déterminer qu'elle timer
** est la source de l'interruption.
**
** Les fonction OSSetTimerISRDescriptor et OSGetTimerISRDescriptor (ZottaOS_Interrupt.h) permette de
** modifier ou de récupèrer les handlers dans les structures TIMERSELECT. */

#define TIMER_ISR_TAB_SIZE 2 // Nombre de sous-source d'un vecteur (ici tjs 2)

typedef struct TIMERSELECT {
   void (*TimerSelector)(struct TIMERSELECT *); // contient tjs _OSTimerSelector
   void *TimerISRTab[TIMER_ISR_TAB_SIZE];       // handler pour les sous-source
   UINT32 BaseRegisterTab[TIMER_ISR_TAB_SIZE];  // Adresse de base des registers des timers
   UINT16 InterruptBit;                         // Valeur du bit correspond à la source d'interruption dans les registres
} TIMERSELECT;

/* _OSTimerSelector: Handler d'interruption utilisé dans le cas d'un vecteur servant plusieurs timers.
** Parameter: (struct TIMERSELECT *) timerSelect: descripteur contenant les informations permettant de déterminer la source d'interruption et les handler à appeler. */
void _OSTimerSelectorHandler(struct TIMERSELECT *timerSelect);

#endif /* ZOTTAOS_TIMER_H */
