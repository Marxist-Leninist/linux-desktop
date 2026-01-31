// SPDX-License-Identifier: GPL-2.0
/*
 * GUI Gesture Recognition
 *
 * Kernel-level gesture recognition for Windows 11-style multi-touch
 * gestures with low latency.
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/gui/compositor.h>

#define MAX_TOUCH_POINTS 10
#define SWIPE_THRESHOLD_PX 100
#define PINCH_THRESHOLD_SCALE 0.3
#define GESTURE_TIMEOUT_MS 500

/**
 * enum gesture_state - State machine for gesture recognition
 */
enum gesture_state {
	GESTURE_STATE_IDLE,
	GESTURE_STATE_TOUCH_START,
	GESTURE_STATE_TRACKING,
	GESTURE_STATE_RECOGNIZED,
	GESTURE_STATE_COMPLETED,
};

/**
 * struct touch_point - Individual touch point tracking
 */
struct touch_point {
	int id;
	int x, y;
	int start_x, start_y;
	unsigned long start_time;
	bool active;
};

/**
 * struct gesture_recognizer - Gesture recognition state
 */
struct gesture_recognizer {
	enum gesture_state state;
	struct touch_point touches[MAX_TOUCH_POINTS];
	int num_active_touches;

	/* Gesture parameters */
	int swipe_direction;  /* -1=none, 0=up, 1=down, 2=left, 3=right */
	float pinch_scale;
	float rotation_angle;

	/* Timing */
	unsigned long gesture_start_time;

	/* Configuration */
	u32 enabled_gestures;
	u32 sensitivity;
	u32 min_fingers;
	u32 max_fingers;
};

static struct gesture_recognizer *gesture_recognizer;

/**
 * gesture_calculate_swipe - Detect swipe gestures
 */
static int gesture_calculate_swipe(struct gesture_recognizer *gr)
{
	int avg_dx = 0, avg_dy = 0;
	int i, count = 0;

	/* Calculate average movement across all touch points */
	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (!gr->touches[i].active)
			continue;

		avg_dx += gr->touches[i].x - gr->touches[i].start_x;
		avg_dy += gr->touches[i].y - gr->touches[i].start_y;
		count++;
	}

	if (count == 0)
		return -1;

	avg_dx /= count;
	avg_dy /= count;

	/* Determine swipe direction based on dominant axis */
	if (abs(avg_dx) > abs(avg_dy)) {
		if (avg_dx > SWIPE_THRESHOLD_PX)
			return 3; /* Right */
		else if (avg_dx < -SWIPE_THRESHOLD_PX)
			return 2; /* Left */
	} else {
		if (avg_dy > SWIPE_THRESHOLD_PX)
			return 1; /* Down */
		else if (avg_dy < -SWIPE_THRESHOLD_PX)
			return 0; /* Up */
	}

	return -1;
}

/**
 * gesture_calculate_pinch - Detect pinch gestures
 */
static float gesture_calculate_pinch(struct gesture_recognizer *gr)
{
	int i, j;
	float initial_dist = 0, current_dist = 0;
	int pairs = 0;

	/* Calculate average distance between touch points */
	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (!gr->touches[i].active)
			continue;

		for (j = i + 1; j < MAX_TOUCH_POINTS; j++) {
			if (!gr->touches[j].active)
				continue;

			int dx_init = gr->touches[i].start_x - gr->touches[j].start_x;
			int dy_init = gr->touches[i].start_y - gr->touches[j].start_y;
			int dx_curr = gr->touches[i].x - gr->touches[j].x;
			int dy_curr = gr->touches[i].y - gr->touches[j].y;

			initial_dist += int_sqrt(dx_init * dx_init + dy_init * dy_init);
			current_dist += int_sqrt(dx_curr * dx_curr + dy_curr * dy_curr);
			pairs++;
		}
	}

	if (pairs == 0 || initial_dist == 0)
		return 1.0;

	return current_dist / initial_dist;
}

/**
 * gui_gesture_touch_down - Handle touch down event
 */
void gui_gesture_touch_down(int id, int x, int y)
{
	struct gesture_recognizer *gr = gesture_recognizer;
	int i;

	if (!gr)
		return;

	/* Find free slot for this touch */
	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (!gr->touches[i].active) {
			gr->touches[i].id = id;
			gr->touches[i].x = x;
			gr->touches[i].y = y;
			gr->touches[i].start_x = x;
			gr->touches[i].start_y = y;
			gr->touches[i].start_time = jiffies;
			gr->touches[i].active = true;
			gr->num_active_touches++;

			pr_debug("GUI Gesture: Touch down id=%d at (%d,%d), active=%d\n",
				 id, x, y, gr->num_active_touches);

			if (gr->state == GESTURE_STATE_IDLE) {
				gr->state = GESTURE_STATE_TOUCH_START;
				gr->gesture_start_time = jiffies;
			}

			break;
		}
	}
}

/**
 * gui_gesture_touch_move - Handle touch move event
 */
void gui_gesture_touch_move(int id, int x, int y)
{
	struct gesture_recognizer *gr = gesture_recognizer;
	int i;

	if (!gr)
		return;

	/* Update touch position */
	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (gr->touches[i].active && gr->touches[i].id == id) {
			gr->touches[i].x = x;
			gr->touches[i].y = y;

			/* Transition to tracking state */
			if (gr->state == GESTURE_STATE_TOUCH_START)
				gr->state = GESTURE_STATE_TRACKING;

			/* Recognize gestures based on number of fingers */
			if (gr->state == GESTURE_STATE_TRACKING) {
				if (gr->num_active_touches == 3) {
					/* Three-finger swipe for task switching */
					gr->swipe_direction = gesture_calculate_swipe(gr);
					if (gr->swipe_direction >= 0) {
						pr_info("GUI: Three-finger swipe detected (direction=%d)\n",
							gr->swipe_direction);
						gr->state = GESTURE_STATE_RECOGNIZED;
					}
				} else if (gr->num_active_touches == 4) {
					/* Four-finger pinch for show desktop */
					gr->pinch_scale = gesture_calculate_pinch(gr);
					if (gr->pinch_scale < (1.0 - PINCH_THRESHOLD_SCALE)) {
						pr_info("GUI: Four-finger pinch detected (scale=%.2f)\n",
							gr->pinch_scale);
						gr->state = GESTURE_STATE_RECOGNIZED;
					}
				}
			}

			break;
		}
	}
}

/**
 * gui_gesture_touch_up - Handle touch up event
 */
void gui_gesture_touch_up(int id)
{
	struct gesture_recognizer *gr = gesture_recognizer;
	int i;

	if (!gr)
		return;

	/* Deactivate touch */
	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (gr->touches[i].active && gr->touches[i].id == id) {
			gr->touches[i].active = false;
			gr->num_active_touches--;

			pr_debug("GUI Gesture: Touch up id=%d, active=%d\n",
				 id, gr->num_active_touches);

			/* If all touches released, complete gesture */
			if (gr->num_active_touches == 0) {
				if (gr->state == GESTURE_STATE_RECOGNIZED) {
					gr->state = GESTURE_STATE_COMPLETED;
					pr_debug("GUI: Gesture completed\n");
				}
				gr->state = GESTURE_STATE_IDLE;
			}

			break;
		}
	}
}

/**
 * gui_gesture_set_config - Configure gesture recognition
 */
int gui_gesture_set_config(struct gui_gesture_config *config)
{
	struct gesture_recognizer *gr = gesture_recognizer;

	if (!gr || !config)
		return -EINVAL;

	gr->enabled_gestures = config->enabled_gestures;
	gr->sensitivity = config->sensitivity;
	gr->min_fingers = config->min_fingers;
	gr->max_fingers = config->max_fingers;

	pr_info("GUI: Gesture config updated (enabled=0x%x, sensitivity=%u)\n",
		gr->enabled_gestures, gr->sensitivity);

	return 0;
}

/**
 * gui_gesture_init - Initialize gesture recognition
 */
int __init gui_gesture_init(void)
{
	gesture_recognizer = kzalloc(sizeof(*gesture_recognizer), GFP_KERNEL);
	if (!gesture_recognizer)
		return -ENOMEM;

	gesture_recognizer->state = GESTURE_STATE_IDLE;
	gesture_recognizer->num_active_touches = 0;
	gesture_recognizer->enabled_gestures = 0xFF; /* All enabled */
	gesture_recognizer->sensitivity = 50;
	gesture_recognizer->min_fingers = 2;
	gesture_recognizer->max_fingers = 10;

	pr_info("GUI: Gesture recognition initialized\n");
	return 0;
}

/**
 * gui_gesture_exit - Clean up gesture recognition
 */
void __exit gui_gesture_exit(void)
{
	kfree(gesture_recognizer);
	gesture_recognizer = NULL;
}
