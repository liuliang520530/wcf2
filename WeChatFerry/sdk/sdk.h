#pragma once

int WxInitSDK(bool debug, int port);
int WxInitSDK(DWORD pid, bool debug, int port);
int WxInitSDK(const wchar_t *wxPath, bool debug, int port);
int WxDestroySDK();
