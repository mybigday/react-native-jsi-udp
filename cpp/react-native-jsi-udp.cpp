#include "react-native-jsi-udp.h"
#include "helper.h"
#include <arpa/inet.h>
#include <jsi/jsi.h>
#include <map>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#if __APPLE__

#import <ifaddrs.h>
#include <net/if.h>

#endif

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

using namespace facebook::jsi;

namespace jsiudp {

std::string error_name(int err) {
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
  case ENETDOWN:
    return "ENETDOWN";
  case ECONNABORTED:
    return "ECONNABORTED";
  case ECONNRESET:
    return "ECONNRESET";
  case ENOTCONN:
    return "ENOTCONN";
  case EHOSTUNREACH:
    return "EHOSTUNREACH";
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
    if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET &&
        !((ifa->ifa_flags & IFF_LOOPBACK) ^ isLoopback) &&
        (ifa->ifa_flags & IFF_UP) &&
        (isAny || reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr)
                          ->sin_addr.s_addr == addr.sin_addr.s_addr)) {
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
    if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET6 &&
        !((ifa->ifa_flags & IFF_LOOPBACK) ^ isLoopback) &&
        (ifa->ifa_flags & IFF_UP) &&
        (isAny ||
         memcmp(&(addr.sin6_addr),
                &(reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr)
                      ->sin6_addr),
                size) == 0)) {
      auto index = if_nametoindex(ifa->ifa_name);
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_BOUND_IF, &index, sizeof(index)) !=
          0) {
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

UdpManager::UdpManager(Runtime *jsiRuntime) : _runtime(jsiRuntime) {

  EXPOSE_FN(*_runtime, datagram_create, 1, BIND_METHOD(UdpManager::create));
  EXPOSE_FN(*_runtime, datagram_bind, 4, BIND_METHOD(UdpManager::bind));
  EXPOSE_FN(*_runtime, datagram_send, 5, BIND_METHOD(UdpManager::send));
  EXPOSE_FN(*_runtime, datagram_close, 1, BIND_METHOD(UdpManager::close));
  EXPOSE_FN(*_runtime, datagram_getOpt, 3, BIND_METHOD(UdpManager::getOpt));
  EXPOSE_FN(*_runtime, datagram_setOpt, 5, BIND_METHOD(UdpManager::setOpt));
  EXPOSE_FN(*_runtime, datagram_getSockName, 2,
            BIND_METHOD(UdpManager::getSockName));
  EXPOSE_FN(*_runtime, datagram_receive, 1, BIND_METHOD(UdpManager::receive));

  auto global = _runtime->global();
  global.setProperty(*_runtime, "dgc_SOL_SOCKET", static_cast<int>(SOL_SOCKET));
  global.setProperty(*_runtime, "dgc_IPPROTO_IP", static_cast<int>(IPPROTO_IP));
  global.setProperty(*_runtime, "dgc_IPPROTO_IPV6",
                     static_cast<int>(IPPROTO_IPV6));
  global.setProperty(*_runtime, "dgc_SO_REUSEADDR",
                     static_cast<int>(SO_REUSEADDR));
  global.setProperty(*_runtime, "dgc_SO_REUSEPORT",
                     static_cast<int>(SO_REUSEPORT));
  global.setProperty(*_runtime, "dgc_SO_BROADCAST",
                     static_cast<int>(SO_BROADCAST));
  global.setProperty(*_runtime, "dgc_SO_RCVBUF", static_cast<int>(SO_RCVBUF));
  global.setProperty(*_runtime, "dgc_SO_SNDBUF", static_cast<int>(SO_SNDBUF));
  global.setProperty(*_runtime, "dgc_IP_MULTICAST_TTL",
                     static_cast<int>(IP_MULTICAST_TTL));
  global.setProperty(*_runtime, "dgc_IP_MULTICAST_LOOP",
                     static_cast<int>(IP_MULTICAST_LOOP));
  global.setProperty(*_runtime, "dgc_IP_ADD_MEMBERSHIP",
                     static_cast<int>(IP_ADD_MEMBERSHIP));
  global.setProperty(*_runtime, "dgc_IP_DROP_MEMBERSHIP",
                     static_cast<int>(IP_DROP_MEMBERSHIP));
  global.setProperty(*_runtime, "dgc_IP_TTL", static_cast<int>(IP_TTL));
  global.setProperty(*_runtime, "datagram_callbacks", Object(*_runtime));
}

UdpManager::~UdpManager() {
  _invalidate = true;
  for (const auto &[id, fd] : idToFdMap) {
    ::close(fd);
  }
}

void UdpManager::closeAll() {
  for (const auto &[id, fd] : idToFdMap) {
    ::close(fd);
  }
  idToFdMap.clear();
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
  tv.tv_usec = RECEIVE_TIMEUS;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  int id = nextId++;
  idToFdMap[id] = fd;

  return id;
}

JSI_HOST_FUNCTION(UdpManager::bind) {
  auto id = static_cast<int>(arguments[0].asNumber());
  auto fd = idToFdMap[id];
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
      ret =
          ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
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
      ret =
          ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    }
  }

  if (ret < 0) {
    throw JSError(runtime, error_name(errno));
  }

  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::close) {
  auto id = static_cast<int>(arguments[0].asNumber());
  auto fd = idToFdMap[id];
  ::close(fd);
  idToFdMap.erase(id);
  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::setOpt) {
  auto id = static_cast<int>(arguments[0].asNumber());
  auto level = static_cast<int>(arguments[1].asNumber());
  auto option = static_cast<int>(arguments[2].asNumber());
  auto fd = idToFdMap[id];

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
      mreq.imr_multiaddr.s_addr =
          inet_addr(arguments[3].asString(runtime).utf8(runtime).c_str());
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
  auto id = static_cast<int>(arguments[0].asNumber());
  auto fd = idToFdMap[id];
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
  auto id = static_cast<int>(arguments[0].asNumber());
  if (idToFdMap.find(id) == idToFdMap.end()) {
    throw JSError(runtime, "E_INVALID_SOCKET");
  }

  auto fd = idToFdMap[id];
  auto type = static_cast<int>(arguments[1].asNumber());
  auto host = arguments[2].asString(runtime).utf8(runtime);
  auto port = static_cast<int>(arguments[3].asNumber());
  auto data = arguments[4].asObject(runtime).getArrayBuffer(runtime);

  long ret = 0;
  try {
    if (type == 4) {
      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      ret = inet_pton(AF_INET, host.c_str(), &(addr.sin_addr));
      if (ret == 1) {
        ret = sendto(fd, data.data(runtime), data.size(runtime), MSG_DONTWAIT,
                     reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
      }
    } else {
      struct sockaddr_in6 addr;
      addr.sin6_family = AF_INET6;
      addr.sin6_port = htons(port);
      ret = inet_pton(AF_INET6, host.c_str(), &(addr.sin6_addr));
      if (ret == 1) {
        ret = sendto(fd, data.data(runtime), data.size(runtime), MSG_DONTWAIT,
                     reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
      }
    }
  } catch (const std::system_error &e) {
    LOGE("System error in send: %s (code: %d)", e.what(), e.code().value());
    throw JSError(runtime, String::createFromAscii(runtime, e.what()));
  } catch (const std::exception &e) {
    LOGE("Error in send: %s", e.what());
    throw JSError(runtime, String::createFromAscii(runtime, e.what()));
  }

  if (ret == 0) {
    throw JSError(runtime, "E_SEND_FAILED");
  } else if (ret < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return Value::undefined();
    }
    throw JSError(runtime, error_name(errno));
  }

  return Value::undefined();
}

JSI_HOST_FUNCTION(UdpManager::getSockName) {
  auto id = static_cast<int>(arguments[0].asNumber());
  auto fd = idToFdMap[id];
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
    result.setProperty(runtime, "address",
                       String::createFromAscii(runtime, host));
    result.setProperty(runtime, "port", static_cast<int>(port));
    result.setProperty(runtime, "family",
                       String::createFromAscii(runtime, "IPv4"));
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
    result.setProperty(runtime, "address",
                       String::createFromAscii(runtime, host));
    result.setProperty(runtime, "port", static_cast<int>(port));
    result.setProperty(runtime, "family",
                       String::createFromAscii(runtime, "IPv6"));
  }
  return result;
}

JSI_HOST_FUNCTION(UdpManager::receive) {
  auto id = static_cast<int>(arguments[0].asNumber());
  auto fd = idToFdMap[id];

  // Create a Promise
  auto promiseCtor = runtime.global().getPropertyAsFunction(runtime, "Promise");
  auto promise = promiseCtor.callAsConstructor(
      runtime,
      Function::createFromHostFunction(
          runtime, PropNameID::forAscii(runtime, "executor"), 2,
          [fd, this](Runtime &runtime, const Value &thisValue,
                     const Value *arguments, size_t count) -> Value {
            struct sockaddr_in in_addr;
            socklen_t in_len = sizeof(in_addr);
            auto resolve = arguments[0].asObject(runtime).asFunction(runtime);
            auto reject = arguments[1].asObject(runtime).asFunction(runtime);

            auto recvn = recvfrom(fd, receiveBuffer, sizeof(receiveBuffer), 0,
                                  (struct sockaddr *)&in_addr, &in_len);

            if (recvn < 0) {
              if (errno != EAGAIN && errno != EWOULDBLOCK) {
                auto errorObj = Object(runtime);
                errorObj.setProperty(runtime, "type",
                                     String::createFromAscii(runtime, "error"));
                errorObj.setProperty(runtime, "error",
                                     String::createFromAscii(
                                         runtime, error_name(errno).c_str()));
                reject.call(runtime, std::move(errorObj));
              } else {
                // No data available, resolve with undefined
                resolve.call(runtime, Value::undefined());
              }
            } else {
              auto eventObj = Object(runtime);
              eventObj.setProperty(runtime, "type",
                                   String::createFromAscii(runtime, "message"));

              auto ArrayBuffer = runtime.global().getPropertyAsFunction(
                  runtime, "ArrayBuffer");
              auto arrayBufferObj =
                  ArrayBuffer
                      .callAsConstructor(runtime, static_cast<int>(recvn))
                      .getObject(runtime);
              auto arrayBuffer = arrayBufferObj.getArrayBuffer(runtime);
              memcpy(arrayBuffer.data(runtime), receiveBuffer, recvn);

              eventObj.setProperty(runtime, "data", std::move(arrayBuffer));
              eventObj.setProperty(
                  runtime, "family",
                  String::createFromAscii(runtime, in_addr.sin_family == AF_INET
                                                       ? "IPv4"
                                                       : "IPv6"));
              eventObj.setProperty(runtime, "address",
                                   String::createFromAscii(
                                       runtime, inet_ntoa(in_addr.sin_addr)));
              eventObj.setProperty(runtime, "port",
                                   static_cast<int>(ntohs(in_addr.sin_port)));

              resolve.call(runtime, std::move(eventObj));
            }

            return Value::undefined();
          }));

  return promise;
}

void UdpManager::suspendAll() {
  if (!suspendedSockets.empty()) {
    LOGW("suspendAll called when sockets are already suspended");
    return;
  }

  for (const auto &[id, fd] : idToFdMap) {
    try {
      SocketState state;
      state.id = id;

      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);
      if (getsockname(fd, (struct sockaddr *)&addr, &len) == 0) {
        state.address = inet_ntoa(addr.sin_addr);
        state.port = ntohs(addr.sin_port);
        state.type = addr.sin_family == AF_INET ? 4 : 6;

        int value;
        socklen_t optlen = sizeof(value);
        if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, &optlen) == 0) {
          state.reuseAddr = value;
        }
        if (getsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &value, &optlen) == 0) {
          state.reusePort = value;
        }
        if (getsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, &optlen) == 0) {
          state.broadcast = value;
        }

        suspendedSockets.push_back(state);
        LOGI("Suspending socket id=%d fd=%d addr=%s port=%d", id, fd,
             state.address.c_str(), state.port);
      } else {
        LOGW("Failed to get socket info for id=%d fd=%d", id, fd);
      }

      ::close(fd);
    } catch (const std::exception &e) {
      LOGW("Error suspending socket id=%d: %s", id, e.what());
    }
  }

  idToFdMap.clear();

  LOGI("Suspended %zu sockets", suspendedSockets.size());
}

void UdpManager::resumeAll() {
  if (suspendedSockets.empty()) {
    LOGW("resumeAll called but no sockets were suspended");
    return;
  }

  LOGI("Resuming %zu sockets", suspendedSockets.size());

  for (const auto &state : suspendedSockets) {
    try {
      auto newFd = socket(state.type == 4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
      if (newFd <= 0) {
        LOGE("Failed to create socket for id=%d: %s", state.id,
             error_name(errno).c_str());
        continue;
      }

      // Set socket timeout
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = RECEIVE_TIMEUS;
      setsockopt(newFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      if (state.reuseAddr) {
        int value = 1;
        setsockopt(newFd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
      }
      if (state.reusePort) {
        int value = 1;
        setsockopt(newFd, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
      }
      if (state.broadcast) {
        int value = 1;
        setsockopt(newFd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));
      }

      bool bindSuccess = false;

      if (state.type == 4) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(state.port);
        inet_pton(AF_INET, state.address.c_str(), &(addr.sin_addr));

        if (setupIface(newFd, addr) == 0 &&
            ::bind(newFd, reinterpret_cast<struct sockaddr *>(&addr),
                   sizeof(addr)) == 0) {
          bindSuccess = true;
        } else {
          LOGW("Failed to bind IPv4 socket id=%d: %s", state.id,
               error_name(errno).c_str());
        }
      } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(state.port);
        inet_pton(AF_INET6, state.address.c_str(), &(addr.sin6_addr));

        if (setupIface(newFd, addr) == 0 &&
            ::bind(newFd, reinterpret_cast<struct sockaddr *>(&addr),
                   sizeof(addr)) == 0) {
          bindSuccess = true;
        } else {
          LOGW("Failed to bind IPv6 socket id=%d: %s", state.id,
               error_name(errno).c_str());
        }
      }

      if (bindSuccess) {
        LOGI("Successfully resumed socket id=%d fd=%d", state.id, newFd);
        idToFdMap[state.id] = newFd;
      } else {
        ::close(newFd);
        LOGE("Failed to resume socket id=%d: %s", state.id,
             error_name(errno).c_str());
      }
    } catch (const std::exception &e) {
      LOGW("Error resuming socket id=%d: %s", state.id, e.what());
    }
  }

  suspendedSockets.clear();
}

} // namespace jsiudp
