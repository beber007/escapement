/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Modes.h: The names of the scheduling algorithms, shared by the three
** kernel variants and by the port.
**
** An application names its algorithm in Escapement_Config.h, and the port tests that
** choice before any kernel header is read. The preprocessor gives an undefined name the
** value 0, so a test such as SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
** made that early compares 0 with 0 and holds whatever the application chose: the soft
** kernel under EDF was given the task control block offsets of deadline-monotonic
** scheduling. Every file that tests the choice includes this one first.
*/
#ifndef ESCAPEMENT_MODES_H_
#define ESCAPEMENT_MODES_H_

#define EARLIEST_DEADLINE_FIRST        1
#define DEADLINE_MONOTONIC_SCHEDULING  2
#define EARLIEST_DEADLINE_FIRST_STAR   3   /* power-aware kernel only: deterministic EDF */

#endif /* ESCAPEMENT_MODES_H_ */
