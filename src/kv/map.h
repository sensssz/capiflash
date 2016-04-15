//
// Created by Jiamin Huang on 4/15/16.
//

#ifndef CAPIFLASH_MAP_H
#define CAPIFLASH_MAP_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>

typedef struct _kv_t          kv_t;
typedef struct _map_t         map_t;

struct _kv_t {
  uint64_t  vlen;
  uint8_t  *val;
  uint64_t  klen;
  uint8_t  *key;
  void     *ref;
};

struct _map_t {
  uint64_t  cap;
  uint64_t  size;
  kv_t      kvs[];
};

// Map interface
static map_t *map_new(uint64_t len);
static void map_free(map_t *map);
static void map_get(map_t *map, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen);
// Return true when it's an insert, and false when it's an update
static bool map_put(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);
static void map_del(map_t *map, uint8_t *key, uint64_t klen);
static void map_clr(map_t *map);
// Map internal functions
static kv_t *map_get_pair(map_t *map, uint8_t *key, uint64_t klen);
static bool map_put_pair(map_t *map, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen, kv_t **pair_out);
static inline void copy_value(uint8_t **val, uint64_t *vlen, const kv_t *pair);
static inline void wipe_pair(map_t *map, uint64_t pos);
static inline void delete_key(map_t *map, uint64_t pos);
static inline uint64_t map_pos(map_t *map, uint8_t *key, uint64_t klen);

#endif //CAPIFLASH_MAP_H
