/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */

/*
 * skreen_drm.h - Userspace API for the SKREEN driver-specific ioctls.
 *
 * This header is shared between the kernel module (skreen_drv.c) and any
 * userspace program that wants to talk to the driver directly through
 * ioctl(2), instead of going through the sysfs 'enabled' attribute.
 *
 * This is a synced copy from the skreen_drive repo (skreen_drm.h). If the
 * ioctl numbers or struct layouts change there, copy the file again instead
 * of editing it independently, or the kernel module and this app will
 * disagree on the ABI.
 */

#ifndef _SKREEN_DRM_H_
#define _SKREEN_DRM_H_

#include <drm/drm.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * struct drm_skreen_enable - Argument for the SKREEN_SET_ENABLED /
 * SKREEN_GET_ENABLED ioctls.
 *
 * @enabled: 0 to disable the virtual monitor, 1 to enable it. For
 *	     SKREEN_GET_ENABLED this field is filled in by the kernel with
 *	     the current state.
 */
struct drm_skreen_enable {
	__u32 enabled;
};

/**
 * struct drm_skreen_resolution - Argument for the SKREEN_SET_RESOLUTION /
 * SKREEN_GET_RESOLUTION ioctls.
 *
 * @width: display width in pixels. For SKREEN_GET_RESOLUTION this field is
 *	   filled in by the kernel with the current width.
 * @height: display height in pixels. For SKREEN_GET_RESOLUTION this field is
 *	    filled in by the kernel with the current height.
 */
struct drm_skreen_resolution {
	__u32 width;
	__u32 height;
};

/* Driver-private ioctl numbers, relative to DRM_COMMAND_BASE */
#define DRM_SKREEN_SET_ENABLED		0x00
#define DRM_SKREEN_GET_ENABLED		0x01
#define DRM_SKREEN_SET_RESOLUTION	0x02
#define DRM_SKREEN_GET_RESOLUTION	0x03

#define DRM_IOCTL_SKREEN_SET_ENABLED \
	DRM_IOW(DRM_COMMAND_BASE + DRM_SKREEN_SET_ENABLED, struct drm_skreen_enable)

#define DRM_IOCTL_SKREEN_GET_ENABLED \
	DRM_IOR(DRM_COMMAND_BASE + DRM_SKREEN_GET_ENABLED, struct drm_skreen_enable)

#define DRM_IOCTL_SKREEN_SET_RESOLUTION \
	DRM_IOW(DRM_COMMAND_BASE + DRM_SKREEN_SET_RESOLUTION, struct drm_skreen_resolution)

#define DRM_IOCTL_SKREEN_GET_RESOLUTION \
	DRM_IOR(DRM_COMMAND_BASE + DRM_SKREEN_GET_RESOLUTION, struct drm_skreen_resolution)

#if defined(__cplusplus)
}
#endif

#endif /* _SKREEN_DRM_H_ */
