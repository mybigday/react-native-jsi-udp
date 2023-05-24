#pragma once
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>

namespace jsiudp {
  typedef std::function<void(std::function<void()> &&)> RunOnJS;

  void install(facebook::jsi::Runtime &jsiRuntime, RunOnJS runOnJS);

  void reset();
}
