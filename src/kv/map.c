//
// Created by Jiamin Huang on 4/15/16.
//

#include <strings.h>
#include <arkdb_trace.h>
#include <assert.h>
#include "am.h"
#include "hash.h"
#include "map.h"

#define INC_CAP(val) (((val) + 1) % (map->cap))

static inline void wipe_pair(map_t *map, uint64_t pos);
static inline void delete_key(map_t *map, uint64_t pos);
static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen);
static void print(map_t *map);

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
  kv_t *pair = map_get_pair(map, key, klen, false);
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
//  puts("======================================================");
//  print(map);
  uint64_t pos = map_pos(map, key, klen);
  if (map->kvs[pos].key == NULL) {
    printf("Key length is %" PRIu64 ", is hashed to pos %" PRIu64 " but not found\n", klen, pos);
    uint64_t index = 0;
    for (; index < map->cap; ++index) {
      if (map->kvs[index].key != NULL &&
          memcmp(map->kvs[index].key, key, klen) == 0) {
        printf("Key found at position %" PRIu64 " while hashed to position %" PRIu64 "\n", index, pos);
      }
    }
  }
  while (map->kvs[pos].klen != klen ||
         memcmp(key, map->kvs[pos].key, klen) != 0) {
    pos = INC_CAP(pos);
  }
//  printf("\nDeleting key at position %" PRIu64 "\n\n", pos);
  delete_key(map, pos);
  uint64_t index = INC_CAP(pos);
  uint64_t off = 1;
  for (; map->kvs[index].klen > 0; index = INC_CAP(index), ++off) {
    if (map->kvs[index].off >= off) {
      map->kvs[pos] = map->kvs[index];
      map->kvs[pos].off -= off;
      map->kvs[pos].ref->pair = map->kvs + pos;
      wipe_pair(map, index);
      map->kvs[index].off = 0;
      off = 0;
      pos = index;
    }
  }
  --(map->size);
//  print(map);
}

void map_clr(map_t *map) {
  uint64_t index = 0;
  for (; index < map->cap; ++index) {
    delete_key(map, index);
  }
  map->size = 0;
}

kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen, bool set_off) {
  uint64_t pos = map_pos(map, key, klen);
  uint64_t off = 0;
  bool found = false;
  while (map->kvs[pos].klen > 0) {
    if (map->kvs[pos].klen == klen ||
        memcmp(key, map->kvs[pos].key, klen) == 0) {
      found = true;
      break;
    }
    pos = INC_CAP(pos);
    ++off;
  }
  if (set_off && !found) {
    map->kvs[pos].off = off;
  }
  return map->kvs + pos;
}

bool map_put_pair(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen, kv_t **pair_out) {
  kv_t *pair = map_get_pair(map, key, klen, true);
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

inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair) {
  if (val != NULL) {
    *vlen = pair->vlen;
    *val = (uint8_t *) am_malloc(*vlen);
    memcpy(*val, pair->val, *vlen);
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
    map->kvs[pos].off = 0;
  }
}

static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t hash = hash_hash(key, klen);
  uint64_t pos = hash % map->cap;
  return pos;
}

static void print(map_t *map) {
  uint64_t  index = 0;
  for (; index < map->cap; ++index) {
    if (map->kvs[index].klen == 0) {
      printf("%" PRIu64 "\t----\t----\n", index);
    } else {
      printf("%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n", index, map_pos(map, map->kvs[index].key, map->kvs[index].klen), map->kvs[index].off);
    }
  }
}