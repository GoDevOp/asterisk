/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * David M. Lee, II <dlee@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Typical cache pattern for Stasis topics.
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/astobj2.h"
#include "asterisk/stasis_cache_pattern.h"

struct stasis_cp_all {
	struct stasis_topic *topic;
	struct stasis_topic *topic_cached;
	struct stasis_cache *cache;
};

struct stasis_cp_single {
	struct stasis_topic *topic;
	struct stasis_caching_topic *topic_cached;

	struct stasis_subscription *forward;
	struct stasis_subscription *forward_cached;
};

static void all_dtor(void *obj)
{
	struct stasis_cp_all *all = obj;

	ao2_cleanup(all->topic);
	ao2_cleanup(all->topic_cached);
	ao2_cleanup(all->cache);
}

struct stasis_cp_all *stasis_cp_all_create(const char *name,
	snapshot_get_id id_fn)
{
	RAII_VAR(char *, cached_name, NULL, ast_free);
	RAII_VAR(struct stasis_cp_all *, all, NULL, ao2_cleanup);

	all = ao2_alloc(sizeof(*all), all_dtor);
	if (!all) {
		return NULL;
	}

	ast_asprintf(&cached_name, "%s-cached", name);
	if (!cached_name) {
		return NULL;
	}

	all->topic = stasis_topic_create(name);
	all->topic_cached = stasis_topic_create(cached_name);
	all->cache = stasis_cache_create(id_fn);

	if (!all->topic || !all->topic_cached || !all->cache) {
		return NULL;
	}

	ao2_ref(all, +1);
	return all;
}

struct stasis_topic *stasis_cp_all_topic(struct stasis_cp_all *all)
{
	if (!all) {
		return NULL;
	}
	return all->topic;
}

struct stasis_topic *stasis_cp_all_topic_cached(
	struct stasis_cp_all *all)
{
	if (!all) {
		return NULL;
	}
	return all->topic_cached;
}

struct stasis_cache *stasis_cp_all_cache(struct stasis_cp_all *all)
{
	if (!all) {
		return NULL;
	}
	return all->cache;
}

static void one_dtor(void *obj)
{
	struct stasis_cp_single *one = obj;

	/* Should already be unsubscribed */
	ast_assert(one->topic_cached == NULL);
	ast_assert(one->forward == NULL);
	ast_assert(one->forward_cached == NULL);

	ao2_cleanup(one->topic);
	one->topic = NULL;
}

struct stasis_cp_single *stasis_cp_single_create(struct stasis_cp_all *all,
	const char *name)
{
	RAII_VAR(struct stasis_cp_single *, one, NULL, ao2_cleanup);

	one = ao2_alloc(sizeof(*one), one_dtor);
	if (!one) {
		return NULL;
	}

	one->topic = stasis_topic_create(name);
	if (!one->topic) {
		return NULL;
	}
	one->topic_cached = stasis_caching_topic_create(one->topic, all->cache);
	if (!one->topic_cached) {
		return NULL;
	}

	one->forward = stasis_forward_all(one->topic, all->topic);
	if (!one->forward) {
		return NULL;
	}
	one->forward_cached = stasis_forward_all(
		stasis_caching_get_topic(one->topic_cached), all->topic_cached);
	if (!one->forward_cached) {
		return NULL;
	}

	ao2_ref(one, +1);
	return one;
}

void stasis_cp_single_unsubscribe(struct stasis_cp_single *one)
{
	if (!one) {
		return;
	}

	stasis_caching_unsubscribe(one->topic_cached);
	one->topic_cached = NULL;
	stasis_unsubscribe(one->forward);
	one->forward = NULL;
	stasis_unsubscribe(one->forward_cached);
	one->forward_cached = NULL;
}

struct stasis_topic *stasis_cp_single_topic(struct stasis_cp_single *one)
{
	if (!one) {
		return NULL;
	}
	return one->topic;
}

struct stasis_topic *stasis_cp_single_topic_cached(
	struct stasis_cp_single *one)
{
	if (!one) {
		return NULL;
	}
	return stasis_caching_get_topic(one->topic_cached);
}

