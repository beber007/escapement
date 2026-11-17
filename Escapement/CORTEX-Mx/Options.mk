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
# shell may set ARCH, say, for something else. An option given empty is not set, as
# ifdef has it. A name outside ALL_OPTIONS, such as a misspelt one, is not checked.
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
