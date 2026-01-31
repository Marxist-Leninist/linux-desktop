// SPDX-License-Identifier: GPL-2.0
/*
 * Kernel GUI Compositor Subsystem
 *
 * This module provides kernel-level support for modern GUI compositing,
 * enabling Windows 11-like visual effects and tight integration with
 * kernel subsystems for optimal performance.
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/sched/prio.h>
#include <linux/gui/compositor.h>
#include <drm/drm_device.h>
#include <drm/drm_vblank.h>

/* Global compositor instance */
struct gui_compositor *gui_compositor;
EXPORT_SYMBOL_GPL(gui_compositor);

/* Sysfs root */
struct kobject *gui_kobj;
EXPORT_SYMBOL_GPL(gui_kobj);

static atomic64_t window_id_counter = ATOMIC64_INIT(0);

/*
 * Window Management
 */

struct gui_window *gui_window_create(pid_t owner_pid)
{
	struct gui_window *window;
	struct task_struct *task;

	window = kzalloc(sizeof(*window), GFP_KERNEL);
	if (!window)
		return ERR_PTR(-ENOMEM);

	window->id = atomic64_inc_return(&window_id_counter);
	window->owner_pid = owner_pid;

	/* Find and reference the owner task */
	rcu_read_lock();
	task = pid_task(find_vpid(owner_pid), PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	window->owner_task = task;
	window->is_focused = false;
	window->is_visible = true;
	window->needs_compositing = true;
	window->sched_boost = 0;
	window->io_priority = 0;
	window->prevent_swap = false;

	kref_init(&window->refcount);
	spin_lock_init(&window->lock);
	INIT_LIST_HEAD(&window->list);

	/* Add to compositor's window list */
	if (gui_compositor) {
		spin_lock(&gui_compositor->windows_lock);
		list_add_tail(&window->list, &gui_compositor->windows);
		spin_unlock(&gui_compositor->windows_lock);
	}

	pr_debug("GUI: Created window %llu for PID %d\n", window->id, owner_pid);

	return window;
}
EXPORT_SYMBOL_GPL(gui_window_create);

static void gui_window_release(struct kref *ref)
{
	struct gui_window *window = container_of(ref, struct gui_window, refcount);

	if (window->owner_task)
		put_task_struct(window->owner_task);

	kfree(window);
}

void gui_window_destroy(struct gui_window *window)
{
	if (!window)
		return;

	/* Remove from compositor's window list */
	if (gui_compositor) {
		spin_lock(&gui_compositor->windows_lock);
		if (gui_compositor->focused_window == window)
			gui_compositor->focused_window = NULL;
		list_del(&window->list);
		spin_unlock(&gui_compositor->windows_lock);
	}

	pr_debug("GUI: Destroyed window %llu\n", window->id);

	kref_put(&window->refcount, gui_window_release);
}
EXPORT_SYMBOL_GPL(gui_window_destroy);

int gui_window_set_config(struct gui_window *window,
			  const struct gui_window_config *config)
{
	if (!window || !config)
		return -EINVAL;

	spin_lock(&window->lock);

	memcpy(&window->config, config, sizeof(*config));

	/* Update kernel hints based on configuration */
	window->is_visible = !!(config->flags & GUI_WINDOW_VISIBLE);
	window->needs_compositing = !!(config->flags &
				      (GUI_WINDOW_ACRYLIC_BG |
				       GUI_WINDOW_MICA_BG |
				       GUI_WINDOW_BLUR_BEHIND |
				       GUI_WINDOW_DROP_SHADOW));

	/* Adjust scheduling priority for focused windows */
	if (config->flags & GUI_WINDOW_FOCUSED) {
		window->sched_boost = 10;
		window->prevent_swap = true;
	} else if (config->state == GUI_WINDOW_MINIMIZED) {
		window->sched_boost = -10;
		window->prevent_swap = false;
	}

	spin_unlock(&window->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_window_set_config);

int gui_window_get_config(struct gui_window *window,
			  struct gui_window_config *config)
{
	if (!window || !config)
		return -EINVAL;

	spin_lock(&window->lock);
	memcpy(config, &window->config, sizeof(*config));
	spin_unlock(&window->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_window_get_config);

/*
 * Focus Management
 */

int gui_window_set_focus(struct gui_window *window)
{
	struct gui_window *prev_focused;

	if (!gui_compositor)
		return -ENODEV;

	spin_lock(&gui_compositor->windows_lock);

	prev_focused = gui_compositor->focused_window;

	/* Remove boost from previously focused window */
	if (prev_focused && prev_focused != window) {
		prev_focused->is_focused = false;
		gui_window_normal_priority(prev_focused);
	}

	/* Set new focused window and boost its priority */
	gui_compositor->focused_window = window;
	if (window) {
		window->is_focused = true;
		gui_window_boost_priority(window);
	}

	spin_unlock(&gui_compositor->windows_lock);

	pr_debug("GUI: Window %llu gained focus\n", window ? window->id : 0);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_window_set_focus);

struct gui_window *gui_window_get_focused(void)
{
	if (!gui_compositor)
		return NULL;

	return READ_ONCE(gui_compositor->focused_window);
}
EXPORT_SYMBOL_GPL(gui_window_get_focused);

/*
 * Scheduler Integration
 */

void gui_window_boost_priority(struct gui_window *window)
{
	struct task_struct *task;

	if (!window)
		return;

	task = window->owner_task;
	if (!task)
		return;

	/*
	 * Boost priority of the focused window's task
	 * This gives it better CPU scheduling for responsive UI
	 */
	// Note: In a real implementation, we would use proper scheduler APIs
	pr_debug("GUI: Boosting priority for window %llu (PID %d)\n",
		 window->id, window->owner_pid);
}
EXPORT_SYMBOL_GPL(gui_window_boost_priority);

void gui_window_normal_priority(struct gui_window *window)
{
	struct task_struct *task;

	if (!window)
		return;

	task = window->owner_task;
	if (!task)
		return;

	pr_debug("GUI: Normalizing priority for window %llu (PID %d)\n",
		 window->id, window->owner_pid);
}
EXPORT_SYMBOL_GPL(gui_window_normal_priority);

/*
 * Frame Submission
 */

int gui_submit_frame(struct gui_frame *frame)
{
	ktime_t now;

	if (!gui_compositor || !frame)
		return -EINVAL;

	now = ktime_get();
	frame->timing.submit_time_ns = ktime_to_ns(now);

	/* Update statistics */
	atomic64_inc(&gui_compositor->total_frames);

	pr_debug("GUI: Frame submitted with %u layers\n", frame->num_layers);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_submit_frame);

/*
 * DRM/KMS Integration
 */

int gui_register_drm_device(struct drm_device *dev)
{
	if (!gui_compositor || !dev)
		return -EINVAL;

	gui_compositor->drm_dev = dev;

	pr_info("GUI: Registered DRM device %s\n", dev_name(dev->dev));

	return 0;
}
EXPORT_SYMBOL_GPL(gui_register_drm_device);

void gui_unregister_drm_device(struct drm_device *dev)
{
	if (gui_compositor && gui_compositor->drm_dev == dev)
		gui_compositor->drm_dev = NULL;
}
EXPORT_SYMBOL_GPL(gui_unregister_drm_device);

void gui_vsync_notify(struct drm_device *dev, u64 timestamp_ns)
{
	if (!gui_compositor)
		return;

	gui_compositor->last_vsync_time_ns = timestamp_ns;

	/* Calculate vsync period for frame pacing */
	if (gui_compositor->vsync_period_ns == 0) {
		/* Assume 60Hz for now, should be queried from DRM */
		gui_compositor->vsync_period_ns = NSEC_PER_SEC / 60;
	}
}
EXPORT_SYMBOL_GPL(gui_vsync_notify);

/*
 * Hardware Capability Queries
 */

bool gui_hw_supports_blur(void)
{
	/* Query DRM device capabilities */
	return true; /* Placeholder */
}
EXPORT_SYMBOL_GPL(gui_hw_supports_blur);

bool gui_hw_supports_shadows(void)
{
	return true; /* Placeholder */
}
EXPORT_SYMBOL_GPL(gui_hw_supports_shadows);

bool gui_hw_supports_rounded_corners(void)
{
	return true; /* Placeholder */
}
EXPORT_SYMBOL_GPL(gui_hw_supports_rounded_corners);

int gui_hw_max_compositor_layers(void)
{
	return 16; /* Placeholder - should query HW */
}
EXPORT_SYMBOL_GPL(gui_hw_max_compositor_layers);

/*
 * Performance Configuration
 */

void gui_set_perf_mode(enum gui_perf_mode mode)
{
	if (!gui_compositor)
		return;

	gui_compositor->perf_config.mode = mode;

	switch (mode) {
	case GUI_PERF_LOW_LATENCY:
		gui_compositor->perf_config.target_fps = 144;
		gui_compositor->perf_config.max_latency_us = 5000;
		gui_compositor->perf_config.enable_hw_effects = 1;
		break;
	case GUI_PERF_POWER_SAVING:
		gui_compositor->perf_config.target_fps = 30;
		gui_compositor->perf_config.max_latency_us = 20000;
		gui_compositor->perf_config.enable_hw_effects = 0;
		break;
	case GUI_PERF_HIGH_QUALITY:
		gui_compositor->perf_config.target_fps = 60;
		gui_compositor->perf_config.max_latency_us = 10000;
		gui_compositor->perf_config.enable_hw_effects = 1;
		break;
	case GUI_PERF_BALANCED:
	default:
		gui_compositor->perf_config.target_fps = 60;
		gui_compositor->perf_config.max_latency_us = 10000;
		gui_compositor->perf_config.enable_hw_effects = 1;
		break;
	}

	pr_info("GUI: Performance mode set to %d\n", mode);
}
EXPORT_SYMBOL_GPL(gui_set_perf_mode);

enum gui_perf_mode gui_get_perf_mode(void)
{
	if (!gui_compositor)
		return GUI_PERF_BALANCED;

	return gui_compositor->perf_config.mode;
}
EXPORT_SYMBOL_GPL(gui_get_perf_mode);

/*
 * Sysfs Interface
 */

static ssize_t latency_target_us_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	if (!gui_compositor)
		return -ENODEV;

	return sprintf(buf, "%u\n", gui_compositor->perf_config.max_latency_us);
}

static ssize_t latency_target_us_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	u32 latency;

	if (!gui_compositor)
		return -ENODEV;

	if (kstrtou32(buf, 10, &latency))
		return -EINVAL;

	gui_compositor->perf_config.max_latency_us = latency;

	return count;
}

static ssize_t enable_hw_effects_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	if (!gui_compositor)
		return -ENODEV;

	return sprintf(buf, "%u\n", gui_compositor->perf_config.enable_hw_effects);
}

static ssize_t enable_hw_effects_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	u32 enable;

	if (!gui_compositor)
		return -ENODEV;

	if (kstrtou32(buf, 10, &enable))
		return -EINVAL;

	gui_compositor->perf_config.enable_hw_effects = !!enable;

	return count;
}

static ssize_t perf_mode_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	const char *mode_str;

	if (!gui_compositor)
		return -ENODEV;

	switch (gui_compositor->perf_config.mode) {
	case GUI_PERF_LOW_LATENCY:
		mode_str = "low_latency";
		break;
	case GUI_PERF_POWER_SAVING:
		mode_str = "power_saving";
		break;
	case GUI_PERF_HIGH_QUALITY:
		mode_str = "high_quality";
		break;
	case GUI_PERF_BALANCED:
	default:
		mode_str = "balanced";
		break;
	}

	return sprintf(buf, "%s\n", mode_str);
}

static ssize_t perf_mode_store(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	enum gui_perf_mode mode;

	if (!gui_compositor)
		return -ENODEV;

	if (sysfs_streq(buf, "low_latency"))
		mode = GUI_PERF_LOW_LATENCY;
	else if (sysfs_streq(buf, "power_saving"))
		mode = GUI_PERF_POWER_SAVING;
	else if (sysfs_streq(buf, "high_quality"))
		mode = GUI_PERF_HIGH_QUALITY;
	else if (sysfs_streq(buf, "balanced"))
		mode = GUI_PERF_BALANCED;
	else
		return -EINVAL;

	gui_set_perf_mode(mode);

	return count;
}

static struct kobj_attribute latency_target_us_attr =
	__ATTR_RW(latency_target_us);
static struct kobj_attribute enable_hw_effects_attr =
	__ATTR_RW(enable_hw_effects);
static struct kobj_attribute perf_mode_attr =
	__ATTR_RW(perf_mode);

static struct attribute *gui_attrs[] = {
	&latency_target_us_attr.attr,
	&enable_hw_effects_attr.attr,
	&perf_mode_attr.attr,
	NULL,
};

static struct attribute_group gui_attr_group = {
	.attrs = gui_attrs,
};

/*
 * Module Initialization
 */

int __init gui_compositor_init(void)
{
	int ret;

	/* Allocate compositor structure */
	gui_compositor = kzalloc(sizeof(*gui_compositor), GFP_KERNEL);
	if (!gui_compositor)
		return -ENOMEM;

	INIT_LIST_HEAD(&gui_compositor->windows);
	spin_lock_init(&gui_compositor->windows_lock);

	atomic64_set(&gui_compositor->total_frames, 0);
	atomic64_set(&gui_compositor->dropped_frames, 0);

	/* Set default performance mode */
	gui_set_perf_mode(GUI_PERF_BALANCED);

	/* Create sysfs directory */
	gui_kobj = kobject_create_and_add("gui", kernel_kobj);
	if (!gui_kobj) {
		ret = -ENOMEM;
		goto err_free_compositor;
	}

	ret = sysfs_create_group(gui_kobj, &gui_attr_group);
	if (ret)
		goto err_put_kobj;

	pr_info("GUI Compositor: Initialized with Windows 11-style integration\n");
	pr_info("GUI: Sysfs interface available at /sys/kernel/gui/\n");

	return 0;

err_put_kobj:
	kobject_put(gui_kobj);
err_free_compositor:
	kfree(gui_compositor);
	gui_compositor = NULL;
	return ret;
}

void __exit gui_compositor_exit(void)
{
	if (gui_kobj) {
		sysfs_remove_group(gui_kobj, &gui_attr_group);
		kobject_put(gui_kobj);
	}

	if (gui_compositor) {
		kfree(gui_compositor);
		gui_compositor = NULL;
	}

	pr_info("GUI Compositor: Exited\n");
}

subsys_initcall(gui_compositor_init);
module_exit(gui_compositor_exit);

MODULE_AUTHOR("Linux Kernel Contributors");
MODULE_DESCRIPTION("Modern GUI Compositor Subsystem");
MODULE_LICENSE("GPL v2");
