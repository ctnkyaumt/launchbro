// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

VOID _app_set_update_failed (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ BOOLEAN is_failed
)
{
	PR_STRING key = NULL;

	if (!pbi)
		return;

	if (pbi->instance_id > 1)
		key = _r_format_string (L"ChromiumUpdateFailed%" TEXT (PR_LONG), pbi->instance_id);

	_r_config_setboolean (key ? key->buffer : L"ChromiumUpdateFailed", is_failed);

	if (key)
		_r_obj_dereference (key);
}

BOOLEAN _app_get_update_failed (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PR_STRING key = NULL;
	BOOLEAN is_failed;

	if (!pbi)
		return FALSE;

	if (pbi->instance_id > 1)
		key = _r_format_string (L"ChromiumUpdateFailed%" TEXT (PR_LONG), pbi->instance_id);

	is_failed = _r_config_getboolean (key ? key->buffer : L"ChromiumUpdateFailed", FALSE);

	if (key)
		_r_obj_dereference (key);

	return is_failed;
}

VOID _app_set_lastcheck (
	_In_ PBROWSER_INFORMATION pbi
)
{
	PR_STRING lastcheck_key = NULL;
	LONG64 timestamp;

	if (!pbi)
		return;

	timestamp = _r_unixtime_now ();

	if (pbi->instance_id > 1)
		lastcheck_key = _r_format_string (L"ChromiumLastCheck%" TEXT (PR_LONG), pbi->instance_id);

	_r_config_setlong64 (lastcheck_key ? lastcheck_key->buffer : L"ChromiumLastCheck", timestamp);

	_app_set_update_failed (pbi, FALSE);

	if (lastcheck_key)
		_r_obj_dereference (lastcheck_key);
}

BOOLEAN _app_ishaveupdate (
	_In_ PBROWSER_INFORMATION pbi
)
{
	return !_r_obj_isstringempty (pbi->download_url) && !_r_obj_isstringempty (pbi->new_version);
}

BOOLEAN _app_isupdatedownloaded (
	_In_ PBROWSER_INFORMATION pbi
)
{
	return !_r_obj_isstringempty (pbi->cache_path) && _r_fs_exists (&pbi->cache_path->sr);
}

BOOLEAN _app_isupdaterequired (
	_In_ PBROWSER_INFORMATION pbi
)
{
	LONG64 timestamp;
	PR_STRING lastcheck_key = NULL;

	if (!_r_fs_exists (&pbi->binary_path->sr))
		return TRUE;

	if (pbi->is_forcecheck)
		return TRUE;

	if (pbi->check_period)
	{
		timestamp = _r_unixtime_now ();
		if (pbi->instance_id > 1)
			lastcheck_key = _r_format_string (L"ChromiumLastCheck%" TEXT (PR_LONG), pbi->instance_id);

		timestamp -= _r_config_getlong64 (lastcheck_key ? lastcheck_key->buffer : L"ChromiumLastCheck", 0);

		if (lastcheck_key)
			_r_obj_dereference (lastcheck_key);

		if (timestamp >= _r_calc_days2seconds (pbi->check_period))
			return TRUE;
	}

	return FALSE;
}

ULONG _app_getactionid (
	_In_ PBROWSER_INFORMATION pbi
)
{
	if (_app_isupdatedownloaded (pbi))
	{
		return IDS_ACTION_INSTALL;
	}
	else if (_app_ishaveupdate (pbi))
	{
		return IDS_ACTION_DOWNLOAD;
	}

	return IDS_ACTION_CHECK;
}

