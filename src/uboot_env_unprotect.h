/*
 * SPDX-License-Identifier:     LGPL-2.1-or-later
 */

#pragma once

typedef struct env_protect env_protect_t;

int env_protect_probe(env_protect_t **pp_obj, const char *devname);
void env_unprotect(env_protect_t *p_obj);
void env_reprotect(env_protect_t *p_obj);
