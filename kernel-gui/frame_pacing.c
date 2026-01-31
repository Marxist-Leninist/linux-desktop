// SPDX-License-Identifier: GPL-2.0
/*
 * GUI Frame Pacing
 *
 * Kernel-managed frame pacing for smooth, tear-free rendering with
 * predictable frame times synchronized to display vsync.
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/gui/compositor.h>

#define DEFAULT_REFRESH_RATE_HZ 60
#define NSEC_PER_FRAME(hz) (NSEC_PER_SEC / (hz))

/**
 * struct frame_pacer - Frame pacing state
 */
struct frame_pacer {
	struct hrtimer timer;
	struct work_struct work;

	/* Timing configuration */
	u32 refresh_rate_hz;
	u64 frame_period_ns;
	u64 next_vsync_ns;

	/* Frame tracking */
	atomic64_t frame_count;
	atomic64_t missed_frames;

	/* Performance mode */
	enum gui_perf_mode perf_mode;

	/* Enable/disable */
	bool enabled;
	bool adaptive;  /* Adaptive sync (VRR) */
};

static struct frame_pacer *frame_pacer;

/**
 * gui_frame_pacer_vsync_callback - Called on each vsync
 */
static void gui_frame_pacer_vsync_callback(struct work_struct *work)
{
	struct frame_pacer *pacer = container_of(work, struct frame_pacer, work);
	ktime_t now = ktime_get();
	u64 now_ns = ktime_to_ns(now);

	/* Update vsync timing */
	pacer->next_vsync_ns = now_ns + pacer->frame_period_ns;

	/* Increment frame counter */
	atomic64_inc(&pacer->frame_count);

	/* Notify compositor of vsync */
	if (gui_compositor && gui_compositor->drm_dev) {
		gui_vsync_notify(gui_compositor->drm_dev, now_ns);
	}

	pr_debug("GUI Frame Pacer: Vsync at %llu ns (frame %lld)\n",
		 now_ns, atomic64_read(&pacer->frame_count));
}

/**
 * gui_frame_pacer_timer_callback - High-resolution timer callback
 */
static enum hrtimer_restart gui_frame_pacer_timer_callback(struct hrtimer *timer)
{
	struct frame_pacer *pacer = container_of(timer, struct frame_pacer, timer);
	ktime_t now;

	if (!pacer->enabled)
		return HRTIMER_NORESTART;

	/* Schedule work to handle vsync */
	schedule_work(&pacer->work);

	/* Schedule next timer */
	now = ktime_get();
	hrtimer_forward(timer, now, ns_to_ktime(pacer->frame_period_ns));

	return HRTIMER_RESTART;
}

/**
 * gui_frame_pacer_set_refresh_rate - Set display refresh rate
 * @refresh_rate_hz: Refresh rate in Hz (e.g., 60, 120, 144)
 */
int gui_frame_pacer_set_refresh_rate(u32 refresh_rate_hz)
{
	if (!frame_pacer)
		return -ENODEV;

	if (refresh_rate_hz == 0 || refresh_rate_hz > 500)
		return -EINVAL;

	frame_pacer->refresh_rate_hz = refresh_rate_hz;
	frame_pacer->frame_period_ns = NSEC_PER_FRAME(refresh_rate_hz);

	if (gui_compositor)
		gui_compositor->vsync_period_ns = frame_pacer->frame_period_ns;

	pr_info("GUI Frame Pacer: Refresh rate set to %u Hz (%llu ns period)\n",
		refresh_rate_hz, frame_pacer->frame_period_ns);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_set_refresh_rate);

/**
 * gui_frame_pacer_get_next_vsync - Get time of next vsync
 *
 * Returns: Time of next vsync in nanoseconds, or 0 if unavailable
 */
u64 gui_frame_pacer_get_next_vsync(void)
{
	if (!frame_pacer)
		return 0;

	return frame_pacer->next_vsync_ns;
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_get_next_vsync);

/**
 * gui_frame_pacer_wait_for_vsync - Wait for next vsync
 *
 * Blocks until the next vsync occurs.
 *
 * Returns: 0 on success, negative error on failure
 */
int gui_frame_pacer_wait_for_vsync(void)
{
	u64 next_vsync_ns;
	ktime_t target;
	s64 sleep_ns;

	if (!frame_pacer || !frame_pacer->enabled)
		return -ENODEV;

	next_vsync_ns = frame_pacer->next_vsync_ns;
	target = ns_to_ktime(next_vsync_ns);
	sleep_ns = ktime_to_ns(ktime_sub(target, ktime_get()));

	if (sleep_ns > 0)
		hrtimer_nanosleep(sleep_ns, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_wait_for_vsync);

/**
 * gui_frame_pacer_start - Start frame pacing
 */
int gui_frame_pacer_start(void)
{
	ktime_t start_time;

	if (!frame_pacer)
		return -ENODEV;

	if (frame_pacer->enabled)
		return -EALREADY;

	/* Initialize vsync timing */
	start_time = ktime_get();
	frame_pacer->next_vsync_ns = ktime_to_ns(start_time) + frame_pacer->frame_period_ns;

	/* Start high-resolution timer */
	hrtimer_start(&frame_pacer->timer,
		      ns_to_ktime(frame_pacer->frame_period_ns),
		      HRTIMER_MODE_REL);

	frame_pacer->enabled = true;

	pr_info("GUI Frame Pacer: Started at %u Hz\n", frame_pacer->refresh_rate_hz);

	return 0;
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_start);

/**
 * gui_frame_pacer_stop - Stop frame pacing
 */
void gui_frame_pacer_stop(void)
{
	if (!frame_pacer || !frame_pacer->enabled)
		return;

	hrtimer_cancel(&frame_pacer->timer);
	cancel_work_sync(&frame_pacer->work);

	frame_pacer->enabled = false;

	pr_info("GUI Frame Pacer: Stopped\n");
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_stop);

/**
 * gui_frame_pacer_set_mode - Set frame pacing mode based on perf config
 * @mode: Performance mode
 */
void gui_frame_pacer_set_mode(enum gui_perf_mode mode)
{
	u32 target_hz;

	if (!frame_pacer)
		return;

	frame_pacer->perf_mode = mode;

	/* Adjust refresh rate based on performance mode */
	switch (mode) {
	case GUI_PERF_LOW_LATENCY:
		target_hz = 144;
		break;
	case GUI_PERF_POWER_SAVING:
		target_hz = 30;
		break;
	case GUI_PERF_HIGH_QUALITY:
	case GUI_PERF_BALANCED:
	default:
		target_hz = 60;
		break;
	}

	gui_frame_pacer_set_refresh_rate(target_hz);
}
EXPORT_SYMBOL_GPL(gui_frame_pacer_set_mode);

/**
 * Sysfs attributes
 */
static ssize_t refresh_rate_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	if (!frame_pacer)
		return -ENODEV;

	return sprintf(buf, "%u\n", frame_pacer->refresh_rate_hz);
}

static ssize_t refresh_rate_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	u32 hz;

	if (kstrtou32(buf, 10, &hz))
		return -EINVAL;

	if (gui_frame_pacer_set_refresh_rate(hz))
		return -EINVAL;

	return count;
}

static ssize_t frame_count_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	if (!frame_pacer)
		return -ENODEV;

	return sprintf(buf, "%lld\n", atomic64_read(&frame_pacer->frame_count));
}

static ssize_t missed_frames_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	if (!frame_pacer)
		return -ENODEV;

	return sprintf(buf, "%lld\n", atomic64_read(&frame_pacer->missed_frames));
}

static ssize_t adaptive_sync_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	if (!frame_pacer)
		return -ENODEV;

	return sprintf(buf, "%d\n", frame_pacer->adaptive);
}

static ssize_t adaptive_sync_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	int adaptive;

	if (!frame_pacer)
		return -ENODEV;

	if (kstrtoint(buf, 10, &adaptive))
		return -EINVAL;

	frame_pacer->adaptive = !!adaptive;

	pr_info("GUI Frame Pacer: Adaptive sync %s\n",
		frame_pacer->adaptive ? "enabled" : "disabled");

	return count;
}

static struct kobj_attribute refresh_rate_attr =
	__ATTR_RW(refresh_rate);
static struct kobj_attribute frame_count_attr =
	__ATTR_RO(frame_count);
static struct kobj_attribute missed_frames_attr =
	__ATTR_RO(missed_frames);
static struct kobj_attribute adaptive_sync_attr =
	__ATTR_RW(adaptive_sync);

static struct attribute *frame_pacing_attrs[] = {
	&refresh_rate_attr.attr,
	&frame_count_attr.attr,
	&missed_frames_attr.attr,
	&adaptive_sync_attr.attr,
	NULL,
};

static struct attribute_group frame_pacing_attr_group = {
	.name = "frame_pacing",
	.attrs = frame_pacing_attrs,
};

/**
 * gui_frame_pacer_init - Initialize frame pacing
 */
int __init gui_frame_pacer_init(void)
{
	int ret;

	frame_pacer = kzalloc(sizeof(*frame_pacer), GFP_KERNEL);
	if (!frame_pacer)
		return -ENOMEM;

	/* Initialize timer and work */
	hrtimer_init(&frame_pacer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	frame_pacer->timer.function = gui_frame_pacer_timer_callback;
	INIT_WORK(&frame_pacer->work, gui_frame_pacer_vsync_callback);

	/* Set default configuration */
	frame_pacer->refresh_rate_hz = DEFAULT_REFRESH_RATE_HZ;
	frame_pacer->frame_period_ns = NSEC_PER_FRAME(DEFAULT_REFRESH_RATE_HZ);
	frame_pacer->perf_mode = GUI_PERF_BALANCED;
	frame_pacer->enabled = false;
	frame_pacer->adaptive = false;

	atomic64_set(&frame_pacer->frame_count, 0);
	atomic64_set(&frame_pacer->missed_frames, 0);

	/* Create sysfs interface */
	if (gui_kobj) {
		ret = sysfs_create_group(gui_kobj, &frame_pacing_attr_group);
		if (ret)
			pr_warn("GUI: Failed to create frame pacing sysfs group\n");
	}

	pr_info("GUI: Frame pacing initialized at %u Hz\n", DEFAULT_REFRESH_RATE_HZ);
	pr_info("GUI:   Frame statistics available at /sys/kernel/gui/frame_pacing/\n");

	return 0;
}

/**
 * gui_frame_pacer_exit - Clean up frame pacing
 */
void __exit gui_frame_pacer_exit(void)
{
	if (!frame_pacer)
		return;

	/* Stop frame pacing */
	gui_frame_pacer_stop();

	/* Remove sysfs interface */
	if (gui_kobj)
		sysfs_remove_group(gui_kobj, &frame_pacing_attr_group);

	kfree(frame_pacer);
	frame_pacer = NULL;

	pr_info("GUI: Frame pacing exited\n");
}
