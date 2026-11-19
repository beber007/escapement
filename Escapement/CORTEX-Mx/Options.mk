# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.

# The options of the examples' Makefiles, refused where they are not taken (#86). A
# Makefile that drops an option builds another image than the one asked for. In #84,
# nucleo-u575's dropped SCHEDULER and built EDF: a mutant of the DM code passed for
# missed.
#
# Before it includes this file, a Makefile names in OPTIONS the options it takes, in
# KERNELS the values of KERNEL it builds besides the hard kernel's (KERNEL unset), and
# in ARCHS those of ARCH besides the Arm cores'. Only the command line is checked: a
# shell may set ARCH, say, for something else. The Pico's and the Pico 2's Makefiles
# check LIVE from the environment too, where it would also go into every image (#91).
# An option given empty is not set, as ifdef has it. A name outside ALL_OPTIONS, such
# as a misspelt one, is not checked.
ALL_OPTIONS = ARCH DEEP DVFS_PHASE_S FAST FLASH KERNEL LIVE MHZ NOHSE PHASES \
              PHASE_PERIODS POWER RUN RUN_VSEL SCHEDULER SLEEP_GATE SLEEP_SPEED SMPS \
              TRACE UNDERVOLT WAKE WCET
comma := ,

NOT_TAKEN = $(filter-out $(OPTIONS),$(ALL_OPTIONS))
GIVEN = $(and $(filter command line,$(origin $(1))),$($(1)))
$(foreach o,$(NOT_TAKEN),$(if $(call GIVEN,$(o)),$(error \
   $(o)=$($(o)) is not an option of this Makefile, which takes: $(OPTIONS))))

ifeq ($(origin KERNEL),command line)
   ifneq ($(filter-out $(KERNELS),$(KERNEL))$(word 2,$(KERNEL)),)
      $(error KERNEL=$(KERNEL) is not a kernel of this Makefile: give none for the \
              hard kernel, or one of: $(KERNELS))
   endif
endif
ifeq ($(origin ARCH),command line)
   ifneq ($(filter-out $(ARCHS),$(ARCH))$(word 2,$(ARCH)),)
      $(error ARCH=$(ARCH) is not an architecture of this Makefile: give none for \
              the Arm cores$(if $(ARCHS),$(comma) or one of: $(ARCHS)))
   endif
endif

# SCHEDULER and POWER take the names of Escapement_Modes.h and EscapementHardPA.h. The
# compiler would read another word as 0 in each #if, and build a kernel nobody asked
# for: SCHEDULER=DM mixed parts of EDF and DM, POWER=DRAA built no power management
# (#92). EDF* and POWER belong to the power-aware kernel. As above, only the command
# line is checked here; the kernels' headers refuse such a name whatever its source,
# when it is compiled. This check gives the reason before anything is built.
SCHEDULER_NAMES = EARLIEST_DEADLINE_FIRST DEADLINE_MONOTONIC_SCHEDULING \
                  $(if $(filter PA,$(KERNEL)),EARLIEST_DEADLINE_FIRST_STAR)
POWER_NAMES = NONE DRA OTE DR_OTE DM_SLACK
EDF_STAR_HINT = $(if $(filter PA,$(KERNELS)),$(if $(filter PA,$(KERNEL)),, (and \
                EARLIEST_DEADLINE_FIRST_STAR with KERNEL=PA)))
ifneq ($(call GIVEN,SCHEDULER),)
   ifneq ($(filter-out $(SCHEDULER_NAMES),$(SCHEDULER))$(word 2,$(SCHEDULER)),)
      $(error SCHEDULER=$(SCHEDULER) is not a scheduler of this kernel: give one of: \
              $(strip $(SCHEDULER_NAMES))$(EDF_STAR_HINT))
   endif
endif
ifneq ($(call GIVEN,POWER),)
   ifneq ($(KERNEL),PA)
      $(error POWER=$(POWER) is not an option of $(if $(KERNEL),KERNEL=$(KERNEL),the \
              hard kernel): give KERNEL=PA$(if $(KERNEL), in its place))
   endif
   ifneq ($(filter-out $(POWER_NAMES),$(POWER))$(word 2,$(POWER)),)
      $(error POWER=$(POWER) is not a policy of the power-aware kernel: give one of: \
              $(POWER_NAMES))
   endif
endif
