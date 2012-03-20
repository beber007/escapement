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
/* File ZottaOS_Atomic.h: Defines the LL/SC atomic instructions used used with the types
**                        defined for ZottaOS kernels.
** Platform version: All CORTEX-M3 microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS.h"

/* ATOMIC INSTRUCTIONS
** LL & SC Atomic Instruction
** The LL/SC pair of instructions are becoming increasing popular among high-end micro-
** controllers. Basically the LL atomically reads the content of a memory location and
** reserves it. The SC instruction used in combination with the LL takes a memory location
** and a value as parameters. It checks whether the memory location is still reserved and
** if so applies the value parameter to it and clears the reservation. On the other hand,
** if the memory location is not reserved at the time of the call, the memory location is
** left unchanged. */

/* OSUINT8_LL: The LL functions are used in conjunction with their corresponding SC
** functions call to provide synchronization support for ZottaOS */
inline UINT8 OSUINT8_LL(UINT8 *memAddr)
{
  UINT8 tmp;
  asm volatile ("LDREXB %0,[%1]":"=&b"(tmp):"r"(memAddr));
  return tmp;
} /* end of OSUINT8_LL */


/* OSUINT8_SC: Store Memory Location if Reserved. If the reservation bit is set by a
** previous call to an LL function, the second parameter is written into the memory
** location specified by the first parameter.*/
inline BOOL OSUINT8_SC(UINT8 *memAddr, register UINT8 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXB %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
} /* end of OSUINT8_SC */


/* OSUINT16_LL: Same as OSUINT8_LL but applied to unsigned 16-bit quantities. */
inline UINT16 OSUINT16_LL(UINT16 *memAddr)
{
  UINT16 tmp;
  asm volatile ("LDREXH %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
} /* end of OSUINT16_LL */


/* OSUINT16_SC: Same as OSUINT8_SC but applied to unsigned 16-bit integers. */
inline BOOL OSUINT16_SC(UINT16 *memAddr, register UINT16 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXH %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
} /* end of OSUINT16_SC */


/* OSINT16_LL: Same as OSUINT16_LL but for signed integers. */
inline INT16 OSINT16_LL(INT16 *memAddr)
{
  INT16 tmp;
  asm volatile ("LDREXH %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
} /* end of OSINT16_LL */


/* OSINT16_SC: Same as OSUINT16_SC but for signed integers. */
inline BOOL OSINT16_SC(INT16 *memAddr, register INT16 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXH %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
} /* end of OSINT16_SC */


/* OSUINT32_LL: Same as OSUINT8_LL but for 32-bit unsigned integers. */
inline UINT32 OSUINT32_LL(UINT32 *memAddr)
{
  UINT32 tmp;
  asm volatile ("LDREX %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
} /* end of OSUINT32_LL */


/* OSUINT32_SC: Same as OSUINT8_SC but for 32-bit unsigned integers. */
inline BOOL OSUINT32_SC(UINT32 *memAddr, register UINT32 newVal)
{
  register BOOL tmp;
  asm volatile ("STREX %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
} /* end of OSUINT32_SC */


/* OSINT32_LL: Same as OSUINT32_LL but for signed integers. */
inline INT32 OSINT32_LL(INT32 *memAddr)
{
  INT32 tmp;
  asm volatile ("LDREX %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
} /* end of OSINT32_LL */


/* OSINT32_SC: Same as OSUINT32_SC but for signed integers. */
inline BOOL OSINT32_SC(INT32 *memAddr, register INT32 newVal)
{
  register BOOL tmp;
  asm volatile ("STREX %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
} /* end of OSINT32_SC */
