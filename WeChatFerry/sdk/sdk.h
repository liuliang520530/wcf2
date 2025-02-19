#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

  __declspec(dllexport) int WxInitSDKDefault(bool debug, int port);
  __declspec(dllexport) int WxInitSDKWithPid(DWORD pid, bool debug, int port);
  __declspec(dllexport) int WxInitSDKWithPath(const wchar_t *wxPath, bool debug, int port);
  __declspec(dllexport) int WxDestroySDK();

#ifdef __cplusplus
}
#endif
