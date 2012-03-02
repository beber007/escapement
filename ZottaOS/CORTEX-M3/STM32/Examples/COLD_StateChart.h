/* Copyright (c) 2012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
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
/* File COLD_StatChart.h: .
** Version identifier: February 2012
** Authors: MIS-TIC
*/

#ifndef _COLD_STATECHART_H_
#define _COLD_STATECHART_H_

#include "ZottaOS_TimerEvent.h"

void InitStateCharts(void);

#if defined(ZOTTAOS_VERSION_HARD)
void InitStateChart(void (*function)(void *), UINT8 initialState, INT32 workLoad);
#elif defined(ZOTTAOS_VERSION_SOFT)
void InitStateChart(void (*function)(void *),UINT8 initialState,INT32 wcet,
                    INT32 workLoad, UINT8 aperiodicUtilization);
#endif

void ScheduleNextState(void *handle, UINT16 nextState, INT32 time);

void UnScheduleNextState(void *handle);

void GotoNextState(void *handle, UINT16 nextState);

UINT16 GetCurrentState(void *handle);

#endif /* _COLD_STATECHART_H_ */
