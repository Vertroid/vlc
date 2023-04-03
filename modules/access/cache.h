/*****************************************************************************
 * cache.h: access cache helper
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
#include <vlc_list.h>

struct vlc_access_cache_entry
{
    void *context;

    char *url;
    char *username;

    mtime_t timeout;
    void (*free_cb)(void *context);

    struct vlc_list node;
};

struct vlc_access_cache
{
    bool init;
    vlc_mutex_t lock;
    vlc_cond_t cond;
    vlc_thread_t thread;
    bool running;

    struct vlc_list entries;
};

#define VLC_ACCESS_CACHE_INITIALIZER(name) { \
    .init = false, \
    .lock = VLC_STATIC_MUTEX, \
    .running = false, \
    .entries = VLC_LIST_INITIALIZER(&name.entries), \
}

static inline char *
vlc_access_cache_entry_CreateSmbUrl(const char *server, const char *share)
{
    char *url;
    if (asprintf(&url, "smb://%s/%s", server, share) == -1)
        return NULL;
    return url;
}

struct vlc_access_cache_entry *
vlc_access_cache_entry_New(void *context, const char *url, const char *username,
                           void (*free_cb)(void *context));

static inline struct vlc_access_cache_entry *
vlc_access_cache_entry_NewSmb(void *context, const char *server,
                              const char *share, const char *username,
                              void (*free_cb)(void *context))
{
    char *url = vlc_access_cache_entry_CreateSmbUrl(server, share);
    if (url == NULL)
        return NULL;

    struct vlc_access_cache_entry *entry =
        vlc_access_cache_entry_New(context, url, username, free_cb);
    free(url);
    return entry;
}

/* Delete the cache entry without firing the free_cb */
void
vlc_access_cache_entry_Delete(struct vlc_access_cache_entry *entry);

void
vlc_access_cache_Destroy(struct vlc_access_cache *cache);

void
vlc_access_cache_AddEntry(struct vlc_access_cache *cache,
                          struct vlc_access_cache_entry *entry);

struct vlc_access_cache_entry *
vlc_access_cache_GetEntry(struct vlc_access_cache *cache,
                          const char *url, const char *username);

static inline struct vlc_access_cache_entry *
vlc_access_cache_GetSmbEntry(struct vlc_access_cache *cache,
                             const char *server, const char *share,
                             const char *username)
{
    char *url = vlc_access_cache_entry_CreateSmbUrl(server, share);
    if (url == NULL)
        return NULL;

    struct vlc_access_cache_entry *entry =
        vlc_access_cache_GetEntry(cache, url, username);
    free(url);

    return entry;
}

#ifdef __has_attribute
  #if __has_attribute(destructor)
    #define VLC_ACCESS_CACHE_CAN_REGISTER
  #endif
#endif

#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
#define VLC_ACCESS_CACHE_REGISTER(name) \
static struct vlc_access_cache name = VLC_ACCESS_CACHE_INITIALIZER(name); \
__attribute__((destructor)) static void vlc_access_cache_destructor_##name(void) \
{ \
    vlc_access_cache_Destroy(&name); \
}
#else
#define VLC_ACCESS_CACHE_REGISTER(name) \
static struct vlc_access_cache name = VLC_ACCESS_CACHE_INITIALIZER(name);
#warning "can't register access cache"
#endif
