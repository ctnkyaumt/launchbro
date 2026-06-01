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
				return _r_format_string (L"\"%s\" %s \"%1\"", pbi->binary_path->buffer, pbi->args_str->buffer);

			return _r_format_string (L"\"%s\" %s -- \"%1\"", pbi->binary_path->buffer, pbi->args_str->buffer);
		}

		if (_app_is_firefox_profile_target (pbi))
			return _r_format_string (L"\"%s\" \"%1\"", pbi->binary_path->buffer);

		return _r_format_string (L"\"%s\" -- \"%1\"", pbi->binary_path->buffer);
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
		is_patched = _app_command_has_profile_dir (command, profile_dir);
	}

	if (command)
		_r_obj_dereference (command);

	if (prog_id)
		_r_obj_dereference (prog_id);

	_r_obj_dereference (assoc_subkey);

	return is_patched;
}

BOOLEAN _app_patch_registry_profile (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PR_STRING prog_id = NULL;
	PR_STRING command = NULL;
	PR_STRING new_command = NULL;
	BOOLEAN is_success = FALSE;

	if (!pbi || _r_obj_isstringempty (pbi->profile_dir))
		return FALSE;

	// read ProgId from UserChoice for http
	if (!_app_reg_read_string (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice", L"ProgId", &prog_id))
		return FALSE;

	// read open command from the ProgId
	if (!_app_reg_read_open_command (prog_id, &command))
	{
		_r_obj_dereference (prog_id);
		return FALSE;
	}

	// insert profile dir
	new_command = _app_build_profile_open_command (pbi, command);

	if (new_command)
	{
		PR_STRING open_command_subkey = _r_format_string (L"%s\\shell\\open\\command", prog_id->buffer);

		if (!open_command_subkey)
		{
			_r_obj_dereference (new_command);
			goto CleanupHttps;
		}

		// Prefer the per-user Classes override because UserChoice defaults are per-user too.
		{
			PR_STRING subkey = _r_format_string (L"Software\\Classes\\%s", open_command_subkey->buffer);

			if (subkey)
			{
				is_success = _app_reg_write_string (HKEY_CURRENT_USER, subkey->buffer, NULL, new_command->buffer);
				_r_obj_dereference (subkey);
			}
		}

		// Fallback to HKCR only if the user-level write did not succeed.
		if (!is_success && _app_reg_write_string (HKEY_CLASSES_ROOT, open_command_subkey->buffer, NULL, new_command->buffer))
		{
			is_success = TRUE;
		}

		_r_obj_dereference (open_command_subkey);
		_r_obj_dereference (new_command);
	}

CleanupHttps:
	// also patch https
	{
		PR_STRING https_prog_id = NULL;
		PR_STRING https_command = NULL;
		PR_STRING https_new_command = NULL;

		if (_app_reg_read_string (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice", L"ProgId", &https_prog_id))
		{
			// only patch if different ProgId or same
			if (_app_reg_read_open_command (https_prog_id, &https_command))
			{
				https_new_command = _app_build_profile_open_command (pbi, https_command);

				if (https_new_command)
				{
					PR_STRING https_open_command_subkey = _r_format_string (L"%s\\shell\\open\\command", https_prog_id->buffer);
					BOOLEAN https_is_success = FALSE;

					if (!https_open_command_subkey)
					{
						_r_obj_dereference (https_new_command);
						_r_obj_dereference (https_command);
						_r_obj_dereference (https_prog_id);
						goto Cleanup;
					}

					{
						PR_STRING subkey = _r_format_string (L"Software\\Classes\\%s", https_open_command_subkey->buffer);

						if (subkey)
						{
							https_is_success = _app_reg_write_string (HKEY_CURRENT_USER, subkey->buffer, NULL, https_new_command->buffer);
							_r_obj_dereference (subkey);
						}
					}

					if (!https_is_success)
					{
						https_is_success = _app_reg_write_string (HKEY_CLASSES_ROOT, https_open_command_subkey->buffer, NULL, https_new_command->buffer);
					}

					if (!is_success && https_is_success)
						is_success = TRUE;

					_r_obj_dereference (https_open_command_subkey);
					_r_obj_dereference (https_new_command);
				}

				_r_obj_dereference (https_command);
			}

			_r_obj_dereference (https_prog_id);
		}
	}

Cleanup:
	_r_obj_dereference (command);
	_r_obj_dereference (prog_id);

	return is_success;
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
