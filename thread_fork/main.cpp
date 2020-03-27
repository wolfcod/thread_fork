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

extern "C" uintptr_t __security_cookie;

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
	SIZE_T endAddress = (SIZE_T)buffer.BaseAddress + buffer.RegionSize;
	SIZE_T transfer = endAddress - (SIZE_T)dwEBP;
	SIZE_T topStack = parent->ChildTopESP;


	/** change return address of this function */
#ifndef _WIN64
	__asm {
		mov ecx, ebp
		add ecx, 0x0c			// three arguments
		sub ecx, esp			// number of bytes to move (from ESP to EBP+08)

		mov edi, [topStack]	// my topStack
		sub edi, [transfer]	// topStack - transfer

		sub edi, ecx
		mov eax, ecx
		mov ebx, ebp

		mov esi, esp
		rep movsb			// transfer bytes from stack
		sub edi, eax
		mov esp, edi
		mov ebp, edi
		add ebp, eax
		sub ebp, 0x0c		// sub ebp { ebp, ret, arg }

		mov	ecx, [transfer]
		lea edi, [ebp + 0x0c]
		mov esi, [dwEBP]
		shr ecx, 2
		rep movsd			// copy parent thread stack in our space!

	}
#else

#endif

	parent->bUnlocked = TRUE;

	ResumeThread(hParent);

	/* fix in our stack the TOP LIST */
#ifndef _WIN64
	__asm {
		mov edi, [topStack]
		mov esi, ebp

stack_frame:
		cmp esi, edi
		// jb stack_frame	// there are more stack frame!
	}
#else
#endif

#ifndef _WIN64
	__asm {
		mov esp, ebp
		pop ebp
		mov eax, [esp]
		add esp, 8
		push eax
		mov eax, 1
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
		mov [w.ParentEBP], esp
	}
#else
#endif

	w.dwParentThreadId = GetCurrentThreadId();
	w.dwTag = 'THF\0';
	w.bUnlocked = FALSE;
	
	OutputDebugStringA("Breakpoint before fork...()");
	DebugBreak();
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
		ExitThread(0);
	}

	Sleep(10000);
	return 0;
}