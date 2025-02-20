#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

  int WxInitSDKDefault(bool debug, int port);
  int WxInitSDKWithPid(DWORD pid, bool debug, int port);
  int WxInitSDKWithPath(const wchar_t *wxPath, bool debug, int port);
  int WxDestroySDK();

#ifdef __cplusplus
}
#endif
