#include "react-native-jsi-udp.h"
#include "helper.h"
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

#import <ifaddrs.h>
#include <net/if.h>

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
    #if __APPLE__
    case 65:
      return "No route to host";
    #endif
    case EPERM:
      return "EPERM";
    case EPIPE:
      return "EPIPE";
    default:
      LOGE("unknown error %d", err);
      return "UNKNOWN";
  }
}

int setupIface(int fd, struct sockaddr_in &addr) {
  #if __APPLE__
  struct ifaddrs *ifaddr, *ifa;
  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }
  auto isAny = addr.sin_addr.s_addr == INADDR_ANY;
  auto isLoopback = addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK);
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (
      ifa->ifa_addr != NULL &&
      ifa->ifa_addr->sa_family == AF_INET &&
      !((ifa->ifa_flags & IFF_LOOPBACK) ^ isLoopback) &&
      (ifa->ifa_flags & IFF_UP) &&
      (
        isAny ||
        reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr.s_addr == addr.sin_addr.s_addr
      )
    ) {
      auto index = if_nametoindex(ifa->ifa_name);
      if (setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &index, sizeof(index)) != 0) {
        return -1;
      }
      LOGI("bound to %s for %d", ifa->ifa_name, fd);
      break;
    }
  }
  freeifaddrs(ifaddr);
  #endif
  return 0;
}

int setupIface(int fd, struct sockaddr_in6 &addr) {
  #if __APPLE__
  struct ifaddrs *ifaddr, *ifa;
  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }
  auto size = sizeof(addr);
  auto isAny = memcmp(&(addr.sin6_addr), &in6addr_any, size) == 0;
  auto isLoopback = memcmp(&(addr.sin6_addr), &in6addr_loopback, size) == 0;
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (
      ifa->ifa_addr != NULL &&
      ifa->ifa_addr->sa_family == AF_INET6 &&
      !((ifa->ifa_flags & IFF_LOOPBACK) ^ isLoopback) &&
      (ifa->ifa_flags & IFF_UP) &&
      (
        isAny ||
        memcmp(
          &(addr.sin6_addr),
          &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr),
          size
        ) == 0
      )
    ) {
      auto index = if_nametoindex(ifa->ifa_name);
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_BOUND_IF, &index, sizeof(index)) != 0) {
        return -1;
      }
      LOGI("bound to %s for %d", ifa->ifa_name, fd);
      break;
    }
  }
  freeifaddrs(ifaddr);
  #endif
  return 0;
}

UdpManager::UdpManager(Runtime &jsiRuntime, RunOnJS runOnJS) : _runtime(jsiRuntime), runOnJS(runOnJS) {
  EXPOSE_FN(jsiRuntime, datagram_create, 1, BIND_METHOD(UdpManager::create));
  EXPOSE_FN(jsiRuntime, datagram_startWorker, 2, BIND_METHOD(UdpManager::startWorker));
  EXPOSE_FN(jsiRuntime, datagram_bind, 4, BIND_METHOD(UdpManager::bind));
  EXPOSE_FN(jsiRuntime, datagram_send, 5, BIND_METHOD(UdpManager::send));
  EXPOSE_FN(jsiRuntime, datagram_close, 1, BIND_METHOD(UdpManager::close));
  EXPOSE_FN(jsiRuntime, datagram_getOpt, 3, BIND_METHOD(UdpManager::getOpt));
  EXPOSE_FN(jsiRuntime, datagram_setOpt, 5, BIND_METHOD(UdpManager::setOpt));
  EXPOSE_FN(jsiRuntime, datagram_getSockName, 2, BIND_METHOD(UdpManager::getSockName));

  {
    auto global = jsiRuntime.global();

    global.setProperty(jsiRuntime, "dgc_SOL_SOCKET", static_cast<int>(SOL_SOCKET));
    global.setProperty(jsiRuntime, "dgc_IPPROTO_IP", static_cast<int>(IPPROTO_IP));
    global.setProperty(jsiRuntime, "dgc_IPPROTO_IPV6", static_cast<int>(IPPROTO_IPV6));
    global.setProperty(jsiRuntime, "dgc_SO_REUSEADDR", static_cast<int>(SO_REUSEADDR));
    global.setProperty(jsiRuntime, "dgc_SO_REUSEPORT", static_cast<int>(SO_REUSEPORT));
    global.setProperty(jsiRuntime, "dgc_SO_BROADCAST", static_cast<int>(SO_BROADCAST));
    global.setProperty(jsiRuntime, "dgc_SO_RCVBUF", static_cast<int>(SO_RCVBUF));
    global.setProperty(jsiRuntime, "dgc_SO_SNDBUF", static_cast<int>(SO_SNDBUF));
    global.setProperty(jsiRuntime, "dgc_IP_MULTICAST_TTL", static_cast<int>(IP_MULTICAST_TTL));
    global.setProperty(jsiRuntime, "dgc_IP_MULTICAST_LOOP", static_cast<int>(IP_MULTICAST_LOOP));
    global.setProperty(jsiRuntime, "dgc_IP_ADD_MEMBERSHIP", static_cast<int>(IP_ADD_MEMBERSHIP));
    global.setProperty(jsiRuntime, "dgc_IP_DROP_MEMBERSHIP", static_cast<int>(IP_DROP_MEMBERSHIP));
    global.setProperty(jsiRuntime, "dgc_IP_TTL", static_cast<int>(IP_TTL));
  }
}

UdpManager::~UdpManager() {
  _invalidate = true;
  for (auto &entry : running) {
    running[entry.first] = false;
  }
  for (auto &entry : workers) {
    if (entry.second.joinable()) entry.second.detach();
  }
}

JSI_HOST_FUNCTION(UdpManager::create) {
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
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  return fd;
}

JSI_HOST_FUNCTION(UdpManager::startWorker) {
  auto fd = static_cast<int>(arguments[0].asNumber());
  auto handler = make_shared<Function>(arguments[1].asObject(runtime).asFunction(runtime));
  if (running.count(fd) > 0 && running[fd]) {
    throw JSError(runtime, "E_ALREADY_RUNNING");
  }
  if (workers.count(fd) > 0) {
    LOGW("worker already exists, fd = %d, joinable = %d", fd, workers[fd].joinable());
    if (workers[fd].joinable())
      workers[fd].join();
  }

  running[fd] = true;
  workers[fd] = thread(
    &UdpManager::workerLoop,
    this,
    fd,
    [this, handler](Event event) {
      if (_invalidate || runOnJS == nullptr) return;
      runOnJS([this, handler, event]() {
        auto eventObj = Object(_runtime);
        eventObj.setProperty(
          _runtime,
          "type",
          String::createFromAscii(
            _runtime,
            event.type == MESSAGE ? "message" : event.type == ERROR ? "error" : "close"
          )
        );
        if (event.type == MESSAGE) {
          auto ArrayBuffer = _runtime.global().getPropertyAsFunction(_runtime, "ArrayBuffer");
          auto arrayBufferObj = ArrayBuffer
            .callAsConstructor(_runtime, static_cast<int>(event.data.size()))
            .getObject(_runtime);
          auto arrayBuffer = arrayBufferObj.getArrayBuffer(_runtime);
          memcpy(arrayBuffer.data(_runtime), event.data.c_str(), event.data.size());
          eventObj.setProperty(
            _runtime,
            "data",
            move(arrayBuffer)
          );
          eventObj.setProperty(
            _runtime,
            "family",
            String::createFromAscii(_runtime, event.family == AF_INET ? "IPv4" : "IPv6")
          );
          eventObj.setProperty(
            _runtime,
            "address",
            String::createFromAscii(_runtime, event.address)
          );
          eventObj.setProperty(
            _runtime,
            "port",
            static_cast<int>(event.port)
          );
        } else if (event.type == ERROR) {
          auto Error = _runtime.global().getPropertyAsFunction(_runtime, "Error");
          auto errorObj = Error
            .callAsConstructor(_runtime, String::createFromAscii(_runtime, event.data))
            .getObject(_runtime);
          eventObj.setProperty(_runtime, "error", errorObj);
        }
        handler->call(_runtime, eventObj);
      });
    }
  );

  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::bind) {
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
      if (setupIface(fd, addr) != 0) {
        throw JSError(runtime, error_name(errno));
      }
      ret = ::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
  } else {
    struct sockaddr_in6 addr;
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    ret = inet_pton(AF_INET6, host.c_str(), &(addr.sin6_addr));
    if (ret == 1) {
      if (setupIface(fd, addr) != 0) {
        throw JSError(runtime, error_name(errno));
      }
      ret = ::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
  }

  if (ret < 0) {
    throw JSError(runtime, error_name(errno));
  }

  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::close) {
  auto fd = static_cast<int>(arguments[0].asNumber());
  if (running.count(fd) > 0 && running[fd]) {
    running[fd] = false;
    workers[fd].join();
  }
  ::close(fd);
  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::setOpt) {
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

JSI_HOST_FUNCTION(UdpManager::getOpt) {
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

JSI_HOST_FUNCTION(UdpManager::send) {
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
      ret = sendto(
        fd,
        data.data(runtime),
        data.size(runtime),
        MSG_DONTWAIT,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
      );
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

JSI_HOST_FUNCTION(UdpManager::getSockName) {
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

void UdpManager::workerLoop(int fd, function<void(Event)> emitEvent) {
  auto buffer = new char[MAX_PACK_SIZE];

  while (running.count(fd) > 0 && running.at(fd)) {
    struct sockaddr_in in_addr;
    socklen_t in_len = sizeof(in_addr);

    auto recvn = ::recvfrom(fd, buffer, MAX_PACK_SIZE, 0, (struct sockaddr *)&in_addr, &in_len);

    if (recvn < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        emitEvent({ ERROR, error_name(errno), 0, "", 0 });
        break;
      } else {
        continue;
      }
    }

    emitEvent({
      MESSAGE,
      string(buffer, recvn),
      in_addr.sin_family,
      inet_ntoa(in_addr.sin_addr),
      ntohs(in_addr.sin_port)
    });
  }

  ::close(fd);
  delete[] buffer;
  emitEvent({ CLOSE, "", 0, "", 0 });
}

} // namespace jsiudp
