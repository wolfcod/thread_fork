#include <windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

typedef struct
{
	DWORD dwTag;
	LPVOID lpParam;
	DWORD dwParentThreadId;
	
	DWORD ChildTopESP;
	DWORD ChildLowESP;

	DWORD ParentEBP;

	operator LPVOID() const {
		return (LPVOID) this;
	}

	BOOL bUnlocked;
} WORKER_PARAM;

int thread_fork_child()
{
	return 1;
}

DWORD WINAPI JoinThread(LPVOID lpParam)
{
	WORKER_PARAM *parent = (WORKER_PARAM *)lpParam;

	HANDLE hParent = OpenThread(THREAD_ALL_ACCESS, FALSE, parent->dwParentThreadId);

	SuspendThread(hParent);

	CONTEXT context;
	ZeroMemory(&context, sizeof(CONTEXT));

	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hParent, &context);

	LPDWORD lpStackParent = (LPDWORD)context.Esp;

	while (*lpStackParent != 'THF\0')
		lpStackParent++;

	DWORD dwEBP = parent->ParentEBP;

	DebugBreak();
	MEMORY_BASIC_INFORMATION buffer = {};
	VirtualQuery((LPCVOID)dwEBP, &buffer, sizeof(MEMORY_BASIC_INFORMATION));

	SIZE_T unused = dwEBP - (DWORD)buffer.BaseAddress;
	SIZE_T stackToCopy = buffer.RegionSize - unused;

	/** change return address of this function */
#ifndef _WIN64
	__asm {
		mov eax, [dwEBP]
		mov eax, [eax+4]
		mov [ebp+4], eax
	}	 
#else
#endif


	parent->bUnlocked = TRUE;

/*#ifndef _WIN64
	__asm {
		mov esi, esp
		mov eax, stackToCopy
		mov edi, esi
		sub edi, eax
		mov ebx, edi
		mov ecx, ebp
		add ecx, 8
		sub ecx, esi
		sub ebp, ecx
		rep movsb
		mov esp, ebx
	}
#else
#endif*/

	//ResumeThread(hParent);

#ifndef _WIN64
	__asm {
		mov esp, ebp
		pop ebp
		mov eax, [esp]
		add esp, 8
		push eax
		ret
	}
#else
	return 1;
#endif
}

int thread_fork()
{
	WORKER_PARAM w;
	DWORD dwThreadId;

#ifndef _WIN64
	__asm {
		mov [w.ParentEBP], ebp
	}
#else
#endif

	w.dwParentThreadId = GetCurrentThreadId();
	w.dwTag = 'THF\0';
	w.bUnlocked = FALSE;
	
	HANDLE hThread = CreateThread(NULL, 0, JoinThread, &w, CREATE_SUSPENDED, &dwThreadId);

	CONTEXT context;
	ZeroMemory(&context, sizeof(CONTEXT));

	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &context);


	MEMORY_BASIC_INFORMATION buffer = {};
	VirtualQuery((LPCVOID)context.Esp, &buffer, sizeof(MEMORY_BASIC_INFORMATION));

	w.ChildTopESP = context.Esp;
	w.ChildLowESP = (DWORD) buffer.BaseAddress;

	ResumeThread(hThread);
	while (w.bUnlocked == FALSE) {
		OutputDebugStringA("Locked....");
		Sleep(100);	// wait...
	}

	return 0;
}

/** WinMain */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int i = thread_fork();

	if (i == 0) {
		OutputDebugStringA("I'm in main thread!");
	}
	else {
		OutputDebugStringA("I'm in child thread!");
	}

	Sleep(10000);
	return 0;
}