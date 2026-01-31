// SPDX-License-Identifier: GPL-2.0
/*
 * GUI Low-Latency Input Path
 *
 * Optimized input event routing for GUI applications with
 * direct-to-focused-window delivery and priority handling.
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/gui/compositor.h>

/**
 * struct gui_input_event - GUI input event with timing info
 */
struct gui_input_event {
	struct list_head list;
	struct input_event event;
	ktime_t kernel_timestamp;
	ktime_t hw_timestamp;
	u64 target_window_id;
};

/**
 * struct gui_input_handler - Low-latency input handler
 */
struct gui_input_handler {
	struct list_head events;
	spinlock_t lock;
	wait_queue_head_t wait;
	atomic64_t event_count;
	atomic64_t delivered_count;

	/* Latency tracking */
	u64 min_latency_ns;
	u64 max_latency_ns;
	u64 avg_latency_ns;

	/* Configuration */
	bool enabled;
	u32 priority_boost;
};

static struct gui_input_handler *gui_input_handler;

/**
 * gui_input_route_event - Route input event to focused window
 * @dev: Input device
 * @type: Event type
 * @code: Event code
 * @value: Event value
 *
 * This function provides a fast path for routing input events directly
 * to the focused window's event queue, bypassing normal input layers
 * when possible for reduced latency.
 */
void gui_input_route_event(struct input_dev *dev, unsigned int type,
			    unsigned int code, int value)
{
	struct gui_input_handler *handler = gui_input_handler;
	struct gui_input_event *gui_event;
	struct gui_window *focused;
	ktime_t now;
	unsigned long flags;

	if (!handler || !handler->enabled)
		return;

	/* Get focused window for routing */
	focused = gui_window_get_focused();
	if (!focused)
		return;

	/* Allocate event structure */
	gui_event = kmalloc(sizeof(*gui_event), GFP_ATOMIC);
	if (!gui_event)
		return;

	/* Fill in event data */
	now = ktime_get();
	gui_event->event.input_event_sec = ktime_get_real_seconds();
	gui_event->event.input_event_usec = ktime_to_us(now) % USEC_PER_SEC;
	gui_event->event.type = type;
	gui_event->event.code = code;
	gui_event->event.value = value;
	gui_event->kernel_timestamp = now;
	gui_event->hw_timestamp = now; /* Would be from hardware if available */
	gui_event->target_window_id = focused->id;

	/* Add to event queue */
	spin_lock_irqsave(&handler->lock, flags);
	list_add_tail(&gui_event->list, &handler->events);
	atomic64_inc(&handler->event_count);
	spin_unlock_irqrestore(&handler->lock, flags);

	/* Wake up waiting consumers */
	wake_up_interruptible(&handler->wait);

	pr_debug("GUI Input: Routed %s event (type=%u, code=%u) to window %llu\n",
		 dev->name, type, code, focused->id);
}
EXPORT_SYMBOL_GPL(gui_input_route_event);

/**
 * gui_input_get_latency_stats - Get input latency statistics
 * @min_ns: Output for minimum latency
 * @max_ns: Output for maximum latency
 * @avg_ns: Output for average latency
 *
 * Returns: 0 on success, negative error on failure
 */
int gui_input_get_latency_stats(u64 *min_ns, u64 *max_ns, u64 *avg_ns)
{
	struct gui_input_handler *handler = gui_input_handler;

	if (!handler)
		return -ENODEV;

	if (min_ns)
		*min_ns = handler->min_latency_ns;
	if (max_ns)
		*max_ns = handler->max_latency_ns;
	if (avg_ns)
		*avg_ns = handler->avg_latency_ns;

	return 0;
}
EXPORT_SYMBOL_GPL(gui_input_get_latency_stats);

/**
 * gui_input_update_latency - Update latency statistics
 * @latency_ns: Measured latency in nanoseconds
 */
static void gui_input_update_latency(u64 latency_ns)
{
	struct gui_input_handler *handler = gui_input_handler;

	if (!handler)
		return;

	/* Update min/max */
	if (handler->min_latency_ns == 0 || latency_ns < handler->min_latency_ns)
		handler->min_latency_ns = latency_ns;
	if (latency_ns > handler->max_latency_ns)
		handler->max_latency_ns = latency_ns;

	/* Update running average (simple exponential moving average) */
	if (handler->avg_latency_ns == 0)
		handler->avg_latency_ns = latency_ns;
	else
		handler->avg_latency_ns = (handler->avg_latency_ns * 7 + latency_ns) / 8;
}

/**
 * gui_input_event_delivered - Mark event as delivered and update stats
 * @gui_event: GUI input event that was delivered
 */
void gui_input_event_delivered(struct gui_input_event *gui_event)
{
	ktime_t now = ktime_get();
	u64 latency_ns;

	if (!gui_event)
		return;

	/* Calculate end-to-end latency */
	latency_ns = ktime_to_ns(ktime_sub(now, gui_event->kernel_timestamp));

	/* Update statistics */
	gui_input_update_latency(latency_ns);

	atomic64_inc(&gui_input_handler->delivered_count);

	pr_debug("GUI Input: Event delivered in %llu ns\n", latency_ns);
}
EXPORT_SYMBOL_GPL(gui_input_event_delivered);

/**
 * gui_input_set_enabled - Enable or disable low-latency path
 * @enabled: true to enable, false to disable
 */
void gui_input_set_enabled(bool enabled)
{
	if (gui_input_handler)
		gui_input_handler->enabled = enabled;
}
EXPORT_SYMBOL_GPL(gui_input_set_enabled);

/**
 * gui_input_set_priority - Set input handling priority boost
 * @priority: Priority boost value (0-100)
 */
void gui_input_set_priority(u32 priority)
{
	if (gui_input_handler && priority <= 100)
		gui_input_handler->priority_boost = priority;
}
EXPORT_SYMBOL_GPL(gui_input_set_priority);

/**
 * Sysfs attributes for input latency monitoring
 */
static ssize_t input_latency_min_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	u64 min_ns;

	if (gui_input_get_latency_stats(&min_ns, NULL, NULL))
		return -ENODEV;

	return sprintf(buf, "%llu\n", min_ns);
}

static ssize_t input_latency_max_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	u64 max_ns;

	if (gui_input_get_latency_stats(NULL, &max_ns, NULL))
		return -ENODEV;

	return sprintf(buf, "%llu\n", max_ns);
}

static ssize_t input_latency_avg_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	u64 avg_ns;

	if (gui_input_get_latency_stats(NULL, NULL, &avg_ns))
		return -ENODEV;

	return sprintf(buf, "%llu\n", avg_ns);
}

static ssize_t input_enabled_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	if (!gui_input_handler)
		return -ENODEV;

	return sprintf(buf, "%d\n", gui_input_handler->enabled);
}

static ssize_t input_enabled_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	int enabled;

	if (kstrtoint(buf, 10, &enabled))
		return -EINVAL;

	gui_input_set_enabled(!!enabled);

	return count;
}

static struct kobj_attribute input_latency_min_attr =
	__ATTR_RO(input_latency_min);
static struct kobj_attribute input_latency_max_attr =
	__ATTR_RO(input_latency_max);
static struct kobj_attribute input_latency_avg_attr =
	__ATTR_RO(input_latency_avg);
static struct kobj_attribute input_enabled_attr =
	__ATTR_RW(input_enabled);

static struct attribute *gui_input_attrs[] = {
	&input_latency_min_attr.attr,
	&input_latency_max_attr.attr,
	&input_latency_avg_attr.attr,
	&input_enabled_attr.attr,
	NULL,
};

static struct attribute_group gui_input_attr_group = {
	.name = "input",
	.attrs = gui_input_attrs,
};

/**
 * gui_input_init - Initialize low-latency input handler
 */
int __init gui_input_init(void)
{
	int ret;

	gui_input_handler = kzalloc(sizeof(*gui_input_handler), GFP_KERNEL);
	if (!gui_input_handler)
		return -ENOMEM;

	INIT_LIST_HEAD(&gui_input_handler->events);
	spin_lock_init(&gui_input_handler->lock);
	init_waitqueue_head(&gui_input_handler->wait);

	atomic64_set(&gui_input_handler->event_count, 0);
	atomic64_set(&gui_input_handler->delivered_count, 0);

	gui_input_handler->enabled = true;
	gui_input_handler->priority_boost = 50;

	/* Create sysfs interface */
	if (gui_kobj) {
		ret = sysfs_create_group(gui_kobj, &gui_input_attr_group);
		if (ret)
			pr_warn("GUI Input: Failed to create sysfs group\n");
	}

	pr_info("GUI: Low-latency input path initialized\n");
	pr_info("GUI:   Input statistics available at /sys/kernel/gui/input/\n");

	return 0;
}

/**
 * gui_input_exit - Clean up low-latency input handler
 */
void __exit gui_input_exit(void)
{
	struct gui_input_event *event, *tmp;

	if (!gui_input_handler)
		return;

	/* Remove sysfs interface */
	if (gui_kobj)
		sysfs_remove_group(gui_kobj, &gui_input_attr_group);

	/* Free any remaining events */
	list_for_each_entry_safe(event, tmp, &gui_input_handler->events, list) {
		list_del(&event->list);
		kfree(event);
	}

	kfree(gui_input_handler);
	gui_input_handler = NULL;

	pr_info("GUI: Low-latency input path exited\n");
}
