# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_LIBRTE_PIPELINE),y)

#
# library name
#
APP = rocc-sender

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-y := main.c
SRCS-y += config.c
SRCS-y += init.c
SRCS-y += runtime.c

include $(RTE_SDK)/mk/rte.app.mk

endif
