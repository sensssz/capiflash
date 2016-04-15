/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/kv/am.c $                                                 */
/*                                                                        */
/* IBM Data Engine for NoSQL - Power Systems Edition User Library Project */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2014,2015                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _AIX
#include "zmalloc.h"
#endif
#include "am.h"

#include <kv_inject.h>
#include <arkdb_trace.h>
#include <errno.h>

int x = 1;
#define H 64
#define ARK_KV_ALIGN 16

void *am_malloc(size_t size)
{
    unsigned char *p=NULL;

    if (check_alloc_error_injects()) {return NULL;}

    if ((int64_t)size > 0)
    {
#ifdef _AIX
      p = malloc(size + (ARK_KV_ALIGN - 1));
#else
      p = zmalloc(size + (ARK_KV_ALIGN - 1));
#endif
      if (p) {memset(p,0x00, size + (ARK_KV_ALIGN - 1));}
      else
      {
          KV_TRC_FFDC(pAT, "ALLOC_FAIL errno=%d", errno);
          if (!errno) {errno = ENOMEM;}
      }
    }
    return p;
}

void am_free(void *ptr)
{
#ifdef _AIX
  if (ptr) {free(ptr); ptr=NULL;}
#else
  if (ptr) {zfree(ptr);ptr=NULL;}
#endif
}

void *am_realloc(void *ptr, size_t size)
{
    unsigned char *p = NULL;

    if (check_alloc_error_injects()) {return NULL;}

    if (ptr && (int64_t)size > 0)
    {
#ifdef _AIX
      p = realloc(ptr, size + (ARK_KV_ALIGN - 1));
#else
      p = zrealloc(ptr, size + (ARK_KV_ALIGN - 1));
#endif
    }
    else
    {
        if (!errno) {errno = ENOMEM;}
        KV_TRC_FFDC(pAT, "REALLOC_FAIL ptr:%p size:%"PRIu64"", ptr, size);
    }

    if (!p)
    {
        KV_TRC_FFDC(pAT, "REALLOC_FAIL errno=%d", errno);
        if (!errno) {errno = ENOMEM;}
    }

    return p;
}

void *ptr_align(void *ptr)
{
  void *new_ptr = NULL;

  if (ptr)
  {
    new_ptr = (void *)(((uintptr_t)(ptr) + ARK_KV_ALIGN - 1) &
                              ~(uintptr_t)(ARK_KV_ALIGN - 1));
  }
  else
  {
      KV_TRC_FFDC(pAT, "ptr_align NULL");
  }

  return new_ptr;
}

