/* Copyright (c) 2006-2009 MIS Institute of the HEIG affiliated to the University of
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
**
** File ZottaOS_msp430.h: This file is a generic include ZottaOS configuration file
**                        controlled by compiler or assembler IDE generated defines.
** To correctly use this file under Code Composer, simply set a predefined symbol to cor-
** respond with the microcontroller under hand. For example,
**   (1) Go to Project Properties -> C/C++ Build -> Tool Settings;
**   (2) Select MSP430 Compiler -> Predefined Symbols;
**   (3) Define the microcontroller symbol you are using as "__MSP430xxx__", for example
**       "__MSP430F2471__". 
** Version date: March 2010
** Authors: MIS-TIC
*/

#if defined (__MSP430C111__)
#include "ZottaOS_msp430x11x.h"

#elif defined (__MSP430C1111__) || defined (__MSP430C1101__)
#include "ZottaOS_msp430x11x1.h"

#elif defined (__MSP430C112__)
#include "ZottaOS_msp430x11x.h"

#elif defined (__MSP430C1121__)
#include "ZottaOS_msp430x11x1.h"

#elif defined (__MSP430C1331__) || defined (__MSP430C1351__)
#include "ZottaOS_msp430x13x1.h"

#elif defined (__MSP430C412__) || defined (__MSP430C413__)
#include "ZottaOS_msp430x41x.h"

#elif defined (__MSP430CG4619__)
#include "ZottaOS_msp430xG46x.h"

#elif defined (__MSP430E112__)
#include "ZottaOS_msp430x11x.h"

#elif defined (__MSP430F110__) || defined (__MSP430F112__)
#include "ZottaOS_msp430x11x.h"

#elif defined (__MSP430F1101__) || defined (__MSP430F1101A__) || defined (__MSP430F1111__) || defined (__MSP430F1111A__)
#include "ZottaOS_msp430x11x1.h"

#elif defined (__MSP430F1121__) || defined (__MSP430F1121A__)
#include "ZottaOS_msp430x11x1.h"

#elif defined (__MSP430F1122__) || defined (__MSP430F1132__)
#include "ZottaOS_msp430x11x2.h"

#elif defined (__MSP430F122__)
#include "ZottaOS_msp430x12x.h"

#elif defined (__MSP430F1222__)
#include "ZottaOS_msp430x12x2.h"

#elif defined (__MSP430F123__)
#include "ZottaOS_msp430x12x.h"

#elif defined (__MSP430F1232__)
#include "ZottaOS_msp430x12x2.h"

#elif defined (__MSP430F133__) || defined (__MSP430F135__)
#include "ZottaOS_msp430x13x.h"

#elif defined (__MSP430F147__) || defined (__MSP430F148__) || defined (__MSP430F149__)
#include "ZottaOS_msp430x14x.h"

#elif defined (__MSP430F1471__) || defined (__MSP430F1481__) || defined (__MSP430F1491__)
#include "ZottaOS_msp430x14x1.h"

#elif defined (__MSP430F155__) || defined (__MSP430F156__) || defined (__MSP430F157__)
#include "ZottaOS_msp430x15x.h"

#elif defined (__MSP430F167__) || defined (__MSP430F168__) || defined (__MSP430F169__) || defined (__MSP430F1610__) || defined (__MSP430F1611__) || defined (__MSP430F1612__)
#include "ZottaOS_msp430x16x.h"

#elif defined (__MSP430F2001__) || defined (__MSP430F2011__)
#include "ZottaOS_msp430x20x1.h"

#elif defined (__MSP430F2002__) || defined (__MSP430F2012__)
#include "ZottaOS_msp430x20x2.h"

#elif defined (__MSP430F2003__) || defined (__MSP430F2013__)
#include "ZottaOS_msp430x20x3.h"

#elif defined (__MSP430F2101__) || defined (__MSP430F2111__) || defined (__MSP430F2121__) || defined (__MSP430F2131__)
#include "ZottaOS_msp430x21x1.h"

#elif defined (__MSP430F2112__) || defined (__MSP430F2122__) || defined (__MSP430F2132__)
#include "ZottaOS_msp430x21x2.h"

#elif defined (__MSP430F2232__) || defined (__MSP430F2252__) || defined (__MSP430F2272__)
#include "ZottaOS_msp430x22x2.h"

#elif defined (__MSP430F2234__) || defined (__MSP430F2254__) || defined (__MSP430F2274__)
#include "ZottaOS_msp430x22x4.h"

#elif defined (__MSP430F2330__) || defined (__MSP430F2350__) || defined (__MSP430F2370__)
#include "ZottaOS_msp430x23x0.h"

#elif defined (__MSP430F233__) || defined (__MSP430F235__)
#include "ZottaOS_msp430x23x.h"

#elif defined (__MSP430F247__) || defined (__MSP430F248__) || defined (__MSP430F249__) || defined (__MSP430F2410__)
#include "ZottaOS_msp430x24x.h"

#elif defined (__MSP430F2471__) || defined (__MSP430F2481__) || defined (__MSP430F2491__)
#include "ZottaOS_msp430x24x1.h"

#elif defined (__MSP430F2416__) || defined (__MSP430F2417__) || defined (__MSP430F2418__) || defined (__MSP430F2419__)
#include "ZottaOS_msp430x241x.h"

#elif defined (__MSP430F2616__) || defined (__MSP430F2617__) || defined (__MSP430F2618__) || defined (__MSP430F2619__)
#include "ZottaOS_msp430x26x.h"

#elif defined (__MSP430F412__) || defined (__MSP430F413__)
#include "ZottaOS_msp430x41x.h"

#elif defined (__MSP430F415__)
#include "ZottaOS_msp430x415.h"

#elif defined (__MSP430F417__)
#include "ZottaOS_msp430x417.h"

#elif defined (__MSP430F4132__) || defined (__MSP430F4152__)
#include "ZottaOS_msp430x41x2.h"

#elif defined (__MSP430F423__) || defined (__MSP430F425__) || defined (__MSP430F427__) || defined (__MSP430F423A__) || defined (__MSP430F425A__) || defined (__MSP430F427A__)
#include "ZottaOS_msp430x42x.h"

#elif defined (__MSP430FE423__) || defined (__MSP430FE425__) || defined (__MSP430FE427__)
#include "ZottaOS_msp430xE42x.h"

#elif defined (__MSP430FE423A__) || defined (__MSP430FE425A__) || defined (__MSP430FE427A__)
#include "ZottaOS_msp430xE42xA.h"

#elif defined (__MSP430FE4232__) || defined (__MSP430FE4242__) || defined (__MSP430FE4252__) || defined (__MSP430FE4272__)
#include "ZottaOS_msp430xE42x2.h"

#elif defined (__MSP430F4250__) || defined (__MSP430F4260__) || defined (__MSP430F4270__)
#include "ZottaOS_msp430x42x0.h"

#elif defined (__MSP430F435__) || defined (__MSP430F436__) || defined (__MSP430F437__)
#include "ZottaOS_msp430x43x.h"

#elif defined (__MSP430F4351__) || defined (__MSP430F4361__) || defined (__MSP430F4371__)
#include "ZottaOS_msp430x43x1.h"

#elif defined (__MSP430F447__) || defined (__MSP430F448__) || defined (__MSP430F449__)
#include "ZottaOS_msp430x44x.h"

#elif defined (__MSP430F4481__) || defined (__MSP430F4491__)
#include "ZottaOS_msp430x44x1.h"

#elif defined (__MSP430F4783__) || defined (__MSP430F4793__)
#include "ZottaOS_msp430x47x3.h"

#elif defined (__MSP430F4784__) || defined (__MSP430F4794__)
#include "ZottaOS_msp430x47x4.h"

#elif defined (__MSP430F47126__)
#include "ZottaOS_msp430f47126.h"

#elif defined (__MSP430F47127__)
#include "ZottaOS_msp430f47127.h"

#elif defined (__MSP430F47163__) || defined (__MSP430F47173__) || defined (__MSP430F47183__) || defined (__MSP430F47193__)
#include "ZottaOS_msp430x471x3.h"

#elif defined (__MSP430F47166__) || defined (__MSP430F47176__) || defined (__MSP430F47186__) || defined (__MSP430F47196__)
#include "ZottaOS_msp430x471x6.h"

#elif defined (__MSP430F47167__) || defined (__MSP430F47177__) || defined (__MSP430F47187__) || defined (__MSP430F47197__)
#include "ZottaOS_msp430x471x7.h"

#elif defined (__MSP430FG4250__) || defined (__MSP430FG4260__) || defined (__MSP430FG4270__)
#include "ZottaOS_msp430xG42x0.h"

#elif defined (__MSP430FW423__) || defined (__MSP430FW425__) || defined (__MSP430FW427__)
#include "ZottaOS_msp430xW42x.h"

#elif defined (__MSP430FG437__) || defined (__MSP430FG438__) || defined (__MSP430FG439__)
#include "ZottaOS_msp430xG43x.h"

#elif defined (__MSP430F4616__) || defined (__MSP430F4617__) || defined (__MSP430F4618__) || defined (__MSP430F4619__)
#include "ZottaOS_msp430x461x.h"

#elif defined (__MSP430F46161__) || defined (__MSP430F46171__) || defined (__MSP430F46181__) || defined (__MSP430F46191__)
#include "ZottaOS_msp430x461x1.h"

#elif defined (__MSP430FG4616__) || defined (__MSP430FG4617__) || defined (__MSP430FG4618__) || defined (__MSP430FG4619__)
#include "ZottaOS_msp430xG461x.h"

#elif defined (__MSP430CG4616__) || defined (__MSP430CG4617__) || defined (__MSP430CG4618__)
#include "ZottaOS_msp430xG461x.h"

#elif defined (__MSP430F477__) || defined (__MSP430F478__) || defined (__MSP430F479__)
#include "ZottaOS_msp430x47x.h"

#elif defined (__MSP430FG477__) || defined (__MSP430FG478__) || defined (__MSP430FG479__)
#include "ZottaOS_msp430xG47x.h"

#elif defined (__XMS430F5438__) || defined (__MSP430F5418__) || defined (__MSP430F5419__) || defined (__MSP430F5435__) || defined (__MSP430F5436__) || defined (__MSP430F5437__) || defined (__MSP430F5438__)
#include "ZottaOS_msp430x54x.h"

#elif defined (__MSP430F5418A__) || defined (__MSP430F5419A__) || defined (__MSP430F5435A__) || defined (__MSP430F5436A__) || defined (__MSP430F5437A__) || defined (__MSP430F5438A__)
#include "ZottaOS_msp430x54xA.h"

#elif defined (__MSP430F5513__) || defined (__MSP430F5514__) || defined (__MSP430F5515__) || defined (__MSP430F5517__) || defined (__MSP430F5519__)
#include "ZottaOS_msp430x551x.h"

#elif defined (__MSP430F5521__) || defined (__MSP430F5522__) || defined (__MSP430F5524__) || defined (__MSP430F5525__) || defined (__MSP430F5526__) || defined (__MSP430F5527__) || defined (__MSP430F5528__) || defined (__MSP430F5529__)
#include "ZottaOS_msp430x552x.h"

#elif defined (__MSP430P112__)
#include "ZottaOS_msp430x11x.h"

#elif defined (__MSP430P313__) || defined (__MSP430P315__) || defined (__MSP430P315S__)
#include "ZottaOS_msp430x31x.h"

#elif defined (__MSP430P325__)
#include "ZottaOS_msp430x32x.h"

#elif defined (__MSP430P337__)
#include "ZottaOS_msp430x33x.h"

#elif defined (__CC430F5133__) || defined (__CC430F5135__) || defined (__CC430F5137__)
#include "ZottaOS_cc430x513x.h"

#elif defined (__CC430F6125__) || defined (__CC430F6126__) || defined (__CC430F6127__)
#include "ZottaOS_cc430x612x.h"

#elif defined (__CC430F6135__) || defined (__CC430F6137__)
#include "ZottaOS_cc430x613x.h"

#elif defined (__MSP430GENERIC__)
#error "msp430 generic device does not have a default include file"

#elif defined (__MSP430XGENERIC__)
#error "msp430X generic device does not have a default include file"

#else
#error "Failed to match a default include file"
#endif
