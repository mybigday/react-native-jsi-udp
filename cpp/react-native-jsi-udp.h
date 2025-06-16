#pragma once
#include "helper.h"
#include <atomic>
#include <jsi/jsi.h>
#include <map>
#include <memory>
#include <string>
#include <tuple>

#define MAX_PACK_SIZE 65535

// 10us, very short to avoid blocking
#define RECEIVE_TIMEUS 10

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
  UdpManager(facebook::jsi::Runtime *jsiRuntime);
  ~UdpManager();

  void closeAll();
  void suspendAll();
  void resumeAll();

protected:
  facebook::jsi::Runtime *_runtime;
  std::atomic<bool> _invalidate = false;

  JSI_HOST_FUNCTION(create);
  JSI_HOST_FUNCTION(send);
  JSI_HOST_FUNCTION(bind);
  JSI_HOST_FUNCTION(setOpt);
  JSI_HOST_FUNCTION(getOpt);
  JSI_HOST_FUNCTION(close);
  JSI_HOST_FUNCTION(getSockName);
  JSI_HOST_FUNCTION(receive);

private:
  std::map<int, int> idToFdMap;
  int nextId = 1;

  char receiveBuffer[MAX_PACK_SIZE];

  std::vector<SocketState> suspendedSockets;
};
} // namespace jsiudp
