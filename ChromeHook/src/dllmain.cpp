// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <windows.h>
#include <tchar.h>

#include "../../ChromeHookCLR/src/MessageWindow.h"

HWND g_hMessageWindow = 0;

UINT g_WindowPosChanged;
UINT g_WindowSizeChanged;
UINT g_WindowStateChanged;
UINT g_WindowDestroyed;
UINT g_WindowActivated;

void initialize();

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		initialize();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}


void initialize()
{
	g_WindowPosChanged = RegisterWindowMessage(WindowPosChangedMessage);
	g_WindowSizeChanged = RegisterWindowMessage(WindowSizeChangedMessage);
	g_WindowStateChanged = RegisterWindowMessage(WindowStateChangedMessage);
	g_WindowDestroyed = RegisterWindowMessage(WindowDestroyedMessage);
	g_WindowActivated = RegisterWindowMessage(WindowActivatedMessage);
}

HWND getMessageWindow()
{
	auto hwnd = g_hMessageWindow;
	if (!hwnd) {
		return g_hMessageWindow = FindWindow(MessageWindowClass, nullptr);
	}
	if (!IsWindow(hwnd)) {
		return g_hMessageWindow = FindWindow(MessageWindowClass, nullptr);
	}
	return hwnd;
}

LRESULT CALLBACK CallWndRetProc(int nAction, WPARAM wParam, LPARAM lParam)
{
	if (nAction >= 0)
	{
		auto hwnd = getMessageWindow();
		if (hwnd) {
			auto cwp = reinterpret_cast<CWPRETSTRUCT*>(lParam);
			switch (cwp->message) {
			case WM_MOVE:
				SendNotifyMessage(hwnd, g_WindowPosChanged, reinterpret_cast<WPARAM>(cwp->hwnd), cwp->lParam);
				break;
			case WM_SIZE:
				SendNotifyMessage(hwnd, g_WindowSizeChanged, reinterpret_cast<WPARAM>(cwp->hwnd), cwp->lParam);
				if(cwp->wParam)
				{
					SendNotifyMessage(hwnd, g_WindowStateChanged, reinterpret_cast<WPARAM>(cwp->hwnd), cwp->wParam);
				}
				break;
			case WM_ACTIVATE:
				SendNotifyMessage(hwnd, g_WindowActivated, cwp->wParam, cwp->lParam);
				break;
			case WM_CLOSE:
				SendNotifyMessage(hwnd, g_WindowDestroyed, reinterpret_cast<WPARAM>(cwp->hwnd), 0);
				break;
			}
		}
	}
	return CallNextHookEx(nullptr, nAction, wParam, lParam);
}