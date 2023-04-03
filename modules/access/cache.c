/*****************************************************************************
 * cache.c: access cache helper
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_threads.h>

#include "access/cache.h"

#include <assert.h>

#define VLC_ACCESS_CACHE_TTL 5000000LL
#define VLC_ACCESS_CACHE_MAX_ENTRY 5

void
vlc_access_cache_entry_Delete(struct vlc_access_cache_entry *entry)
{
    free(entry->url);
    free(entry->username);

    free(entry);
}

struct vlc_access_cache_entry *
vlc_access_cache_entry_New(void *context, const char *url, const char *username,
                           void (*free_cb)(void *context))
{
    struct vlc_access_cache_entry *entry = malloc(sizeof(*entry));
    if (unlikely(entry == NULL))
        return NULL;

    entry->url = strdup(url);
    entry->username = username ? strdup(username) : NULL;
    if (!entry->url || (entry->username == NULL) != (username == NULL))
    {
        free(entry->url);
        free(entry);
        return NULL;
    }

    entry->context = context;
    entry->free_cb = free_cb;

    return entry;
}

static void *
vlc_access_cache_Thread(void *data)
{
    struct vlc_access_cache *cache = data;

    vlc_mutex_lock(&cache->lock);
    while (cache->running)
    {
        if (!vlc_list_is_empty(&cache->entries))
        {
            struct vlc_access_cache_entry *entry =
                vlc_list_first_entry_or_null(&cache->entries,
                                             struct vlc_access_cache_entry, node);

            if (entry->timeout == 0 ||
                vlc_cond_timedwait(&cache->cond, &cache->lock, entry->timeout) != 0)
            {
                vlc_list_remove(&entry->node);

                vlc_mutex_unlock(&cache->lock);

                entry->free_cb(entry->context);
                vlc_access_cache_entry_Delete(entry);

                vlc_mutex_lock(&cache->lock);
            }
        }
        else
            vlc_cond_wait(&cache->cond, &cache->lock);
    }
    vlc_mutex_unlock(&cache->lock);

    return NULL;
}

static void
vlc_access_cache_InitOnce(void *data)
{
    struct vlc_access_cache *cache = data;

    if (cache->init)
        return;
    cache->init = true;

    vlc_cond_init(&cache->cond);

#ifdef VLC_ACCESS_CACHE_CAN_REGISTER

    cache->running = true;
    int ret = vlc_clone(&cache->thread, vlc_access_cache_Thread, cache,
                        VLC_THREAD_PRIORITY_LOW);
    if (ret != 0)
        cache->running = false;
#endif
}

void
vlc_access_cache_Destroy(struct vlc_access_cache *cache)
{
    vlc_mutex_lock(&cache->lock);
    if (cache->running)
    {
        cache->running = false;
        vlc_cond_signal(&cache->cond);
        vlc_mutex_unlock(&cache->lock);
        vlc_join(cache->thread, NULL);
    }
    else
        vlc_mutex_unlock(&cache->lock);

    struct vlc_access_cache_entry *entry;
    vlc_list_foreach(entry, &cache->entries, node)
    {
        entry->free_cb(entry->context);
        vlc_access_cache_entry_Delete(entry);
    }

    vlc_mutex_destroy(&cache->lock);
    vlc_cond_destroy(&cache->cond);
}

void
vlc_access_cache_AddEntry(struct vlc_access_cache *cache,
                          struct vlc_access_cache_entry *entry)
{
    vlc_mutex_lock(&cache->lock);

    vlc_access_cache_InitOnce(cache);

    if (!cache->running)
    {
        vlc_mutex_unlock(&cache->lock);
        entry->free_cb(entry->context);
        vlc_access_cache_entry_Delete(entry);
        return;
    }

    struct vlc_access_cache_entry *it;
    size_t count = 0;
    vlc_list_foreach(it, &cache->entries, node)
        count++;

    if (count >= VLC_ACCESS_CACHE_MAX_ENTRY)
    {
        /* Too many entries, signal the thread that will delete the first one */
        it = vlc_list_first_entry_or_null(&cache->entries,
                                          struct vlc_access_cache_entry, node);
        it->timeout = 0;
    }

    entry->timeout = mdate() + VLC_ACCESS_CACHE_TTL;
    vlc_list_append(&entry->node, &cache->entries);

    vlc_cond_signal(&cache->cond);
    vlc_mutex_unlock(&cache->lock);
}

struct vlc_access_cache_entry *
vlc_access_cache_GetEntry(struct vlc_access_cache *cache,
                          const char *url, const char *username)
{
    vlc_mutex_lock(&cache->lock);

    vlc_access_cache_InitOnce(cache);

    struct vlc_access_cache_entry *it;

    vlc_list_foreach(it, &cache->entries, node)
    {

        if (strcmp(url, it->url) == 0
         && (username == NULL) == (it->username == NULL)
         && (username != NULL ? strcmp(username, it->username) == 0 : true))
        {
            vlc_list_remove(&it->node);
            vlc_cond_signal(&cache->cond);
            vlc_mutex_unlock(&cache->lock);
            return it;
        }
    }

    vlc_mutex_unlock(&cache->lock);

    return NULL;
}
