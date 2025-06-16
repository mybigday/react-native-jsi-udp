#include <memory>
#include <jni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvokerHolder.h>
#include "react-native-jsi-udp.h"

std::shared_ptr<jsiudp::UdpManager> manager;

extern "C"
JNIEXPORT void JNICALL
Java_com_jsiudp_JsiUdpModule_nativeInstall(JNIEnv *env, jclass _, jlong jsiPtr, jobject jsCallInvokerHolder) {
  auto runtime { reinterpret_cast<facebook::jsi::Runtime*>(jsiPtr) };
  auto jsCallInvoker {
    facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
      reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(jsCallInvokerHolder)
    }->cthis()->getCallInvoker()
  };

  manager = std::make_shared<jsiudp::UdpManager>(runtime);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_jsiudp_JsiUdpModule_nativeReset(JNIEnv *env, jclass _) {
  manager.reset();
}