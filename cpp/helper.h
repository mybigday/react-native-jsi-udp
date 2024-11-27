#pragma once

#define JSI_HOST_FUNCTION(NAME)                                                \
  facebook::jsi::Value NAME(                                                   \
      facebook::jsi::Runtime &runtime, const facebook::jsi::Value &thisValue,  \
      const facebook::jsi::Value *arguments, size_t count)

#define EXPOSE_FN(RUNTIME, NAME, ARGC, FUNCTION)                               \
  {                                                                            \
    auto NAME = Function::createFromHostFunction(                              \
        (RUNTIME), facebook::jsi::PropNameID::forAscii((RUNTIME), #NAME),      \
        ARGC, FUNCTION);                                                       \
    (RUNTIME).global().setProperty((RUNTIME), #NAME, std::move(NAME));         \
  }

#define BIND_METHOD(METHOD)                                                    \
  std::bind(&METHOD, this, std::placeholders::_1, std::placeholders::_2,       \
            std::placeholders::_3, std::placeholders::_4)
