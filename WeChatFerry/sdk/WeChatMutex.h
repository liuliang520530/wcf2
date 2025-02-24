#ifndef WECHAT_MUTEX_H
#define WECHAT_MUTEX_H

#include <windows.h>

// 关闭微信互斥锁，支持指定 PID 或遍历所有 WeChat.exe（默认遍历）
int CloseWeChatMutex(DWORD targetPid = 0);

#endif // WECHAT_MUTEX_H