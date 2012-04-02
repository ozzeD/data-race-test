//===-- tsan_mutex.h --------------------------------------------*- C++ -*-===//
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
//===----------------------------------------------------------------------===//
#ifndef TSAN_MUTEX_H
#define TSAN_MUTEX_H

#include "tsan_atomic.h"
#include "tsan_defs.h"

namespace __tsan {

class Mutex {
 public:
  explicit Mutex(StatType stat_type);
  ~Mutex();

  void Lock();
  void Unlock();

  void ReadLock();
  void ReadUnlock();

 private:
  atomic_uintptr_t state_;
#ifdef TSAN_COLLECT_STATS
  StatType stat_type_;
#endif

  Mutex(const Mutex&);
  void operator = (const Mutex&);
};

class Lock {
 public:
  explicit Lock(Mutex *m);
  ~Lock();

 private:
  Mutex *m_;

  Lock(const Lock&);
  void operator = (const Lock&);
};

class ReadLock {
 public:
  explicit ReadLock(Mutex *m);
  ~ReadLock();

 private:
  Mutex *m_;

  ReadLock(const ReadLock&);
  void operator = (const ReadLock&);
};

}  // namespace __tsan

#endif  // TSAN_MUTEX_H
