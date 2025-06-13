#ifndef COMMON_BASE_NONCOPYABLE_H
#define COMMON_BASE_NONCOPYABLE_H

namespace common
{

class noncopyable
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace common

#endif  // COMMON_BASE_NONCOPYABLE_H
