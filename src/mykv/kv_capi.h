//
// Created by Jiamin Huang on 4/18/16.
//

#ifndef CAPIFLASH_KV_CAPI_H
#define CAPIFLASH_KV_CAPI_H

#include <unordered_map>
#include <list>

using std::unordered_map;
using std::list;

int const FL_CACHE_SIZE = 1000;
int const SL_CACHE_SIZE = 100;

struct key_t {
  uint8_t  *key;
  uint64_t  klen;

  key_t(uint8_t *key_in, uint64_t klen_in) : key(key_in), klen(klen_in) {}

  bool operator=(key &rhs) {
    return klen == rhs.klen &&
           memcmp(key, rhs.key) == 0;
  }

  class hasher {
    std::size_t operator()(const key& k) const {
      size_t hash = 0;
      for (uint64_t index = 0; index < k.klen; ++index) {
        hash = hash * 65559 + k.key[index];
      }
      return hash;
    }
  };
};

struct val_t {
  uint8_t  *val;
  uint64_t  vlen;
  list<key_t> likely_keys;

  val_t(uint8_t *val_in, uint64_t vlen_in) : val(val_in), vlen(vlen_in) {}

  bool operator=(val &rhs) {
    return vlen == rhs.vlen &&
           memcmp(val, rhs.val) == 0;
  }
};

struct kv_t {
  key_t key;
  val_t val;
};

class kv {
private:
  int capi;
  unordered_map<key_t, val_t> map;
  list<kv_t> lru;

  int open_capi();

public:
  bool fail;

  kv();

  void kv_get(key_t key, val_t &val);
  void kv_put(key_t key, val_t val);
  void kv_del(key_t key);
};

#endif //CAPIFLASH_KV_CAPI_H
