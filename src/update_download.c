// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "resource.h"
#include "update_release.h"

extern R_QUEUED_LOCK lock_download;

VOID _app_update_browser_info (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_setstatus (
	_In_ HWND hwnd,
	_In_opt_ HWND htaskbar,
	_In_opt_ PBROWSER_INFORMATION pbi,
	_In_opt_ LPCWSTR string,
	_In_opt_ ULONG64 total_read,
	_In_opt_ ULONG64 total_length
);

BOOLEAN _app_ishaveupdate (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_isupdatedownloaded (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_isupdaterequired (
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_set_lastcheck (
	_In_ PBROWSER_INFORMATION pbi
);

static PR_STRING _app_json_read_string (
	_In_ LPCWSTR value_ptr
)
{
	BOOLEAN is_escaped = FALSE;
	LPCWSTR start;
	LPCWSTR end;

	if (!value_ptr || value_ptr[0] != L'"')
		return NULL;

	start = value_ptr + 1;
	end = start;

	while (end[0])
	{
		if (is_escaped)
		{
			is_escaped = FALSE;
			end += 1;
			continue;
		}

		if (end[0] == L'\\')
		{
			is_escaped = TRUE;
			end += 1;
			continue;
		}

		if (end[0] == L'"')
			break;

		end += 1;
	}

	if (end[0] != L'"')
		return NULL;

	return _r_obj_createstring_ex (start, (ULONG_PTR)(end - start) * sizeof (WCHAR));
}

static LPCWSTR _app_json_skip_whitespace (
	_In_ LPCWSTR ptr
)
{
	if (!ptr)
		return NULL;

	while (ptr[0] == L' ' || ptr[0] == L'\t' || ptr[0] == L'\r' || ptr[0] == L'\n')
		ptr += 1;

	return ptr;
}

PR_STRING _app_json_find_string_value (
	_In_ LPCWSTR json,
	_In_ LPCWSTR key
)
{
	WCHAR pattern[128] = {0};
	LPCWSTR key_ptr;
	LPCWSTR colon_ptr;
	LPCWSTR value_ptr;

	if (!json || !key || !key[0])
		return NULL;

	swprintf_s (pattern, RTL_NUMBER_OF (pattern), L"\"%s\"", key);

	key_ptr = wcsstr (json, pattern);

	if (!key_ptr)
		return NULL;

	colon_ptr = wcschr (key_ptr + 1, L':');

	if (!colon_ptr)
		return NULL;

	value_ptr = _app_json_skip_whitespace (colon_ptr + 1);

	if (!value_ptr || value_ptr[0] != L'"')
		return NULL;

	return _app_json_read_string (value_ptr);
}

static BOOLEAN _app_checkupdate_github_latest_release (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_In_ LPCWSTR release_api_url,
	_In_ BOOLEAN is_exists,
	_In_ int policy,
	_In_opt_ LPCWSTR tag_suffix,
	_Out_ PBOOLEAN is_error_ptr
)
{
	R_DOWNLOAD_INFO download_info;
	PR_STRING proxy_string = NULL;
	HINTERNET hsession = NULL;
	PR_STRING json = NULL;
	PR_STRING tag_name = NULL;
	PR_STRING selected_url = NULL;
	LPCWSTR assets_ptr = NULL;
	LPCWSTR assets_limit = NULL;
	LPCWSTR scan_ptr;
	BOOLEAN is_success = FALSE;
	NTSTATUS status;

	*is_error_ptr = FALSE;

	proxy_string = _r_app_getproxyconfiguration ();
	hsession = _r_inet_createsession (_r_app_getuseragent (), proxy_string);

	if (!hsession)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", GetLastError (), release_api_url);
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

	{
		R_STRINGREF api_url_sr;
		_r_obj_initializestringref_ex (&api_url_sr, (LPWSTR)release_api_url, wcslen (release_api_url) * sizeof (WCHAR));
		status = _r_inet_begindownload (hsession, &api_url_sr, &download_info);
	}

	if (status != STATUS_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", status, release_api_url);
		*is_error_ptr = TRUE;
		_r_inet_destroydownload (&download_info);
		goto CleanupExit;
	}

	if (_r_obj_isstringempty (download_info.string))
	{
		_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
		*is_error_ptr = TRUE;
		_r_inet_destroydownload (&download_info);
		goto CleanupExit;
	}

	_r_obj_movereference ((PVOID_PTR)&json, download_info.string);
	download_info.string = NULL;
	_r_inet_destroydownload (&download_info);

	if (policy == RELEASE_POLICY_MACCHROME)
	{
		LPCWSTR search = json->buffer;

		while ((search = wcsstr (search, L"\"tag_name\"")) != NULL)
		{
			PR_STRING candidate_tag = _app_json_find_string_value (search, L"tag_name");
			LPCWSTR next_search = wcsstr (search + 10, L"\"tag_name\"");
			LPCWSTR rel_limit = next_search ? next_search : (json->buffer + (json->length / sizeof (WCHAR)));
			WCHAR ver_buf[32];

			if (candidate_tag)
			{
				BOOLEAN is_prerelease = FALSE;
				LPCWSTR pre_ptr = wcsstr (search, L"\"prerelease\"");

				if (pre_ptr && pre_ptr < rel_limit)
				{
					LPCWSTR colon_ptr = wcschr (pre_ptr, L':');
					if (colon_ptr && colon_ptr < rel_limit)
					{
						LPCWSTR val_ptr = _app_json_skip_whitespace (colon_ptr + 1);
						if (val_ptr && wcsncmp (val_ptr, L"true", 4) == 0)
							is_prerelease = TRUE;
					}
				}

				if (!is_prerelease && _app_release_macchrome_tag_version (candidate_tag->buffer, ver_buf, RTL_NUMBER_OF (ver_buf)))
				{
					assets_ptr = wcsstr (search, L"\"assets\"");

					if (assets_ptr && assets_ptr < rel_limit)
					{
						LPCWSTR p = assets_ptr;
						BOOLEAN has_matching_asset = FALSE;

						while ((p = wcsstr (p, L"\"name\"")) != NULL && p < rel_limit)
						{
							PR_STRING asset_name = _app_json_find_string_value (p, L"name");

							if (asset_name)
							{
								if (_app_release_asset_score (asset_name->buffer, pbi->architecture, policy) > 0)
									has_matching_asset = TRUE;

								_r_obj_dereference (asset_name);
							}

							if (has_matching_asset)
								break;

							p += 6;
						}

						if (has_matching_asset)
						{
							assets_limit = rel_limit;
							tag_name = _r_obj_createstring (ver_buf);
							_r_obj_dereference (candidate_tag);
							break;
						}
					}
				}

				_r_obj_dereference (candidate_tag);
			}

			search += 10;
		}

		if (!tag_name)
		{
			_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
			*is_error_ptr = TRUE;
			goto CleanupExit;
		}
	}
	else if (tag_suffix)
	{
		LPCWSTR search = json->buffer;

		while ((search = wcsstr (search, L"\"tag_name\"")) != NULL)
		{
			PR_STRING candidate_tag = _app_json_find_string_value (search, L"tag_name");

			if (candidate_tag)
			{
				if (_app_release_tag_endswith (candidate_tag->buffer, tag_suffix))
				{
					tag_name = candidate_tag;
					break;
				}

				_r_obj_dereference (candidate_tag);
			}

			search += 10;
		}

		if (!tag_name || !search)
		{
			_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
			*is_error_ptr = TRUE;
			goto CleanupExit;
		}

		assets_ptr = wcsstr (search, L"\"assets\"");
		assets_limit = wcsstr (search + 10, L"\"tag_name\"");

		if (!assets_limit)
			assets_limit = json->buffer + (json->length / sizeof (WCHAR));
	}
	else
	{
		tag_name = _app_json_find_string_value (json->buffer, L"tag_name");

		if (!tag_name)
		{
			_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
			*is_error_ptr = TRUE;
			goto CleanupExit;
		}

		assets_ptr = wcsstr (json->buffer, L"\"assets\"");
		assets_limit = json->buffer + (json->length / sizeof (WCHAR));
	}

	if (!assets_ptr || assets_ptr >= assets_limit)
	{
		_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	if (tag_name->length >= sizeof (WCHAR) && (tag_name->buffer[0] == L'v' || tag_name->buffer[0] == L'V'))
	{
		PR_STRING trimmed = _r_obj_createstring_ex (tag_name->buffer + 1, tag_name->length - sizeof (WCHAR));

		if (trimmed)
		{
			_r_obj_dereference (tag_name);
			tag_name = trimmed;
		}
	}

	if (policy == RELEASE_POLICY_UNGOOGLED || policy == RELEASE_POLICY_HIBBIKI || policy == RELEASE_POLICY_ROBRICH)
	{
		SIZE_T version_length = _app_release_chromium_version_length (tag_name->buffer);

		if (!version_length)
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_INVALID_DATA, release_api_url);
			*is_error_ptr = TRUE;
			goto CleanupExit;
		}

		tag_name->length = version_length * sizeof (WCHAR);
		tag_name->buffer[version_length] = UNICODE_NULL;
	}

	scan_ptr = assets_ptr;

	{
		LONG best_score = -1;
		LPCWSTR p = scan_ptr;

		while ((p = wcsstr (p, L"\"name\"")) != NULL && p < assets_limit)
		{
			PR_STRING asset_name = NULL;
			PR_STRING asset_url = NULL;
			LPCWSTR next_name;
			LPCWSTR url_key;
			LPCWSTR url_search_end;
			LONG score = 0;

			asset_name = _app_json_find_string_value (p, L"name");

			if (!asset_name)
			{
				p += 6;
				continue;
			}

			next_name = wcsstr (p + 6, L"\"name\"");
			url_search_end = (next_name && next_name < assets_limit) ? next_name : assets_limit;

			url_key = wcsstr (p, L"\"browser_download_url\"");

			if (!url_key || url_key >= url_search_end)
			{
				_r_obj_dereference (asset_name);
				p += 6;
				continue;
			}

			asset_url = _app_json_find_string_value (url_key, L"browser_download_url");

			if (!asset_url)
			{
				_r_obj_dereference (asset_name);
				p += 6;
				continue;
			}

			score = _app_release_asset_score (asset_name->buffer, pbi->architecture, policy);

			if (wcsncmp (asset_url->buffer, L"https://github.com/", 19) != 0)
				score = -1;

			if (score > best_score)
			{
				best_score = score;

				if (selected_url)
					_r_obj_dereference (selected_url);

				selected_url = asset_url;
				asset_url = NULL;
			}

			_r_obj_dereference (asset_name);

			if (asset_url)
				_r_obj_dereference (asset_url);

			p += 6;
		}
	}

	if (!selected_url)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_NOT_FOUND, L"No compatible portable browser archive in GitHub release");
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	_r_obj_movereference ((PVOID_PTR)&pbi->download_url, selected_url);
	selected_url = NULL;

	_r_obj_movereference ((PVOID_PTR)&pbi->new_version, tag_name);
	tag_name = NULL;

	_app_update_browser_info (hwnd, pbi);

	if (pbi->new_version && pbi->current_version)
		is_success = (_r_str_versioncompare (&pbi->current_version->sr, &pbi->new_version->sr) == -1);
	else
		is_success = TRUE;

	if (is_exists && !is_success)
	{
		SAFE_DELETE_REFERENCE (pbi->download_url);
		_app_set_lastcheck (pbi);
	}

CleanupExit:
	_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);

	if (selected_url)
		_r_obj_dereference (selected_url);

	if (tag_name)
		_r_obj_dereference (tag_name);

	if (json)
		_r_obj_dereference (json);

	if (hsession)
		_r_inet_close (hsession);

	if (proxy_string)
		_r_obj_dereference (proxy_string);

	return is_success;
}

static BOOLEAN _app_checkupdate_snapshot (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_In_ BOOLEAN is_exists,
	_Out_ PBOOLEAN is_error_ptr
)
{
	R_DOWNLOAD_INFO download_info;
	PR_STRING proxy_string = NULL;
	HINTERNET hsession = NULL;
	PR_STRING content = NULL;
	PR_STRING base_url = NULL;
	PR_STRING last_change_url = NULL;
	PR_STRING archive_url = NULL;
	PR_STRING new_rev_str = NULL;
	PR_STRING stored_rev_str = NULL;
	PR_STRING rev_key = NULL;
	LPCWSTR folder;
	SIZE_T rev_len;
	LONG64 current_rev;
	LONG64 new_rev;
	BOOLEAN is_newer = FALSE;
	BOOLEAN is_success = FALSE;
	NTSTATUS status;

	*is_error_ptr = FALSE;

	folder = (pbi->architecture == 64) ? L"Win_x64" : L"Win";
	base_url = _r_format_string (CHROMIUM_SNAPSHOT_URL, folder);

	if (!base_url)
	{
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	last_change_url = _r_format_string (L"%sLAST_CHANGE", base_url->buffer);

	if (!last_change_url)
	{
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	proxy_string = _r_app_getproxyconfiguration ();
	hsession = _r_inet_createsession (_r_app_getuseragent (), proxy_string);

	if (!hsession)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate_snapshot", GetLastError (), last_change_url->buffer);
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

	status = _r_inet_begindownload (hsession, &last_change_url->sr, &download_info);

	if (status != STATUS_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate_snapshot", status, last_change_url->buffer);
		*is_error_ptr = TRUE;
		_r_inet_destroydownload (&download_info);
		goto CleanupExit;
	}

	if (_r_obj_isstringempty (download_info.string))
	{
		_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_ERROR_CONFIG_NOT_FOUND));
		*is_error_ptr = TRUE;
		_r_inet_destroydownload (&download_info);
		goto CleanupExit;
	}

	_r_obj_movereference ((PVOID_PTR)&content, download_info.string);
	download_info.string = NULL;
	_r_inet_destroydownload (&download_info);

	rev_len = _app_release_revision_length (content->buffer);

	if (!rev_len)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate_snapshot", ERROR_INVALID_DATA, last_change_url->buffer);
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	new_rev_str = _r_obj_createstring_ex (content->buffer, rev_len * sizeof (WCHAR));

	if (!new_rev_str)
	{
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	_r_obj_movereference ((PVOID_PTR)&pbi->new_version, new_rev_str);
	new_rev_str = NULL;

	archive_url = _r_format_string (L"%s%s/chrome-win.zip", base_url->buffer, pbi->new_version->buffer);

	if (!archive_url)
	{
		*is_error_ptr = TRUE;
		goto CleanupExit;
	}

	_r_obj_movereference ((PVOID_PTR)&pbi->download_url, archive_url);
	archive_url = NULL;

	if (pbi->instance_id >= 2 && pbi->instance_id <= 4)
		rev_key = _r_format_string (L"ChromiumSnapshotRevision%" TEXT (PR_LONG), pbi->instance_id);

	stored_rev_str = _r_config_getstring (rev_key ? rev_key->buffer : L"ChromiumSnapshotRevision", NULL);

	if (rev_key)
	{
		_r_obj_dereference (rev_key);
		rev_key = NULL;
	}

	new_rev = _r_str_tolong64 (&pbi->new_version->sr);

	if (!is_exists)
	{
		is_newer = TRUE;
	}
	else if (!stored_rev_str || _r_obj_isstringempty (stored_rev_str))
	{
		is_newer = TRUE;
	}
	else
	{
		current_rev = _r_str_tolong64 (&stored_rev_str->sr);

		if (new_rev > current_rev)
			is_newer = TRUE;
	}

	if (stored_rev_str)
	{
		_r_obj_dereference (stored_rev_str);
		stored_rev_str = NULL;
	}

	_app_update_browser_info (hwnd, pbi);

	is_success = is_newer;

	if (is_exists && !is_success)
	{
		SAFE_DELETE_REFERENCE (pbi->download_url);
		_app_set_lastcheck (pbi);
	}

CleanupExit:
	_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);

	if (new_rev_str)
		_r_obj_dereference (new_rev_str);

	if (archive_url)
		_r_obj_dereference (archive_url);

	if (last_change_url)
		_r_obj_dereference (last_change_url);

	if (base_url)
		_r_obj_dereference (base_url);

	if (content)
		_r_obj_dereference (content);

	if (hsession)
		_r_inet_close (hsession);

	if (proxy_string)
		_r_obj_dereference (proxy_string);

	return is_success;
}

BOOLEAN _app_checkupdate (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_Out_ PBOOLEAN is_error_ptr
)
{
	PR_HASHTABLE hashtable = NULL;
	R_DOWNLOAD_INFO download_info;
	PR_STRING update_url;
	PR_STRING url = NULL;
	HINTERNET hsession;
	PR_STRING string;
	PR_STRING proxy_string;
	BOOLEAN is_updaterequired;
	BOOLEAN is_newversion = FALSE;
	BOOLEAN is_success = FALSE;
	BOOLEAN is_exists;
	NTSTATUS status;

	*is_error_ptr = FALSE;

	if (_app_ishaveupdate (pbi))
		return TRUE;

	is_exists = _r_fs_exists (&pbi->binary_path->sr);
	is_updaterequired = _app_isupdaterequired (pbi);

	_app_setstatus (hwnd, pbi->htaskbar, pbi, _r_locale_getstring (IDS_STATUS_CHECK), 0, 0);

	SAFE_DELETE_REFERENCE (pbi->new_version);
	pbi->timestamp = 0;

	_app_update_browser_info (hwnd, pbi);

	if (!is_exists || is_updaterequired)
	{
		APP_BROWSER_KIND kind = _app_get_browser_kind (pbi->browser_type);

		if (kind == BROWSER_KIND_R3DFOX)
			return _app_checkupdate_github_latest_release (hwnd, pbi, UPDATE_URL_R3DFOX, is_exists, RELEASE_POLICY_GENERIC, NULL, is_error_ptr);

		if (kind == BROWSER_KIND_ICEWEASEL)
			return _app_checkupdate_github_latest_release (hwnd, pbi, UPDATE_URL_ICEWEASEL, is_exists, RELEASE_POLICY_GENERIC, NULL, is_error_ptr);

		update_url = _r_config_getstring (L"ChromiumUpdateUrl", NULL);

		if (update_url)
		{
			url = _r_format_string (update_url->buffer, pbi->architecture, pbi->browser_type ? pbi->browser_type->buffer : L"");
			_r_obj_dereference (update_url);
			update_url = NULL;
		}
		else
		{
			switch (kind)
			{
				case BROWSER_KIND_UNGOOGLED:
				{
					if (pbi->architecture == 64)
						return _app_checkupdate_github_latest_release (hwnd, pbi, CHROMIUM_UPDATE_URL_MACCHROME, is_exists, RELEASE_POLICY_MACCHROME, NULL, is_error_ptr);

					return _app_checkupdate_github_latest_release (hwnd, pbi, CHROMIUM_UPDATE_URL_UNGOOGLED, is_exists, RELEASE_POLICY_UNGOOGLED, NULL, is_error_ptr);
				}

				case BROWSER_KIND_CROMITE:
				{
					url = _r_obj_createstring (CHROMIUM_UPDATE_URL_CROMITE);
					break;
				}

				case BROWSER_KIND_STABLE_CODECS_SYNC:
				{
					if (pbi->architecture != 64)
					{
						_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_NOT_SUPPORTED, L"stable-codecs-sync provides 64-bit builds only");
						*is_error_ptr = TRUE;
						_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);
						return FALSE;
					}

					return _app_checkupdate_github_latest_release (hwnd, pbi, CHROMIUM_UPDATE_URL_HIBBIKI, is_exists, RELEASE_POLICY_HIBBIKI, NULL, is_error_ptr);
				}

				case BROWSER_KIND_DEV_NOSYNC:
				{
					if (pbi->architecture != 64)
					{
						_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_NOT_SUPPORTED, L"dev-nosync provides 64-bit builds only");
						*is_error_ptr = TRUE;
						_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);
						return FALSE;
					}

					return _app_checkupdate_github_latest_release (hwnd, pbi, CHROMIUM_UPDATE_URL_ROBRICH, is_exists, RELEASE_POLICY_ROBRICH, L"-win64-avx2", is_error_ptr);
				}

				case BROWSER_KIND_DEV_OFFICIAL:
				{
					return _app_checkupdate_snapshot (hwnd, pbi, is_exists, is_error_ptr);
				}

				default:
				{
					_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_NOT_SUPPORTED, L"Unsupported ChromiumType");
					*is_error_ptr = TRUE;
					_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);
					return FALSE;
				}
			}
		}

		if (url)
		{
			proxy_string = _r_app_getproxyconfiguration ();

			hsession = _r_inet_createsession (_r_app_getuseragent (), proxy_string);

			if (hsession)
			{
				_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

				status = _r_inet_begindownload (hsession, &url->sr, &download_info);

				if (status == STATUS_SUCCESS)
				{
					string = download_info.string;

					if (_r_obj_isstringempty (string))
					{
						// non-fatal: server returned no config; caller surfaces a tray notification
						_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_INVALID_DATA, url->buffer);

						*is_error_ptr = TRUE;
					}
					else
					{
						hashtable = _r_str_unserialize (&string->sr, L';', L'=');
					}
				}
				else
				{
					// transient network/DNS failures (e.g. 12007) happen on startup before the
					// connection is ready - log instead of blocking the user with a modal dialog
					_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", status, url->buffer);

					*is_error_ptr = TRUE;
				}

				_r_inet_destroydownload (&download_info);

				_r_inet_close (hsession);
			}
			else
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", GetLastError (), url->buffer);
				*is_error_ptr = TRUE;
			}

			if (proxy_string)
				_r_obj_dereference (proxy_string);

			_r_obj_dereference (url);
		}
	}

	if (hashtable)
	{
		R_STRINGREF download_key = PR_STRINGREF_INIT (L"download");
		R_STRINGREF version_key = PR_STRINGREF_INIT (L"version");
		R_STRINGREF timestamp_key = PR_STRINGREF_INIT (L"timestamp");

		string = _r_obj_findhashtablepointer (hashtable, _r_str_gethash2 (&download_key, TRUE));

		_r_obj_movereference ((PVOID_PTR)&pbi->download_url, string);

		string = _r_obj_findhashtablepointer (hashtable, _r_str_gethash2 (&version_key, TRUE));

		_r_obj_movereference ((PVOID_PTR)&pbi->new_version, string);

		string = _r_obj_findhashtablepointer (hashtable, _r_str_gethash2 (&timestamp_key, TRUE));

		if (string)
		{
			pbi->timestamp = _r_str_tolong64 (&string->sr);

			_r_obj_dereference (string);
		}

		_app_update_browser_info (hwnd, pbi);

		if (!_app_ishaveupdate (pbi))
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_checkupdate", ERROR_INVALID_DATA, L"Update configuration must contain download and version");
			SAFE_DELETE_REFERENCE (pbi->download_url);
			SAFE_DELETE_REFERENCE (pbi->new_version);
			*is_error_ptr = TRUE;
			_r_obj_dereference (hashtable);
			_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);
			return FALSE;
		}

		if (pbi->new_version && pbi->current_version)
			is_newversion = (_r_str_versioncompare (&pbi->current_version->sr, &pbi->new_version->sr) == -1);

		if (!is_exists || is_newversion)
		{
			is_success = TRUE;
		}
		else
		{
			SAFE_DELETE_REFERENCE (pbi->download_url);

			_app_set_lastcheck (pbi);
		}

		_r_obj_dereference (hashtable);
	}

	_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);

	return is_success;
}

BOOLEAN NTAPI _app_downloadupdate_callback (
	_In_ ULONG total_written,
	_In_ ULONG total_length,
	_In_ PVOID lparam
)
{
	PBROWSER_INFORMATION pbi;
	LONG percent;

	pbi = lparam;

	pbi->download_written = total_written;
	pbi->download_total = total_length;

	percent = total_length ? (LONG)(((ULONG64)total_written * 100) / total_length) : 0;

	if (percent != pbi->progress_percent)
	{
		pbi->progress_percent = percent;
		_app_setstatus (pbi->hwnd, pbi->htaskbar, pbi, _r_locale_getstring (IDS_STATUS_DOWNLOAD), total_written, total_length);
	}

	return TRUE;
}

BOOLEAN _app_downloadupdate (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_Out_ PBOOLEAN is_error_ptr
)
{
	R_DOWNLOAD_INFO download_info;
	PR_STRING proxy_string;
	PR_STRING temp_file;
	HINTERNET hsession;
	HANDLE hfile;
	BOOLEAN is_success = FALSE;
	NTSTATUS status;
	INT retry_count = 0;
	const INT max_retries = 3;

	*is_error_ptr = FALSE;

	if (_app_isupdatedownloaded (pbi))
		return TRUE;

	if (_r_obj_isstringempty (pbi->download_url))
	{
		*is_error_ptr = TRUE;
		return FALSE;
	}

	temp_file = _r_obj_concatstrings (
		2,
		pbi->cache_path->buffer,
		L".tmp"
	);

	_r_fs_deletefile (&pbi->cache_path->sr, NULL);

	_app_setstatus (hwnd, pbi->htaskbar, pbi, _r_locale_getstring (IDS_STATUS_DOWNLOAD), 0, 1);

	_r_queuedlock_acquireshared (&lock_download);

	proxy_string = _r_app_getproxyconfiguration ();

	hsession = _r_inet_createsession (_r_app_getuseragent (), proxy_string);

	if (hsession)
	{
		for (retry_count = 0; retry_count <= max_retries; retry_count++)
		{
			status = _r_fs_createfile (
				&temp_file->sr,
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
			{
				_r_show_errormessage (hwnd, _r_locale_getstring (IDS_ERROR_WRITE_FILE), status, temp_file->buffer, ET_NATIVE);

				*is_error_ptr = TRUE;
				break;
			}

			pbi->download_written = 0;
			pbi->download_total = 0;
			pbi->progress_percent = -1;

			_r_inet_initializedownload (&download_info, hfile, &_app_downloadupdate_callback, pbi);

			status = _r_inet_begindownload (hsession, &pbi->download_url->sr, &download_info);

			_r_inet_destroydownload (&download_info);

			if (status == STATUS_SUCCESS)
			{
				if (_app_download_is_complete (pbi->download_written, pbi->download_total))
				{
					status = _r_fs_movefile (&temp_file->sr, &pbi->cache_path->sr, FALSE);

					if (!NT_SUCCESS (status))
					{
						_r_log (LOG_LEVEL_ERROR, NULL, L"_app_downloadupdate", status, pbi->cache_path->buffer);
						_r_show_errormessage (hwnd, _r_locale_getstring (IDS_ERROR_SAVE_UPDATE), status, pbi->cache_path->buffer, ET_NATIVE);
						*is_error_ptr = TRUE;
						_r_fs_deletefile (&temp_file->sr, NULL);
						break;
					}

					SAFE_DELETE_REFERENCE (pbi->download_url);

					is_success = TRUE;
					_r_fs_deletefile (&temp_file->sr, NULL);
					break;
				}

				status = STATUS_DATA_ERROR;
			}

			_r_fs_deletefile (&temp_file->sr, NULL);

			if (retry_count < max_retries)
			{
				ULONG delay_ms = 1000 * (1 << retry_count);
				Sleep (delay_ms);
			}
			else
			{
				if (status == STATUS_DATA_ERROR)
				{
					_r_show_errormessage (hwnd, _r_locale_getstring (IDS_ERROR_DOWNLOAD_INCOMPLETE), status, pbi->download_url->buffer, ET_NATIVE);
				}
				else
				{
					_r_show_errormessage (hwnd, _r_locale_getstring (IDS_ERROR_DOWNLOAD), status, pbi->download_url->buffer, ET_WINHTTP);
				}

				_r_fs_deletefile (&pbi->cache_path->sr, NULL);

				*is_error_ptr = TRUE;
			}
		}

		_r_inet_close (hsession);
	}
	else
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_downloadupdate", GetLastError (), pbi->download_url->buffer);
		*is_error_ptr = TRUE;
	}

	_r_queuedlock_releaseshared (&lock_download);

	if (proxy_string)
		_r_obj_dereference (proxy_string);

	_r_obj_dereference (temp_file);

	_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);

	return is_success;
}

