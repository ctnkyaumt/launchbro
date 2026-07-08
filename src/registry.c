// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

static BOOLEAN _app_reg_read_string (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name,
	_Out_ PR_STRING* string_ptr
)
{
	HKEY key = NULL;
	LSTATUS status;
	ULONG type;
	ULONG size = 0;
	PR_STRING result;

	status = RegOpenKeyExW (hkey, subkey, 0, KEY_READ, &key);

	if (status != ERROR_SUCCESS)
		return FALSE;

	status = RegQueryValueExW (key, value_name, NULL, &type, NULL, &size);

	if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA)
	{
		RegCloseKey (key);
		return FALSE;
	}

	if (type != REG_SZ && type != REG_EXPAND_SZ)
	{
		RegCloseKey (key);
		return FALSE;
	}

	result = _r_obj_createstring_ex (NULL, size + sizeof (WCHAR));

	status = RegQueryValueExW (key, value_name, NULL, &type, (LPBYTE)result->buffer, &size);

	if (status != ERROR_SUCCESS)
	{
		_r_obj_dereference (result);
		RegCloseKey (key);
		return FALSE;
	}

	// ensure null-termination
	result->buffer[size / sizeof (WCHAR)] = L'\0';
	result->length = size;

	_r_str_trimtonullterminator (&result->sr);

	RegCloseKey (key);

	*string_ptr = result;

	return TRUE;
}

static BOOLEAN _app_reg_write_string (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name,
	_In_ LPCWSTR data
)
{
	HKEY key = NULL;
	LSTATUS status;
	ULONG disposition;
	ULONG data_size;

	status = RegOpenKeyExW (hkey, subkey, 0, KEY_SET_VALUE, &key);

	if (status != ERROR_SUCCESS)
	{
		status = RegCreateKeyExW (hkey, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, &disposition);

		if (status != ERROR_SUCCESS)
			return FALSE;
	}

	data_size = (ULONG)((wcslen (data) + 1) * sizeof (WCHAR));

	status = RegSetValueExW (key, value_name, 0, REG_SZ, (const BYTE*)data, data_size);

	RegCloseKey (key);

	return (status == ERROR_SUCCESS);
}

static VOID _app_reg_delete_value (
	_In_ HKEY hkey,
	_In_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
)
{
	HKEY key = NULL;

	if (RegOpenKeyExW (hkey, subkey, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS)
	{
		RegDeleteValueW (key, value_name);
		RegCloseKey (key);
	}
}

// Some Chromium builds register a `DelegateExecute` COM handler under <ProgId>\shell\open.
// When present, the shell launches the browser through that handler and IGNORES the
// (Default) command string we patch with --user-data-dir. Deleting the value forces the
// shell to fall back to the command line we control. Deleting is a no-op when it is absent
// (the common ungoogled-chromium case), so this matches the known-working manual edit which
// only touches the command. An earlier build wrote an *empty* DelegateExecute here, which
// itself blocked activation; deleting also repairs those machines.
static VOID _app_reg_delete_delegate_execute (
	_In_ LPCWSTR prog_id
)
{
	PR_STRING hkcr_open;
	PR_STRING hkcu_open;

	hkcr_open = _r_format_string (L"%s\\shell\\open", prog_id);

	if (hkcr_open)
	{
		_app_reg_delete_value (HKEY_CLASSES_ROOT, hkcr_open->buffer, L"DelegateExecute");
		_r_obj_dereference (hkcr_open);
	}

	hkcu_open = _r_format_string (L"Software\\Classes\\%s\\shell\\open", prog_id);

	if (hkcu_open)
	{
		_app_reg_delete_value (HKEY_CURRENT_USER, hkcu_open->buffer, L"DelegateExecute");
		_r_obj_dereference (hkcu_open);
	}
}

// Write the patched open command exactly like the proven manual fix: edit
// HKEY_CLASSES_ROOT\<ProgId>\shell\open\command. Also mirror it into the per-user
// Software\Classes override so it still applies when HKCR resolves to an HKLM key we
// cannot write without elevation. Success if either write lands.
static BOOLEAN _app_patch_progid_command (
	_In_ PR_STRING prog_id,
	_In_ PR_STRING new_command
)
{
	PR_STRING hkcr_command;
	PR_STRING hkcu_command;
	BOOLEAN is_success = FALSE;

	hkcr_command = _r_format_string (L"%s\\shell\\open\\command", prog_id->buffer);

	if (hkcr_command)
	{
		if (_app_reg_write_string (HKEY_CLASSES_ROOT, hkcr_command->buffer, NULL, new_command->buffer))
			is_success = TRUE;

		_r_obj_dereference (hkcr_command);
	}

	hkcu_command = _r_format_string (L"Software\\Classes\\%s\\shell\\open\\command", prog_id->buffer);

	if (hkcu_command)
	{
		if (_app_reg_write_string (HKEY_CURRENT_USER, hkcu_command->buffer, NULL, new_command->buffer))
			is_success = TRUE;

		_r_obj_dereference (hkcu_command);
	}

	if (is_success)
		_app_reg_delete_delegate_execute (prog_id->buffer);

	return is_success;
}

static BOOLEAN _app_reg_read_open_command (
	_In_ PR_STRING prog_id,
	_Out_ PR_STRING* command
)
{
	PR_STRING subkey = NULL;
	PR_STRING classes_subkey = NULL;
	BOOLEAN is_success;

	if (!prog_id || _r_obj_isstringempty (prog_id) || !command)
		return FALSE;

	subkey = _r_format_string (L"%s\\shell\\open\\command", prog_id->buffer);

	if (!subkey)
		return FALSE;

	if (_app_reg_read_string (HKEY_CLASSES_ROOT, subkey->buffer, NULL, command))
	{
		_r_obj_dereference (subkey);
		return TRUE;
	}

	classes_subkey = _r_format_string (L"Software\\Classes\\%s", subkey->buffer);

	if (!classes_subkey)
	{
		_r_obj_dereference (subkey);
		return FALSE;
	}

	is_success = _app_reg_read_string (HKEY_CURRENT_USER, classes_subkey->buffer, NULL, command);

	_r_obj_dereference (classes_subkey);
	_r_obj_dereference (subkey);

	return is_success;
}

static PR_STRING _app_insert_profile_dir_in_command (
	_In_ PR_STRING command,
	_In_ PR_STRING profile_dir
);

static BOOLEAN _app_is_firefox_profile_target (
	_In_ PBROWSER_INFORMATION pbi
)
{
	R_STRINGREF r3dfox_type = PR_STRINGREF_INIT (L"r3dfox");
	R_STRINGREF iceweasel_type = PR_STRINGREF_INIT (L"iceweasel");

	if (!pbi || _r_obj_isstringempty (pbi->browser_type))
		return FALSE;

	return
		_r_str_isequal (&pbi->browser_type->sr, &r3dfox_type, TRUE) ||
		_r_str_isequal (&pbi->browser_type->sr, &iceweasel_type, TRUE);
}

static BOOLEAN _app_command_targets_launchbro (
	_In_ PR_STRING command
)
{
	if (!command || _r_obj_isstringempty (command))
		return FALSE;

	return (StrStrIW (command->buffer, L"launchbro.exe") != NULL);
}

static BOOLEAN _app_command_targets_selected_browser (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ PR_STRING command
)
{
	if (!pbi || !command || _r_obj_isstringempty (command))
		return FALSE;

	if (_app_command_targets_launchbro (command))
		return TRUE;

	if (!_r_obj_isstringempty (pbi->binary_path) && StrStrIW (command->buffer, pbi->binary_path->buffer))
		return TRUE;

	return FALSE;
}

static PR_STRING _app_build_profile_open_command (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ PR_STRING command
)
{
	if (!pbi || _r_obj_isstringempty (pbi->binary_path))
		return NULL;

	if (!_app_command_targets_selected_browser (pbi, command))
		return NULL;

	if (_app_command_targets_launchbro (command))
	{
		if (!_r_obj_isstringempty (pbi->args_str))
		{
			if (_app_is_firefox_profile_target (pbi))
				return _r_format_string (L"\"%s\" %s %1", pbi->binary_path->buffer, pbi->args_str->buffer);

			return _r_format_string (L"\"%s\" %s --single-argument %1", pbi->binary_path->buffer, pbi->args_str->buffer);
		}

		if (_app_is_firefox_profile_target (pbi))
			return _r_format_string (L"\"%s\" %1", pbi->binary_path->buffer);

		return _r_format_string (L"\"%s\" --single-argument %1", pbi->binary_path->buffer);
	}

	return _app_insert_profile_dir_in_command (command, pbi->profile_dir);
}

static PR_STRING _app_insert_profile_dir_in_command (
	_In_ PR_STRING command,
	_In_ PR_STRING profile_dir
)
{
	static const WCHAR SINGLE_ARGUMENT[] = L"--single-argument";
	static const WCHAR USER_DATA_DIR[] = L"--user-data-dir";
	static const WCHAR URL_TOKEN[] = L"-- \"%1";
	static R_STRINGREF space_sr = PR_STRINGREF_INIT (L" ");

	PR_STRING profile_arg;
	PR_STRING result;
	LPCWSTR existing_ptr;
	LPCWSTR replace_start;
	LPCWSTR replace_end;
	LPCWSTR suffix_ptr;
	LPCWSTR insert_pos;
	LPCWSTR url_pos;
	LPCWSTR command_end;
	R_STRINGREF prefix_sr;
	R_STRINGREF suffix_sr;
	ULONG_PTR prefix_len;
	ULONG_PTR suffix_len;
	BOOLEAN is_quote = FALSE;

	profile_arg = _r_format_string (L"--user-data-dir=\"%s\"", profile_dir->buffer);

	if (!profile_arg)
		return NULL;

	existing_ptr = StrStrIW (command->buffer, USER_DATA_DIR);

	if (existing_ptr)
	{
		replace_start = existing_ptr;
		command_end = command->buffer + (command->length / sizeof (WCHAR));

		if (replace_start > command->buffer && *(replace_start - 1) == L' ')
			replace_start--;

		replace_end = existing_ptr;

		while (replace_end < command_end && *replace_end)
		{
			if (*replace_end == L'"')
				is_quote = !is_quote;

			if (!is_quote && (*replace_end == L' ' || *replace_end == L'\t'))
				break;

			replace_end++;
		}

		suffix_ptr = replace_end;

		while (*suffix_ptr == L' ' || *suffix_ptr == L'\t')
			suffix_ptr++;

		prefix_len = (ULONG_PTR)(replace_start - command->buffer) * sizeof (WCHAR);
		suffix_len = (ULONG_PTR)(command_end - suffix_ptr) * sizeof (WCHAR);
		_r_obj_initializestringref_ex (&prefix_sr, command->buffer, prefix_len);
		_r_obj_initializestringref_ex (&suffix_sr, (LPWSTR)suffix_ptr, suffix_len);

		if (prefix_len && suffix_len)
			result = _r_obj_concatstringrefs (5, &prefix_sr, &space_sr, &profile_arg->sr, &space_sr, &suffix_sr);
		else if (prefix_len)
			result = _r_obj_concatstringrefs (3, &prefix_sr, &space_sr, &profile_arg->sr);
		else if (suffix_len)
			result = _r_obj_concatstringrefs (3, &profile_arg->sr, &space_sr, &suffix_sr);
		else
			result = _r_obj_createstring (profile_arg->buffer);

		_r_obj_dereference (profile_arg);

		return result;
	}

	// find --single-argument to insert before it
	insert_pos = StrStrIW (command->buffer, SINGLE_ARGUMENT);

	if (insert_pos)
	{
		// insert before --single-argument
		// skip back over any whitespace before --single-argument
		while (insert_pos > command->buffer && *(insert_pos - 1) == L' ')
			insert_pos--;

		prefix_len = (ULONG_PTR)(insert_pos - command->buffer) * sizeof (WCHAR);
		suffix_len = (ULONG_PTR)command->length - prefix_len;
		_r_obj_initializestringref_ex (&prefix_sr, command->buffer, prefix_len);
		_r_obj_initializestringref_ex (&suffix_sr, (LPWSTR)insert_pos, suffix_len);

		if (prefix_len)
			result = _r_obj_concatstringrefs (5, &prefix_sr, &space_sr, &profile_arg->sr, &space_sr, &suffix_sr);
		else
			result = _r_obj_concatstringrefs (3, &profile_arg->sr, &space_sr, &suffix_sr);
	}
	else
	{
		// no --single-argument found; insert before -- "%1" or at end
		url_pos = StrStrIW (command->buffer, URL_TOKEN);

		if (url_pos)
		{
			while (url_pos > command->buffer && *(url_pos - 1) == L' ')
				url_pos--;

			prefix_len = (ULONG_PTR)(url_pos - command->buffer) * sizeof (WCHAR);
			suffix_len = (ULONG_PTR)command->length - prefix_len;
			_r_obj_initializestringref_ex (&prefix_sr, command->buffer, prefix_len);
			_r_obj_initializestringref_ex (&suffix_sr, (LPWSTR)url_pos, suffix_len);

			if (prefix_len)
				result = _r_obj_concatstringrefs (5, &prefix_sr, &space_sr, &profile_arg->sr, &space_sr, &suffix_sr);
			else
				result = _r_obj_concatstringrefs (3, &profile_arg->sr, &space_sr, &suffix_sr);
		}
		else
		{
			result = _r_obj_concatstringrefs (3, &command->sr, &space_sr, &profile_arg->sr);
		}
	}

	_r_obj_dereference (profile_arg);

	return result;
}

static BOOLEAN _app_command_has_profile_dir (
	_In_ PR_STRING command,
	_In_ PR_STRING profile_dir
)
{
	PR_STRING profile_arg;
	BOOLEAN is_match;

	if (!command || _r_obj_isstringempty (command) || !profile_dir || _r_obj_isstringempty (profile_dir))
		return FALSE;

	profile_arg = _r_format_string (L"--user-data-dir=\"%s\"", profile_dir->buffer);

	if (!profile_arg)
		return FALSE;

	is_match = (StrStrIW (command->buffer, profile_arg->buffer) != NULL);

	_r_obj_dereference (profile_arg);

	return is_match;
}

// Returns TRUE only when NO DelegateExecute is registered (absent) for this ProgId in the
// effective (merged HKCR) view. A present value - even an empty one left by an older build -
// blocks activation, so it counts as not cleared and forces a re-patch that deletes it.
static BOOLEAN _app_is_delegate_execute_cleared (
	_In_ LPCWSTR prog_id
)
{
	PR_STRING open_subkey;
	PR_STRING delegate = NULL;
	BOOLEAN is_cleared = TRUE;

	open_subkey = _r_format_string (L"%s\\shell\\open", prog_id);

	if (!open_subkey)
		return TRUE;

	if (_app_reg_read_string (HKEY_CLASSES_ROOT, open_subkey->buffer, L"DelegateExecute", &delegate))
	{
		// value exists (empty or a real CLSID) -> not cleared
		is_cleared = FALSE;
		_r_obj_dereference (delegate);
	}

	_r_obj_dereference (open_subkey);

	return is_cleared;
}

static BOOLEAN _app_is_protocol_registry_patched (
	_In_ LPCWSTR protocol,
	_In_ PR_STRING profile_dir
)
{
	PR_STRING assoc_subkey;
	PR_STRING prog_id = NULL;
	PR_STRING command = NULL;
	BOOLEAN is_patched = FALSE;

	assoc_subkey = _r_format_string (
		L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%s\\UserChoice",
		protocol
	);

	if (!assoc_subkey)
		return FALSE;

	if (_app_reg_read_string (HKEY_CURRENT_USER, assoc_subkey->buffer, L"ProgId", &prog_id) &&
		_app_reg_read_open_command (prog_id, &command))
	{
		is_patched = _app_command_has_profile_dir (command, profile_dir) &&
			_app_is_delegate_execute_cleared (prog_id->buffer);
	}

	if (command)
		_r_obj_dereference (command);

	if (prog_id)
		_r_obj_dereference (prog_id);

	_r_obj_dereference (assoc_subkey);

	return is_patched;
}

static BOOLEAN _app_patch_protocol_profile (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ LPCWSTR protocol
)
{
	PR_STRING assoc_subkey;
	PR_STRING prog_id = NULL;
	PR_STRING command = NULL;
	PR_STRING new_command;
	BOOLEAN is_success = FALSE;

	assoc_subkey = _r_format_string (
		L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%s\\UserChoice",
		protocol
	);

	if (!assoc_subkey)
		return FALSE;

	// read the ProgId the user chose for this protocol, then its shell\open\command
	if (_app_reg_read_string (HKEY_CURRENT_USER, assoc_subkey->buffer, L"ProgId", &prog_id) &&
		_app_reg_read_open_command (prog_id, &command))
	{
		new_command = _app_build_profile_open_command (pbi, command);

		if (new_command)
		{
			is_success = _app_patch_progid_command (prog_id, new_command);

			_r_obj_dereference (new_command);
		}
	}

	if (command)
		_r_obj_dereference (command);

	if (prog_id)
		_r_obj_dereference (prog_id);

	_r_obj_dereference (assoc_subkey);

	return is_success;
}

BOOLEAN _app_patch_registry_profile (
	_In_ PBROWSER_INFORMATION pbi
)
{
	BOOLEAN http_ok;
	BOOLEAN https_ok;

	if (!pbi || _r_obj_isstringempty (pbi->profile_dir))
		return FALSE;

	http_ok = _app_patch_protocol_profile (pbi, L"http");
	https_ok = _app_patch_protocol_profile (pbi, L"https");

	return (http_ok || https_ok);
}

// Remove the --user-data-dir="..." switch we injected, returning the command to its
// original form. Returns NULL when the switch is not present.
static PR_STRING _app_remove_user_data_dir (
	_In_ PR_STRING command
)
{
	static const WCHAR USER_DATA_DIR[] = L"--user-data-dir";

	LPCWSTR match;
	LPCWSTR start;
	LPCWSTR end;
	LPCWSTR command_end;
	R_STRINGREF prefix_sr;
	R_STRINGREF suffix_sr;
	BOOLEAN is_quote = FALSE;

	if (!command || _r_obj_isstringempty (command))
		return NULL;

	match = StrStrIW (command->buffer, USER_DATA_DIR);

	if (!match)
		return NULL;

	command_end = command->buffer + (command->length / sizeof (WCHAR));

	// swallow one separating space before the switch
	start = match;

	while (start > command->buffer && *(start - 1) == L' ')
		start--;

	// scan to the end of --user-data-dir="..." honouring quotes
	end = match;

	while (end < command_end && *end)
	{
		if (*end == L'"')
			is_quote = !is_quote;

		if (!is_quote && (*end == L' ' || *end == L'\t'))
			break;

		end++;
	}

	_r_obj_initializestringref_ex (&prefix_sr, command->buffer, (ULONG_PTR)(start - command->buffer) * sizeof (WCHAR));
	_r_obj_initializestringref_ex (&suffix_sr, (LPWSTR)end, (ULONG_PTR)(command_end - end) * sizeof (WCHAR));

	return _r_obj_concatstringrefs (2, &prefix_sr, &suffix_sr);
}

// Uninstall cleanup: strip the injected profile switch from the default-browser command
// so we don't leave the http/https handler pointing at a deleted profile folder.
VOID _app_unpatch_registry_associations (VOID)
{
	static LPCWSTR protocols[] = {L"http", L"https"};

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (protocols); i++)
	{
		PR_STRING assoc_subkey;
		PR_STRING prog_id = NULL;
		PR_STRING command = NULL;
		PR_STRING new_command;

		assoc_subkey = _r_format_string (
			L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%s\\UserChoice",
			protocols[i]
		);

		if (!assoc_subkey)
			continue;

		if (_app_reg_read_string (HKEY_CURRENT_USER, assoc_subkey->buffer, L"ProgId", &prog_id) &&
			_app_reg_read_open_command (prog_id, &command))
		{
			new_command = _app_remove_user_data_dir (command);

			if (new_command)
			{
				_app_patch_progid_command (prog_id, new_command);

				_r_obj_dereference (new_command);
			}
		}

		if (command)
			_r_obj_dereference (command);

		if (prog_id)
			_r_obj_dereference (prog_id);

		_r_obj_dereference (assoc_subkey);
	}
}

BOOLEAN _app_is_registry_patched (
	_In_ PBROWSER_INFORMATION pbi
)
{
	if (!pbi || _r_obj_isstringempty (pbi->profile_dir))
		return FALSE;

	if (!_app_is_protocol_registry_patched (L"http", pbi->profile_dir))
		return FALSE;

	return _app_is_protocol_registry_patched (L"https", pbi->profile_dir);
}
