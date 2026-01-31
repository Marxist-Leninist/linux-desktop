# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the modern GUI compositor subsystem
#

obj-$(CONFIG_GUI_COMPOSITOR) += gui-core.o

gui-core-y := compositor.o

# Optional components
gui-core-$(CONFIG_GUI_SCHED_INTEGRATION) += sched_integration.o
gui-core-$(CONFIG_GUI_INPUT_LOWLATENCY) += input_lowlatency.o
gui-core-$(CONFIG_GUI_GESTURE_RECOGNITION) += gestures.o
gui-core-$(CONFIG_GUI_FRAME_PACING) += frame_pacing.o

ccflags-$(CONFIG_GUI_DEBUG) += -DDEBUG
