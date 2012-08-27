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
/* File ZottaOS_Interrupts.c: Interrupt implementation functions.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version date: August 2012
** Authors: MIS-TIC
*/
#include "ZottaOS.h"           /* Insert the user API with the specific kernel */

/* Global interrupt vector with one entry per source */
extern void *_OSTabDevice[];


/* OSSetISRDescriptor: Associates an ISR descriptor with an _OSTabDevice entry.
** Parameters:
**   (1) (UINT16) index of the _OSTabDevice entry;
**   (2) (void *) ISR descriptor for the specified interrupt.
** Returned value: none. */
void OSSetISRDescriptor(UINT16 entry, void *descriptor)
{
  _OSTabDevice[entry] = descriptor;
} /* end of OSSetISRDescriptor */


/* OSGetISRDescriptor: Returns the ISR descriptor associated with an _OSTabDevice entry.
** Parameter: (UINT16) index of _OSTabDevice where the ISR descriptor is held.
** Returned value: (void *) The requested ISR descriptor is returned. If no previous
**    OSSetIODescriptor was previously made for the specified entry, the returned value
**    is undefined. */
void *OSGetISRDescriptor(UINT16 entry)
{
  return _OSTabDevice[entry];
} /* end of OSGetISRDescriptor */
