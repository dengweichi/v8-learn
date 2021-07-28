#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"

v8::Platform* g_default_platform = nullptr;

GTEST_API_ int main(int argc, char** argv){
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  g_default_platform = platform.get();
  v8::V8::InitializePlatform(g_default_platform);
  v8::V8::Initialize();
  testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  return result;
}