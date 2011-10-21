/* Copyright (c) 2006-2009 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File ZottaOS_Timer.h:
** Version identifier: August 2009
** Authors: MIS-TIC
*/

/* Claude : Comentaires à revoir dans tout le fichier */

#ifndef ZOTTAOS_TIMER_H
#define ZOTTAOS_TIMER_H

#include "ZottaOS.h"

/* _OSGenerateSoftTimerInterrupt: */
void _OSGenerateSoftTimerInterrupt(void);

/* _OSEnableSoftTimerInterrupt: */
void _OSEnableSoftTimerInterrupt(void);

/* _OSInitializeTimer: */
void _OSInitializeTimer(void);

/* _OSStartTimer: */
void _OSStartTimer(void);

/* _OSUpdateTime: */
void _OSUpdateTime(void);

/* _OSGetTime: */
INT32 _OSGetTime(void);

/* _OSShiftTime: 
** Parameter: (INT32) 
*/
void _OSShiftTime(INT32 shiftTimeLimit);

/* _OSSetTimer:
** Parameter: (INT32)
*/ 
void _OSSetTimer(INT32 nextArrival);

/* _OSSleep: . */
void _OSSleep(void);

#endif /* ZOTTAOS_TIMER_H */
