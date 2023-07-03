#pragma once
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <functional>
#include <tuple>
#include "helper.h"

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

  enum EventType {
    MESSAGE,
    ERROR,
    CLOSE
  };

  struct Event {
    EventType type;
    std::string data;
    int family;
    std::string address;
    int port;
  };

  class UdpManager {
  public:
    UdpManager(facebook::jsi::Runtime &jsiRuntime, RunOnJS runOnJS);
    ~UdpManager();

  protected:
    facebook::jsi::Runtime &_runtime;
    RunOnJS runOnJS;
    std::map<int, std::thread> workers;
    std::map<int, bool> running;
    bool _invalidate = false;

    JSI_HOST_FUNCTION(create);
    JSI_HOST_FUNCTION(send);
    JSI_HOST_FUNCTION(startWorker);
    JSI_HOST_FUNCTION(bind);
    JSI_HOST_FUNCTION(setOpt);
    JSI_HOST_FUNCTION(getOpt);
    JSI_HOST_FUNCTION(close);
    JSI_HOST_FUNCTION(getSockName);

    void workerLoop(int fd, std::function<void(Event)> handler);
  };
}
