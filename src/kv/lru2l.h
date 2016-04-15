//
// Created by Jiamin Huang on 4/15/16.
//

#ifndef CAPIFLASH_LRU2L_H
#define CAPIFLASH_LRU2L_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include "list.h"

typedef struct _first_l_list  lru2l_t;

lru2l_t *lru_new();
void lru_free(lru2l_t *lru);
void lru_get(lru2l_t *lru, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen);
void lru_access(lru2l_t *lru, uint8_t *prev_key, uint64_t prev_klen, uint8_t *key, uint64_t klen);
void lru_put(lru2l_t *lru, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);
void lru_del(lru2l_t *lru, uint8_t *key, uint64_t klen);

#endif //CAPIFLASH_LRU2L_H
