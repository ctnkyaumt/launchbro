// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include <shlobj.h>
#include <shobjidl.h>

// The shortcut itself is created here without an explicit PKEY_AppUserModel_ID.
// On Windows 10, the implicit path-based default was enough for taskbar unification.
// On Windows 11, the taskbar is stricter: the shortcut's AUMID must exactly match
// the running process's explicit AUMID. To handle this, _app_try_stamp_shortcut_aumid
// (in shortcut_aumid.c) runs after the browser launches, queries the browser window's
// AUMID via SHGetPropertyStoreForWindow, and stamps it onto this shortcut file.

PR_STRING _app_get_shortcut_path (
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_create_profileshortcut (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PR_STRING link_path = NULL;
	HRESULT hr_init;
	HRESULT hr;
	IShellLinkW *psl = NULL;
	IPersistFile *ppf = NULL;

	if (!pbi || _r_obj_isstringempty (pbi->binary_path) || _r_obj_isstringempty (pbi->profile_dir))
		return;

	if (!_r_fs_exists (&pbi->binary_path->sr))
		return;

	if (!_r_fs_exists (&pbi->profile_dir->sr))
	{
		PR_STRING base_dir;

		base_dir = _r_path_getbasedirectory (&pbi->profile_dir->sr);

		if (base_dir)
		{
			if (!_r_fs_exists (&base_dir->sr))
				_r_fs_createdirectory (&base_dir->sr);

			_r_obj_dereference (base_dir);
		}

		_r_fs_createdirectory (&pbi->profile_dir->sr);
	}

	link_path = _app_get_shortcut_path (pbi);

	if (!link_path)
		return;

	hr_init = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	hr = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (PVOID_PTR)&psl);

	if (SUCCEEDED (hr) && psl)
	{
		PR_STRING link_title;

		if (pbi->instance_id <= 1)
			link_title = _r_format_string (L"%s (%" TEXT (PR_LONG) L"-bit)", _r_obj_getstring (pbi->browser_type), pbi->architecture);
		else
			link_title = _r_format_string (L"%s (%" TEXT (PR_LONG) L"-bit) #%" TEXT (PR_LONG), _r_obj_getstring (pbi->browser_type), pbi->architecture, pbi->instance_id);

		psl->lpVtbl->SetPath (psl, pbi->binary_path->buffer);

		if (!_r_obj_isstringempty (pbi->binary_dir))
			psl->lpVtbl->SetWorkingDirectory (psl, pbi->binary_dir->buffer);

		if (!_r_obj_isstringempty (pbi->args_str))
			psl->lpVtbl->SetArguments (psl, pbi->args_str->buffer);

		psl->lpVtbl->SetIconLocation (psl, pbi->binary_path->buffer, 0);

		if (link_title)
		{
			psl->lpVtbl->SetDescription (psl, link_title->buffer);
			_r_obj_dereference (link_title);
		}

		hr = psl->lpVtbl->QueryInterface (psl, &IID_IPersistFile, (PVOID_PTR)&ppf);

		if (SUCCEEDED (hr) && ppf)
		{
			ppf->lpVtbl->Save (ppf, link_path->buffer, TRUE);
			ppf->lpVtbl->Release (ppf);
		}

		psl->lpVtbl->Release (psl);
	}

	if (SUCCEEDED (hr_init))
		CoUninitialize ();

	_r_obj_dereference (link_path);
}

