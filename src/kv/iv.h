/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/kv/iv.h $                                                 */
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
#ifndef __IV_H__
#define __IV_H__
#include <stdint.h>


typedef struct _iv
{
  uint64_t n;
  uint64_t m;
  uint64_t bits;
  uint64_t words;
  uint64_t bar;
  uint64_t mask;
  uint64_t data[];
} IV;

IV     *iv_new(uint64_t n, uint64_t m);
IV     *iv_resize(IV *iv, uint64_t n, uint64_t m);
void    iv_delete(IV *iv);

int     iv_set(IV *iv, uint64_t i, uint64_t v);
int64_t iv_get(IV *iv, uint64_t i);

#endif
