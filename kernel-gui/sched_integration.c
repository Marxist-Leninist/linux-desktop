// SPDX-License-Identifier: GPL-2.0
/*
 * GUI Scheduler Integration
 *
 * Integrates GUI compositor hints with the CPU scheduler to boost
 * priority of focused window tasks for better responsiveness.
 *
 * Copyright (C) 2026 Linux Kernel Contributors
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/prio.h>
#include <linux/sched/task.h>
#include <linux/gui/compositor.h>

/* Priority boost values */
#define GUI_FOCUSED_PRIO_BOOST    5
#define GUI_COMPOSITOR_PRIO_BOOST 10
#define GUI_MINIMIZED_PRIO_PENALTY 5

/**
 * gui_sched_boost_task - Boost scheduling priority for a task
 * @task: Task to boost
 * @boost: Priority boost value (positive = higher priority, negative = lower)
 *
 * This function adjusts the task's scheduling priority to improve
 * responsiveness. For focused windows, we boost priority; for
 * minimized windows, we may reduce it.
 *
 * Uses set_user_nice() for SCHED_NORMAL tasks. The nice value is adjusted
 * inversely to the boost value (higher boost = lower nice = higher priority).
 */
static void gui_sched_boost_task(struct task_struct *task, int boost)
{
	int current_nice, target_nice;

	if (!task)
		return;

	/* Only adjust SCHED_NORMAL/SCHED_BATCH tasks, not RT tasks */
	if (task->policy != SCHED_NORMAL && task->policy != SCHED_BATCH) {
		pr_debug("GUI Sched: Skipping RT task %d (%s)\n",
			 task->pid, task->comm);
		return;
	}

	current_nice = task_nice(task);

	/*
	 * Calculate target nice value:
	 * - boost > 0: decrease nice (increase priority)
	 * - boost < 0: increase nice (decrease priority)
	 * - boost = 0: restore to default (nice 0)
	 *
	 * We use the boost value directly as nice adjustment.
	 * Nice values range from -20 (highest priority) to 19 (lowest).
	 */
	if (boost == 0) {
		target_nice = 0;  /* Restore to default */
	} else {
		target_nice = -boost;  /* Invert: positive boost = negative nice */
	}

	/* Clamp to valid nice range */
	if (target_nice < MIN_NICE)
		target_nice = MIN_NICE;
	if (target_nice > MAX_NICE)
		target_nice = MAX_NICE;

	/* Only adjust if there's a change */
	if (current_nice != target_nice) {
		set_user_nice(task, target_nice);
		pr_debug("GUI Sched: Task %d (%s) nice %d -> %d (boost %d)\n",
			 task->pid, task->comm, current_nice, target_nice, boost);
	}
}

/**
 * gui_sched_update_window_priority - Update task priority based on window state
 * @window: GUI window
 *
 * Called when window focus or state changes to adjust the owner task's
 * scheduling priority accordingly.
 */
void gui_sched_update_window_priority(struct gui_window *window)
{
	int boost = 0;

	if (!window || !window->owner_task)
		return;

	/* Determine priority boost based on window state */
	if (window->is_focused) {
		boost = GUI_FOCUSED_PRIO_BOOST;
	} else if (window->config.state == GUI_WINDOW_MINIMIZED) {
		boost = -GUI_MINIMIZED_PRIO_PENALTY;
	}

	/* Apply the boost */
	if (boost != window->sched_boost) {
		gui_sched_boost_task(window->owner_task, boost);
		window->sched_boost = boost;
	}
}

/**
 * gui_sched_should_preempt - Check if GUI task should preempt current task
 * @task: Task requesting preemption
 * @current: Currently running task
 *
 * Returns: true if the GUI task should preempt current task
 */
bool gui_sched_should_preempt(struct task_struct *task,
			      struct task_struct *current)
{
	struct gui_window *focused;

	if (!gui_compositor || !task || !current)
		return false;

	focused = gui_window_get_focused();
	if (!focused)
		return false;

	/* If the requesting task owns the focused window, allow preemption */
	if (focused->owner_task == task)
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(gui_sched_should_preempt);

/**
 * gui_sched_tick - Called on each scheduler tick for GUI tasks
 * @task: Current task
 *
 * Provides scheduler tick feedback for GUI tasks to help with
 * frame pacing and latency reduction.
 */
void gui_sched_tick(struct task_struct *task)
{
	struct gui_window *focused;

	if (!gui_compositor || !task)
		return;

	focused = gui_window_get_focused();
	if (!focused || focused->owner_task != task)
		return;

	/*
	 * For focused window tasks, we could:
	 * - Track time slices to ensure smooth frame delivery
	 * - Adjust dynamic priority based on frame timing
	 * - Prevent premature preemption during critical rendering
	 */
}
EXPORT_SYMBOL_GPL(gui_sched_tick);

/**
 * gui_sched_wakeup - Called when a GUI task wakes up
 * @task: Waking task
 *
 * Optimize CPU selection for GUI tasks based on cache affinity
 * and current load.
 */
int gui_sched_wakeup(struct task_struct *task)
{
	struct gui_window *focused;

	if (!gui_compositor || !task)
		return -1; /* Use default CPU selection */

	focused = gui_window_get_focused();
	if (!focused || focused->owner_task != task)
		return -1;

	/*
	 * For focused window tasks, we could:
	 * - Prefer the CPU where compositor is running (cache affinity)
	 * - Prefer CPUs with lower latency (favor P-cores over E-cores)
	 * - Avoid CPUs running heavy background tasks
	 */

	pr_debug("GUI Sched: Waking focused window task %d (%s)\n",
		 task->pid, task->comm);

	return -1; /* Use default for now */
}
EXPORT_SYMBOL_GPL(gui_sched_wakeup);

/**
 * gui_sched_compositor_boost - Boost compositor task priority
 * @pid: Compositor process ID
 *
 * Called when a process registers as the compositor to boost its
 * scheduling priority for low-latency composition.
 */
int gui_sched_compositor_boost(pid_t pid)
{
	struct task_struct *task;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (task) {
		get_task_struct(task);
		gui_sched_boost_task(task, GUI_COMPOSITOR_PRIO_BOOST);
		put_task_struct(task);
	}
	rcu_read_unlock();

	if (!task)
		return -ESRCH;

	pr_info("GUI Sched: Boosted compositor task %d\n", pid);
	return 0;
}
EXPORT_SYMBOL_GPL(gui_sched_compositor_boost);

/**
 * gui_sched_init - Initialize scheduler integration
 */
int __init gui_sched_init(void)
{
	pr_info("GUI: Scheduler integration initialized\n");
	pr_info("GUI:   Focused window boost: +%d\n", GUI_FOCUSED_PRIO_BOOST);
	pr_info("GUI:   Compositor boost: +%d\n", GUI_COMPOSITOR_PRIO_BOOST);
	pr_info("GUI:   Minimized window penalty: -%d\n", GUI_MINIMIZED_PRIO_PENALTY);

	return 0;
}

/**
 * gui_sched_exit - Clean up scheduler integration
 */
void __exit gui_sched_exit(void)
{
	pr_info("GUI: Scheduler integration exited\n");
}
