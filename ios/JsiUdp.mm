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
    _manager->invalidate();
  }
}

- (void)setBridge:(RCTBridge *)bridge {
  _bridge = bridge;
}

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

void installApi(
  std::shared_ptr<facebook::react::CallInvoker> callInvoker,
  facebook::jsi::Runtime *runtime
) {
  _manager = std::make_shared<jsiudp::UdpManager>(runtime, std::move(callInvoker));
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install)
{
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)_bridge;
  if (cxxBridge.runtime != nullptr) {
    installApi(cxxBridge.jsCallInvoker, (facebook::jsi::Runtime *)cxxBridge.runtime);
    return @(true);
  }
  return @(false);
}

// Don't compile this code when we build for the old architecture.
#ifdef RCT_NEW_ARCH_ENABLED
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)_bridge;
  installApi(cxxBridge.jsCallInvoker, (facebook::jsi::Runtime *)cxxBridge.runtime);

  return std::make_shared<facebook::react::NativeJsiUdpSpecJSI>(params);
}
#endif

@end
