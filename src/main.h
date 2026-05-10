// launchbro
// 2026 ctnkyaumt

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

DEFINE_GUID (GUID_TrayIcon, 0xEAD41630, 0x90BB, 0x4836, 0x82, 0x41, 0xAE, 0xAE, 0x12, 0xE8, 0x69, 0x12);

// config
#define LANG_MENU 4

#define FOOTER_STRING L"<a href=\"https://github.com/ctnkyaumt\">github.com/ctnkyaumt</a>\r\n" \
	L"<a href=\"https://chromium.woolyss.com\">chromium.woolyss.com</a>"

#define CHROMIUM_UPDATE_URL L"https://chromium.woolyss.com/api/v3/?os=windows&bit=%d&type=%s&out=string"
#define CHROMIUM_UPDATE_URL_CROMITE L"https://github.com/uazo/cromite/releases/latest/download/updateurl.txt"

#define CHROMIUM_TYPE L"ungoogled-chromium"
#define CHROMIUM_COMMAND_LINE L"--flag-switches-begin --user-data-dir=..\\profile --no-default-browser-check --disable-logging --no-report-upload --flag-switches-end"

#define LAUNCHBRO_UPDATE_URL L"https://api.github.com/repos/ctnkyaumt/launchbro/releases/latest"

typedef struct _BROWSER_INFORMATION
{
	HANDLE htaskbar;
	HANDLE hwnd;

	PR_STRING args_str;
	PR_STRING urls_str;

	PR_STRING chrome_plus_dir;
	PR_STRING browser_name;
	PR_STRING browser_type;
	PR_STRING cache_path;
	PR_STRING binary_dir;
	PR_STRING binary_path;
	PR_STRING profile_dir;
	PR_STRING download_url;
	PR_STRING current_version;
	PR_STRING new_version;

	LONG64 timestamp;
	LONG64 reserved1;

	LONG check_period;
	LONG architecture;
	LONG instance_id;

	BOOLEAN is_autodownload;
	BOOLEAN is_bringtofront;
	BOOLEAN is_deletetorecycle;
	BOOLEAN is_forcecheck;
	BOOLEAN is_hasurls;
	BOOLEAN is_onlyupdate;
	BOOLEAN is_opennewwindow;
	BOOLEAN is_newinstance;
	BOOLEAN is_taskupdate;
	BOOLEAN is_waitdownloadend;
} BROWSER_INFORMATION, *PBROWSER_INFORMATION;

BOOLEAN _app_ensure_registry_profile (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_is_registry_patched (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_patch_registry_profile (
	_In_ PBROWSER_INFORMATION pbi
);

// task update
BOOLEAN _app_taskupdate_istaskpresent ();
BOOLEAN _app_taskupdate_setstartwhenavailable ();
BOOLEAN _app_taskupdate_createtask ();
BOOLEAN _app_taskupdate_deletetask ();

// self-update
BOOLEAN _app_check_self_update (
	_In_ HWND hwnd,
	_Out_ PR_STRING* new_version_ptr,
	_Out_ PR_STRING* download_url_ptr
);

BOOLEAN _app_download_self_update (
	_In_ HWND hwnd,
	_In_ PR_STRING download_url,
	_Out_ PR_STRING* out_path_ptr
);

VOID _app_apply_self_update (
	_In_ HWND hwnd,
	_In_ PR_STRING zip_path
);

BOOLEAN _app_check_migration ();

VOID _app_perform_migration (
	_In_ HWND hwnd
);

// profile management / uninstall
VOID _app_export_profile (
	_In_ HWND hwnd
);

VOID _app_import_profile (
	_In_ HWND hwnd
);

VOID _app_uninstall_app (
	_In_ HWND hwnd
);

