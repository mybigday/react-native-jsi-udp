#import "JsiUdp.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTBridge.h>
#import <React/RCTUtils.h>
#import <ReactCommon/RCTTurboModule.h>

@implementation JsiUdp

@synthesize bridge = _bridge;

RCT_EXPORT_MODULE()

std::shared_ptr<jsiudp::UdpManager> _manager;

- (void)invalidate {
  if (_manager) {
    _manager->closeAll();
  }
}

- (void)setBridge:(RCTBridge *)bridge {
  _bridge = bridge;
}

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

void installApi(facebook::jsi::Runtime *runtime) {
  _manager = std::make_shared<jsiudp::UdpManager>(runtime);
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)_bridge;
  if (cxxBridge.runtime != nullptr) {
    installApi((facebook::jsi::Runtime *)cxxBridge.runtime);
    return @(true);
  }
  return @(false);
}

// Don't compile this code when we build for the old architecture.
#ifdef RCT_NEW_ARCH_ENABLED
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)_bridge;
  installApi((facebook::jsi::Runtime *)cxxBridge.runtime);

  return std::make_shared<facebook::react::NativeJsiUdpSpecJSI>(params);
}
#endif

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (instancetype)init {
  if (self = [super init]) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleAppStateChange:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleAppStateChange:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)handleAppStateChange:(NSNotification *)notification {
  if ([notification.name
          isEqualToString:UIApplicationWillResignActiveNotification]) {
    if (_manager) {
      _manager->suspendAll();
    }
  } else if ([notification.name
                 isEqualToString:UIApplicationDidBecomeActiveNotification]) {
    if (_manager) {
      _manager->resumeAll();
    }
  }
}

@end
