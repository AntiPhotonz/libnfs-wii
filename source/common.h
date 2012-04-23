/*
 common.h
 Common definitions and included files for libnfs 

 Copyright (c) 2012 r-win

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _COMMON_H
#define _COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "mem_allocate.h"
#include "structs.h"

#define MAX_FILENAME_LENGTH 768             // 256 UCS-2 characters encoded into UTF-8 can use up to 768 UTF-8 chars

// When compiling for NDS, make sure NDS is defined
#ifndef NDS
 #if defined ARM9 || defined ARM7
  #define NDS
 #endif
#endif

// Platform specific includes
#if defined(__gamecube__) || defined (__wii__)
   #include <gctypes.h>
   #include <gccore.h>
#elif defined(NDS)
   #include <nds/ndstypes.h>
   #include <nds/system.h>
#elif defined(GBA)
   #include <gba_types.h>
#endif

// Platform specific options
#if   defined (__wii__)
   #define USE_LWP_LOCK
#elif defined (__gamecube__)
   #define USE_LWP_LOCK
#endif

extern void fhandle3_cleancopy(fhandle3 *dest, fhandle3 *src);
extern void fhandle3_copy(fhandle3 *dest, fhandle3 *src);
extern void fhandle3_free(fhandle3 *handle);

#endif // _COMMON_H
