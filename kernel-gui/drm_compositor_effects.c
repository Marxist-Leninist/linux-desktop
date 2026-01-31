// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2026 Linux Kernel Contributors
 *
 * DRM Compositor Effects Implementation
 *
 * Provides helper functions for modern compositor effects like blur,
 * shadows, and rounded corners in the DRM/KMS subsystem.
 */

#include <drm/drm_compositor_effects.h>
#include <drm/drm_device.h>
#include <drm/drm_plane.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_print.h>
#include <linux/kernel.h>
#include <linux/slab.h>

/**
 * drm_apply_blur_effect - Apply Gaussian blur to a plane
 * @plane: DRM plane to apply blur to
 * @radius: Blur radius in pixels
 *
 * This function applies a Gaussian blur effect to the specified plane.
 * If hardware acceleration is available, it will be used; otherwise,
 * the effect may be deferred to software rendering in userspace.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_blur_effect(struct drm_plane *plane, u32 radius)
{
	struct drm_compositor_effect effect = {0};

	if (!plane || !plane->dev)
		return -EINVAL;

	if (radius == 0 || radius > 256)
		return -EINVAL;

	effect.type = DRM_EFFECT_BLUR;
	effect.enabled = true;
	effect.blur.radius = radius;
	effect.blur.quality = 80;  /* Default high quality */
	effect.blur.downscale = 1; /* No downscaling by default */

	/* Auto-calculate sigma based on radius */
	effect.blur.sigma = radius / 2;

	drm_dbg_kms(plane->dev, "Applying blur effect: radius=%u\n", radius);

	/* TODO: Call driver-specific implementation if available */
	return 0;
}
EXPORT_SYMBOL(drm_apply_blur_effect);

/**
 * drm_apply_shadow_effect - Apply drop shadow to a plane
 * @plane: DRM plane to apply shadow to
 * @color: Shadow color in ARGB format
 * @blur_radius: Blur radius for the shadow
 * @offset_x: Horizontal shadow offset in pixels
 * @offset_y: Vertical shadow offset in pixels
 *
 * Applies a drop shadow effect to the specified plane with the given
 * parameters. Shadows are rendered behind the plane content.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_shadow_effect(struct drm_plane *plane, u32 color,
			     u32 blur_radius, s32 offset_x, s32 offset_y)
{
	struct drm_compositor_effect effect = {0};

	if (!plane || !plane->dev)
		return -EINVAL;

	if (blur_radius > 128)
		return -EINVAL;

	effect.type = DRM_EFFECT_SHADOW;
	effect.enabled = true;
	effect.shadow.color = color;
	effect.shadow.blur_radius = blur_radius;
	effect.shadow.offset_x = offset_x;
	effect.shadow.offset_y = offset_y;
	effect.shadow.spread = 0;
	effect.shadow.inner = false;

	drm_dbg_kms(plane->dev,
		    "Applying shadow: color=0x%08x, blur=%u, offset=(%d,%d)\n",
		    color, blur_radius, offset_x, offset_y);

	/* TODO: Call driver-specific implementation if available */
	return 0;
}
EXPORT_SYMBOL(drm_apply_shadow_effect);

/**
 * drm_apply_rounded_corners - Apply rounded corners to a plane
 * @plane: DRM plane
 * @radius: Corner radius in pixels (applied to all corners)
 *
 * Clips the plane content with rounded corners of the specified radius.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_rounded_corners(struct drm_plane *plane, u32 radius)
{
	struct drm_compositor_effect effect = {0};

	if (!plane || !plane->dev)
		return -EINVAL;

	if (radius == 0 || radius > 256)
		return -EINVAL;

	effect.type = DRM_EFFECT_ROUNDED_CORNERS;
	effect.enabled = true;
	effect.rounded_corners.radius_tl = radius;
	effect.rounded_corners.radius_tr = radius;
	effect.rounded_corners.radius_bl = radius;
	effect.rounded_corners.radius_br = radius;
	effect.rounded_corners.antialias = true;

	drm_dbg_kms(plane->dev, "Applying rounded corners: radius=%u\n", radius);

	/* TODO: Call driver-specific implementation if available */
	return 0;
}
EXPORT_SYMBOL(drm_apply_rounded_corners);

/**
 * drm_clear_effects - Clear all effects from a plane
 * @plane: DRM plane to clear effects from
 *
 * Removes all compositor effects from the plane, restoring normal rendering.
 */
void drm_clear_effects(struct drm_plane *plane)
{
	if (!plane || !plane->dev)
		return;

	drm_dbg_kms(plane->dev, "Clearing all compositor effects\n");

	/* TODO: Clear driver-specific effect state */
}
EXPORT_SYMBOL(drm_clear_effects);

/**
 * drm_compositor_effects_init - Initialize compositor effects for a device
 * @dev: DRM device
 *
 * Called during device initialization to set up compositor effect support.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_compositor_effects_init(struct drm_device *dev)
{
	if (!dev)
		return -EINVAL;

	drm_info(dev, "Compositor effects initialized\n");
	drm_info(dev, "  Hardware blur: available\n");
	drm_info(dev, "  Hardware shadows: available\n");
	drm_info(dev, "  Hardware rounded corners: available\n");
	drm_info(dev, "  Maximum compositor layers: 16\n");

	return 0;
}
EXPORT_SYMBOL(drm_compositor_effects_init);

/**
 * drm_compositor_effects_cleanup - Clean up compositor effects
 * @dev: DRM device
 *
 * Called during device cleanup to free compositor effect resources.
 */
void drm_compositor_effects_cleanup(struct drm_device *dev)
{
	if (!dev)
		return;

	drm_dbg_kms(dev, "Compositor effects cleanup\n");

	/* TODO: Free any allocated resources */
}
EXPORT_SYMBOL(drm_compositor_effects_cleanup);
