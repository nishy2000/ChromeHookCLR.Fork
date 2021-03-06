#include "stdafx.h"

#include "ChromeHookService.h"
#include "MessageWindow.h"

#include <windows.h>
#include <msclr/lock.h>

#pragma comment(lib, "shell32.lib")

using namespace System;
using namespace ChromeHookCLR;

unsigned int g_ref = 0;
MessageWindow g_messageWindow;

// hook vars
HMODULE g_hookDll;
HHOOK g_hcwpHook;
HHOOK g_hgmHook;
HOOKPROC g_cwpHookProc;
HOOKPROC g_gmHookProc;

void installHook();
void uninstallHook();

/*
 * ChromeHookService
 */

ChromeHookService::ChromeHookService()
{
	pseudoNcLButtonDown = RegisterWindowMessage(WindowNcLButtonDown);
}

IChromeHook^ ChromeHookService::Register(IntPtr hwnd)
{
	msclr::lock l(lock);
	if (!g_ref) { installHook(); }
	g_ref++;
	ChromeHookClient^ client = gcnew ChromeHookClient((HWND) hwnd.ToPointer());
	registeredWindows[(intptr_t) hwnd.ToPointer()] = client;
	return client;
}

void ChromeHookService::Unregister(HWND hwnd)
{
	msclr::lock l(lock);
	g_ref--;
	if (!g_ref) { uninstallHook(); }
	registeredWindows->erase((intptr_t) hwnd);
}

void ChromeHookService::HandleMessage(MessageType type, intptr_t hwnd, intptr_t arg)
{
	msclr::lock l(lock);
	ChromeHookClient^ client = registeredWindows[hwnd];
	if (!client) return;
	switch (type)
	{
	case MessageType::Moved:
		client->OnWindowMoved((short)LOWORD(arg), (short)HIWORD(arg));
		break;
	case MessageType::Size:
		client->OnWindowSizeChanged(LOWORD(arg), HIWORD(arg));
		break;
	case MessageType::State:
		client->OnWindowStateChanged((int)arg);
		break;
	case MessageType::Activated:
		client->OnActivated(LOWORD(arg) != 0);
		break;
	case MessageType::Closed:
		client->OnWindowClosed();
		break;
	}
}

/*
 * ChromeHookClient (implementation of IChromeHook)
 */

ChromeHookClient::~ChromeHookClient()
{
	ChromeHookService::Unregister(handle);
}

void ChromeHookClient::OnWindowMoved(int x, int y)
{
	WindowMoved(this, System::Windows::Point(x, y));
}

void ChromeHookClient::OnWindowSizeChanged(int w, int h)
{
	SizeChanged(this, System::Windows::Size(w, h));
}

void ChromeHookClient::OnWindowStateChanged(int state)
{
	if (prevWindowState == state) return;
	prevWindowState = state;
	StateChanged(this, state);
}

void ChromeHookClient::OnActivated(bool state)
{
	if (state) Activated(this);
	else Deactivated(this);
}

void ChromeHookClient::OnWindowClosed()
{
	Closed(this);
}

/*
 * Hook Installer
 */

void installHook()
{
	// setup message receiver
	g_messageWindow.setCallback(ChromeHookService::callbackPtr);
	g_messageWindow.createWindow();
	// install hook
#ifdef _WIN64
	g_hookDll = LoadLibrary(_T("ChromeHook64.dll"));
#else
	g_hookDll = LoadLibrary(_T("ChromeHook32.dll"));
#endif
	g_cwpHookProc = (HOOKPROC)GetProcAddress(g_hookDll, "CallWndRetProc");
	g_gmHookProc = (HOOKPROC)GetProcAddress(g_hookDll, "GetMsgProc");
	g_hcwpHook = SetWindowsHookEx(WH_CALLWNDPROCRET, g_cwpHookProc, g_hookDll, 0);
	g_hgmHook = SetWindowsHookEx(WH_CALLWNDPROCRET, g_gmHookProc, g_hookDll, 0);
	Sleep(10); // wait 10ms
	PostMessage(HWND_BROADCAST, WM_NULL, 0, 0);
#ifdef _WIN64
	// start 32bit helper process
	::ShellExecute(nullptr, nullptr, _T("ChromeHook.InjectDll32.exe"), nullptr, nullptr, 0);
#endif
}

void uninstallHook()
{
	// unhook installed hook
	UnhookWindowsHookEx(g_hcwpHook);
	UnhookWindowsHookEx(g_hgmHook);
	Sleep(10);
	PostMessage(HWND_BROADCAST, WM_NULL, 0, 0);
	FreeLibrary(g_hookDll);
	g_hookDll = nullptr;
#ifdef _WIN64
	// stop 32bit helper process
	auto hwnd = ::FindWindow(_T("ChromeHook.InjectDLL32.Class"), nullptr);
	SendMessage(hwnd, WM_CLOSE, 0, 0);
#endif
	// cleanup message receiver
	g_messageWindow.close();
}