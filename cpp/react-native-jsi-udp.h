#pragma once
#include "helper.h"
#include <ReactCommon/CallInvoker.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <jsi/jsi.h>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <tuple>

#if __APPLE__

#define LOGI(...)                                                              \
  printf("[JsiUdp] INFO: ");                                                   \
  printf(__VA_ARGS__);                                                         \
  printf("\n")
#define LOGD(...)                                                              \
  printf("[JsiUdp] DEBUG: ");                                                  \
  printf(__VA_ARGS__);                                                         \
  printf("\n")
#define LOGW(...)                                                              \
  printf("[JsiUdp] WARN: ");                                                   \
  printf(__VA_ARGS__);                                                         \
  printf("\n")
#define LOGE(...)                                                              \
  printf("[JsiUdp] ERROR: ");                                                  \
  printf(__VA_ARGS__);                                                         \
  printf("\n")

#else

#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JsiUdp", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "JsiUdp", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "JsiUdp", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "JsiUdp", __VA_ARGS__)

#endif

namespace jsiudp {
enum EventType { MESSAGE, ERROR, CLOSE };

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
  UdpManager(facebook::jsi::Runtime *jsiRuntime,
             std::shared_ptr<facebook::react::CallInvoker> callInvoker);
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
  int getFdOrThrow(facebook::jsi::Runtime &runtime, int id);

  // poll-based I/O (replaces worker pool busy-polling)
  void watchFd(int fd);
  void unwatchFd(int fd);
  void pollLoop();
  void wakePoller();

private:
  std::condition_variable cond;
  std::mutex mutex;
  std::queue<Event> events;
  std::map<int, int> idToFdMap;
  int nextId = 1;

  // poll-based I/O
  std::thread _pollThread;
  int _wakePipe[2] = {-1, -1};
  std::set<int> _watchedFds;
  std::mutex _watchMutex;

  std::vector<SocketState> suspendedSockets;
};
} // namespace jsiudp
