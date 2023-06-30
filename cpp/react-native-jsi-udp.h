#pragma once
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>

#if __APPLE__

#define LOGI(...) printf("[JsiUdp] INFO: "); printf(__VA_ARGS__); printf("\n")
#define LOGD(...) printf("[JsiUdp] DEBUG: "); printf(__VA_ARGS__); printf("\n")
#define LOGW(...) printf("[JsiUdp] WARN: "); printf(__VA_ARGS__); printf("\n")
#define LOGE(...) printf("[JsiUdp] ERROR: "); printf(__VA_ARGS__); printf("\n")

#else

#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JsiUdp", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "JsiUdp", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "JsiUdp", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "JsiUdp", __VA_ARGS__)

#endif

namespace jsiudp {
  typedef std::function<void(std::function<void()> &&)> RunOnJS;

  void install(facebook::jsi::Runtime &jsiRuntime, RunOnJS runOnJS);

  void reset();
}
