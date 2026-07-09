// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "resource.h"
#include <shlobj.h>
#include <shellapi.h>

static PR_STRING _app_get_app_root_dir ()
{
	WCHAR exe_path[4096] = {0};
	WCHAR dir_buf[4096] = {0};
	PWSTR slash_ptr;

	if (!GetModuleFileNameW (NULL, exe_path, RTL_NUMBER_OF (exe_path)))
		return _r_obj_createstring (L".");

	_r_str_copy (dir_buf, RTL_NUMBER_OF (dir_buf), exe_path);

	slash_ptr = wcsrchr (dir_buf, L'\\');
	if (slash_ptr)
		*slash_ptr = 0;

	slash_ptr = wcsrchr (dir_buf, L'\\');
	if (slash_ptr)
	{
		LPCWSTR leaf = slash_ptr + 1;
		if (_r_str_compare (leaf, L"32", 2) == 0 || _r_str_compare (leaf, L"64", 2) == 0)
			*slash_ptr = 0;
	}

	return _r_obj_createstring (dir_buf);
}

// Profiles live under an architecture subfolder (e.g. <app_root>\64\profile), matching
// _app_get_profile_dir() in browser_info.c. _app_get_app_root_dir() strips that subfolder,
// so scanning it directly finds nothing. Resolve the folder that actually holds the
// profile* directories (falling back to the 64-bit root, then legacy flat layout).
static BOOLEAN _app_root_has_profiles (
	_In_ PR_STRING root
)
{
	PR_STRING probe;
	BOOLEAN is_hit = FALSE;

	probe = _r_format_string (L"%s\\profile", root->buffer);

	if (probe)
	{
		is_hit = _r_fs_exists (&probe->sr);
		_r_obj_dereference (probe);
	}

	if (is_hit)
		return TRUE;

	probe = _r_format_string (L"%s\\bin", root->buffer);

	if (probe)
	{
		is_hit = _r_fs_exists (&probe->sr);
		_r_obj_dereference (probe);
	}

	return is_hit;
}

static PR_STRING _app_get_profile_root_dir ()
{
	static const INT arch_list[] = {64, 32};

	PR_STRING app_root;
	PR_STRING candidate;

	app_root = _app_get_app_root_dir ();

	if (!app_root)
		return NULL;

	// prefer an architecture subfolder that already holds profiles or an installed browser
	for (ULONG_PTR a = 0; a < RTL_NUMBER_OF (arch_list); a++)
	{
		candidate = _r_format_string (L"%s\\%d", app_root->buffer, arch_list[a]);

		if (!candidate)
			continue;

		if (_app_root_has_profiles (candidate))
		{
			_r_obj_dereference (app_root);
			return candidate;
		}

		_r_obj_dereference (candidate);
	}

	// legacy flat layout: profiles directly under the app root
	if (_app_root_has_profiles (app_root))
		return app_root;

	// nothing found yet: default to the 64-bit arch root the app would create
	candidate = _r_format_string (L"%s\\64", app_root->buffer);

	_r_obj_dereference (app_root);

	return candidate;
}

static PR_STRING _app_browse_for_folder (
	_In_ HWND hwnd,
	_In_ LPCWSTR title
)
{
	PR_STRING result = NULL;
	BROWSEINFOW bi = {0};
	LPITEMIDLIST pidl;
	WCHAR buffer[MAX_PATH] = {0};

	bi.hwndOwner = hwnd;
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

	pidl = SHBrowseForFolderW (&bi);

	if (pidl)
	{
		if (SHGetPathFromIDListW (pidl, buffer))
		{
			result = _r_obj_createstring (buffer);
		}

		CoTaskMemFree (pidl);
	}

	return result;
}

static PR_STRING _app_get_profile_label (
	_In_ LONG instance_id,
	_In_ PR_STRING profile_dir
)
{
	PR_STRING browser_type = NULL;
	PR_STRING label;

	if (instance_id >= 2 && instance_id <= 4)
	{
		PR_STRING type_key = _r_format_string (L"ChromiumType%" TEXT (PR_LONG), instance_id);
		if (type_key)
		{
			browser_type = _r_config_getstring (type_key->buffer, NULL);
			_r_obj_dereference (type_key);
		}
	}

	if (!browser_type)
		browser_type = _r_config_getstring (L"ChromiumType", L"chromium");

	label = _r_format_string (L"launchbro_profile%" TEXT (PR_LONG) L"_%s", instance_id, browser_type ? browser_type->buffer : L"chromium");

	if (browser_type)
		_r_obj_dereference (browser_type);

	return label;
}

static BOOLEAN _app_copy_directory (
	_In_ PR_STRING src,
	_In_ PR_STRING dst
)
{
	PR_STRING search_path;
	PR_STRING src_file;
	PR_STRING dst_file;
	HANDLE hfind;
	WIN32_FIND_DATAW find_data;
	BOOLEAN is_success = TRUE;

	_r_fs_createdirectory (&dst->sr);

	search_path = _r_format_string (L"%s\\*", src->buffer);

	if (!search_path)
		return FALSE;

	hfind = FindFirstFileW (search_path->buffer, &find_data);

	if (hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (_r_str_compare (find_data.cFileName, L".", 1) == 0 ||
				_r_str_compare (find_data.cFileName, L"..", 2) == 0)
			{
				continue;
			}

			src_file = _r_format_string (L"%s\\%s", src->buffer, find_data.cFileName);
			dst_file = _r_format_string (L"%s\\%s", dst->buffer, find_data.cFileName);

			if (!src_file || !dst_file)
			{
				is_success = FALSE;
				break;
			}

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!_app_copy_directory (src_file, dst_file))
					is_success = FALSE;
			}
			else
			{
				_r_fs_copyfile (&src_file->sr, &dst_file->sr, FALSE);
			}

			_r_obj_dereference (src_file);
			_r_obj_dereference (dst_file);
		}
		while (FindNextFileW (hfind, &find_data));

		FindClose (hfind);
	}

	_r_obj_dereference (search_path);

	return is_success;
}

// Does this folder look like an actual browser profile / user-data dir? Chromium user-data
// dirs carry a "Local State" file and/or a "Default" subfolder; Firefox profiles carry
// prefs.js / times.json. Used so import works no matter what the folder is named.
static BOOLEAN _app_dir_is_profile (
	_In_ LPCWSTR dir
)
{
	static LPCWSTR markers[] = {L"Local State", L"Default", L"prefs.js", L"times.json"};

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (markers); i++)
	{
		PR_STRING probe = _r_format_string (L"%s\\%s", dir, markers[i]);
		BOOLEAN is_hit = FALSE;

		if (probe)
		{
			is_hit = _r_fs_exists (&probe->sr);
			_r_obj_dereference (probe);
		}

		if (is_hit)
			return TRUE;
	}

	return FALSE;
}

// Given the folder the user picked in the import dialog, figure out the real profile folder to
// copy from - whether they picked the profile folder itself (any name), or a parent that holds
// it under a known name (the export format launchbro_profile{N}_{type}, launchbro_profile{N},
// profile{N}, or a plain "profile"), or a parent that holds a single profile-looking subfolder.
static PR_STRING _app_find_import_source (
	_In_ PR_STRING import_dir,
	_In_ LONG instance_id
)
{
	PR_STRING candidate;
	PR_STRING label;
	PR_STRING search_path;
	HANDLE hfind;
	WIN32_FIND_DATAW find_data;

	// 1. the picked folder is itself a profile
	if (_app_dir_is_profile (import_dir->buffer))
		return _r_obj_createstring (import_dir->buffer);

	// 2. known subfolder names, most specific first
	label = _app_get_profile_label (instance_id, NULL);

	if (label)
	{
		candidate = _r_format_string (L"%s\\%s", import_dir->buffer, label->buffer);

		_r_obj_dereference (label);

		if (candidate)
		{
			if (_r_fs_exists (&candidate->sr))
				return candidate;

			_r_obj_dereference (candidate);
		}
	}

	{
		PR_STRING names[3];
		ULONG_PTR count = 0;

		names[count++] = _r_format_string (L"launchbro_profile%" TEXT (PR_LONG), instance_id);

		if (instance_id <= 1)
			names[count++] = _r_obj_createstring (L"profile");
		else
			names[count++] = _r_format_string (L"profile%" TEXT (PR_LONG), instance_id);

		names[count++] = _r_obj_createstring (L"profile");

		for (ULONG_PTR i = 0; i < count; i++)
		{
			candidate = NULL;

			if (names[i])
			{
				candidate = _r_format_string (L"%s\\%s", import_dir->buffer, names[i]->buffer);
				_r_obj_dereference (names[i]);
			}

			if (candidate)
			{
				if (_r_fs_exists (&candidate->sr))
				{
					// free any names we didn't get to
					for (ULONG_PTR j = i + 1; j < count; j++)
					{
						if (names[j])
							_r_obj_dereference (names[j]);
					}

					return candidate;
				}

				_r_obj_dereference (candidate);
			}
		}
	}

	// 3. last resort: a single subfolder that itself looks like a profile
	search_path = _r_format_string (L"%s\\*", import_dir->buffer);

	if (search_path)
	{
		hfind = FindFirstFileW (search_path->buffer, &find_data);

		if (hfind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					continue;

				if (_r_str_compare (find_data.cFileName, L".", 1) == 0 ||
					_r_str_compare (find_data.cFileName, L"..", 2) == 0)
					continue;

				candidate = _r_format_string (L"%s\\%s", import_dir->buffer, find_data.cFileName);

				if (candidate)
				{
					if (_app_dir_is_profile (candidate->buffer))
					{
						FindClose (hfind);
						_r_obj_dereference (search_path);

						return candidate;
					}

					_r_obj_dereference (candidate);
				}
			}
			while (FindNextFileW (hfind, &find_data));

			FindClose (hfind);
		}

		_r_obj_dereference (search_path);
	}

	return NULL;
}

VOID _app_export_profile (
	_In_ HWND hwnd
)
{
	PR_STRING app_root;
	PR_STRING export_dir;
	PR_STRING profile_dir;
	PR_STRING export_subdir;
	PR_STRING label;
	LONG instance_id;
	BOOLEAN any_exported = FALSE;

	app_root = _app_get_profile_root_dir ();

	if (!app_root)
		return;

	export_dir = _app_browse_for_folder (hwnd, _r_locale_getstring (IDS_EXPORTPROFILE_TITLE));

	if (!export_dir)
	{
		_r_obj_dereference (app_root);
		return;
	}

	for (instance_id = 1; instance_id <= 4; instance_id++)
	{
		if (instance_id == 1)
			profile_dir = _r_format_string (L"%s\\profile", app_root->buffer);
		else
			profile_dir = _r_format_string (L"%s\\profile%" TEXT (PR_LONG), app_root->buffer, instance_id);

		if (!profile_dir)
			continue;

		if (!_r_fs_exists (&profile_dir->sr))
		{
			_r_obj_dereference (profile_dir);
			continue;
		}

		label = _app_get_profile_label (instance_id, profile_dir);

		if (!label)
		{
			_r_obj_dereference (profile_dir);
			continue;
		}

		export_subdir = _r_format_string (L"%s\\%s", export_dir->buffer, label->buffer);

		if (export_subdir)
		{
			_app_copy_directory (profile_dir, export_subdir);
			any_exported = TRUE;
			_r_obj_dereference (export_subdir);
		}

		_r_obj_dereference (label);
		_r_obj_dereference (profile_dir);
	}

	if (any_exported)
		_r_show_message (hwnd, MB_OK | MB_ICONINFORMATION, NULL, _r_locale_getstring (IDS_PROFILE_EXPORTED));

	_r_obj_dereference (export_dir);
	_r_obj_dereference (app_root);
}

VOID _app_import_profile (
	_In_ HWND hwnd
)
{
	PR_STRING app_root;
	PR_STRING import_dir;
	PR_STRING profile_dir;
	PR_STRING import_subdir;
	LONG instance_id;
	INT selected_instance;
	BOOLEAN any_imported = FALSE;

	app_root = _app_get_profile_root_dir ();

	if (!app_root)
		return;

	import_dir = _app_browse_for_folder (hwnd, _r_locale_getstring (IDS_IMPORTPROFILE_TITLE));

	if (!import_dir)
	{
		_r_obj_dereference (app_root);
		return;
	}

	// Ask user which instance to import into (1-4)
	selected_instance = _r_show_message (hwnd, MB_YESNOCANCEL | MB_ICONQUESTION, NULL,
		L"Import into instance 1?\n\n"
		L"Yes = Instance 1\n"
		L"No = Other instances (will prompt)\n"
		L"Cancel = Abort");

	if (selected_instance == IDCANCEL)
	{
		_r_obj_dereference (import_dir);
		_r_obj_dereference (app_root);
		return;
	}

	if (selected_instance == IDYES)
	{
		instance_id = 1;
	}
	else
	{
		// Simple dialog for other instances
		PR_STRING input = NULL;
		// Since we don't have a custom input dialog easily, use a simple message-based approach
		// Ask for instance 2, 3, or 4 using multiple message boxes
		if (_r_show_message (hwnd, MB_YESNOCANCEL | MB_ICONQUESTION, NULL, L"Import into instance 2?\n\nYes = Instance 2, No = Instance 3, Cancel = Instance 4") == IDYES)
		{
			instance_id = 2;
		}
		else if (_r_show_message (hwnd, MB_YESNOCANCEL | MB_ICONQUESTION, NULL, L"Import into instance 3?\n\nYes = Instance 3, No = Instance 4, Cancel = Abort") == IDYES)
		{
			instance_id = 3;
		}
		else if (_r_show_message (hwnd, MB_YESNOCANCEL | MB_ICONQUESTION, NULL, L"Import into instance 4?\n\nYes = Instance 4, No = Abort, Cancel = Abort") == IDYES)
		{
			instance_id = 4;
		}
		else
		{
			_r_obj_dereference (import_dir);
			_r_obj_dereference (app_root);
			return;
		}
	}

	// Locate the real profile folder from whatever the user picked (the profile itself under
	// any name, or a parent holding it). This is the fix for import silently doing nothing when
	// the folder wasn't named exactly as the old code expected.
	import_subdir = _app_find_import_source (import_dir, instance_id);

	if (!import_subdir)
	{
		_r_show_message (hwnd, MB_OK | MB_ICONWARNING, NULL,
			L"No browser profile was found in the selected folder.\r\n\r\n"
			L"Pick the profile folder itself, or a folder that directly contains it.");

		_r_obj_dereference (import_dir);
		_r_obj_dereference (app_root);
		return;
	}

	if (instance_id == 1)
		profile_dir = _r_format_string (L"%s\\profile", app_root->buffer);
	else
		profile_dir = _r_format_string (L"%s\\profile%" TEXT (PR_LONG), app_root->buffer, instance_id);

	if (profile_dir)
	{
		// Check if target exists and confirm overwrite
		if (_r_fs_exists (&profile_dir->sr))
		{
			if (_r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, NULL,
				L"Target profile already exists. Overwrite?") != IDYES)
			{
				_r_obj_dereference (profile_dir);
				_r_obj_dereference (import_subdir);
				_r_obj_dereference (import_dir);
				_r_obj_dereference (app_root);
				return;
			}
		}

		_r_fs_createdirectory (&profile_dir->sr);

		_app_copy_directory (import_subdir, profile_dir);
		any_imported = TRUE;

		_r_obj_dereference (profile_dir);
	}

	_r_obj_dereference (import_subdir);

	if (any_imported)
		_r_show_message (hwnd, MB_OK | MB_ICONINFORMATION, NULL, _r_locale_getstring (IDS_PROFILE_IMPORTED));

	_r_obj_dereference (import_dir);
	_r_obj_dereference (app_root);
}

static BOOLEAN _app_delete_directory_contents (
	_In_ PR_STRING dir
)
{
	PR_STRING search_path;
	PR_STRING file_path;
	HANDLE hfind;
	WIN32_FIND_DATAW find_data;
	BOOLEAN is_success = TRUE;

	search_path = _r_format_string (L"%s\\*", dir->buffer);

	if (!search_path)
		return FALSE;

	hfind = FindFirstFileW (search_path->buffer, &find_data);

	if (hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (_r_str_compare (find_data.cFileName, L".", 1) == 0 ||
				_r_str_compare (find_data.cFileName, L"..", 2) == 0)
			{
				continue;
			}

			file_path = _r_format_string (L"%s\\%s", dir->buffer, find_data.cFileName);

			if (!file_path)
			{
				is_success = FALSE;
				continue;
			}

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!_app_delete_directory_contents (file_path))
					is_success = FALSE;

				RemoveDirectoryW (file_path->buffer);
			}
			else
			{
				_r_fs_deletefile (&file_path->sr, NULL);
			}

			_r_obj_dereference (file_path);
		}
		while (FindNextFileW (hfind, &find_data));

		FindClose (hfind);
	}

	_r_obj_dereference (search_path);

	return is_success;
}

VOID _app_uninstall_app (
	_In_ HWND hwnd
)
{
	PR_STRING app_root;
	PR_STRING temp_dir = NULL;
	PR_STRING script_path = NULL;
	PR_STRING script_params = NULL;
	PR_STRING script_content = NULL;
	HANDLE hfile;
	ULONG written;
	NTSTATUS status;
	BOOLEAN keep_profiles = FALSE;
	SHELLEXECUTEINFOW sei = {0};
	INT msg_result;

	msg_result = _r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, NULL, _r_locale_getstring (IDS_UNINSTALL_CONFIRM));

	if (msg_result != IDYES)
		return;

	msg_result = _r_show_message (hwnd, MB_YESNO | MB_ICONQUESTION, NULL, _r_locale_getstring (IDS_UNINSTALL_KEEPPROFILES));

	if (msg_result == IDYES)
		keep_profiles = TRUE;

	app_root = _app_get_app_root_dir ();

	if (!app_root)
		return;

	temp_dir = _r_sys_gettempdirectory ();

	if (!temp_dir)
		goto CleanupExit;

	script_path = _r_format_string (
		L"%s\\_launchbro_uninstall_%" TEXT (PR_ULONG) L".ps1",
		temp_dir->buffer,
		_r_str_gethash2 (&app_root->sr, TRUE)
	);

	if (!script_path)
		goto CleanupExit;

	if (keep_profiles)
	{
		script_content = _r_format_string (
			L"$Root = '%s'\r\n"
			L"$ScriptPath = '%s'\r\n"
			L"Start-Sleep -Seconds 2\r\n"
			L"$roots = @($Root)\r\n"
			L"$legacy = Join-Path ([System.IO.Path]::GetDirectoryName($Root)) 'chrlauncher'\r\n"
			L"if (Test-Path $legacy) { $roots += $legacy }\r\n"
			L"Get-CimInstance Win32_Process | Where-Object {\r\n"
			L"  $path = $_.ExecutablePath\r\n"
			L"  $path -and @($roots | Where-Object { $path.StartsWith($_, [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0\r\n"
			L"} | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }\r\n"
			L"while (Get-Process launchbro -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1 }\r\n"
			L"$wsh = New-Object -ComObject WScript.Shell\r\n"
			L"foreach ($d in @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('CommonDesktopDirectory'))) {\r\n"
			L"  if (!$d -or !(Test-Path $d)) { continue }\r\n"
			L"  Get-ChildItem -LiteralPath $d -Filter *.lnk -ErrorAction SilentlyContinue | ForEach-Object {\r\n"
			L"    try {\r\n"
			L"      $t = $wsh.CreateShortcut($_.FullName).TargetPath\r\n"
			L"      if ($t -and @($roots | Where-Object { $t.StartsWith($_, [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0) {\r\n"
			L"        Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue\r\n"
			L"      }\r\n"
			L"    } catch {}\r\n"
			L"  }\r\n"
			L"}\r\n"
			L"foreach ($h in @('HKCU:', 'HKLM:')) {\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Classes\\launchbroHTML') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Classes\\launchbroURL') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Clients\\StartMenuInternet\\launchbro') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Clients\\StartMenuInternet\\launchbro.EXE') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-ItemProperty -LiteralPath ($h + '\\Software\\RegisteredApplications') -Name 'launchbro' -ErrorAction SilentlyContinue\r\n"
			L"}\r\n"
			L"foreach ($arch in @('32', '64')) {\r\n"
			L"  $archRoot = Join-Path $Root $arch\r\n"
			L"  if (!(Test-Path $archRoot)) { continue }\r\n"
			L"  Get-ChildItem -LiteralPath $archRoot -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'bin*' } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Get-ChildItem -LiteralPath $archRoot -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.exe', '.dll' } | Remove-Item -Force -ErrorAction SilentlyContinue\r\n"
			L"}\r\n"
			L"Get-ChildItem -LiteralPath $Root -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.exe', '.dll', '.bat', '.ini', '.lng', '.txt' } | Remove-Item -Force -ErrorAction SilentlyContinue\r\n"
			L"Remove-Item -LiteralPath (Join-Path $Root 'i18n') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"Start-Sleep -Milliseconds 500\r\n"
			L"Remove-Item -LiteralPath $ScriptPath -Force -ErrorAction SilentlyContinue\r\n",
			app_root->buffer,
			script_path->buffer
		);
	}
	else
	{
		script_content = _r_format_string (
			L"$Root = '%s'\r\n"
			L"$ScriptPath = '%s'\r\n"
			L"Start-Sleep -Seconds 2\r\n"
			L"$roots = @($Root)\r\n"
			L"$legacy = Join-Path ([System.IO.Path]::GetDirectoryName($Root)) 'chrlauncher'\r\n"
			L"if (Test-Path $legacy) { $roots += $legacy }\r\n"
			L"Get-CimInstance Win32_Process | Where-Object {\r\n"
			L"  $path = $_.ExecutablePath\r\n"
			L"  $path -and @($roots | Where-Object { $path.StartsWith($_, [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0\r\n"
			L"} | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }\r\n"
			L"while (Get-Process launchbro -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1 }\r\n"
			L"$wsh = New-Object -ComObject WScript.Shell\r\n"
			L"foreach ($d in @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('CommonDesktopDirectory'))) {\r\n"
			L"  if (!$d -or !(Test-Path $d)) { continue }\r\n"
			L"  Get-ChildItem -LiteralPath $d -Filter *.lnk -ErrorAction SilentlyContinue | ForEach-Object {\r\n"
			L"    try {\r\n"
			L"      $t = $wsh.CreateShortcut($_.FullName).TargetPath\r\n"
			L"      if ($t -and @($roots | Where-Object { $t.StartsWith($_, [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0) {\r\n"
			L"        Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue\r\n"
			L"      }\r\n"
			L"    } catch {}\r\n"
			L"  }\r\n"
			L"}\r\n"
			L"foreach ($h in @('HKCU:', 'HKLM:')) {\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Classes\\launchbroHTML') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Classes\\launchbroURL') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Clients\\StartMenuInternet\\launchbro') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-Item -LiteralPath ($h + '\\Software\\Clients\\StartMenuInternet\\launchbro.EXE') -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"  Remove-ItemProperty -LiteralPath ($h + '\\Software\\RegisteredApplications') -Name 'launchbro' -ErrorAction SilentlyContinue\r\n"
			L"}\r\n"
			L"Remove-Item -LiteralPath $Root -Recurse -Force -ErrorAction SilentlyContinue\r\n"
			L"Start-Sleep -Milliseconds 500\r\n"
			L"Remove-Item -LiteralPath $ScriptPath -Force -ErrorAction SilentlyContinue\r\n",
			app_root->buffer,
			script_path->buffer
		);
	}

	if (!script_content)
		goto CleanupExit;

	status = _r_fs_createfile (
		&script_path->sr,
		FILE_OVERWRITE_IF,
		FILE_GENERIC_WRITE,
		FILE_SHARE_READ,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SEQUENTIAL_ONLY,
		FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	WriteFile (hfile, script_content->buffer, (ULONG)script_content->length, &written, NULL);
	CloseHandle (hfile);

	// Delete scheduled task if present
	_app_taskupdate_deletetask ();

	// Revert the default-browser profile edit so we don't leave the http/https handler
	// pointing at a profile folder that is about to be deleted.
	_app_unpatch_registry_associations ();

	script_params = _r_format_string (
		L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\"",
		script_path->buffer
	);

	if (!script_params)
		goto CleanupExit;

	sei.cbSize = sizeof (sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
	sei.lpVerb = L"open";
	sei.lpFile = L"powershell.exe";
	sei.lpParameters = script_params->buffer;
	sei.nShow = SW_HIDE;

	if (ShellExecuteExW (&sei))
		_r_wnd_sendmessage (hwnd, 0, WM_DESTROY, 0, 0);

CleanupExit:

	if (script_content)
		_r_obj_dereference (script_content);

	if (script_params)
		_r_obj_dereference (script_params);

	if (script_path)
		_r_obj_dereference (script_path);

	if (temp_dir)
		_r_obj_dereference (temp_dir);

	if (app_root)
		_r_obj_dereference (app_root);
}

