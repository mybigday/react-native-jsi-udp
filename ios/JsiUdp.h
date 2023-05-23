#ifdef __cplusplus
#import "react-native-jsi-udp.h"
#endif

#ifdef RCT_NEW_ARCH_ENABLED
#import "RNJsiUdpSpec.h"

@interface JsiUdp : NSObject <NativeJsiUdpSpec>
#else
#import <React/RCTBridgeModule.h>

@interface JsiUdp : NSObject <RCTBridgeModule>
#endif

@end
