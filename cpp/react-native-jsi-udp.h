#pragma once
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <functional>
#include <tuple>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <atomic>
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
  enum EventType {
    MESSAGE,
    ERROR,
    CLOSE
  };

  struct Event {
    int fd;
    EventType type;
    std::string data;
    int family;
    std::string address;
    int port;
  };

  struct SocketState {
    int id;
    std::string address;
    int port;
    int type;
    bool reuseAddr;
    bool reusePort;
    bool broadcast;
  };

  class UdpManager {
  public:
    UdpManager(facebook::jsi::Runtime *jsiRuntime, std::shared_ptr<facebook::react::CallInvoker> callInvoker);
    ~UdpManager();

    void closeAll();
    void suspendAll();
    void resumeAll();

  protected:
    facebook::jsi::Runtime *_runtime;
    std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
    std::atomic<bool> _invalidate = false;
    std::thread eventThread;

    JSI_HOST_FUNCTION(create);
    JSI_HOST_FUNCTION(send);
    JSI_HOST_FUNCTION(bind);
    JSI_HOST_FUNCTION(setOpt);
    JSI_HOST_FUNCTION(getOpt);
    JSI_HOST_FUNCTION(close);
    JSI_HOST_FUNCTION(getSockName);

    void runOnJS(std::function<void()> &&f);

    void sendEvent(Event event);
    void receiveEvent();

    // receive worker
    void emplaceFd(int fd);
    void removeFd(int fd);
    void createWorker();

  private:
    std::condition_variable cond;
    std::mutex mutex;
    std::queue<Event> events;
    std::vector<std::thread> workerPool;
    std::queue<int> fdQueue;
    std::mutex fdQueueMutex;
    std::condition_variable fdQueueCond;
    std::map<int, int> idToFdMap;
    int nextId = 1;

    std::vector<SocketState> suspendedSockets;
  };
}
