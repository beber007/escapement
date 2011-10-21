/* Copyright (c) 2006-2010 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZER-
** LAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PRO-
** VIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG AND NOR THE
** UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION TO PROVIDE
** MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File DMA.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef DMA_H_
#define DMA_H_

//this function init a DMA channel
void DMAInit(UINT8 channel);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA0
// Support only for data in <64k memory area.
void * DMAMemcpy0(void * dest, const void *  source, UINT8 count);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA1
// Support only for data in <64k memory area.
void * DMAMemcpy1(void * dest, const void * source, UINT8 count);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA2
// Support only for data in <64k memory area.
void * DMAMemcpy2(void * dest, const void * source, UINT8 count);

#endif /*DMA_H_*/
