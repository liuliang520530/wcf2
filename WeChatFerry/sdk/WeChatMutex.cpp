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
  return pids;
}

int CloseWeChatMutex(DWORD targetPid)
{
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
    wsprintfW(msg, L"Target PID: %lu", targetPid);
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
    MessageBox(NULL, L"ZwQuerySystemInformation failed (initial)", L"Error", 0);
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
    MessageBox(NULL, L"ZwQuerySystemInformation failed (second)", L"Error", 0);
    return 0;
  }

  ULONG handleCount = 0;
  memcpy(&handleCount, buffer, sizeof(ULONG));
  if (handleCount == 0 || handleCount > (bufferSize - sizeof(ULONG)) / sizeof(SYSTEM_HANDLE_INFORMATION))
  {
    VirtualFree(buffer, 0, MEM_RELEASE);
    MessageBox(NULL, L"Invalid handle count", L"Error", 0);
    return 0;
  }

  SYSTEM_HANDLE_INFORMATION *handleInfo = (SYSTEM_HANDLE_INFORMATION *)((BYTE *)buffer + sizeof(ULONG));

  for (ULONG i = 0; i < handleCount; i++)
  {
    for (DWORD pid : pids)
    {
      if (handleInfo[i].ProcessId == pid)
      {
        HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (!hProcess)
        {
          wchar_t msg[64];
          wsprintfW(msg, L"OpenProcess failed for PID: %lu", pid);
          MessageBox(NULL, msg, L"Error", 0);
          continue;
        }

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
                if (objName->Name.Buffer)
                {
                  wchar_t msg[512];
                  wsprintfW(msg, L"Mutex name: %s", objName->Name.Buffer);
                  MessageBox(NULL, msg, L"Debug", 0);
                }
                if (wcsstr(objName->Name.Buffer, L"_WeChat_App_Instance_Identity_Mutex_Name"))
                {
                  CloseHandle(hHandle);
                  if (DuplicateHandle(hProcess, (HANDLE)handleInfo[i].Handle,
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
              else
              {
                MessageBox(NULL, L"NtQueryObject failed (name)", L"Error", 0);
              }
            }
          }
          else
          {
            MessageBox(NULL, L"NtQueryObject failed (type)", L"Error", 0);
          }
          CloseHandle(hHandle);
        }
        else
        {
          MessageBox(NULL, L"DuplicateHandle failed (first)", L"Error", 0);
        }
        CloseHandle(hProcess);
      }
    }
  }

  VirtualFree(buffer, 0, MEM_RELEASE);
  MessageBox(NULL, L"Mutex not found or failed to close", L"Error", 0);
  return 0;
}