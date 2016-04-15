//
// Created by Jiamin Huang on 4/15/16.
//

#include <strings.h>
#include <arkdb_trace.h>
#include "am.h"
#include "hash.h"
#include "map.h"

static inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair);
static inline void wipe_pair(map_t *map, uint64_t pos);
static inline void delete_key(map_t *map, uint64_t pos);
static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen);

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

void map_get(map_t *map, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  kv_t *pair = map_get_pair(map, key, klen);
  if (pair->key != NULL) {
    copy_value(val, vlen, pair);
  } else {
    *vlen = 0;
    *val = NULL;
  }
}

bool map_put(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen) {
  return map_put_pair(map, key, klen, val, vlen, NULL);
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

void map_clr(map_t *map) {
  uint64_t index = 0;
  for (; index < map->cap; ++index) {
    delete_key(map, index);
  }
  map->size = 0;
}

kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t pos = map_pos(map, key, klen);
  while (map->kvs[pos].klen > 0) {
    if (memcmp(key, map->kvs[pos].key, klen) == 0) {
      break;
    }
    pos = (pos + 1) % map->cap;
  }
  return map->kvs + pos;
}

bool map_put_pair(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen, kv_t **pair_out) {
  kv_t *pair = map_get_pair(map, key, klen);
  if (pair_out != NULL) {
    *pair_out = pair;
  }
  if (pair->key != NULL) {
    if (pair->vlen < vlen) {
      am_free(pair->val);
      pair->val = (uint8_t *) am_malloc(vlen);
    }
    pair->vlen = vlen;
    memcpy(pair->val, val, vlen);
    return false;
  } else {
    pair->klen = klen;
    pair->vlen = vlen;
    pair->key = (uint8_t *) am_malloc(klen);
    pair->val = (uint8_t *) am_malloc(vlen);
    memcpy(pair->key, key, klen);
    memcpy(pair->val, val, vlen);
    ++(map->size);
    return true;
  }
}

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

static inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair) {
  if (val != NULL) {
    *vlen = pair->vlen;
    *val = (uint8_t *) am_malloc(*vlen);
    memcpy(*val, pair->val, *vlen);
  }
}
