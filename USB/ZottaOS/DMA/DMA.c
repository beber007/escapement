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
/* File DMA.c: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#include "../ZottaOS.h"
#include "../ZottaOS_msp430.h"
#include "DMA.h"


//this function init a DMA channel
void DMAInit(UINT8 channel)
{
  switch (channel) {
     case 0:
        DMACTL0 &= ~DMA0TSEL_31;         // DMA0 is triggered by DMAREQ
        DMACTL0 |= DMA0TSEL_0;           // DMA0 is triggered by DMAREQ
        DMA0CTL = (DMADT_1 + DMASBDB + DMASRCINCR_3 +   // configure block transfer (byte-wise) with increasing source
                       DMADSTINCR_3 );                  // and destination address
        DMACTL4 |= ENNMI;               // enable NMI interrupt
        break;
    case 1:
        DMACTL0 &= ~DMA1TSEL_31;         // DMA1 is triggered by DMAREQ
        DMACTL0 |= DMA1TSEL_0;           // DMA1 is triggered by DMAREQ
        DMA1CTL = (DMADT_1 + DMASBDB + DMASRCINCR_3 +   // configure block transfer (byte-wise) with increasing source
                       DMADSTINCR_3 );                  // and destination address
        DMACTL4 |= ENNMI;               // enable NMI interrupt
        break;
    case 2:
       DMACTL0 &= ~DMA2TSEL_31;         // DMA2 is triggered by DMAREQ
       DMACTL0 |= DMA2TSEL_0;           // DMA2 is triggered by DMAREQ
       DMA2CTL = (DMADT_1 + DMASBDB + DMASRCINCR_3 +   // configure block transfer (byte-wise) with increasing source
                       DMADSTINCR_3 );                 // and destination address
       DMACTL4 |= ENNMI;               // enable NMI interrupt
       break;
  }
}


// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA0
// Support only for data in <64k memory area.
void * DMAMemcpy0(void * dest, const void *  source, UINT8 count)
{
  if (count == 0)         // do nothing if zero bytes to transfer
     return dest;
  DMA0DAL = (UINT16)dest;   // set destination for DMAx
  DMA0SAL = (UINT16)source; // set source for DMAx
  DMA0SZ = count;         // how many bytes to transfer
  DMA0CTL |= DMAEN;       // enable DMAx
  DMA0CTL |= DMAREQ;      // trigger DMAx
  //wait for DMA transfer finished
  while (!(DMA0CTL & DMAIFG));
  DMA0CTL &= ~DMAEN;      // disable DMAx
  return dest;
}


// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA1
// Support only for data in <64k memory area.
void * DMAMemcpy1(void * dest, const void * source, UINT8 count)
{
  if (count == 0)         // do nothing if zero bytes to transfer
     return dest;
  DMA1DAL = (UINT16)dest;   // set destination for DMAx
  DMA1SAL = (UINT16)source; // set source for DMAx
  DMA1SZ = count;         // how many bytes to transfer
  DMA1CTL |= DMAEN;       // enable DMAx
  DMA1CTL |= DMAREQ;      // trigger DMAx
  //wait for DMA transfer finished
  while (!(DMA1CTL & DMAIFG));
  DMA1CTL &= ~DMAEN;      // disable DMAx
  return dest;
}


// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA2
// Support only for data in <64k memory area.
void * DMAMemcpy2(void * dest, const void * source, UINT8 count)
{
  if (count == 0)         // do nothing if zero bytes to transfer
     return dest;
  DMA2DAL = (UINT16)dest;   // set destination for DMAx
  DMA2SAL = (UINT16)source; // set source for DMAx
  DMA2SZ = count;         // how many bytes to transfer
  DMA2CTL |= DMAEN;       // enable DMAx
  DMA2CTL |= DMAREQ;      // trigger DMAx
  //wait for DMA transfer finished
  while (!(DMA2CTL & DMAIFG));
  DMA2CTL &= ~DMAEN;      // disable DMAx
  return dest;
}
