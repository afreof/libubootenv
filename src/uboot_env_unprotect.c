/*
 * SPDX-License-Identifier:     LGPL-2.1-or-later
 *
 * For some hardware types, the uboot environment variables are stored in a
 * read-only memory area. How this memory area can be switched to a writable
 * mode depends on the hardware. This file provides three functions:
 *  - env_protect_probe
 *  - env_unprotect
 *  - env_reprotect
 * The probe function internally calls various hardware specific probe
 * functions until an implementation that matches the hardware in use returns
 * an env_protect_t object. The env_protect_t object provides two function
 * pointers which point to the hardware matching implementation. This allows
 * to use polyporhism to deal with many hardware specific implementations.
 *
 * The reprotect function does not enable write protection if the memory
 * was not protected before the unprotect function was called.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "uboot_private.h"
#include "uboot_env_unprotect.h"

typedef void (*fp_unprotect)(env_protect_t*);
typedef void (*fp_reprotect)(env_protect_t*);

typedef struct env_protect {
	fp_unprotect unprotect;
	fp_reprotect reprotect;
} env_protect_t;

#define SYSFS_PATH_MAX 120



static const char c_sys_path_1[] = "/sys/class/block/";
static const char c_sys_path_2[] = "/force_ro";
static const char c_dev_name_1[] = "mmcblk";
static const char c_dev_name_2[] = "boot";

/** mmcblk_boot device specific class inherited from generic env_protect_t */
typedef struct env_protect_mmcblkboot {
	env_protect_t parent;  // must be first, means derived from env_protect_t
	char sysfs_path[SYSFS_PATH_MAX];
	char current_prot;
} env_protect_mmcblkboot_t;

/** mmcblk_boot device specific unprotect implementation */
static void mmcblkboot_unprotect(env_protect_t *p_obj) {
	const char c_unprot_char = '0';
	const char c_prot_char = '1';
	env_protect_mmcblkboot_t *p_obj_casted = (env_protect_mmcblkboot_t *)p_obj;
	int fd;
	ssize_t n;

	fd = open(p_obj_casted->sysfs_path, O_RDWR);
	if (fd == -1) {
		return;
	}

	// Verify and archive the current write protect state, unprotect the device
	n = read(fd, &(p_obj_casted->current_prot), 1);
	if (n == 1 && (p_obj_casted->current_prot == c_unprot_char || p_obj_casted->current_prot == c_prot_char)) {
		write(fd, &c_unprot_char, 1);
	} else {
		p_obj_casted->current_prot = 0; // undefined state
	}
	close(fd);
}

/** mmcblk_boot device specific protect implementation */
static void mmcblkboot_reprotect(env_protect_t *p_obj) {
	env_protect_mmcblkboot_t *p_obj_casted = (env_protect_mmcblkboot_t *)p_obj;
	int fd;

	if (p_obj_casted->current_prot != 0) {
		fd = open(p_obj_casted->sysfs_path, O_WRONLY);
		if (fd == -1) {
			return;
		}

		write(fd, &(p_obj_casted->current_prot), 1);
		close(fd);
	}
}

/**
 * mmcblk_boot device specific constructor
 *
 * Gets active if:
 * - devname matches /dev/mmcblk[0-9]boot[0-9]
 * - if a corresponding sysfs entry "force_ro" exists
 */
static int mmcblkboot_create(env_protect_t **pp_obj, const char *devname) {
	env_protect_mmcblkboot_t *p_obj = NULL;
	const char *devfile = devname;
	int ret;

	if (strncmp("/dev/", devname, 5) == 0) {
		devfile = devname + 5;
	} else {
		return 0;
	}

	ret = strncmp(devfile, c_dev_name_1, sizeof(c_dev_name_1) - 1);
	if (ret != 0) {
		return 0;
	}

	if (strncmp(devfile + sizeof(c_dev_name_1), c_dev_name_2, sizeof(c_dev_name_2) - 1) != 0) {
		return 0;
	}

	if (*(devfile + sizeof(c_dev_name_1) - 1) < '0' ||
	    *(devfile + sizeof(c_dev_name_1) - 1) > '9') {
		return 0;
	}

	if (*(devfile + sizeof(c_dev_name_1) + sizeof(c_dev_name_2) - 1) < '0' ||
	    *(devfile + sizeof(c_dev_name_1) + sizeof(c_dev_name_2) - 1) > '9') {
		return 0;
	}

	p_obj = (env_protect_mmcblkboot_t *)calloc(1, sizeof(env_protect_mmcblkboot_t));
	if (p_obj == NULL) {
		return -ENOMEM;
	}
	snprintf(p_obj->sysfs_path, SYSFS_PATH_MAX, "%s%s%s", c_sys_path_1, devfile, c_sys_path_2);

	if (access(p_obj->sysfs_path, W_OK) == -1) {
		free(p_obj);
		return 0;
	}

	p_obj->parent.unprotect = mmcblkboot_unprotect;
	p_obj->parent.reprotect = mmcblkboot_reprotect;

	*pp_obj = (env_protect_t*)p_obj;
	return 1;
}



int env_protect_probe(env_protect_t **pp_obj, const char *devname) {
	int ret;
	ret = mmcblkboot_create(pp_obj, devname);
	return ret;
}

void env_unprotect(env_protect_t *p_obj) {
	if (p_obj->unprotect) {
		(p_obj->unprotect)(p_obj);
	}
}

void env_reprotect(env_protect_t *p_obj) {
	if (p_obj->reprotect) {
		(p_obj->reprotect)(p_obj);
	}
}
