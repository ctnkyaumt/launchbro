// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>

// Define PKEY_AppUserModel_ID inline so we do not need to link propsys.lib.
// {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, pid 5
DEFINE_PROPERTYKEY (PKEY_AppUserModel_ID_local, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 5);

// Context passed through EnumWindows when querying the browser's AppUserModelID.
typedef struct _AUMID_QUERY_CONTEXT
{
	PBROWSER_INFORMATION pbi;
	LPWSTR aumid;      // allocated via CoTaskMemAlloc; freed by caller
} AUMID_QUERY_CONTEXT, *PAUMID_QUERY_CONTEXT;

// EnumWindows callback: find a visible top-level window belonging to the browser
// process and read its System.AppUserModel.ID property via IPropertyStore.
static BOOL CALLBACK _app_query_aumid_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	PAUMID_QUERY_CONTEXT ctx;
	PR_STRING process_path;
	HANDLE hprocess;
	ULONG pid;
	NTSTATUS status;

	ctx = (PAUMID_QUERY_CONTEXT)lparam;

	// already found one
	if (ctx->aumid)
		return FALSE;

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
		if (_r_str_isequal (&ctx->pbi->binary_path->sr, &process_path->sr, TRUE))
		{
			IPropertyStore *pps = NULL;
			HRESULT hr;

			hr = SHGetPropertyStoreForWindow (hwnd, &IID_IPropertyStore, (PVOID_PTR)&pps);

			if (SUCCEEDED (hr) && pps)
			{
				PROPVARIANT pv;

				PropVariantInit (&pv);

				hr = pps->lpVtbl->GetValue (pps, &PKEY_AppUserModel_ID_local, &pv);

				if (SUCCEEDED (hr) && pv.vt == VT_LPWSTR && pv.pwszVal && pv.pwszVal[0])
				{
					// duplicate the string so it survives PropVariantClear
					ctx->aumid = CoTaskMemAlloc ((wcslen (pv.pwszVal) + 1) * sizeof (WCHAR));

					if (ctx->aumid)
						wcscpy_s (ctx->aumid, wcslen (pv.pwszVal) + 1, pv.pwszVal);
				}

				PropVariantClear (&pv);

				pps->lpVtbl->Release (pps);
			}
		}

		_r_obj_dereference (process_path);
	}

	NtClose (hprocess);

	// stop enumerating if we found the AUMID
	return ctx->aumid ? FALSE : TRUE;
}

// Build the full path to the desktop shortcut for this browser instance.
// Returns a PR_STRING that the caller must dereference, or NULL on failure.
PR_STRING _app_get_shortcut_path (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PWSTR desktop_path = NULL;
	PR_STRING link_title = NULL;
	PR_STRING link_path = NULL;
	HRESULT hr;

	hr = SHGetKnownFolderPath (&FOLDERID_Desktop, KF_FLAG_DEFAULT, NULL, &desktop_path);

	if (FAILED (hr) || !desktop_path)
		return NULL;

	if (pbi->instance_id <= 1)
		link_title = _r_format_string (L"%s (%" TEXT (PR_LONG) L"-bit)", _r_obj_getstring (pbi->browser_type), pbi->architecture);
	else
		link_title = _r_format_string (L"%s (%" TEXT (PR_LONG) L"-bit) #%" TEXT (PR_LONG), _r_obj_getstring (pbi->browser_type), pbi->architecture, pbi->instance_id);

	if (!link_title)
	{
		CoTaskMemFree (desktop_path);
		return NULL;
	}

	link_path = _r_format_string (L"%s\\%s.lnk", desktop_path, link_title->buffer);

	CoTaskMemFree (desktop_path);
	_r_obj_dereference (link_title);

	return link_path;
}

// Stamp the given AUMID onto an existing desktop shortcut (.lnk) for this browser
// instance, so that Windows 11's taskbar correctly unifies the pinned shortcut with
// the running browser window.
static BOOLEAN _app_stamp_shortcut_aumid (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ LPCWSTR aumid
)
{
	PR_STRING link_path;
	HRESULT hr_init;
	HRESULT hr;
	IShellLinkW *psl = NULL;
	IPropertyStore *pps = NULL;
	IPersistFile *ppf = NULL;
	BOOLEAN result = FALSE;

	link_path = _app_get_shortcut_path (pbi);

	if (!link_path)
		return FALSE;

	if (!_r_fs_exists (&link_path->sr))
	{
		_r_obj_dereference (link_path);
		return FALSE;
	}

	hr_init = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	hr = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (PVOID_PTR)&psl);

	if (SUCCEEDED (hr) && psl)
	{
		hr = psl->lpVtbl->QueryInterface (psl, &IID_IPersistFile, (PVOID_PTR)&ppf);

		if (SUCCEEDED (hr) && ppf)
		{
			hr = ppf->lpVtbl->Load (ppf, link_path->buffer, STGM_READWRITE);

			if (SUCCEEDED (hr))
			{
				hr = psl->lpVtbl->QueryInterface (psl, &IID_IPropertyStore, (PVOID_PTR)&pps);

				if (SUCCEEDED (hr) && pps)
				{
					PROPVARIANT pv;

					PropVariantInit (&pv);

					pv.vt = VT_LPWSTR;
					pv.pwszVal = (LPWSTR)aumid;

					hr = pps->lpVtbl->SetValue (pps, &PKEY_AppUserModel_ID_local, &pv);

					if (SUCCEEDED (hr))
					{
						pps->lpVtbl->Commit (pps);

						ppf->lpVtbl->Save (ppf, link_path->buffer, TRUE);

						result = TRUE;
					}

					// do NOT call PropVariantClear here - aumid is caller-owned
					pps->lpVtbl->Release (pps);
				}
			}

			ppf->lpVtbl->Release (ppf);
		}

		psl->lpVtbl->Release (psl);
	}

	if (SUCCEEDED (hr_init))
		CoUninitialize ();

	_r_obj_dereference (link_path);

	return result;
}

// After the browser has been launched, wait briefly for it to create its first window,
// then query the window's AppUserModelID and stamp it onto the desktop shortcut.
// This ensures Windows 11's taskbar correctly unifies the pinned shortcut icon with
// the running browser window.
//
// Called from the update thread after _app_openbrowser; a short delay is acceptable
// here because the thread is about to finish or the launcher is about to close.
VOID _app_try_stamp_shortcut_aumid (
	_In_ PBROWSER_INFORMATION pbi
)
{
	AUMID_QUERY_CONTEXT ctx = {0};

	if (!pbi || _r_obj_isstringempty (pbi->binary_path))
		return;

	// poll for the browser's AUMID: check every 500ms, up to 5 seconds
	ctx.pbi = pbi;
	ctx.aumid = NULL;

	for (INT attempt = 0; attempt < 10; attempt++)
	{
		Sleep (500);

		EnumWindows (&_app_query_aumid_callback, (LPARAM)&ctx);

		if (ctx.aumid)
			break;
	}

	if (ctx.aumid)
	{
		_app_stamp_shortcut_aumid (pbi, ctx.aumid);

		CoTaskMemFree (ctx.aumid);
	}
}
