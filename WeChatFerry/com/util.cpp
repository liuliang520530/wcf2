﻿#include "Shlwapi.h"
#include "framework.h"
#include <Psapi.h>
#include <codecvt>
#include <locale>
#include <string.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <vector>
#include <wchar.h>

#include "log.hpp"
#include "util.h"

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Psapi.lib")

using namespace std;

wstring String2Wstring(string s)
{
  if (s.empty())
    return wstring();
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
  wstring ws(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &ws[0], size_needed);
  return ws;
}

string Wstring2String(wstring ws)
{
  if (ws.empty())
    return string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), NULL, 0, NULL, NULL);
  string s(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), &s[0], size_needed, NULL, NULL);
  return s;
}

string GB2312ToUtf8(const char *gb2312)
{
  int size_needed = 0;

  size_needed = MultiByteToWideChar(CP_ACP, 0, gb2312, -1, NULL, 0);
  wstring ws(size_needed, 0);
  MultiByteToWideChar(CP_ACP, 0, gb2312, -1, &ws[0], size_needed);

  size_needed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], -1, NULL, 0, NULL, NULL);
  string s(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &ws[0], -1, &s[0], size_needed, NULL, NULL);

  return s;
}

static int GetWeChatPath(wchar_t *path)
{
  int ret = -1;
  HKEY hKey = NULL;
  // HKEY_CURRENT_USER\Software\Tencent\WeChat InstallPath = xx
  if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, L"Software\\Tencent\\WeChat", &hKey))
  {
    ret = GetLastError();
    return ret;
  }

  DWORD Type = REG_SZ;
  DWORD cbData = MAX_PATH * sizeof(WCHAR);
  if (ERROR_SUCCESS != RegQueryValueEx(hKey, L"InstallPath", 0, &Type, (LPBYTE)path, &cbData))
  {
    ret = GetLastError();
    goto __exit;
  }

  if (path != NULL)
  {
    PathAppend(path, WECHAREXE);
  }

__exit:
  if (hKey)
  {
    RegCloseKey(hKey);
  }

  return ERROR_SUCCESS;
}

// 重载函数：通过 PID 获取可执行文件路径
static int GetWeChatPath(DWORD pid, wchar_t *path)
{
  int ret = -1;
  HANDLE hProcess = NULL;

  // 打开进程
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (hProcess == NULL)
  {
    ret = GetLastError();
    return ret;
  }

  // 获取可执行文件路径
  DWORD cbData = MAX_PATH;
  if (GetModuleFileNameExW(hProcess, NULL, path, cbData) == 0)
  {
    ret = GetLastError();
    goto __exit;
  }

  // 检查路径是否有效
  if (path[0] == L'\0')
  {
    ret = ERROR_PATH_NOT_FOUND; // 路径为空
    goto __exit;
  }

  ret = ERROR_SUCCESS; // 成功

__exit:
  if (hProcess)
  {
    CloseHandle(hProcess);
  }
  return ret;
}

static int GetWeChatWinDLLPath(wchar_t *path)
{
  DWORD pid = GetWeChatPid();
  int ret = GetWeChatPath(pid, path);
  if (ret != ERROR_SUCCESS)
  {
    return ret;
  }

  PathRemoveFileSpecW(path);
  PathAppendW(path, WECHATWINDLL);
  if (!PathFileExists(path))
  {
    // 微信从（大约）3.7开始，增加了一层版本目录: [3.7.0.29]
    PathRemoveFileSpec(path);
    _wfinddata_t findData;
    wstring dir = wstring(path) + L"\\[*.*";
    intptr_t handle = _wfindfirst(dir.c_str(), &findData);
    if (handle == -1)
    { // 检查是否成功
      return -1;
    }
    wstring dllPath = wstring(path) + L"\\" + findData.name;
    wcscpy_s(path, MAX_PATH, dllPath.c_str());
    PathAppend(path, WECHATWINDLL);
  }

  return ret;
}

static bool GetFileVersion(const wchar_t *filePath, wchar_t *version)
{
  if (wcslen(filePath) > 0 && PathFileExists(filePath))
  {
    VS_FIXEDFILEINFO *pVerInfo = NULL;
    DWORD dwTemp, dwSize;
    BYTE *pData = NULL;
    UINT uLen;

    dwSize = GetFileVersionInfoSize(filePath, &dwTemp);
    if (dwSize == 0)
    {
      return false;
    }

    pData = new BYTE[dwSize + 1];
    if (pData == NULL)
    {
      return false;
    }

    if (!GetFileVersionInfo(filePath, 0, dwSize, pData))
    {
      delete[] pData;
      return false;
    }

    if (!VerQueryValue(pData, TEXT("\\"), (void **)&pVerInfo, &uLen))
    {
      delete[] pData;
      return false;
    }

    UINT64 verMS = pVerInfo->dwFileVersionMS;
    UINT64 verLS = pVerInfo->dwFileVersionLS;
    UINT64 major = HIWORD(verMS);
    UINT64 minor = LOWORD(verMS);
    UINT64 build = HIWORD(verLS);
    UINT64 revision = LOWORD(verLS);
    delete[] pData;

    StringCbPrintf(version, 0x20, TEXT("%d.%d.%d.%d"), major, minor, build, revision);

    return true;
  }

  return false;
}

int GetWeChatVersion(wchar_t *version)
{
  WCHAR Path[MAX_PATH] = {0};

  int ret = GetWeChatWinDLLPath(Path);
  if (ret != ERROR_SUCCESS)
  {
    return ret;
  }

  ret = GetFileVersion(Path, version);

  return ret;
}

// 函数目标：查找微信进程的 PID，单次遍历，优先返回当前进程的 PID
DWORD GetWeChatPid()
{
  // 初始化找到的微信 PID 为 0
  DWORD foundPid = 0;

  // 获取当前进程的 PID
  DWORD currentPid = GetCurrentProcessId();

  // 创建进程快照
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return 0;

  // 定义 PROCESSENTRY32 结构并初始化大小
  PROCESSENTRY32 pe32 = {sizeof(PROCESSENTRY32)};

  // 获取第一个进程信息，若失败则清理并返回
  if (!Process32First(hSnapshot, &pe32))
  {
    CloseHandle(hSnapshot);
    return 0;
  }

  // 单次遍历：优先检查当前进程，若不是则记录其他微信 PID
  do
  {
    wstring strProcess = pe32.szExeFile;
    if (strProcess == WECHAREXE)
    {
      if (pe32.th32ProcessID == currentPid)
      {
        CloseHandle(hSnapshot); // 找到当前进程，关闭句柄
        return currentPid;      // 优先返回当前进程 PID
      }
      else if (foundPid == 0) // 只保存第一个非当前进程的微信 PID
      {
        foundPid = pe32.th32ProcessID;
      }
    }
  } while (Process32Next(hSnapshot, &pe32));

  // 关闭快照句柄
  CloseHandle(hSnapshot);

  // 返回找到的其他微信 PID，或 0（未找到）
  return foundPid;
}

enum class WindowsArchiture
{
  x32,
  x64
};
static WindowsArchiture GetWindowsArchitecture()
{
#ifdef _WIN64
  return WindowsArchiture::x64;
#else
  return WindowsArchiture::x32;
#endif
}

BOOL IsProcessX64(DWORD pid)
{
  BOOL isWow64 = false;
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid);
  if (!hProcess)
    return false;
  BOOL result = IsWow64Process(hProcess, &isWow64);
  CloseHandle(hProcess);
  if (!result)
    return false;
  if (isWow64)
    return false;
  else if (GetWindowsArchitecture() == WindowsArchiture::x32)
    return false;
  else
    return true;
}

int OpenWeChat(DWORD *pid)
{
  *pid = GetWeChatPid();
  if (*pid)
  {
    return ERROR_SUCCESS;
  }

  int ret = -1;
  STARTUPINFO si = {sizeof(si)};
  WCHAR Path[MAX_PATH] = {0};
  PROCESS_INFORMATION pi = {0};

  ret = GetWeChatPath(Path);
  if (ERROR_SUCCESS != ret)
  {
    return ret;
  }

  if (!CreateProcess(NULL, Path, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
  {
    return GetLastError();
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  *pid = pi.dwProcessId;

  return ERROR_SUCCESS;
}

size_t GetWstringByAddress(UINT64 addr, wchar_t *buffer, UINT64 buffer_size)
{
  size_t strLength = GET_DWORD(addr + 8);
  if (strLength == 0)
  {
    return 0;
  }
  else if (strLength > buffer_size)
  {
    strLength = buffer_size - 1;
  }

  wmemcpy_s(buffer, strLength + 1, GET_WSTRING(addr), strLength + 1);

  return strLength;
}

string GetStringByAddress(UINT64 addr)
{
  size_t strLength = GET_DWORD(addr + 8);
  return Wstring2String(wstring(GET_WSTRING(addr), strLength));
}

string GetStringByStrAddr(UINT64 addr)
{
  size_t strLength = GET_DWORD(addr + 8);
  return strLength ? string(GET_STRING(addr), strLength) : string();
}

string GetStringByWstrAddr(UINT64 addr)
{
  size_t strLength = GET_DWORD(addr + 8);
  return strLength ? Wstring2String(wstring(GET_WSTRING(addr), strLength)) : string();
}

UINT32 GetMemoryIntByAddress(HANDLE hProcess, UINT64 addr)
{
  UINT32 value = 0;

  unsigned char data[4] = {0};
  if (ReadProcessMemory(hProcess, (LPVOID)addr, data, 4, 0))
  {
    value = data[0] & 0xFF;
    value |= ((data[1] << 8) & 0xFF00);
    value |= ((data[2] << 16) & 0xFF0000);
    value |= ((data[3] << 24) & 0xFF000000);
  }

  return value;
}

wstring GetUnicodeInfoByAddress(HANDLE hProcess, UINT64 address)
{
  wstring value = L"";

  UINT64 strAddress = GetMemoryIntByAddress(hProcess, address);
  UINT64 strLen = GetMemoryIntByAddress(hProcess, address + 0x4);
  if (strLen > 500)
    return value;

  wchar_t cValue[500] = {0};
  memset(cValue, 0, sizeof(cValue) / sizeof(wchar_t));
  if (ReadProcessMemory(hProcess, (LPVOID)strAddress, cValue, (strLen + 1) * 2, 0))
  {
    value = wstring(cValue);
  }

  return value;
}

void DbgMsg(const char *zcFormat, ...)
{
  // initialize use of the variable argument array
  va_list vaArgs;
  va_start(vaArgs, zcFormat);

  // reliably acquire the size
  // from a copy of the variable argument array
  // and a functionally reliable call to mock the formatting
  va_list vaArgsCopy;
  va_copy(vaArgsCopy, vaArgs);
  const int iLen = std::vsnprintf(NULL, 0, zcFormat, vaArgsCopy);
  va_end(vaArgsCopy);

  // return a formatted string without risking memory mismanagement
  // and without assuming any compiler or platform specific behavior
  std::vector<char> zc(iLen + 1);
  std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
  va_end(vaArgs);
  std::string strText(zc.data(), iLen);

  OutputDebugStringA(strText.c_str());
}

WxString *NewWxStringFromStr(const string &str) { return NewWxStringFromWstr(String2Wstring(str)); }

WxString *NewWxStringFromWstr(const wstring &ws)
{
  WxString *p = (WxString *)HeapAlloc(GetProcessHeap(), 0, sizeof(WxString));
  wchar_t *pWstring = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (ws.size() + 1) * 2);
  if (p == NULL || pWstring == NULL)
  {
    LOG_ERROR("Out of Memory...");
    return NULL;
  }

  wmemcpy(pWstring, ws.c_str(), ws.size() + 1);
  p->wptr = pWstring;
  p->size = (DWORD)ws.size();
  p->capacity = (DWORD)ws.size();
  p->ptr = 0;
  p->clen = 0;
  return p;
}
