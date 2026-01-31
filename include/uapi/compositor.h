/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Modern GUI Compositor Interface
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 *
 * This header defines the user-kernel interface for the modern
 * compositor subsystem, enabling Windows 11-like visual effects
 * and tight kernel integration.
 */

#ifndef _UAPI_LINUX_GUI_COMPOSITOR_H
#define _UAPI_LINUX_GUI_COMPOSITOR_H

#include <linux/types.h>

/*
 * Window States and Properties
 */
enum gui_window_state {
	GUI_WINDOW_NORMAL = 0,
	GUI_WINDOW_MINIMIZED = 1,
	GUI_WINDOW_MAXIMIZED = 2,
	GUI_WINDOW_FULLSCREEN = 3,
	GUI_WINDOW_SNAPPED_LEFT = 4,
	GUI_WINDOW_SNAPPED_RIGHT = 5,
};

enum gui_window_flags {
	GUI_WINDOW_FOCUSED = (1 << 0),
	GUI_WINDOW_VISIBLE = (1 << 1),
	GUI_WINDOW_ACRYLIC_BG = (1 << 2),	/* Enable acrylic background */
	GUI_WINDOW_MICA_BG = (1 << 3),		/* Enable mica material */
	GUI_WINDOW_DROP_SHADOW = (1 << 4),	/* Enable drop shadow */
	GUI_WINDOW_ROUNDED_CORNERS = (1 << 5),	/* Enable rounded corners */
	GUI_WINDOW_BLUR_BEHIND = (1 << 6),	/* Blur content behind window */
	GUI_WINDOW_ALWAYS_ON_TOP = (1 << 7),
	GUI_WINDOW_HW_ACCELERATED = (1 << 8),	/* Request HW composition */
};

/*
 * Material Types (Windows 11-style)
 */
enum gui_material_type {
	GUI_MATERIAL_NONE = 0,
	GUI_MATERIAL_ACRYLIC = 1,		/* Blur + noise + tint */
	GUI_MATERIAL_MICA = 2,			/* Wallpaper-aware translucency */
	GUI_MATERIAL_MICA_ALT = 3,		/* Alternative mica style */
	GUI_MATERIAL_SMOKE = 4,			/* Subtle smoky effect */
};

struct gui_material {
	__u32 type;				/* enum gui_material_type */
	__u32 tint_color;			/* ARGB color */
	__u32 tint_opacity;			/* 0-255 */
	__u32 blur_radius;			/* pixels */
	__u32 noise_intensity;			/* 0-100 */
	__u32 luminosity_opacity;		/* 0-100 */
	__u32 flags;
	__u32 reserved[8];
};

struct gui_shadow {
	__u32 color;				/* ARGB color */
	__u32 blur_radius;			/* pixels */
	__s32 offset_x;				/* pixels */
	__s32 offset_y;				/* pixels */
	__u32 spread;				/* pixels */
	__u32 reserved[4];
};

struct gui_rounded_corners {
	__u32 top_left;				/* radius in pixels */
	__u32 top_right;
	__u32 bottom_left;
	__u32 bottom_right;
};

/*
 * Window configuration structure
 */
struct gui_window_config {
	__u32 state;				/* enum gui_window_state */
	__u32 flags;				/* enum gui_window_flags */
	__u32 priority;				/* Scheduling priority hint */
	__u32 opacity;				/* 0-255 */

	struct gui_material material;
	struct gui_shadow shadow;
	struct gui_rounded_corners corners;

	/* Animation hints */
	__u32 animation_duration_ms;		/* Desired animation duration */
	__u32 animation_curve;			/* Easing curve */

	__u64 reserved[16];
};

/*
 * Frame submission and synchronization
 */
enum gui_frame_flags {
	GUI_FRAME_VSYNC = (1 << 0),		/* Wait for vsync */
	GUI_FRAME_LOW_LATENCY = (1 << 1),	/* Minimize latency */
	GUI_FRAME_POWER_SAVING = (1 << 2),	/* Optimize for power */
	GUI_FRAME_HW_CURSOR = (1 << 3),		/* Use hardware cursor */
};

struct gui_frame_timing {
	__u64 target_present_time_ns;		/* When to present frame */
	__u64 submit_time_ns;			/* When frame was submitted */
	__u64 render_start_time_ns;		/* When rendering started */
	__u64 vsync_period_ns;			/* Display refresh period */
	__u32 missed_frames;			/* Dropped frame count */
	__u32 reserved;
};

struct gui_frame {
	__u32 flags;				/* enum gui_frame_flags */
	__u32 num_layers;			/* Number of composition layers */
	__u64 layer_handles;			/* Pointer to layer array */
	struct gui_frame_timing timing;
	__u64 fence_fd;				/* Sync fence for completion */
	__u64 reserved[8];
};

/*
 * Gesture and input hints
 */
enum gui_gesture_type {
	GUI_GESTURE_SWIPE_UP = 0,
	GUI_GESTURE_SWIPE_DOWN = 1,
	GUI_GESTURE_SWIPE_LEFT = 2,
	GUI_GESTURE_SWIPE_RIGHT = 3,
	GUI_GESTURE_PINCH_IN = 4,
	GUI_GESTURE_PINCH_OUT = 5,
	GUI_GESTURE_ROTATE = 6,
	GUI_GESTURE_EDGE_SWIPE = 7,
};

struct gui_gesture_config {
	__u32 enabled_gestures;			/* Bitmask of enabled gestures */
	__u32 sensitivity;			/* 0-100 */
	__u32 min_fingers;
	__u32 max_fingers;
	__u64 reserved[4];
};

/*
 * Performance and power hints
 */
enum gui_perf_mode {
	GUI_PERF_BALANCED = 0,
	GUI_PERF_LOW_LATENCY = 1,		/* Desktop/gaming mode */
	GUI_PERF_POWER_SAVING = 2,		/* Laptop on battery */
	GUI_PERF_HIGH_QUALITY = 3,		/* Enable all effects */
};

struct gui_perf_config {
	__u32 mode;				/* enum gui_perf_mode */
	__u32 target_fps;			/* Desired frame rate */
	__u32 max_latency_us;			/* Maximum acceptable latency */
	__u32 enable_hw_effects;		/* Hardware effect acceleration */
	__u64 reserved[8];
};

/*
 * IOCTLs for compositor interface
 */
#define GUI_IOC_MAGIC 'G'

#define GUI_IOC_SET_WINDOW_CONFIG	_IOW(GUI_IOC_MAGIC, 1, struct gui_window_config)
#define GUI_IOC_GET_WINDOW_CONFIG	_IOR(GUI_IOC_MAGIC, 2, struct gui_window_config)
#define GUI_IOC_SUBMIT_FRAME		_IOWR(GUI_IOC_MAGIC, 3, struct gui_frame)
#define GUI_IOC_SET_GESTURE_CONFIG	_IOW(GUI_IOC_MAGIC, 4, struct gui_gesture_config)
#define GUI_IOC_SET_PERF_CONFIG		_IOW(GUI_IOC_MAGIC, 5, struct gui_perf_config)
#define GUI_IOC_GET_FRAME_TIMING	_IOR(GUI_IOC_MAGIC, 6, struct gui_frame_timing)

#endif /* _UAPI_LINUX_GUI_COMPOSITOR_H */
