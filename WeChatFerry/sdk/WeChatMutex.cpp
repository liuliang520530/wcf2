#include "WeChatMutex.h"
#include <tlhelp32.h>
#include <vector>

typedef LONG NTSTATUS;
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004

typedef NTSTATUS(WINAPI *PFN_ZWQUERYSYSTEMINFORMATION)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(WINAPI *PFN_NTQUERYOBJECT)(HANDLE, ULONG, PVOID, ULONG, PULONG);

typedef struct _SYSTEM_HANDLE_INFORMATION
{
  ULONG ProcessId;
  UCHAR ObjectTypeNumber;
  UCHAR Flags;
  USHORT Handle;
  PVOID Object;
  ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _UNICODE_STRING
{
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING;

typedef struct _OBJECT_TYPE_INFORMATION
{
  UNICODE_STRING Name;
  ULONG TotalNumberOfObjects;
  ULONG TotalNumberOfHandles;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

typedef struct _OBJECT_NAME_INFORMATION
{
  UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

// 获取所有 WeChat.exe 的 PID
static std::vector<DWORD> GetWeChatPids()
{
  std::vector<DWORD> pids;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return pids;

  PROCESSENTRY32W pe32 = {sizeof(pe32)};
  if (Process32FirstW(snapshot, &pe32))
  {
    do
    {
      if (_wcsicmp(pe32.szExeFile, L"WeChat.exe") == 0)
      {
        pids.push_back(pe32.th32ProcessID);
      }
    } while (Process32NextW(snapshot, &pe32));
  }
  CloseHandle(snapshot);
  return pids;
}

// 关闭微信互斥锁，支持指定 PID 或遍历
int CloseWeChatMutex(DWORD targetPid)
{
  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
  if (!hNtDll)
    return 0;

  PFN_ZWQUERYSYSTEMINFORMATION ZwQuerySystemInformation =
      (PFN_ZWQUERYSYSTEMINFORMATION)GetProcAddress(hNtDll, "ZwQuerySystemInformation");
  PFN_NTQUERYOBJECT NtQueryObject =
      (PFN_NTQUERYOBJECT)GetProcAddress(hNtDll, "NtQueryObject");
  if (!ZwQuerySystemInformation || !NtQueryObject)
    return 0;

  // 获取目标 PID 列表
  std::vector<DWORD> pids;
  if (targetPid != 0)
  {
    pids.push_back(targetPid); // 指定 PID
  }
  else
  {
    pids = GetWeChatPids(); // 遍历 WeChat.exe
    if (pids.empty())
    {
      MessageBox(NULL, L"没有找到微信进程", L"没有找到微信进程", 0);
      return 0;
    }
  }

  // 查询系统句柄
  ULONG bufferSize = 4096;
  PVOID buffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
  if (!buffer)
  {
    MessageBox(NULL, L"buffer err", L"buffer err", 0);
    return 0;
  }

  ULONG returnLength = 0;
  NTSTATUS status = ZwQuerySystemInformation(16, buffer, bufferSize, &returnLength);
  VirtualFree(buffer, 0, MEM_RELEASE);

  if (status != STATUS_INFO_LENGTH_MISMATCH || returnLength * 2 > 67108864)
  {
    MessageBox(NULL, L"status err ", L"status err ", 0);
    return 0;
  }

  bufferSize = returnLength * 2;
  buffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
  if (!buffer)
  {
    MessageBox(NULL, L"buffer err2", L"buffer err2", 0);
    return 0;
  }

  status = ZwQuerySystemInformation(16, buffer, bufferSize, NULL);
  if (status < 0)
  {
    VirtualFree(buffer, 0, MEM_RELEASE);
    MessageBox(NULL, L"status err3", L"status err3", 0);
    return 0;
  }

  // 首先确保buffer大小足够
  if (bufferSize < sizeof(ULONG))
  {
    MessageBox(NULL, L"Buffer too small for handle count", L"Buffer too small for handle count", 0);
    return 0;
  }

  // 使用memcpy来安全地复制数据
  ULONG handleCount = 0;
  memcpy(&handleCount, buffer, sizeof(ULONG));

  // 验证句柄数量的合理性
  if (handleCount == 0 || handleCount > (bufferSize - sizeof(ULONG)) / sizeof(SYSTEM_HANDLE_INFORMATION))
  {
    MessageBox(NULL, L"Invalid handle count", L"Invalid handle count", 0);
    return 0;
  }

  SYSTEM_HANDLE_INFORMATION *handleInfo = (SYSTEM_HANDLE_INFORMATION *)((BYTE *)buffer + sizeof(ULONG));

  // 遍历句柄，关闭互斥锁
  for (ULONG i = 0; i < handleCount; i++)
  {
    for (DWORD pid : pids)
    {
      if (handleInfo[i].ProcessId == pid)
      {
        HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (!hProcess)
          continue;

        HANDLE hHandle = NULL;
        if (DuplicateHandle(hProcess, (HANDLE)handleInfo[i].Handle,
                            GetCurrentProcess(), &hHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
          BYTE typeInfo[128] = {0};
          if (NtQueryObject(hHandle, 1, typeInfo, sizeof(typeInfo), NULL) >= 0)
          {
            POBJECT_TYPE_INFORMATION objType = (POBJECT_TYPE_INFORMATION)typeInfo;
            if (_wcsicmp(objType->Name.Buffer, L"Mutant") == 0)
            {
              BYTE nameInfo[512] = {0};
              if (NtQueryObject(hHandle, 0, nameInfo, sizeof(nameInfo), NULL) >= 0)
              {
                POBJECT_NAME_INFORMATION objName = (POBJECT_NAME_INFORMATION)nameInfo;
                if (wcsstr(objName->Name.Buffer, L"_WeChat_App_Instance_Identity_Mutex_Name"))
                {
                  CloseHandle(hHandle);
                  if (DuplicateHandle(hProcess, (HANDLE)handleInfo[i].Handle,
                                      GetCurrentProcess(), &hHandle, 0, FALSE, DUPLICATE_CLOSE_SOURCE))
                  {
                    CloseHandle(hHandle);
                    CloseHandle(hProcess);
                    VirtualFree(buffer, 0, MEM_RELEASE);
                    MessageBox(NULL, L"成功关闭", L"成功关闭", 0);
                    return 1; // 成功关闭
                  }
                }
              }
            }
          }
          CloseHandle(hHandle);
        }
        CloseHandle(hProcess);
      }
    }
  }

  VirtualFree(buffer, 0, MEM_RELEASE);
  MessageBox(NULL, L"未找到或关闭失败", L"未找到或关闭失败", 0);
  return 0; // 未找到或关闭失败
}