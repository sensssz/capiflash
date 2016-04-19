//
// Created by Jiamin Huang on 4/18/16.
//

#include "kv_capi.h"
#include "capiblock.h"

int kv::open_capi() {
  int rc = cblk_init(NULL,0);
  if (rc) {
    fprintf(stderr,"cblk_init failed with rc = %d and errno = %d\n",
            rc,errno);
    return -1;
  }
  int id = cblk_open("/dev/sg15", QD, O_RDWR, 0, 0);
  if (id == NULL_CHUNK_ID) {
    if      (ENOSPC == errno) fprintf(stderr,"cblk_open: ENOSPC\n");
    else if (ENODEV == errno) fprintf(stderr,"cblk_open: ENODEV\n");
    else                      fprintf(stderr,"cblk_open: errno:%d\n",errno);
    cblk_term(NULL,0);
    return -1;
  }

  size_t nblks = 0;
  rc = cblk_get_size(id, &nblks, 0);
  if (rc) {
    fprintf(stderr, "cblk_get_lun_size failed: errno: %d\n", errno);
    return -1;
  }
  return id;
}

kv::kv() {
  capi = open_capi();
  fail = capi == -1;
}
