
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.


//
// This is a public header file, it must only include public header files.

#ifndef COMMON_NET_TIMERID_H
#define COMMON_NET_TIMERID_H

#include "../base/copyable.h"

namespace common
{
namespace net
{

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
class TimerId : public common::copyable
{
 public:
  TimerId()
    : timer_(NULL),
      sequence_(0)
  {
  }

  TimerId(Timer* timer, int64_t seq)
    : timer_(timer),
      sequence_(seq)
  {
  }

  // default copy-ctor, dtor and assignment are okay

  friend class TimerQueue;

 private:
  Timer* timer_;
  int64_t sequence_;
};

}  // namespace net
}  // namespace common

#endif  // COMMON_NET_TIMERID_H
