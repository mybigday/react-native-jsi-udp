#import "JsiUdp.h"

#import <React/RCTBridgeModule.h>
#import <React/RCTBridge+Private.h>
#import <ReactCommon/RCTTurboModule.h>
#import <React/RCTEventEmitter.h>
#import <React/RCTUtils.h>

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

// Renamed to avoid duplicate symbol with react-native-worklets-core
static void installJsiUdpApi(facebook::jsi::Runtime *runtime) {
  _manager = std::make_shared<jsiudp::UdpManager>(runtime);
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  RCTBridge *bridge = _bridge ?: [RCTBridge currentBridge];
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)bridge;
  if (cxxBridge.runtime != nullptr) {
    installJsiUdpApi((facebook::jsi::Runtime *)cxxBridge.runtime);
    return @(true);
  }
  return @(false);
}

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
