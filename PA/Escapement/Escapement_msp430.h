/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
**
** File Escapement_msp430.h: This file is a generic include Escapement configuration file
**                        controlled by compiler or assembler IDE generated defines.
** To correctly use this file under Code Composer, simply set a predefined symbol to cor-
** respond with the microcontroller under hand. For example,
**   (1) Go to Project Properties -> C/C++ Build -> Tool Settings;
**   (2) Select MSP430 Compiler -> Predefined Symbols;
**   (3) Define the microcontroller symbol you are using as "__MSP430xxx__", for example
**       "__MSP430F2471__". 
** Version date: May 2011
*/

#if defined (__MSP430C111__)
#include "Escapement_msp430x11x.h"

#elif defined (__MSP430C1111__) || defined (__MSP430C1101__)
#include "Escapement_msp430x11x1.h"

#elif defined (__MSP430C112__)
#include "Escapement_msp430x11x.h"

#elif defined (__MSP430C1121__)
#include "Escapement_msp430x11x1.h"

#elif defined (__MSP430C1331__) || defined (__MSP430C1351__)
#include "Escapement_msp430x13x1.h"

#elif defined (__MSP430C412__) || defined (__MSP430C413__)
#include "Escapement_msp430x41x.h"

#elif defined (__MSP430CG4619__)
#include "Escapement_msp430xG46x.h"

#elif defined (__MSP430E112__)
#include "Escapement_msp430x11x.h"

#elif defined (__MSP430F110__) || defined (__MSP430F112__)
#include "Escapement_msp430x11x.h"

#elif defined (__MSP430F1101__) || defined (__MSP430F1101A__) || defined (__MSP430F1111__) || defined (__MSP430F1111A__)
#include "Escapement_msp430x11x1.h"

#elif defined (__MSP430F1121__) || defined (__MSP430F1121A__)
#include "Escapement_msp430x11x1.h"

#elif defined (__MSP430F1122__) || defined (__MSP430F1132__)
#include "Escapement_msp430x11x2.h"

#elif defined (__MSP430F122__)
#include "Escapement_msp430x12x.h"

#elif defined (__MSP430F1222__)
#include "Escapement_msp430x12x2.h"

#elif defined (__MSP430F123__)
#include "Escapement_msp430x12x.h"

#elif defined (__MSP430F1232__)
#include "Escapement_msp430x12x2.h"

#elif defined (__MSP430F133__) || defined (__MSP430F135__)
#include "Escapement_msp430x13x.h"

#elif defined (__MSP430F147__) || defined (__MSP430F148__) || defined (__MSP430F149__)
#include "Escapement_msp430x14x.h"

#elif defined (__MSP430F1471__) || defined (__MSP430F1481__) || defined (__MSP430F1491__)
#include "Escapement_msp430x14x1.h"

#elif defined (__MSP430F155__) || defined (__MSP430F156__) || defined (__MSP430F157__)
#include "Escapement_msp430x15x.h"

#elif defined (__MSP430F167__) || defined (__MSP430F168__) || defined (__MSP430F169__) || defined (__MSP430F1610__) || defined (__MSP430F1611__) || defined (__MSP430F1612__)
#include "Escapement_msp430x16x.h"

#elif defined (__MSP430AFE221__) || defined (__MSP430AFE231__) || defined (__MSP430AFE251__)
#include "Escapement_msp430afe2x1.h"

#elif defined (__MSP430AFE222__) || defined (__MSP430AFE232__) || defined (__MSP430AFE252__)
#include "Escapement_msp430afe2x2.h"

#elif defined (__MSP430AFE223__) || defined (__MSP430AFE233__) || defined (__MSP430AFE253__)
#include "Escapement_msp430afe2x3.h"

#elif defined (__MSP430F2001__) || defined (__MSP430F2011__)
#include "Escapement_msp430x20x1.h"

#elif defined (__MSP430F2002__) || defined (__MSP430F2012__)
#include "Escapement_msp430x20x2.h"

#elif defined (__MSP430F2003__) || defined (__MSP430F2013__)
#include "Escapement_msp430x20x3.h"

#elif defined (__MSP430F2101__) || defined (__MSP430F2111__) || defined (__MSP430F2121__) || defined (__MSP430F2131__)
#include "Escapement_msp430x21x1.h"

#elif defined (__MSP430F2112__) || defined (__MSP430F2122__) || defined (__MSP430F2132__)
#include "Escapement_msp430x21x2.h"

#elif defined (__MSP430F2232__) || defined (__MSP430F2252__) || defined (__MSP430F2272__)
#include "Escapement_msp430x22x2.h"

#elif defined (__MSP430F2234__) || defined (__MSP430F2254__) || defined (__MSP430F2274__)
#include "Escapement_msp430x22x4.h"

#elif defined (__MSP430F2330__) || defined (__MSP430F2350__) || defined (__MSP430F2370__)
#include "Escapement_msp430x23x0.h"

#elif defined (__MSP430F233__) || defined (__MSP430F235__)
#include "Escapement_msp430x23x.h"

#elif defined (__MSP430F247__) || defined (__MSP430F248__) || defined (__MSP430F249__) || defined (__MSP430F2410__)
#include "Escapement_msp430x24x.h"

#elif defined (__MSP430F2471__) || defined (__MSP430F2481__) || defined (__MSP430F2491__)
#include "Escapement_msp430x24x1.h"

#elif defined (__MSP430F2416__) || defined (__MSP430F2417__) || defined (__MSP430F2418__) || defined (__MSP430F2419__)
#include "Escapement_msp430x241x.h"

#elif defined (__MSP430F2616__) || defined (__MSP430F2617__) || defined (__MSP430F2618__) || defined (__MSP430F2619__)
#include "Escapement_msp430x26x.h"

#elif defined (__MSP430F412__) || defined (__MSP430F413__)
#include "Escapement_msp430x41x.h"

#elif defined (__MSP430F415__)
#include "Escapement_msp430x415.h"

#elif defined (__MSP430F417__)
#include "Escapement_msp430x417.h"

#elif defined (__MSP430F4132__) || defined (__MSP430F4152__)
#include "Escapement_msp430x41x2.h"

#elif defined (__MSP430F423__) || defined (__MSP430F425__) || defined (__MSP430F427__) || defined (__MSP430F423A__) || defined (__MSP430F425A__) || defined (__MSP430F427A__)
#include "Escapement_msp430x42x.h"

#elif defined (__MSP430FE423__) || defined (__MSP430FE425__) || defined (__MSP430FE427__)
#include "Escapement_msp430xE42x.h"

#elif defined (__MSP430FE423A__) || defined (__MSP430FE425A__) || defined (__MSP430FE427A__)
#include "Escapement_msp430xE42xA.h"

#elif defined (__MSP430FE4232__) || defined (__MSP430FE4242__) || defined (__MSP430FE4252__) || defined (__MSP430FE4272__)
#include "Escapement_msp430xE42x2.h"

#elif defined (__MSP430F4250__) || defined (__MSP430F4260__) || defined (__MSP430F4270__)
#include "Escapement_msp430x42x0.h"

#elif defined (__MSP430F435__) || defined (__MSP430F436__) || defined (__MSP430F437__)
#include "Escapement_msp430x43x.h"

#elif defined (__MSP430F438__) || defined (__MSP430F439__)
#include "Escapement_msp430f43x.h"

#elif defined (__MSP430F4351__) || defined (__MSP430F4361__) || defined (__MSP430F4371__)
#include "Escapement_msp430x43x1.h"

#elif defined (__MSP430F447__) || defined (__MSP430F448__) || defined (__MSP430F449__)
#include "Escapement_msp430x44x.h"

#elif defined (__MSP430F4481__) || defined (__MSP430F4491__)
#include "Escapement_msp430x44x1.h"

#elif defined (__MSP430F4783__) || defined (__MSP430F4793__)
#include "Escapement_msp430x47x3.h"

#elif defined (__MSP430F4784__) || defined (__MSP430F4794__)
#include "Escapement_msp430x47x4.h"

#elif defined (__MSP430F47126__) 
#include "Escapement_msp430f47126.h"

#elif defined (__MSP430F47127__)
#include "Escapement_msp430f47127.h"

#elif defined (__MSP430F47163__) || defined (__MSP430F47173__) || defined (__MSP430F47183__) || defined (__MSP430F47193__)
#include "Escapement_msp430x471x3.h"

#elif defined (__MSP430F47166__) || defined (__MSP430F47176__) || defined (__MSP430F47186__) || defined (__MSP430F47196__)
#include "Escapement_msp430x471x6.h"

#elif defined (__MSP430F47167__) || defined (__MSP430F47177__) || defined (__MSP430F47187__) || defined (__MSP430F47197__)
#include "Escapement_msp430x471x7.h"

#elif defined (__MSP430FG4250__) || defined (__MSP430FG4260__) || defined (__MSP430FG4270__)
#include "Escapement_msp430xG42x0.h"

#elif defined (__MSP430FW423__) || defined (__MSP430FW425__) || defined (__MSP430FW427__)
#include "Escapement_msp430xW42x.h"

#elif defined (__MSP430FG437__) || defined (__MSP430FG438__) || defined (__MSP430FG439__)
#include "Escapement_msp430xG43x.h"

#elif defined (__MSP430F4616__) || defined (__MSP430F4617__) || defined (__MSP430F4618__) || defined (__MSP430F4619__)
#include "Escapement_msp430x461x.h"

#elif defined (__MSP430F46161__) || defined (__MSP430F46171__) || defined (__MSP430F46181__) || defined (__MSP430F46191__)
#include "Escapement_msp430x461x1.h"

#elif defined (__MSP430FG4616__) || defined (__MSP430FG4617__) || defined (__MSP430FG4618__) || defined (__MSP430FG4619__)
#include "Escapement_msp430xG461x.h"

#elif defined (__MSP430CG4616__) || defined (__MSP430CG4617__) || defined (__MSP430CG4618__)
#include "Escapement_msp430xG461x.h"

#elif defined (__MSP430F477__) || defined (__MSP430F478__) || defined (__MSP430F479__)
#include "Escapement_msp430x47x.h"

#elif defined (__MSP430FG477__) || defined (__MSP430FG478__) || defined (__MSP430FG479__)
#include "Escapement_msp430xG47x.h"

#elif defined (__CC430F5133__) || defined (__CC430F5135__) || defined (__CC430F5137__)
#include "Escapement_cc430x513x.h"

#elif defined (__MSP430BT5190__)
#include "Escapement_msp430bt5190.h"

#elif defined (__MSP430F5304__)
#include "Escapement_msp430f5304.h"

#elif defined (__MSP430F5308__) || defined (__MSP430F5309__) || defined (__MSP430F5310__)
#include "Escapement_msp430f5308-10.h"

#elif defined (__MSP430F5418__) || defined (__MSP430F5435__) || defined (__MSP430F5437__)
#include "Escapement_msp430f5418_35_37.h"

#elif defined (__MSP430F5418A__) || defined (__MSP430F5435A__) || defined (__MSP430F5437A__)
#include "Escapement_msp430f5418A_35A_37A.h"

#elif defined (__XMS430F5438__) || defined (__MSP430F5419__) || defined (__MSP430F5436__) || defined (__MSP430F5438__)
#include "Escapement_msp430f5419_36_38.h"

#elif defined (__MSP430F5419A__) || defined (__MSP430F5436A__) || defined (__MSP430F5438A__)
#include "Escapement_msp430f5419A_36A_38A.h"

#elif defined (__MSP430F5508__) || defined (__MSP430F5509__) || defined (__MSP430F5510__)
#include "Escapement_msp430f5508-10.h"

#elif defined (__MSP430F5513__) || defined (__MSP430F5514__) || defined (__MSP430F5515__) || defined (__MSP430F5517__) || defined (__MSP430F5519__)
#include "Escapement_msp430x551x.h"

#elif defined (__MSP430F5521__) || defined (__MSP430F5522__) || defined (__MSP430F5524__) || defined (__MSP430F5525__) || defined (__MSP430F5526__) || defined (__MSP430F5527__) || defined (__MSP430F5528__) || defined (__MSP430F5529__)
#include "Escapement_msp430x552x.h"

#elif defined (__MSP430P112__)
#include "Escapement_msp430x11x.h"

#elif defined (__MSP430F5500__) || defined (__MSP430F5501__) || defined (__MSP430F5502__) || defined (__MSP430F5503__)
#include "Escapement_msp430f5500-03.h"

#elif defined (__MSP430F5504__) || defined (__MSP430F5505__) || defined (__MSP430F5506__) || defined (__MSP430F5507__)
#include "Escapement_msp430f5504-07.h"

#elif defined (__MSP430F5508__) || defined (__MSP430F5509__) || defined (__MSP430F5510__)
#include "Escapement_msp430f5508-10.h"

#elif defined (__MSP430F5630__) || defined (__MSP430F5631__) || defined (__MSP430F5632__)
#include "Escapement_msp430f5630-32.h"

#elif defined (__MSP430F5633__) || defined (__MSP430F5634__) || defined (__MSP430F5635__)
#include "Escapement_msp430f5633-35.h"

#elif defined (__MSP430F5636__) || defined (__MSP430F5637__) || defined (__MSP430F5638__)
#include "Escapement_msp430f5636-38.h"

#elif defined (__MSP430FR5720__)
#include "Escapement_msp430fr5720.h"

#elif defined (__MSP430FR5725__)
#include "Escapement_msp430fr5725.h"

#elif defined (__MSP430FR5728__)
#include "Escapement_msp430fr5728.h"

#elif defined (__MSP430FR5729__)
#include "Escapement_msp430fr5729.h"

#elif defined (__MSP430FR5730__)
#include "Escapement_msp430fr5730.h"

#elif defined (__MSP430FR5735__)
#include "Escapement_msp430fr5735.h"

#elif defined (__MSP430FR5738__)
#include "Escapement_msp430fr5738.h"

#elif defined (__MSP430FR5739__)
#include "Escapement_msp430fr5739.h"

#elif defined (__CC430F6125__) || defined (__CC430F6126__) || defined (__CC430F6127__)
#include "Escapement_cc430x612x.h"

#elif defined (__CC430F6135__) || defined (__CC430F6137__)
#include "Escapement_cc430x613x.h"

#elif defined (__MSP430F6630__) || defined (__MSP430F6631__) || defined (__MSP430F6632__)
#include "Escapement_msp430f6630-32.h"

#elif defined (__MSP430F6633__) || defined (__MSP430F6634__) || defined (__MSP430F6635__)
#include "Escapement_msp430f6633-35.h"

#elif defined (__MSP430F6636__) || defined (__MSP430F6637__) || defined (__MSP430F6638__)
#include "Escapement_msp430f6636-38.h"


#elif defined (__MSP430GENERIC__)
#error "msp430 generic device does not have a default include file"

#elif defined (__MSP430XGENERIC__)
#error "msp430X generic device does not have a default include file"

#else
#error "Failed to match a default include file"
#endif
