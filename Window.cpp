#include <Windows.h>
#include <iostream>
#include "resource.h"
#include "Config.h"

static DWORD RenderThreadID;
static DWORD PhysicsThreadID;
static HANDLE RenderThreadH;
static HANDLE PhysicsThreadH;
LRESULT CALLBACK mainwndproc(HWND, UINT, WPARAM, LPARAM);
DWORD RenderThread(LPVOID OutputWindow);
DWORD PhysicsThread(LPVOID UnusedParam);
void InitPhysics(void);
void EndPhysics(DWORD ThreadID);
void EndRender(DWORD ThreadID);

HANDLE RenderEvent;
HANDLE PhysicsEvent;

int wmain() {
	SetProcessDPIAware();
	WNDCLASSW wcmainwin;
	wcmainwin.hInstance = GetModuleHandleW(NULL);
	wcmainwin.lpszClassName = L"wcmainwin";
	wcmainwin.cbClsExtra = 0;
	wcmainwin.cbWndExtra = 0;
	wcmainwin.style = CS_OWNDC;
	wcmainwin.hCursor = LoadCursorW(NULL,IDC_ARROW);
	wcmainwin.hIcon = LoadIconW(wcmainwin.hInstance, MAKEINTRESOURCE(REDBALL_ICON));
	wcmainwin.lpfnWndProc = mainwndproc;
	wcmainwin.hbrBackground = NULL;
	wcmainwin.lpszMenuName = NULL;
	
	RegisterClassW(&wcmainwin);

	RECT wndrect = { 0,0,INITIALWIDTH,INITIALHEIGHT };
	AdjustWindowRect(&wndrect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hmainwin = CreateWindowExW(NULL
		,L"wcmainwin",L"Red Ball",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wndrect.right - wndrect.left,wndrect.bottom - wndrect.top,
		NULL,NULL,
		wcmainwin.hInstance,NULL);
	/*Test*/{
		RECT clientrect;
		GetClientRect(hmainwin,&clientrect);
		std::cout << clientrect.left << " " << clientrect.top << " " << clientrect.right << " " << clientrect.bottom <<"\n";
	}

//	InitPhysics();
	RenderEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	PhysicsEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	RenderThreadH = CreateThread(NULL, NULL, PhysicsThread, NULL, NULL,&PhysicsThreadID);
	PhysicsThreadH = CreateThread(NULL, NULL, RenderThread, hmainwin, NULL, &RenderThreadID);
	Sleep(500);
	ShowWindow(hmainwin, SW_NORMAL);
	MSG message;
	while (GetMessageW(&message, 0, 0, 0)) {
		DispatchMessageW(&message);
	}

	PostThreadMessageW(RenderThreadID, WM_QUIT, NULL, NULL);
	PostThreadMessageW(PhysicsThreadID, WM_QUIT, NULL, NULL);

	WaitForSingleObject(RenderThreadH, INFINITE);
	WaitForSingleObject(PhysicsThreadH, INFINITE);
	return 0;
}

LRESULT CALLBACK mainwndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY: {
		PostQuitMessage(0);
		break;
	}
	case WM_SIZE: {
		PostThreadMessageW(RenderThreadID, WM_SIZE, 0, lparam);
		PostThreadMessageW(PhysicsThreadID, WM_SIZE, 0, lparam);
		break;
	}
	case WM_LBUTTONDOWN: {
		SetCapture(hwnd);
		PostThreadMessageW(PhysicsThreadID, WM_LBUTTONDOWN, wparam, lparam);
		break;
	}
	case WM_LBUTTONUP: {
		ReleaseCapture();
		PostThreadMessageW(PhysicsThreadID, WM_LBUTTONUP, wparam, lparam);
		break;
	}
	case WM_MOUSEMOVE: {
		PostThreadMessageW(PhysicsThreadID, WM_MOUSEMOVE, wparam, lparam);
		break;
	}
	case WM_KEYDOWN: {
		PostThreadMessageW(PhysicsThreadID, WM_KEYDOWN, wparam, lparam);
		break;
	}
	case WM_MOVE: {
		PostThreadMessageW(PhysicsThreadID, WM_MOVE, wparam, lparam);
		break;
	}
	}
	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

