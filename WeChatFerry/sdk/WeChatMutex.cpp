#include "WeChatMutex.h"
#include <tlhelp32.h>
#include <vector>

typedef LONG NTSTATUS;
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004

typedef NTSTATUS(WINAPI *PFN_ZWQUERYSYSTEMINFORMATION)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(WINAPI *PFN_NTQUERYOBJECT)(HANDLE, ULONG, PVOID, ULONG, PULONG);

// 64 位结构（20 字节）
#pragma pack(push, 1)
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
  USHORT UniqueProcessId;       // 2 bytes
  USHORT CreatorBackTraceIndex; // 2 bytes
  UCHAR ObjectTypeIndex;        // 1 byte
  UCHAR HandleAttributes;       // 1 byte
  USHORT HandleValue;           // 2 bytes
  PVOID Object;                 // 8 bytes (64-bit)
  ULONG GrantedAccess;          // 4 bytes
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION1
{
  ULONG NumberOfHandles;                     // 4 bytes
  SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1]; // 变长数组
} SYSTEM_HANDLE_INFORMATION1, *PSYSTEM_HANDLE_INFORMATION1;
#pragma pack(pop)

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

// 启用 SeDebugPrivilege
static BOOL EnableDebugPrivilege()
{
  HANDLE hToken;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
  {
    MessageBox(NULL, L"OpenProcessToken failed", L"Error", 0);
    return FALSE;
  }

  TOKEN_PRIVILEGES tp;
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
  {
    MessageBox(NULL, L"LookupPrivilegeValue failed", L"Error", 0);
    CloseHandle(hToken);
    return FALSE;
  }

  if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
  {
    MessageBox(NULL, L"AdjustTokenPrivileges failed", L"Error", 0);
    CloseHandle(hToken);
    return FALSE;
  }

  CloseHandle(hToken);
  MessageBox(NULL, L"SeDebugPrivilege enabled", L"Info", 0);
  return TRUE;
}

static std::vector<DWORD> GetWeChatPids()
{
  std::vector<DWORD> pids;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
  {
    MessageBox(NULL, L"CreateToolhelp32Snapshot failed", L"Error", 0);
    return pids;
  }

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
  if (pids.empty())
  {
    MessageBox(NULL, L"No WeChat.exe found", L"Info", 0);
  }
  else
  {
    wchar_t msg[128];
    swprintf_s(msg, 128, L"Found %zu WeChat PIDs", pids.size());
    MessageBox(NULL, msg, L"Info", 0);
    for (DWORD pid : pids)
    {
      swprintf_s(msg, 128, L"WeChat PID: %lu", pid);
      MessageBox(NULL, msg, L"Debug", 0);
    }
  }
  return pids;
}

int CloseWeChatMutex(DWORD targetPid)
{
  if (!EnableDebugPrivilege())
  {
    MessageBox(NULL, L"Failed to enable SeDebugPrivilege", L"Error", 0);
    return 0;
  }

  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
  if (!hNtDll)
  {
    MessageBox(NULL, L"GetModuleHandle failed", L"Error", 0);
    return 0;
  }

  PFN_ZWQUERYSYSTEMINFORMATION ZwQuerySystemInformation =
      (PFN_ZWQUERYSYSTEMINFORMATION)GetProcAddress(hNtDll, "ZwQuerySystemInformation");
  PFN_NTQUERYOBJECT NtQueryObject =
      (PFN_NTQUERYOBJECT)GetProcAddress(hNtDll, "NtQueryObject");
  if (!ZwQuerySystemInformation || !NtQueryObject)
  {
    MessageBox(NULL, L"GetProcAddress failed", L"Error", 0);
    return 0;
  }

  std::vector<DWORD> pids;
  if (targetPid != 0)
  {
    pids.push_back(targetPid);
    wchar_t msg[64];
    swprintf_s(msg, 64, L"Targeting PID: %lu", targetPid);
    MessageBox(NULL, msg, L"Info", 0);
  }
  else
  {
    pids = GetWeChatPids();
    if (pids.empty())
    {
      MessageBox(NULL, L"No WeChat processes found", L"Error", 0);
      return 0;
    }
  }

  ULONG bufferSize = 4096;
  PVOID buffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
  if (!buffer)
  {
    MessageBox(NULL, L"VirtualAlloc failed (initial)", L"Error", 0);
    return 0;
  }

  ULONG returnLength = 0;
  NTSTATUS status = ZwQuerySystemInformation(16, buffer, bufferSize, &returnLength);
  VirtualFree(buffer, 0, MEM_RELEASE);

  if (status != STATUS_INFO_LENGTH_MISMATCH || returnLength * 2 > 67108864)
  {
    wchar_t msg[64];
    swprintf_s(msg, 64, L"Initial ZwQuery failed, status: %ld", status);
    MessageBox(NULL, msg, L"Error", 0);
    return 0;
  }

  bufferSize = returnLength * 2;
  buffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
  if (!buffer)
  {
    MessageBox(NULL, L"VirtualAlloc failed (second)", L"Error", 0);
    return 0;
  }

  status = ZwQuerySystemInformation(16, buffer, bufferSize, NULL);
  if (status < 0)
  {
    VirtualFree(buffer, 0, MEM_RELEASE);
    wchar_t msg[64];
    swprintf_s(msg, 64, L"Second ZwQuery failed, status: %ld", status);
    MessageBox(NULL, msg, L"Error", 0);
    return 0;
  }

  PSYSTEM_HANDLE_INFORMATION1 handleInfo = (PSYSTEM_HANDLE_INFORMATION1)buffer;
  ULONG handleCount = handleInfo->NumberOfHandles;
  if (handleCount == 0 || handleCount > (bufferSize - sizeof(ULONG)) / sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO))
  {
    VirtualFree(buffer, 0, MEM_RELEASE);
    MessageBox(NULL, L"Invalid handle count", L"Error", 0);
    return 0;
  }
  wchar_t countMsg[64];
  swprintf_s(countMsg, 64, L"Handle count: %lu", handleCount);
  MessageBox(NULL, countMsg, L"Info", 0);

  BYTE *pBuffer = (BYTE *)buffer + sizeof(ULONG);
  SYSTEM_HANDLE_TABLE_ENTRY_INFO handleTemp;
  for (ULONG i = 0; i < min(5, handleCount); i++)
  {
    memcpy(&handleTemp, pBuffer + i * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO), sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO));
    wchar_t msg[64];
    swprintf_s(msg, 64, L"Sample PID[%lu]: %u", i, handleTemp.UniqueProcessId);
    MessageBox(NULL, msg, L"Debug", 0);
  }

  ULONG weChatHandleCount = 0;
  for (ULONG i = 0; i < handleCount; i++)
  {
    memcpy(&handleTemp, pBuffer + i * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO), sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO));
    wchar_t pidCheck[64];
    swprintf_s(pidCheck, 64, L"PID at %lu: %u", i, handleTemp.UniqueProcessId);
    if (i < 10)
      MessageBox(NULL, pidCheck, L"Debug All", 0); // 检查前 10 个 PID
    for (DWORD pid : pids)
    {
      if (handleTemp.UniqueProcessId == pid)
      {
        weChatHandleCount++;
        wchar_t pidMsg[64];
        swprintf_s(pidMsg, 64, L"Checking PID: %u, Handle: %u", handleTemp.UniqueProcessId, handleTemp.HandleValue);
        MessageBox(NULL, pidMsg, L"Debug", 0);

        HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (!hProcess)
        {
          wchar_t msg[64];
          swprintf_s(msg, 64, L"OpenProcess failed for PID: %lu, Error: %lu", pid, GetLastError());
          MessageBox(NULL, msg, L"Error", 0);
          continue;
        }

        HANDLE hHandle = NULL;
        if (!DuplicateHandle(hProcess, (HANDLE)handleTemp.HandleValue,
                             GetCurrentProcess(), &hHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
          wchar_t msg[64];
          swprintf_s(msg, 64, L"DuplicateHandle failed for PID: %lu, Error: %lu", pid, GetLastError());
          MessageBox(NULL, msg, L"Error", 0);
          CloseHandle(hProcess);
          continue;
        }

        BYTE typeInfo[128] = {0};
        status = NtQueryObject(hHandle, 1, typeInfo, sizeof(typeInfo), NULL);
        if (status < 0)
        {
          wchar_t msg[64];
          swprintf_s(msg, 64, L"NtQueryObject (type) failed, status: %ld", status);
          MessageBox(NULL, msg, L"Error", 0);
          CloseHandle(hHandle);
          CloseHandle(hProcess);
          continue;
        }

        POBJECT_TYPE_INFORMATION objType = (POBJECT_TYPE_INFORMATION)typeInfo;
        wchar_t typeMsg[128];
        swprintf_s(typeMsg, 128, L"Object type: %s", objType->Name.Buffer ? objType->Name.Buffer : L"Unknown");
        MessageBox(NULL, typeMsg, L"Debug", 0);

        BYTE nameInfo[512] = {0};
        status = NtQueryObject(hHandle, 0, nameInfo, sizeof(nameInfo), NULL);
        if (status >= 0)
        {
          POBJECT_NAME_INFORMATION objName = (POBJECT_NAME_INFORMATION)nameInfo;
          if (objName->Name.Buffer)
          {
            wchar_t msg[512];
            swprintf_s(msg, 512, L"Found object name: %s", objName->Name.Buffer);
            MessageBox(NULL, msg, L"Debug", 0);
          }
          if (wcsstr(objName->Name.Buffer, L"_WeChat_App_Instance_Identity_Mutex_Name"))
          {
            CloseHandle(hHandle);
            if (DuplicateHandle(hProcess, (HANDLE)handleTemp.HandleValue,
                                GetCurrentProcess(), &hHandle, 0, FALSE, DUPLICATE_CLOSE_SOURCE))
            {
              CloseHandle(hHandle);
              CloseHandle(hProcess);
              VirtualFree(buffer, 0, MEM_RELEASE);
              MessageBox(NULL, L"Mutex closed successfully", L"Success", 0);
              return 1;
            }
            else
            {
              MessageBox(NULL, L"Failed to close mutex", L"Error", 0);
            }
          }
        }

        CloseHandle(hHandle);
        CloseHandle(hProcess);
      }
    }
  }

  VirtualFree(buffer, 0, MEM_RELEASE);
  if (weChatHandleCount == 0)
  {
    wchar_t msg[64];
    swprintf_s(msg, 64, L"No handles found for WeChat PID: %lu", pids[0]);
    MessageBox(NULL, msg, L"Error", 0);
  }
  else
  {
    wchar_t msg[64];
    swprintf_s(msg, 64, L"Found %lu handles for WeChat PID", weChatHandleCount);
    MessageBox(NULL, msg, L"Info", 0);
  }
  MessageBox(NULL, L"Mutex not found or failed to close", L"Error", 0);
  return 0;
}