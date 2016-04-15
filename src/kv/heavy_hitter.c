//
// Created by Jiamin Huang on 4/4/16.
//

#include <strings.h>
#include <arkdb_trace.h>
#include <assert.h>
#include "am.h"
#include "heavy_hitter.h"
#include "hash.h"
#include "list.h"

typedef struct _bucket    bucket_t;
typedef struct _counter   counter_t;
typedef struct _kv_t      kv_t;
typedef struct _map_t     map_t;

// Map interface
static map_t *map_new(uint64_t len);
static void map_free(map_t *map);

// static void map_get(map_t *map, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen);
// static void map_clr(map_t *map);
static bool map_put(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);
static void map_del(map_t *map, uint8_t *key, uint64_t klen);
// Map internal functions
static kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen);
static void map_put_pair(map_t *map, kv_t *pair, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);
static inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair);
static inline void wipe_pair(map_t *map, uint64_t pos);
static inline void delete_key(map_t *map, uint64_t pos);
static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen);

// Bucket function
static bucket_t *bucket_list_new(uint64_t num_counters);
static void bucket_list_free(bucket_t *list);

static void move_counter(hit_list_t *list, counter_t *counter);

struct _hit_list {
  bucket_t    *buckets;
  map_t       *map;
  pthread_mutex_t mutex;
};

struct _bucket {
  uint64_t  count;
  counter_t *counters;
  bucket_t  *prev;
  bucket_t  *next;
};

struct _counter {
  kv_t      *pair;
  uint64_t  inheritted_count;
  bucket_t  *bucket;
  counter_t *prev;
  counter_t *next;
};

struct _kv_t {
  uint64_t  vlen;
  uint8_t   *val;
  uint64_t  klen;
  uint8_t   *key;
  counter_t *counter;
};

struct _map_t {
  uint64_t  cap;
  uint64_t  size;
  kv_t      kvs[];
};

static inline void wipe_pair(map_t *map, uint64_t pos) {
  map->kvs[pos].key = map->kvs[pos].val = NULL;
  map->kvs[pos].klen = map->kvs[pos].vlen = 0;
}

static inline void delete_key(map_t *map, uint64_t pos) {
  if (map->kvs[pos].klen > 0) {
    am_free(map->kvs[pos].key);
    am_free(map->kvs[pos].val);
    wipe_pair(map, pos);
  }
}

static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t hash = hash_hash(key, klen);
  uint64_t pos = hash % map->cap;
  return pos;
}

map_t *map_new(uint64_t len) {
  map_t *map = (map_t *) am_malloc(sizeof(map_t) + sizeof(kv_t) * len);
  if (map) {
    bzero(map, sizeof(map_t) + sizeof(kv_t) * len);
    map->cap = len;
    map->size = 0;
  } else {
    KV_TRC_FFDC(pAT, "HASH malloc FAILED");
  }
  return map;
}

void map_free(map_t *map) {
  uint64_t index = 0;
  if (map) {
    for (; index < map->cap; ++index) {
      delete_key(map, index);
    }
    am_free(map);
  } else {
    KV_TRC_FFDC(pAT, "hash_free NULL");
  }
}

static kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t pos = map_pos(map, key, klen);
  while (map->kvs[pos].klen > 0) {
    if (memcmp(key, map->kvs[pos].key, klen) == 0) {
      break;
    }
    pos = (pos + 1) % map->cap;
  }
  return map->kvs + pos;
}

static void map_put_pair(map_t *map, kv_t *pair, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  pair->klen = klen;
  pair->vlen = vlen;
  pair->key = (uint8_t *) am_malloc(klen);
  pair->val = (uint8_t *) am_malloc(vlen);
  memcpy(pair->key, key, klen);
  memcpy(pair->val, val, vlen);
  ++(map->size);
}

static inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair) {
  *vlen = pair->vlen;
  *val = (uint8_t *) am_malloc(*vlen);
  memcpy(*val, pair->val, *vlen);
}

/*
void map_get(map_t *map, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  kv_t *pair = map_get_pair(map, key, klen);
  if (pair->key != NULL) {
    copy_value(val, vlen, pair);
  } else {
    *vlen = 0;
    *val = NULL;
  }
}
*/

bool map_put(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  kv_t *pair = map_get_pair(map, key, klen);
  if (pair->key != NULL) {
    if (pair->vlen < vlen) {
      am_free(pair->val);
      pair->val = (uint8_t *) am_malloc(vlen);
    }
    pair->vlen = vlen;
    memcpy(pair->val, val, vlen);
    return false;
  } else {
    map_put_pair(map, pair, key, klen, val, vlen);
    return true;
  }
}

void map_del(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t pos = map_pos(map, key, klen);
  while (memcmp(key, map->kvs[pos].key, klen) != 0) {
    pos = (pos + 1) % map->cap;
  }
  delete_key(map, pos);
  uint64_t index = pos + 1;
  while (map->kvs[index].klen > 0) {
    uint64_t k_pos = map_pos(map, map->kvs[index].key, map->kvs[index].klen);
    if (k_pos <= pos) {
      map->kvs[pos] = map->kvs[index];
      wipe_pair(map, index);
    }
    pos = index;
    index = (index + 1) % map->cap;
  }
  --(map->size);
}

/*
void map_clr(map_t *map) {
  uint64_t index = 0;
  for (; index < map->cap; ++index) {
    delete_key(map, index);
  }
  map->size = 0;
}
*/

static bucket_t *bucket_list_new(uint64_t num_counters) {
  uint64_t index = 0;
  bucket_t *list = (bucket_t *) am_malloc(sizeof(bucket_t));
  list->count = 0;
  list->counters = (counter_t *) am_malloc(num_counters * sizeof(counter_t));
  list->prev = NULL;
  list->next = NULL;
  // Now connect all counters;
  for (; index < num_counters; ++index) {
    list->counters[index].inheritted_count = 0;
    list->counters[index].pair = NULL;
    list->counters[index].bucket = list;
    list->counters[index].next = list->counters + index + 1;
  }
  list->counters[num_counters - 1].next = NULL;
  return list;
}

static void bucket_list_free(bucket_t *list) {
  if (list == NULL) {
    return;
  }
  am_free(list->counters);
  am_free(list);
}

static void move_counter(hit_list_t *list, counter_t *counter) {
  bucket_t  *bucket = counter->bucket;
  bucket_t  *next_bucket = bucket->next;
  if (!next_bucket || next_bucket->count != bucket->count + 1) {
    // If the corresponding bucket does not exist, create it
    next_bucket = (bucket_t *) am_malloc(sizeof(bucket_t));
    next_bucket->counters = counter;
    next_bucket->count = counter->bucket->count + 1;
    DL_APPEND_ELEM(list->buckets, bucket, next_bucket);
  }
  DL_DELETE(bucket->counters, counter);
  DL_PREPEND(next_bucket->counters, counter);
  if (bucket->counters == NULL) {
    DL_DELETE(list->buckets, bucket);
  }
}

hit_list_t *hit_list_new(uint64_t len) {
  hit_list_t *list = (hit_list_t *) am_malloc(sizeof(hit_list_t));
  list->buckets = bucket_list_new(len);
  list->map = map_new(2 * len);
  pthread_mutex_init(&list->mutex, NULL);
  return list;
}

void hit_list_free(hit_list_t *list) {
  if (list == NULL) {
    return;
  }
  bucket_list_free(list->buckets);
  map_free(list->map);
  am_free(list);
  pthread_mutex_destroy(&list->mutex);
}

void hit_list_get(hit_list_t *list, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  pthread_mutex_lock(&list->mutex);
  kv_t *pair = map_get_pair(list->map, key, klen);
  if (pair->key != NULL) {
    copy_value(val, vlen, pair);
    move_counter(list, pair->counter);
  } else {
    *val = NULL;
    *vlen = 0;
  }
  pthread_mutex_unlock(&list->mutex);
}

/*
 * If the key always exists in the list, update the value only
 * Otherwise, put the key-value pair into the list
 */
void hit_list_put(hit_list_t *list, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  pthread_mutex_lock(&list->mutex);
  kv_t *pair = map_get_pair(list->map, key, klen);
  if (pair->key == NULL) {
    counter_t *counter = list->buckets->counters;
    if (list->buckets->count == 0) {
      // There are still empty counters
      map_put_pair(list->map, pair, key, klen, val, vlen);
      pair->counter = counter;
    } else {
      map_del(list->map, key, klen);
      map_put(list->map, key, klen, val, vlen);
    }
  } else {
    if (pair->vlen < vlen) {
      am_free(pair->val);
      pair->val = (uint8_t *) am_malloc(vlen);
    }
    pair->vlen = vlen;
    memcpy(pair->val, val, vlen);
  }
  pthread_mutex_unlock(&list->mutex);
}
