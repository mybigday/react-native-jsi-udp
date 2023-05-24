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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

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

map<int, bool> running;
map<int, int> inetTypes;
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
    default:
      return "UNKNOWN";
  }
}

constexpr unsigned int str2int(const char* str, int h = 0) {
    return !str[h] ? 5381 : (str2int(str, h+1) * 33) ^ str[h];
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

  delete[] buffer;
  onevent({ CLOSE, "", 0, "", 0 });
}

void reset() {
  eventHandlers.clear();
  for (auto &kv : running) {
    if (kv.second) {
      running[kv.first] = false;
      ::close(kv.first);
    }
  }
  running.clear();
  inetTypes.clear();
}

void install(Runtime &jsiRuntime, RunOnJS runOnJS) {

  auto datagram_create = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_create"),
    1,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto type = arguments[0].asString(runtime).utf8(runtime);

      if (type != "udp4" && type != "udp6") {
        throw JSError(runtime, "E_INVALID_TYPE");
      }

      auto inetType = type == "udp4" ? AF_INET : AF_INET6;

      auto fd = socket(inetType, SOCK_DGRAM, 0);
      if (fd < 0) {
        throw JSError(runtime, String::createFromAscii(runtime, error_name(errno)));
      }

      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 100000; // 100ms
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      inetTypes[fd] = inetType;

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

      eventHandlers[fd] = make_shared<Object>(arguments[1].asObject(runtime));

      running[fd] = true;
      auto t = thread(
        worker_loop,
        fd,
        [fd, runOnJS, &runtime](Event event) {
          if (eventHandlers.count(fd) == 0) return;
          runOnJS([&]() {
            auto handler = eventHandlers.at(fd);
            auto eventObj = Object(runtime);
            eventObj.setProperty(
              runtime,
              "type",
              event.type == MESSAGE ? "message" : event.type == ERROR ? "error" : "close"
            );
            if (event.type == MESSAGE) {
              auto ArrayBuffer = runtime.global().getPropertyAsFunction(runtime, "ArrayBuffer");
              auto arrayBufferObj = ArrayBuffer
                .callAsConstructor(runtime, static_cast<int>(event.data.size()))
                .getObject(runtime);
              auto arrayBuffer = arrayBufferObj.getArrayBuffer(runtime);
              memcpy(arrayBuffer.data(runtime), event.data.c_str(), event.data.size());
              eventObj.setProperty(runtime, "data", arrayBuffer);
              eventObj.setProperty(runtime, "family", event.family == AF_INET ? "IPv4" : "IPv6");
              eventObj.setProperty(runtime, "address", move(event.address));
              eventObj.setProperty(runtime, "port", static_cast<int>(event.port));
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
      t.detach();

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_startWorker", move(datagram_startWorker));


  auto datagram_bind = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_bind"),
    3,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto host = arguments[1].asString(runtime).utf8(runtime);
      auto port = static_cast<int>(arguments[2].asNumber());

      struct sockaddr_in addr;
      addr.sin_family = inetTypes.at(fd);
      addr.sin_port = htons(port);
      auto ret = inet_aton(host.c_str(), &addr.sin_addr);
      if (ret == 0) {
        throw JSError(runtime, "E_INVALID_ADDRESS");
      }

      auto result = ::bind(fd, (struct sockaddr *)&addr, sizeof(addr));
      if (result < 0) {
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
        ::close(fd);
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_close", move(datagram_close));


  auto datagram_setOpt = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_setOpt"),
    3,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto key = arguments[1].asString(runtime).utf8(runtime);

      switch (str2int(key.c_str())) {
      case str2int("SO_BROADCAST"): {
        uint8_t value = static_cast<int>(arguments[2].asNumber());
        auto result = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));
        if (result < 0) {
          throw JSError(runtime, error_name(errno));
        }
        break;
      }
      case str2int("SO_RCVBUF"): {
        uint32_t value = static_cast<int>(arguments[2].asNumber());
        auto result = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value));
        if (result < 0) {
          throw JSError(runtime, error_name(errno));
        }
        break;
      }
      case str2int("SO_SNDBUF"): {
        uint32_t value = static_cast<int>(arguments[2].asNumber());
        auto result = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value));
        if (result < 0) {
          throw JSError(runtime, String::createFromAscii(runtime, error_name(errno)));
        }
        break;
      }
      // add membership
      case str2int("IP_ADD_MEMBERSHIP"): {
        auto value = arguments[2].asString(runtime).utf8(runtime);
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(value.c_str());
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        auto result = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        if (result < 0) {
          throw JSError(runtime, error_name(errno));
        }
        break;
      }
      // drop membership
      case str2int("IP_DROP_MEMBERSHIP"): {
        auto value = arguments[2].asString(runtime).utf8(runtime);
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(value.c_str());
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        auto result = setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        if (result < 0) {
          throw JSError(runtime, error_name(errno));
        }
        break;
      }
      default:
        throw JSError(runtime, "E_INVALID_OPTION");
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_setOpt", move(datagram_setOpt));


  auto datagram_send = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_send"),
    4,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());
      auto host = arguments[1].asString(runtime).utf8(runtime);
      auto port = static_cast<int>(arguments[2].asNumber());
      auto data = arguments[3].asObject(runtime).getArrayBuffer(runtime);

      struct sockaddr_in addr;
      addr.sin_family = inetTypes.at(fd);
      addr.sin_port = htons(port);
      auto ret = inet_aton(host.c_str(), &addr.sin_addr);
      if (ret == 0) {
        throw JSError(runtime, "E_INVALID_ADDRESS");
      }

      auto result = sendto(
        fd,
        data.data(runtime),
        data.size(runtime),
        O_NONBLOCK,
        (struct sockaddr *)&addr,
        sizeof(addr)
      );

      if (result < 0 && errno != EWOULDBLOCK) {
        throw JSError(runtime, error_name(errno));
      }

      return Value::undefined();
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_send", move(datagram_send));


  auto datagram_getSockName = Function::createFromHostFunction(
    jsiRuntime,
    PropNameID::forAscii(jsiRuntime, "datagram_getSockName"),
    1,
    [](Runtime &runtime, const Value &thisValue, const Value *arguments, size_t count) -> Value {
      auto fd = static_cast<int>(arguments[0].asNumber());

      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);
      auto result = getsockname(fd, (struct sockaddr *)&addr, &len);
      if (result < 0) {
        throw JSError(runtime, error_name(errno));
      }

      auto host = inet_ntoa(addr.sin_addr);
      auto port = ntohs(addr.sin_port);

      auto ret = Object(runtime);
      ret.setProperty(
        runtime,
        "address",
        String::createFromAscii(runtime, host)
      );
      ret.setProperty(
        runtime,
        "port",
        Value(static_cast<int>(port))
      );
      ret.setProperty(
        runtime,
        "family",
        String::createFromAscii(runtime, addr.sin_family == AF_INET ? "IPv4" : "IPv6")
      );
      return ret;
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "datagram_getSockName", move(datagram_getSockName));

}

} // namespace jsiudp
