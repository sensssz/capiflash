//
// Created by Jiamin Huang on 4/15/16.
//

#include <strings.h>
#include <arkdb_trace.h>
#include <assert.h>
#include "am.h"
#include "hash.h"
#include "map.h"
#include "list.h"

static inline void delete_list(map_t *map, uint64_t pos);
static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen);
//static void print(map_t *map);
static inline void validate(map_t *map);

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
      delete_list(map, index);
    }
    am_free(map);
  } else {
    KV_TRC_FFDC(pAT, "hash_free NULL");
  }
}

void map_get(map_t *map, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen) {
  kv_t *pair = map_get_pair(map, key, klen);
  if (pair != NULL) {
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
  validate(map);
  uint64_t pos = map_pos(map, key, klen);
  kv_t *node = map_get_pair(map, key, klen);
  assert(node);
  DL_DELETE(map->kvs[pos], node);
  --(map->size);
  validate(map);
}

void map_clr(map_t *map) {
  uint64_t index = 0;
  for (; index < map->cap; ++index) {
    delete_list(map, index);
  }
  map->size = 0;
}

kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen) {
  validate(map);
  uint64_t pos = map_pos(map, key, klen);
  kv_t *node = map->kvs[pos];
  while (node) {
    if (node->klen == klen &&
        memcmp(node->key, key, klen) == 0) {
      break;
    }
    node = node->next;
  }
  validate(map);
  return node;
}

bool map_put_pair(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen, kv_t **pair_out) {
  validate(map);
  kv_t *pair = map_get_pair(map, key, klen);
  bool insert = false;
  if (pair != NULL) {
    if (pair->vlen < vlen) {
      am_free(pair->val);
      pair->val = (uint8_t *) am_malloc(vlen);
    }
    pair->vlen = vlen;
    memcpy(pair->val, val, vlen);
    validate(map);
  } else {
    uint64_t pos = map_pos(map, key, klen);
    pair = (kv_t *) am_malloc(sizeof(kv_t));
    pair->klen = klen;
    pair->vlen = vlen;
    pair->key = (uint8_t *) am_malloc(klen);
    pair->val = (uint8_t *) am_malloc(vlen);
    pair->prev = pair->next = NULL;
    memcpy(pair->key, key, klen);
    memcpy(pair->val, val, vlen);
    DL_PREPEND(map->kvs[pos], pair);
    ++(map->size);
    validate(map);
    insert = true;
  }
  if (pair_out != NULL) {
    *pair_out = pair;
  }
  return insert;
}

inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair) {
  if (val != NULL) {
    *vlen = pair->vlen;
    *val = (uint8_t *) am_malloc(*vlen);
    memcpy(*val, pair->val, *vlen);
  }
}

static inline void delete_list(map_t *map, uint64_t pos) {
  kv_t *node = map->kvs[pos];
  while (node != NULL) {
    kv_t *victim = node;
    node = node->next;
    am_free(victim);
  }
  map->kvs[pos] = NULL;
}

static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen) {
  uint64_t hash = hash_hash(key, klen);
  uint64_t pos = hash % map->cap;
  return pos;
}

//static void print(map_t *map) {
//  uint64_t  index = 0;
//  for (; index < map->cap; ++index) {
//    if (map->kvs[index].klen == 0) {
//      printf("%" PRIu64 "\t----\t----\n", index);
//    } else {
//      printf("%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n", index, map_pos(map, map->kvs[index].key, map->kvs[index].klen), map->kvs[index].off);
//    }
//  }
//}

static void validate(map_t *map) {
#ifdef DEBUG
  uint64_t num_pairs = 0;
  uint64_t index = 0;
  for (; index < map->cap; ++index) {
    kv_t *pair = map->kvs + index;
    if (pair->klen > 0) {
      uint64_t hashed_pos = map_pos(map, pair->key, pair->klen);
      ++num_pairs;
    }
  }
#endif
}