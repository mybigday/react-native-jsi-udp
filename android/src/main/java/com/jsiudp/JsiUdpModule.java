package com.jsiudp;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;

@ReactModule(name = JsiUdpModule.NAME)
public class JsiUdpModule extends ReactContextBaseJavaModule {
  public static final String NAME = "JsiUdp";
  private boolean mInstalled = false;

  public JsiUdpModule(ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @Override
  @NonNull
  public String getName() {
    return NAME;
  }

  private static native void nativeInstall(long jsiPtr, CallInvokerHolderImpl jsCallInvokerHolder);

  private static native void nativeReset();

  @Override
  public void invalidate() {
    super.invalidate();
    if (mInstalled) {
      nativeReset();
    }
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install() {
    try {
      System.loadLibrary("jsiudp");

      ReactApplicationContext context = getReactApplicationContext();
      CallInvokerHolderImpl holder = (CallInvokerHolderImpl) context.getCatalystInstance().getJSCallInvokerHolder();
      nativeInstall(context.getJavaScriptContextHolder().get(), holder);
      mInstalled = true;
      return true;
    } catch (Exception exception) {
      return false;
    }
  }
}
