
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.



#include "Timer.h"

using namespace common;
using namespace common::net;

AtomicInt64 Timer::s_numCreated_;

void Timer::restart(Timestamp now)
{
  if (repeat_)
  {
    expiration_ = addTime(now, interval_);
  }
  else
  {
    expiration_ = Timestamp::invalid();
  }
}
