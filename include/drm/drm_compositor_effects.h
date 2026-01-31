/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2026 Linux Kernel Contributors
 *
 * DRM Compositor Effects Extension
 *
 * This header defines extensions to the DRM/KMS API for modern
 * compositor effects like blur, shadows, and rounded corners.
 */

#ifndef _DRM_COMPOSITOR_EFFECTS_H_
#define _DRM_COMPOSITOR_EFFECTS_H_

#include <drm/drm_plane.h>
#include <drm/drm_crtc.h>
#include <linux/types.h>

struct drm_device;
struct drm_framebuffer;

/**
 * enum drm_compositor_effect_type - Types of compositor effects
 */
enum drm_compositor_effect_type {
	DRM_EFFECT_NONE = 0,
	DRM_EFFECT_BLUR,		/* Gaussian blur */
	DRM_EFFECT_SHADOW,		/* Drop shadow */
	DRM_EFFECT_ROUNDED_CORNERS,	/* Rounded corner clipping */
	DRM_EFFECT_ACRYLIC,		/* Acrylic material (blur + noise + tint) */
	DRM_EFFECT_MICA,		/* Mica material (wallpaper-aware) */
	DRM_EFFECT_COLOR_TINT,		/* Color overlay with blending */
	DRM_EFFECT_MAX
};

/**
 * struct drm_blur_effect - Gaussian blur effect parameters
 */
struct drm_blur_effect {
	/** @radius: Blur radius in pixels */
	u32 radius;

	/** @quality: Quality level (0-100), affects performance */
	u32 quality;

	/** @sigma: Gaussian sigma value (auto-calculated if 0) */
	u32 sigma;

	/** @downscale: Downscale factor for performance (1, 2, 4) */
	u32 downscale;
};

/**
 * struct drm_shadow_effect - Drop shadow effect parameters
 */
struct drm_shadow_effect {
	/** @color: ARGB shadow color */
	u32 color;

	/** @blur_radius: Shadow blur radius */
	u32 blur_radius;

	/** @offset_x: Horizontal shadow offset */
	s32 offset_x;

	/** @offset_y: Vertical shadow offset */
	s32 offset_y;

	/** @spread: Shadow spread distance */
	u32 spread;

	/** @inner: True for inner shadow, false for drop shadow */
	bool inner;
};

/**
 * struct drm_rounded_corners_effect - Rounded corners effect
 */
struct drm_rounded_corners_effect {
	/** @radius_tl: Top-left corner radius */
	u32 radius_tl;

	/** @radius_tr: Top-right corner radius */
	u32 radius_tr;

	/** @radius_bl: Bottom-left corner radius */
	u32 radius_bl;

	/** @radius_br: Bottom-right corner radius */
	u32 radius_br;

	/** @antialias: Enable anti-aliasing */
	bool antialias;
};

/**
 * struct drm_acrylic_effect - Acrylic material effect (Windows 11-style)
 */
struct drm_acrylic_effect {
	/** @blur: Blur parameters */
	struct drm_blur_effect blur;

	/** @tint_color: Tint color (ARGB) */
	u32 tint_color;

	/** @tint_opacity: Tint opacity (0-255) */
	u8 tint_opacity;

	/** @noise_intensity: Noise overlay intensity (0-100) */
	u8 noise_intensity;

	/** @luminosity_opacity: Luminosity layer opacity (0-100) */
	u8 luminosity_opacity;
};

/**
 * struct drm_mica_effect - Mica material effect (Windows 11-style)
 */
struct drm_mica_effect {
	/** @wallpaper_fb: Framebuffer containing wallpaper */
	struct drm_framebuffer *wallpaper_fb;

	/** @tint_color: Tint color (ARGB) */
	u32 tint_color;

	/** @tint_opacity: Tint opacity (0-255) */
	u8 tint_opacity;

	/** @luminosity_opacity: Luminosity layer opacity (0-100) */
	u8 luminosity_opacity;

	/** @alternative_style: Use alternative mica style */
	bool alternative_style;
};

/**
 * struct drm_compositor_effect - Compositor effect descriptor
 */
struct drm_compositor_effect {
	/** @type: Effect type */
	enum drm_compositor_effect_type type;

	/** @enabled: Effect is enabled */
	bool enabled;

	/** Effect-specific parameters */
	union {
		struct drm_blur_effect blur;
		struct drm_shadow_effect shadow;
		struct drm_rounded_corners_effect rounded_corners;
		struct drm_acrylic_effect acrylic;
		struct drm_mica_effect mica;
	};
};

/**
 * struct drm_plane_compositor_state - Extended plane state for compositor
 *
 * This extends drm_plane_state with compositor-specific properties.
 */
struct drm_plane_compositor_state {
	/** @base: Base plane state */
	struct drm_plane_state base;

	/** @effects: Array of effects to apply */
	struct drm_compositor_effect effects[8];

	/** @num_effects: Number of active effects */
	u32 num_effects;

	/** @hw_accelerated: Effects are hardware-accelerated */
	bool hw_accelerated;

	/** @effect_cache_valid: Effect rendering cache is valid */
	bool effect_cache_valid;
};

/**
 * struct drm_compositor_effect_ops - Compositor effect operations
 *
 * Drivers implement these operations to support compositor effects.
 */
struct drm_compositor_effect_ops {
	/**
	 * @supports_effect:
	 *
	 * Check if the hardware supports a given effect type.
	 *
	 * Returns:
	 * true if supported, false otherwise.
	 */
	bool (*supports_effect)(struct drm_device *dev,
			        enum drm_compositor_effect_type type);

	/**
	 * @apply_effect:
	 *
	 * Apply an effect to a plane. The driver should configure hardware
	 * to render the effect or fall back to software rendering.
	 *
	 * Returns:
	 * 0 on success, negative error code on failure.
	 */
	int (*apply_effect)(struct drm_plane *plane,
			    struct drm_compositor_effect *effect);

	/**
	 * @prepare_effects:
	 *
	 * Prepare for rendering multiple effects. Called before applying
	 * a batch of effects to allow driver optimization.
	 *
	 * Returns:
	 * 0 on success, negative error code on failure.
	 */
	int (*prepare_effects)(struct drm_plane *plane, u32 num_effects);

	/**
	 * @finish_effects:
	 *
	 * Finalize effect rendering. Called after all effects are applied.
	 */
	void (*finish_effects)(struct drm_plane *plane);
};

/**
 * drm_plane_supports_effects - Check if plane supports compositor effects
 * @plane: DRM plane
 *
 * Returns:
 * true if the plane supports at least one compositor effect
 */
static inline bool drm_plane_supports_effects(struct drm_plane *plane)
{
	/* Check if plane has compositor effect capabilities */
	return plane && plane->dev;
}

/**
 * drm_apply_blur_effect - Apply Gaussian blur to a plane
 * @plane: DRM plane
 * @radius: Blur radius in pixels
 *
 * Convenience function to apply a blur effect to a plane.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_blur_effect(struct drm_plane *plane, u32 radius);

/**
 * drm_apply_shadow_effect - Apply drop shadow to a plane
 * @plane: DRM plane
 * @color: Shadow color (ARGB)
 * @blur_radius: Shadow blur radius
 * @offset_x: Horizontal offset
 * @offset_y: Vertical offset
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_shadow_effect(struct drm_plane *plane, u32 color,
			     u32 blur_radius, s32 offset_x, s32 offset_y);

/**
 * drm_apply_rounded_corners - Apply rounded corners to a plane
 * @plane: DRM plane
 * @radius: Corner radius (same for all corners)
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int drm_apply_rounded_corners(struct drm_plane *plane, u32 radius);

/**
 * drm_clear_effects - Clear all effects from a plane
 * @plane: DRM plane
 */
void drm_clear_effects(struct drm_plane *plane);

#endif /* _DRM_COMPOSITOR_EFFECTS_H_ */
