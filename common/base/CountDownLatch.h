// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//


#ifndef COMMON_BASE_COUNTDOWNLATCH_H
#define COMMON_BASE_COUNTDOWNLATCH_H

#include "Condition.h"
#include "Mutex.h"

namespace common
{

class CountDownLatch : noncopyable
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  Condition condition_ GUARDED_BY(mutex_);
  int count_ GUARDED_BY(mutex_);
};

}  // namespace common
#endif  // COMMON_BASE_COUNTDOWNLATCH_H
