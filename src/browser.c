// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include <propsys.h>
#include <propkey.h>

#pragma comment(lib, "Propsys.lib")

// Windows groups a taskbar pin with a running window only when both carry the same
// AppUserModelID. The shortcut we create declares one explicitly (see shortcut.c), but if
// the browser process was ever seen running BEFORE that pin existed (e.g. launched once from
// the raw exe, or from a shortcut created by an older build), Explorer can end up showing the
// pin and the live window as two separate taskbar buttons. Fix it after every launch by
// stamping the same AppUserModelID onto the live window - checking first so we only touch the
// property when it actually differs.
//
// This runs on a plain CreateThread thread rather than the app's own worker queue, and uses
// only raw Win32/COM calls (no _r_* framework helpers) since a manually created thread isn't
// guaranteed to be safe for whatever thread-local setup the framework's own worker threads get.
#define TASKBAR_APPID_FIX_PATH_LENGTH 1024

typedef struct _TASKBAR_APPID_FIX_CONTEXT
{
	WCHAR binary_path[TASKBAR_APPID_FIX_PATH_LENGTH];
	BOOLEAN is_firefox;
} TASKBAR_APPID_FIX_CONTEXT, *PTASKBAR_APPID_FIX_CONTEXT;

typedef struct _FIND_WINDOW_BY_PATH_CONTEXT
{
	LPCWSTR binary_path;
	HWND found_hwnd;
} FIND_WINDOW_BY_PATH_CONTEXT, *PFIND_WINDOW_BY_PATH_CONTEXT;

static BOOL CALLBACK _app_find_window_by_path_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	PFIND_WINDOW_BY_PATH_CONTEXT ctx;
	HANDLE hprocess;
	WCHAR path_buffer[TASKBAR_APPID_FIX_PATH_LENGTH];
	ULONG path_size;
	ULONG pid;

	ctx = (PFIND_WINDOW_BY_PATH_CONTEXT)lparam;

	if (!IsWindowVisible (hwnd) || GetWindow (hwnd, GW_OWNER) != NULL)
		return TRUE;

	GetWindowThreadProcessId (hwnd, &pid);

	if (pid == GetCurrentProcessId ())
		return TRUE;

	hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

	if (!hprocess)
		return TRUE;

	path_size = RTL_NUMBER_OF (path_buffer);

	if (QueryFullProcessImageNameW (hprocess, 0, path_buffer, &path_size))
	{
		if (_wcsicmp (path_buffer, ctx->binary_path) == 0)
		{
			ctx->found_hwnd = hwnd;

			CloseHandle (hprocess);

			return FALSE;
		}
	}

	CloseHandle (hprocess);

	return TRUE;
}

static DWORD WINAPI _app_taskbar_appid_fix_threadproc (
	_In_ LPVOID lparam
)
{
	PTASKBAR_APPID_FIX_CONTEXT ctx;
	LPCWSTR appid;
	HRESULT hr_init;
	HWND target_hwnd = NULL;

	ctx = (PTASKBAR_APPID_FIX_CONTEXT)lparam;
	appid = ctx->is_firefox ? L"Firefox" : L"Chromium";

	hr_init = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// wait for the browser's main window to appear; give up quietly if it never does
	for (INT attempt = 0; attempt < 40 && !target_hwnd; attempt++)
	{
		FIND_WINDOW_BY_PATH_CONTEXT find_ctx = {0};

		find_ctx.binary_path = ctx->binary_path;

		EnumWindows (&_app_find_window_by_path_callback, (LPARAM)&find_ctx);

		target_hwnd = find_ctx.found_hwnd;

		if (!target_hwnd)
			Sleep (250);
	}

	if (target_hwnd)
	{
		IPropertyStore *pps = NULL;
		HRESULT hr;

		hr = SHGetPropertyStoreForWindow (target_hwnd, &IID_IPropertyStore, (PVOID_PTR)&pps);

		if (SUCCEEDED (hr) && pps)
		{
			PROPVARIANT pv_current = {0};
			BOOLEAN needs_update = TRUE;

			if (SUCCEEDED (pps->lpVtbl->GetValue (pps, &PKEY_AppUserModel_ID, &pv_current)))
			{
				if (pv_current.vt == VT_LPWSTR && pv_current.pwszVal && _wcsicmp (pv_current.pwszVal, appid) == 0)
					needs_update = FALSE;

				PropVariantClear (&pv_current);
			}

			if (needs_update)
			{
				PROPVARIANT pv = {0};

				pv.vt = VT_LPWSTR;
				pv.pwszVal = CoTaskMemAlloc ((wcslen (appid) + 1) * sizeof (WCHAR));

				if (pv.pwszVal)
				{
					wcscpy_s (pv.pwszVal, wcslen (appid) + 1, appid);

					if (SUCCEEDED (pps->lpVtbl->SetValue (pps, &PKEY_AppUserModel_ID, &pv)))
						pps->lpVtbl->Commit (pps);

					PropVariantClear (&pv);
				}
			}

			pps->lpVtbl->Release (pps);
		}
	}

	if (SUCCEEDED (hr_init))
		CoUninitialize ();

	HeapFree (GetProcessHeap (), 0, ctx);

	return 0;
}

static VOID _app_fix_taskbar_appid_async (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PTASKBAR_APPID_FIX_CONTEXT ctx;
	HANDLE hthread;
	R_STRINGREF r3dfox_type = PR_STRINGREF_INIT (L"r3dfox");
	R_STRINGREF iceweasel_type = PR_STRINGREF_INIT (L"iceweasel");

	if (_r_obj_isstringempty (pbi->binary_path) || pbi->binary_path->length / sizeof (WCHAR) >= TASKBAR_APPID_FIX_PATH_LENGTH)
		return;

	ctx = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, sizeof (TASKBAR_APPID_FIX_CONTEXT));

	if (!ctx)
		return;

	wcscpy_s (ctx->binary_path, TASKBAR_APPID_FIX_PATH_LENGTH, pbi->binary_path->buffer);

	ctx->is_firefox = pbi->browser_type && (
		_r_str_isequal (&pbi->browser_type->sr, &r3dfox_type, TRUE) ||
		_r_str_isequal (&pbi->browser_type->sr, &iceweasel_type, TRUE));

	hthread = CreateThread (NULL, 0, &_app_taskbar_appid_fix_threadproc, ctx, 0, NULL);

	if (hthread)
		CloseHandle (hthread);
	else
		HeapFree (GetProcessHeap (), 0, ctx);
}

static BOOL CALLBACK _app_activate_browser_window_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	PBROWSER_INFORMATION pbi;
	PR_STRING process_path;
	HANDLE hprocess;
	ULONG pid;
	BOOL is_success = TRUE;
	NTSTATUS status;

	GetWindowThreadProcessId (hwnd, &pid);

	if (HandleToULong (NtCurrentProcessId ()) == pid)
		return TRUE;

	if (!_r_wnd_isvisible (hwnd, FALSE))
		return TRUE;

	status = _r_sys_openprocess ((HANDLE)(ULONG_PTR)pid, PROCESS_QUERY_LIMITED_INFORMATION, &hprocess);

	if (!NT_SUCCESS (status))
		return TRUE;

	status = _r_sys_queryprocessstring (hprocess, ProcessImageFileNameWin32, &process_path);

	if (NT_SUCCESS (status))
	{
		pbi = (PBROWSER_INFORMATION)lparam;

		if (_r_str_isequal (&pbi->binary_path->sr, &process_path->sr, TRUE))
		{
			_r_wnd_toggle (hwnd, TRUE);

			is_success = FALSE;
		}

		_r_obj_dereference (process_path);
	}

	NtClose (hprocess);

	return is_success;
}

VOID _app_openbrowser (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PR_STRING args_string;
	PR_STRING cmdline;
	LPWSTR ptr;
	ULONG_PTR args_length = 0;
	NTSTATUS status;

	if (_r_obj_isstringempty (pbi->binary_path) || !_r_fs_exists (&pbi->binary_path->sr))
	{
		_r_show_errormessage (_r_app_gethwnd (), NULL, STATUS_OBJECT_PATH_NOT_FOUND, _r_obj_getstring (pbi->binary_path), ET_NATIVE);

		return;
	}

	if (_r_fs_isfileused (&pbi->binary_path->sr) && !pbi->is_hasurls && !pbi->is_opennewwindow)
	{
		EnumWindows (&_app_activate_browser_window_callback, (LPARAM)pbi);

		return;
	}

	if (pbi->args_str)
		args_length += pbi->args_str->length;

	if (pbi->is_hasurls && pbi->urls_str)
		args_length += pbi->urls_str->length;

	args_length += sizeof (WCHAR); // for space

	args_string = _r_obj_createstring_ex (NULL, args_length);

	if (pbi->args_str)
		RtlCopyMemory (args_string->buffer, pbi->args_str->buffer, pbi->args_str->length);

	if (pbi->is_hasurls)
	{
		if (pbi->urls_str)
		{
			if (pbi->args_str)
			{
				ptr = PTR_ADD_OFFSET (args_string->buffer, pbi->args_str->length);
				RtlCopyMemory (ptr, L" ", sizeof (WCHAR)); // insert space

				ptr = PTR_ADD_OFFSET (args_string->buffer, pbi->args_str->length + sizeof (WCHAR));
				RtlCopyMemory (ptr, pbi->urls_str->buffer, pbi->urls_str->length);
			}
			else
			{
				RtlCopyMemory (args_string->buffer, pbi->urls_str->buffer, pbi->urls_str->length);
			}

			_r_obj_clearreference ((PVOID_PTR)&pbi->urls_str);
		}

		pbi->is_hasurls = FALSE; // reset
	}

	_r_str_trimtonullterminator (&args_string->sr);

	pbi->is_opennewwindow = FALSE;

	cmdline = _r_format_string (L"\"%s\" -url %s", pbi->binary_path->buffer, args_string->buffer);

	status = _r_sys_createprocess (pbi->binary_path->buffer, cmdline->buffer, pbi->binary_dir->buffer, FALSE);

	if (!NT_SUCCESS (status))
		_r_show_errormessage (_r_app_gethwnd (), NULL, status, pbi->binary_path->buffer, ET_NATIVE);
	else
		_app_fix_taskbar_appid_async (pbi);

	_r_obj_dereference (args_string);
	_r_obj_dereference (cmdline);
}

