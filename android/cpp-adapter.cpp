#include <jni.h>
#include "react-native-jsi-udp.h"

extern "C"
JNIEXPORT jint JNICALL
Java_com_jsiudp_JsiUdpModule_nativeMultiply(JNIEnv *env, jclass type, jdouble a, jdouble b) {
    return jsiudp::multiply(a, b);
}
