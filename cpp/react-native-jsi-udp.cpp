#include "react-native-jsi-udp.h"
#include <jsi/jsi.h>
#include <thread>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <queue>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#if __APPLE__

#define LOGI(...) printf("[JsiUdp] INFO: "); printf(__VA_ARGS__); printf("\n")
#define LOGD(...) printf("[JsiUdp] DEBUG: "); printf(__VA_ARGS__); printf("\n")

#else

#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JsiUdp", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "JsiUdp", __VA_ARGS__)

#endif

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP     IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP    IPV6_LEAVE_GROUP
#endif

#define MAX_PACK_SIZE 65535

using namespace facebook::jsi;
using namespace facebook::react;
using namespace std;

namespace jsiudp {

enum EventType {
  MESSAGE,
  ERROR,
  CLOSE
};

struct Event {
  EventType type;
  string data;
  int family;
  string address;
  int port;
};

map<int, thread> workers;
map<int, bool> running;
map<int, shared_ptr<Object>> eventHandlers;

string error_name(int err) {
  switch (err) {
    case EACCES:
      return "EACCES";
    case EADDRINUSE:
      return "EADDRINUSE";
    case EADDRNOTAVAIL:
      return "EADDRNOTAVAIL";
    case EAFNOSUPPORT:
      return "EAFNOSUPPORT";
    case EAGAIN:
      return "EAGAIN";
    case EALREADY:
      return "EALREADY";
    case EBADF:
      return "EBADF";
    case ECONNREFUSED:
      return "ECONNREFUSED";
    case EFAULT:
      return "EFAULT";
    case EINPROGRESS:
      return "EINPROGRESS";
    case EINTR:
      return "EINTR";
    case EISCONN:
      return "EISCONN";
    case ENETUNREACH:
      return "ENETUNREACH";
    case ENOTSOCK:
      return "ENOTSOCK";
    case ETIMEDOUT:
      return "ETIMEDOUT";
    case ENOPROTOOPT:
      return "ENOPROTOOPT";
    case EINVAL:
      return "EINVAL";
    case EDOM:
      return "EDOM";
    case ENOMEM:
      return "ENOMEM";
    case ENOBUFS:
      return "ENOBUFS";
    case EOPNOTSUPP:
      return "EOPNOTSUPP";
    default:
      return "UNKNOWN";
  }
}

void worker_loop(int fd, function<void(Event)> onevent) {
  auto buffer = new char[MAX_PACK_SIZE];

  while (running.count(fd) > 0 && running.at(fd)) {
    struct sockaddr_in in_addr;
    socklen_t in_len = sizeof(in_addr);

    auto recvn = ::recvfrom(fd, buffer, MAX_PACK_SIZE, 0, (struct sockaddr *)&in_addr, &in_len);

    if (recvn < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        onevent({ ERROR, error_name(errno), 0, "", 0 });
        break;
      } else {
        continue;
      }
    }

    onevent({
      MESSAGE,
      string(buffer, recvn),
      in_addr.sin_family,
      inet_ntoa(in_addr.sin_addr),
      ntohs(in_addr.sin_port)
    });
  }

  ::close(fd);
  delete[] buffer;
  onevent({ CLOSE, "", 0, "", 0 });
}

void reset() {
  for (auto it = running.begin(); it != running.end(); ++it) {
    auto fd = it->first;
    if (it->second) {
      eventHandlers.erase(fd);
      it->second = false;
      workers[fd].detach();
      workers.erase(fd);
    }
  }
}

void install(Runtime &jsiRuntime, RunOnJS runOnJS) {

  auto datagram_create = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_create"),
    1,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto type = static_cast<int>(arguments[0].asNumber());

      if (type != 4 && type != 6) {
        throw JSError(runtime, "E_INVALID_TYPE");
      }

      auto inetType = type == 4 ? AF_INET : AF_INET6;

      auto fd = socket(inetType, SOCK_DGRAM, 0);
      if (fd <= 0) {
        throw JSError(runtime, String::createFromAscii(runtime, error_name(errno)));
      }

      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 100000; // 100ms
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      return fd;
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_create", move(datagram_create));


  auto datagram_startWorker = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_startWorker"),
    2,
    [runOnJS](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      if (running.count(fd) > 0 && running.at(fd)) {
        throw JSError(runtime, "E_ALREADY_RUNNING");
      }
      if (workers.count(fd) > 0) {
        running[fd] = false;
        workers[fd].join();
      }

      eventHandlers[fd] = make_shared<Object>(arguments[1].asObject(runtime));

      running[fd] = true;
      workers[fd] = thread(
        worker_loop,
        fd,
        [fd, runOnJS, &runtime](Event event) {
          if (eventHandlers.count(fd) == 0) return;
          runOnJS([fd, event, &runtime]() {
            auto handler = eventHandlers.at(fd);
            auto eventObj = Object(runtime);
            eventObj.setProperty(
              runtime,
              "type",
              String::createFromAscii(
                runtime,
                event.type == MESSAGE ? "message" : event.type == ERROR ? "error" : "close"
              )
            );
            if (event.type == MESSAGE) {
              auto ArrayBuffer = runtime.global().getPropertyAsFunction(runtime, "ArrayBuffer");
              auto arrayBufferObj = ArrayBuffer
                .callAsConstructor(runtime, static_cast<int>(event.data.size()))
                .getObject(runtime);
              auto arrayBuffer = arrayBufferObj.getArrayBuffer(runtime);
              memcpy(arrayBuffer.data(runtime), event.data.c_str(), event.data.size());
              eventObj.setProperty(
                runtime,
                "data",
                move(arrayBuffer)
              );
              eventObj.setProperty(
                runtime,
                "family",
                String::createFromAscii(runtime, event.family == AF_INET ? "IPv4" : "IPv6")
              );
              eventObj.setProperty(
                runtime,
                "address",
                String::createFromAscii(runtime, event.address)
              );
              eventObj.setProperty(
                runtime,
                "port",
                static_cast<int>(event.port)
              );
            } else if (event.type == ERROR) {
              auto Error = runtime.global().getPropertyAsFunction(runtime, "Error");
              auto errorObj = Error
                .callAsConstructor(runtime, String::createFromAscii(runtime, event.data))
                .getObject(runtime);
              eventObj.setProperty(runtime, "error", errorObj);
            }
            handler->asFunction(runtime).call(runtime, eventObj);
          });
        }
      );

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_startWorker", move(datagram_startWorker));


  auto datagram_bind = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_bind"),
    4,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto type = static_cast<int>(arguments[1].asNumber());
      auto host = arguments[2].asString(runtime).utf8(runtime);
      auto port = static_cast<int>(arguments[3].asNumber());

      long ret = 0;
      if (type == 4) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        ret = inet_pton(AF_INET, host.c_str(), &(addr.sin_addr));
        if (ret == 1) {
          ret = ::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        }
      } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        ret = inet_pton(AF_INET6, host.c_str(), &(addr.sin6_addr));
        if (ret == 1) {
          ret = ::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        }
      }

      if (ret < 0) {
        throw JSError(runtime, error_name(errno));
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_bind", move(datagram_bind));


  auto datagram_close = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_close"),
    1,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = arguments[0].asNumber();

      if (running.count(fd) > 0 && running.at(fd)) {
        running[fd] = false;
        workers[fd].join();
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_close", move(datagram_close));


  auto datagram_setOpt = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_setOpt"),
    5,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto level = static_cast<int>(arguments[1].asNumber());
      auto option = static_cast<int>(arguments[2].asNumber());

      long result = 0;
      if (level == SOL_SOCKET) {
        int value = static_cast<int>(arguments[3].asNumber());
        result = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));
      } else if (level == IPPROTO_IP) {
        switch (option) {
        case IP_TTL:
        case IP_MULTICAST_TTL:
        case IP_MULTICAST_LOOP: {
          int value = static_cast<int>(arguments[3].asNumber());
          result = setsockopt(fd, IPPROTO_IP, option, &value, sizeof(value));
          break;
        }
        case IP_ADD_MEMBERSHIP:
        case IP_DROP_MEMBERSHIP: {
          struct ip_mreq mreq;
          mreq.imr_multiaddr.s_addr = inet_addr(arguments[3].asString(runtime).utf8(runtime).c_str());
          if (arguments[4].isString()) {
            auto value = arguments[4].asString(runtime).utf8(runtime);
            mreq.imr_interface.s_addr = inet_addr(value.c_str());
          } else {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
          }
          result = setsockopt(fd, IPPROTO_IP, option, &mreq, sizeof(mreq));
          LOGD("member of %s", inet_ntoa(mreq.imr_multiaddr));
          break;
        }
        default:
          throw JSError(runtime, "E_INVALID_OPTION");
        }
      } else if (level == IPPROTO_IPV6) {
        switch (option) {
        case IPV6_MULTICAST_HOPS:
        case IPV6_MULTICAST_LOOP: {
          int value = static_cast<int>(arguments[3].asNumber());
          result = setsockopt(fd, IPPROTO_IPV6, option, &value, sizeof(value));
          break;
        }
        case IPV6_ADD_MEMBERSHIP:
        case IPV6_DROP_MEMBERSHIP: {
          struct ipv6_mreq mreq;
          auto value = arguments[3].asString(runtime).utf8(runtime);
          auto ret = inet_pton(AF_INET6, value.c_str(), &(mreq.ipv6mr_multiaddr));
          if (ret != 1) {
            throw JSError(runtime, error_name(errno));
          }
          if (arguments[4].isString()) {
            auto value = arguments[4].asString(runtime).utf8(runtime);
            inet_pton(AF_INET6, value.c_str(), &(mreq.ipv6mr_interface));
          } else {
            mreq.ipv6mr_interface = 0;
          }
          result = setsockopt(fd, IPPROTO_IPV6, option, &mreq, sizeof(mreq));
          break;
        }
        default:
          throw JSError(runtime, "E_INVALID_OPTION");
        }
      } else {
        throw JSError(runtime, "E_INVALID_LEVEL");
      }
      if (result < 0) {
        throw JSError(runtime, error_name(errno));
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_setOpt", move(datagram_setOpt));

  auto datagram_getOpt = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_getOpt"),
    3,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto level = static_cast<int>(arguments[1].asNumber());
      auto option = static_cast<int>(arguments[2].asNumber());

      if (level == SOL_SOCKET) {
        uint32_t value;
        socklen_t len = sizeof(value);
        auto result = getsockopt(fd, level, option, &value, &len);
        if (result < 0) {
          throw JSError(runtime, error_name(errno));
        }
        return static_cast<int>(value);
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_getOpt", move(datagram_getOpt));

  auto datagram_send = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_send"),
    5,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto type = static_cast<int>(arguments[1].asNumber());
      auto host = arguments[2].asString(runtime).utf8(runtime);
      auto port = static_cast<int>(arguments[3].asNumber());
      auto data = arguments[4].asObject(runtime).getArrayBuffer(runtime);

      long ret = 0;
      if (type == 4) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        ret = inet_pton(AF_INET, host.c_str(), &(addr.sin_addr));
        if (ret == 1) {
          LOGD(
            "send %s:%d, parsed %X, is broadcast %d",
            host.c_str(),
            port,
            addr.sin_addr.s_addr,
            INADDR_BROADCAST == addr.sin_addr.s_addr
          );
          ret = sendto(
            fd,
            data.data(runtime),
            data.size(runtime),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
          );
          LOGD("send size %d", data.size(runtime));
        }
      } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        ret = inet_pton(AF_INET6, host.c_str(), &(addr.sin6_addr));
        if (ret == 1) {
          ret = sendto(
            fd,
            data.data(runtime),
            data.size(runtime),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
          );
        }
      }

      if (ret < 0 && errno != EWOULDBLOCK) {
        throw JSError(runtime, error_name(errno));
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_send", move(datagram_send));


  auto datagram_getSockName = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_getSockName"),
    2,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      int type = static_cast<int>(arguments[1].asNumber());

      auto result = Object(runtime);
      if (type == 4) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        auto ret = getsockname(fd, (struct sockaddr *)&addr, &len);
        if (ret < 0) {
          throw JSError(runtime, error_name(errno));
        }
        auto host = inet_ntoa(addr.sin_addr);
        auto port = ntohs(addr.sin_port);
        result.setProperty(
          runtime,
          "address",
          String::createFromAscii(runtime, host)
        );
        result.setProperty(
          runtime,
          "port",
          static_cast<int>(port)
        );
        result.setProperty(
          runtime,
          "family",
          String::createFromAscii(runtime, "IPv4")
        );
      } else {
        struct sockaddr_in6 addr;
        socklen_t len = sizeof(addr);
        auto ret = getsockname(fd, (struct sockaddr *)&addr, &len);
        if (ret < 0) {
          throw JSError(runtime, error_name(errno));
        }
        char host[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr.sin6_addr, host, INET6_ADDRSTRLEN);
        auto port = ntohs(addr.sin6_port);
        result.setProperty(
          runtime,
          "address",
          String::createFromAscii(runtime, host)
        );
        result.setProperty(
          runtime,
          "port",
          static_cast<int>(port)
        );
        result.setProperty(
          runtime,
          "family",
          String::createFromAscii(runtime, "IPv6")
        );
      }
      return result;
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_getSockName", move(datagram_getSockName));

}

} // namespace jsiudp
