/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel GUI Compositor Subsystem
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#ifndef _LINUX_GUI_COMPOSITOR_H
#define _LINUX_GUI_COMPOSITOR_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/kref.h>
#include <linux/workqueue.h>
#include <uapi/linux/gui/compositor.h>

struct drm_device;
struct task_struct;
struct input_dev;

/*
 * GUI Window Descriptor
 *
 * Represents a window managed by the compositor, with kernel-level
 * state tracking for optimization purposes.
 */
struct gui_window {
	struct list_head list;
	struct kref refcount;

	/* Window identification */
	u64 id;
	pid_t owner_pid;
	struct task_struct *owner_task;

	/* Configuration and state */
	struct gui_window_config config;

	/* Kernel hints for optimization */
	bool is_focused;
	bool is_visible;
	bool needs_compositing;

	/* Scheduling hints */
	int sched_boost;		/* Additional scheduler priority */

	/* Memory and I/O hints */
	bool prevent_swap;		/* Don't swap this window's memory */
	int io_priority;		/* I/O scheduler priority */

	/* Performance tracking */
	u64 last_frame_time_ns;
	u32 frame_count;
	u32 dropped_frames;

	spinlock_t lock;
};

/*
 * GUI Compositor State
 *
 * Global compositor state managed by the kernel.
 */
struct gui_compositor {
	struct list_head windows;
	spinlock_t windows_lock;

	/* Focused window for input routing and scheduling */
	struct gui_window *focused_window;

	/* Display and timing */
	struct drm_device *drm_dev;
	u64 vsync_period_ns;
	u64 last_vsync_time_ns;

	/* Performance configuration */
	struct gui_perf_config perf_config;

	/* Frame pacing */
	struct hrtimer frame_timer;
	struct work_struct frame_work;
	bool frame_pacing_enabled;

	/* Statistics */
	atomic64_t total_frames;
	atomic64_t dropped_frames;
	u64 avg_frame_time_ns;
};

/* Global compositor instance */
extern struct gui_compositor *gui_compositor;

/*
 * Core API functions
 */

/* Initialize the GUI subsystem */
int gui_compositor_init(void);
void gui_compositor_exit(void);

/* Window management */
struct gui_window *gui_window_create(pid_t owner_pid);
void gui_window_destroy(struct gui_window *window);
int gui_window_set_config(struct gui_window *window,
			  const struct gui_window_config *config);
int gui_window_get_config(struct gui_window *window,
			  struct gui_window_config *config);

/* Focus management */
int gui_window_set_focus(struct gui_window *window);
struct gui_window *gui_window_get_focused(void);

/* Frame submission */
int gui_submit_frame(struct gui_frame *frame);

/* Scheduler integration */
void gui_window_boost_priority(struct gui_window *window);
void gui_window_normal_priority(struct gui_window *window);

/* Input routing */
void gui_input_event_notify(struct input_dev *dev);

/* Performance hints */
void gui_set_perf_mode(enum gui_perf_mode mode);
enum gui_perf_mode gui_get_perf_mode(void);

/* DRM/KMS integration */
int gui_register_drm_device(struct drm_device *dev);
void gui_unregister_drm_device(struct drm_device *dev);
void gui_vsync_notify(struct drm_device *dev, u64 timestamp_ns);

/* Effect acceleration queries */
bool gui_hw_supports_blur(void);
bool gui_hw_supports_shadows(void);
bool gui_hw_supports_rounded_corners(void);
int gui_hw_max_compositor_layers(void);

/*
 * Scheduler policy helpers
 */
static inline bool gui_is_window_focused(struct gui_window *window)
{
	return window && window->is_focused;
}

static inline bool gui_should_boost_priority(struct task_struct *task)
{
	struct gui_compositor *comp = gui_compositor;
	struct gui_window *focused;

	if (!comp)
		return false;

	focused = READ_ONCE(comp->focused_window);
	return focused && focused->owner_task == task;
}

/*
 * Sysfs interface
 */
extern struct kobject *gui_kobj;

#endif /* _LINUX_GUI_COMPOSITOR_H */
