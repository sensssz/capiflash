//
// Created by Jiamin Huang on 4/4/16.
//

#ifndef CAPIFLASH_MAP_H
#define CAPIFLASH_MAP_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>

typedef struct _hit_list  hit_list_t;

hit_list_t *hit_list_new(uint64_t len);
void hit_list_free(hit_list_t *list);
void hit_list_get(hit_list_t *list, uint8_t *key, uint64_t klen, uint8_t **val, uint64_t *vlen);
void hit_list_put(hit_list_t *list, uint8_t *key, uint64_t klen, uint8_t *val, uint64_t vlen);

#endif //CAPIFLASH_MAP_H
