//===-- tsan_platform_linux.cc ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
// Linux-specific code.
//===----------------------------------------------------------------------===//

#include "tsan_platform.h"
#include "tsan_rtl.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

namespace __tsan {


ScopedInRrl::ScopedInRrl()
    : thr_(cur_thread()) {
  thr_->in_rtl++;
  errno_ = errno;
}

ScopedInRrl::~ScopedInRrl() {
  thr_->in_rtl--;
  errno = errno_;
}

void Printf(const char *format, ...) {
  ScopedInRrl in_rtl;
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

void Report(const char *format, ...) {
  ScopedInRrl in_rtl;
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

void Die() {
  _exit(1);
}

static void *my_mmap(void *addr, size_t length, int prot, int flags,
                    int fd, u64 offset) {
  ScopedInRrl in_rtl;
# if __WORDSIZE == 64
  return (void *)syscall(__NR_mmap, addr, length, prot, flags, fd, offset);
# else
  return (void *)syscall(__NR_mmap2, addr, length, prot, flags, fd, offset);
# endif
}

void *virtual_alloc(uptr size) {
  void *p = my_mmap(0, size, PROT_READ|PROT_WRITE,
      MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) {
    Report("FATAL: ThreadSanitizer can not allocate %lu MB\n", size<<20);
    Die();
  }
  return p;
}

void virtual_free(void *p, uptr size) {
  ScopedInRrl in_rtl;
  if (munmap(p, size)) {
    Report("FATAL: ThreadSanitizer munmap failed\n");
    Die();
  }
}

void sched_yield() {
  ScopedInRrl in_rtl;
  syscall(__NR_sched_yield);
}

static void ProtectRange(uptr beg, uptr end) {
  ScopedInRrl in_rtl;
  if (beg != (uptr)my_mmap((void*)(beg), end - beg,
      PROT_NONE,
      MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_NORESERVE,
      -1, 0)) {
    Report("FATAL: ThreadSanitizer can not protect [%p,%p]\n", beg, end);
    Report("FATAL: Make sure you are not using unlimited stack\n");
    Die();
  }
}

void InitializeShadowMemory() {
  const uptr kClosedLowBeg  = 0x200000;
  const uptr kClosedLowEnd  = kLinuxShadowBeg - 1;
  const uptr kClosedHighBeg = kLinuxShadowEnd + 1;
  const uptr kClosedHighEnd = kLinuxAppMemBeg - 1;
  uptr shadow = (uptr)my_mmap((void*)kLinuxShadowBeg,
      kLinuxShadowEnd - kLinuxShadowBeg,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_NORESERVE,
      0, 0);
  if (shadow != kLinuxShadowBeg) {
    Report("FATAL: ThreadSanitizer can not mmap the shadow memory\n");
    Report("FATAL: Make shoure to compile with -fPIE and to link with -pie.\n");
    Die();
  }
  ProtectRange(kClosedLowBeg, kClosedLowEnd);
  ProtectRange(kClosedHighBeg, kClosedHighEnd);
  DPrintf("kClosedLowBeg   %p\n", kClosedLowBeg);
  DPrintf("kClosedLowEnd   %p\n", kClosedLowEnd);
  DPrintf("kLinuxShadowBeg %p\n", kLinuxShadowBeg);
  DPrintf("kLinuxShadowEnd %p\n", kLinuxShadowEnd);
  DPrintf("kClosedHighBeg  %p\n", kClosedHighBeg);
  DPrintf("kClosedHighEnd  %p\n", kClosedHighEnd);
  DPrintf("kLinuxAppMemBeg %p\n", kLinuxAppMemBeg);
  DPrintf("kLinuxAppMemEnd %p\n", kLinuxAppMemEnd);
  DPrintf("stack           %p\n", &shadow);
  DPrintf("InitializeShadowMemory: %p %p\n", shadow);
}

static void CheckPIE() {
  // Ensure that the binary is indeed compiled with -pie.
  int fmaps = open("/proc/self/maps", O_RDONLY);
  if (fmaps != -1) {
    char buf[20];
    if (read(fmaps, buf, sizeof(buf)) == sizeof(buf)) {
      buf[sizeof(buf) - 1] = 0;
      u64 addr = strtoll(buf, 0, 16);
      if ((u64)addr < 0x7ef000000000) {
        Report("FATAL: ThreadSanitizer can not mmap the shadow memory ("
               "something is mapped at 0x%p)\n", addr);
        Report("FATAL: Make shoure to compile with -fPIE"
               " and to link with -pie.\n");
        Die();
      }
    }
    close(fmaps);
  }
}

void InitializePlatform() {
  void *p = 0;
  if (sizeof(p) == 8) {
    // Disable core dumps, dumping of 16TB usually takes a bit long.
    // The following magic is to prevent clang from replacing it with memset.
    volatile rlimit lim;
    lim.rlim_cur = 0;
    lim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, (rlimit*)&lim);
  }

  CheckPIE();
}

}  // namespace __tsan
