//
// Created by Jiamin Huang on 4/15/16.
//

#include <strings.h>
#include <arkdb_trace.h>
#include "am.h"
#include "map.h"
#include "list.h"
#include "lru2l.h"

#define FLRU_CAP 10
#define SLRU_CAP 10

typedef struct _first_l_list  flist_t;
typedef struct _first_l_node  fnode_t;
typedef struct _second_l_list slist_t;
typedef struct _second_l_node snode_t;

static flist_t *flist_new();
static void flist_free(flist_t *flist);
static void flist_get(flist_t *lru, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen);
static void flist_access(lru2l_t *lru, uint8_t *prev_key, uint64_t prev_klen, uint8_t *key, uint64_t klen);
static void flist_put(flist_t *lru, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);
static void flist_del(flist_t *lru, uint8_t *key, uint64_t klen);

static slist_t *slist_new(uint64_t cap);
static void slist_free(slist_t *slist);
static void slist_access(slist_t *slist, uint8_t *key, uint64_t klen);

struct _first_l_list {
  uint64_t  cap;
  uint64_t  len;
  fnode_t  *first;
  fnode_t  *last;
  map_t    *hot_cache;
  map_t    *likely_cache;
  pthread_mutex_t mutex;
};

struct _first_l_node {
  kv_t     *pair;
  fnode_t  *prev;
  fnode_t  *next;
  slist_t  *likely_keys;
};

struct _second_l_list {
  uint64_t  cap;
  uint64_t  len;
  snode_t  *first;
  snode_t  *last;
};

struct _second_l_node {
  uint64_t  klen;
  uint8_t  *key;
  snode_t  *prev;
  snode_t  *next;
};

lru2l_t *lru_new() {
  return flist_new();
}

void lru_free(lru2l_t *lru) {
  flist_free(lru);
}

void lru_get(lru2l_t *lru, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  flist_get(lru, key, klen, val, vlen);
}

void lru_access(lru2l_t *lru, uint8_t *prev_key, uint64_t prev_klen, uint8_t *key, uint64_t klen) {
  flist_access(lru, prev_key, prev_klen, key, klen);
}

void lru_put(lru2l_t *lru, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  flist_put(lru, key, klen, val, vlen);
}

void lru_del(lru2l_t *lru, uint8_t *key, uint64_t klen) {
  flist_del(lru, key, klen);
}

static flist_t *flist_new() {
  flist_t *list = (flist_t *) am_malloc(sizeof(flist_t));
  list->cap = FLRU_CAP;
  list->len = 0;
  list->first = list->last = NULL;
  list->hot_cache = map_new(2 * FLRU_CAP);
  list->likely_cache = map_new(2 * FLRU_CAP);
  pthread_mutex_init(&list->mutex, NULL);
  return list;
}

static void flist_free(flist_t *flist) {
  fnode_t *node = flist->first;
  while (node) {
    fnode_t *victim = node;
    node = node->next;
    slist_free(victim->likely_keys);
    am_free(victim);
  }
  map_free(flist->hot_cache);
  map_free(flist->likely_cache);
  pthread_mutex_destroy(&flist->mutex);
  am_free(flist);
}

static void flist_get(flist_t *lru, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  pthread_mutex_lock(&lru->mutex);
  kv_t *pair = map_get_pair(lru->hot_cache, key, klen);
  if (pair->key == NULL) {
    pthread_mutex_unlock(&lru->mutex);
    return;
  }
  copy_value(val, vlen, pair);
  fnode_t *node = (fnode_t *) pair->ref;
  DL_DELETE(lru->first, node);
  DL_PREPEND(lru->first, node);
  pthread_mutex_unlock(&lru->mutex);
}

static void flist_access(lru2l_t *lru, uint8_t *prev_key, uint64_t prev_klen, uint8_t *key, uint64_t klen) {
  kv_t *pair = map_get_pair(lru->hot_cache, prev_key, prev_klen);
  if (pair->key != NULL) {
    fnode_t *node = (fnode_t *) pair->ref;
    slist_access(node->likely_keys, key, klen);
  }
}

static void flist_put(flist_t *lru, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  pthread_mutex_lock(&lru->mutex);
  kv_t *pair = NULL;
//  if (lru->len == lru->cap &&
//      map_get_pair(lru->hot_cache, key, klen)->klen == 0) {
//    pthread_mutex_unlock(&lru->mutex);
//    return;
//  }
  if (map_put_pair(lru->hot_cache, key, klen, val, vlen, &pair)) {
    // It's an insert. Need to check if we need to evict an item
    fnode_t *node = (fnode_t *) am_malloc(sizeof (fnode_t));
    node->pair = pair;
    node->prev = node->next = NULL;
    node->likely_keys = slist_new(SLRU_CAP);
    pair->ref = node;

    DL_PREPEND(lru->first, node);
    if (lru->len == lru->cap) {
      fnode_t *last = lru->last;
      lru->last = last->prev;
      lru->last->next = NULL;
      map_del(lru->hot_cache, last->pair->key, last->pair->klen);
      am_free(last);
    } else {
      ++(lru->len);
      if (lru->last == NULL) {
        lru->last = node;
      }
    }
  } else {
    // It's an update. We just need to move the node to the front.
    fnode_t *node = (fnode_t *) pair->ref;
    if (lru->len > 1) {
      if (node == lru->last) {
        lru->last = node->prev;
      }
      DL_DELETE(lru->first, node);
      DL_PREPEND(lru->first, node);
    }
  }
  pthread_mutex_unlock(&lru->mutex);
}

static void flist_del(lru2l_t *lru, uint8_t *key, uint64_t klen) {
  pthread_mutex_lock(&lru->mutex);
  kv_t *pair = map_get_pair(lru->hot_cache, key, klen);
  if (pair->klen > 0) {
    fnode_t *node = (fnode_t *) pair->ref;
    if (node == lru->last) {
      lru->last = node->prev;
    }
    DL_DELETE(lru->first, node);
    am_free(node);
    map_del(lru->hot_cache, key, klen);
    --(lru->len);
  }
  pthread_mutex_unlock(&lru->mutex);
}

static slist_t *slist_new(uint64_t cap) {
  slist_t *list = (slist_t *) am_malloc(sizeof(slist_t));
  list->cap = cap;
  list->len = 0;
  list->first = list->last = NULL;
  return list;
}

static void slist_free(slist_t *slist) {
  snode_t *node = slist->first;
  while (node != NULL) {
    snode_t *victim = node;
    node = node->next;
    am_free(victim);
  }
  am_free(slist);
}

static void slist_access(slist_t *slist, uint8_t *key, uint64_t klen) {
  snode_t *node = NULL;
  DL_FOREACH(slist->first, node) {
    if (memcmp(node->key, key, klen) == 0) {
      break;
    }
  }
  if (node == NULL) {
    node = (snode_t *) am_malloc(sizeof(snode_t));
    node->prev = node->next = NULL;
    node->key = am_malloc(klen);
    node->klen = klen;
    memcpy(node->key, key, klen);
    DL_PREPEND(slist->first, node);
    if (slist->len == slist->cap) {
      snode_t *last = slist->last;
      slist->last = last->prev;
      slist->last->next = NULL;
      am_free(last);
    } else {
      ++(slist->len);
      if (slist->last == NULL) {
        slist->last = node;
      }
    }
  } else {
    if (slist->len > 1) {
      if (slist->last == node) {
        slist->last = node->prev;
      }
      DL_DELETE(slist->first, node);
      DL_PREPEND(slist->first, node);
    }
  }
}