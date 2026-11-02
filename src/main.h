// launchbro
// 2026 ctnkyaumt

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

DEFINE_GUID (GUID_TrayIcon, 0xEAD41630, 0x90BB, 0x4836, 0x82, 0x41, 0xAE, 0xAE, 0x12, 0xE8, 0x69, 0x12);

// config
#define LANG_MENU 6

#define FOOTER_STRING L"<a href=\"https://github.com/ctnkyaumt\">github.com/ctnkyaumt</a>\r\n" \
	L"<a href=\"https://github.com/macchrome/winchrome/releases\">ungoogled (64-bit)</a> / <a href=\"https://github.com/ungoogled-software/ungoogled-chromium-windows/releases\">(32-bit)</a>"

#define CHROMIUM_UPDATE_URL_MACCHROME L"https://api.github.com/repos/macchrome/winchrome/releases?per_page=20"

#define CHROMIUM_UPDATE_URL_CROMITE L"https://github.com/uazo/cromite/releases/latest/download/updateurl.txt"
#define CHROMIUM_UPDATE_URL_UNGOOGLED L"https://api.github.com/repos/ungoogled-software/ungoogled-chromium-windows/releases/latest"
#define CHROMIUM_UPDATE_URL_HIBBIKI L"https://api.github.com/repos/Hibbiki/chromium-win64/releases/latest"
#define CHROMIUM_UPDATE_URL_ROBRICH L"https://api.github.com/repos/RobRich999/Chromium_Clang/releases?per_page=50"
#define CHROMIUM_SNAPSHOT_URL L"https://storage.googleapis.com/chromium-browser-snapshots/%s/"
#define UPDATE_URL_R3DFOX L"https://api.github.com/repos/Eclipse-Community/r3dfox/releases/latest"
#define UPDATE_URL_ICEWEASEL L"https://api.github.com/repos/adonais/iceweasel/releases/latest"

#define CHROMIUM_TYPE L"ungoogled-chromium"
#define CHROMIUM_COMMAND_LINE L"--flag-switches-begin --user-data-dir=..\\profile --no-default-browser-check --disable-logging --no-report-upload --flag-switches-end"

#define LAUNCHBRO_UPDATE_URL L"https://api.github.com/repos/ctnkyaumt/launchbro/releases/latest"

typedef enum _APP_BROWSER_KIND
{
	BROWSER_KIND_UNKNOWN = 0,
	BROWSER_KIND_UNGOOGLED,
	BROWSER_KIND_CROMITE,
	BROWSER_KIND_STABLE_CODECS_SYNC,
	BROWSER_KIND_DEV_NOSYNC,
	BROWSER_KIND_DEV_OFFICIAL,
	BROWSER_KIND_R3DFOX,
	BROWSER_KIND_ICEWEASEL,
} APP_BROWSER_KIND;

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
	LONG progress_percent;

	ULONG64 download_written;
	ULONG64 download_total;

	BOOLEAN is_autodownload;
	BOOLEAN is_bringtofront;
	BOOLEAN is_cleanup;
	BOOLEAN is_deletetorecycle;
	BOOLEAN is_forcecheck;
	BOOLEAN is_forcedownload;
	BOOLEAN is_hasargs;
	BOOLEAN is_hasurls;
	BOOLEAN is_onlyupdate;
	BOOLEAN is_opennewwindow;
	BOOLEAN is_newinstance;
	BOOLEAN is_taskupdate;
	BOOLEAN is_waitdownloadend;
} BROWSER_INFORMATION, *PBROWSER_INFORMATION;

VOID _app_init_browser_info (
	_Inout_ PBROWSER_INFORMATION pbi
);

APP_BROWSER_KIND _app_get_browser_kind (
	_In_opt_ PR_STRING browser_type
);

BOOLEAN _app_is_firefox_kind (
	_In_ APP_BROWSER_KIND kind
);

PR_STRING _app_get_instance_type (
	_In_ LONG instance_id,
	_In_opt_ LPCWSTR default_type
);

BOOLEAN _app_is_firefox_fork (
	_In_ PBROWSER_INFORMATION pbi
);

PR_STRING _app_json_find_string_value (
	_In_ LPCWSTR json,
	_In_ LPCWSTR key
);

BOOLEAN _app_ensure_registry_profile (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_maybe_create_shortcut (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ BOOLEAN is_firstinstall
);

VOID _app_try_stamp_shortcut_aumid (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_is_registry_patched (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_patch_registry_profile (
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_set_update_failed (
	_In_ PBROWSER_INFORMATION pbi,
	_In_ BOOLEAN is_failed
);

BOOLEAN _app_get_update_failed (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_is_default_browser_other (
	_In_ PBROWSER_INFORMATION pbi
);

VOID _app_unpatch_registry_associations (VOID);

BOOLEAN _app_dir_is_profile (
	_In_ LPCWSTR dir
);

// task update
BOOLEAN _app_taskupdate_istaskpresent ();
BOOLEAN _app_taskupdate_setstartwhenavailable ();
BOOLEAN _app_taskupdate_createtask ();
BOOLEAN _app_taskupdate_deletetask ();
BOOLEAN _app_taskupdate_terminateprocesses (
	_In_ PBROWSER_INFORMATION pbi
);

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

VOID _app_setstatus (
	_In_ HWND hwnd,
	_In_opt_ HWND htaskbar,
	_In_opt_ PBROWSER_INFORMATION pbi,
	_In_opt_ LPCWSTR string,
	_In_opt_ ULONG64 total_read,
	_In_opt_ ULONG64 total_length
);

VOID _app_seticonstate (
	_In_ HWND hwnd,
	_In_opt_ PBROWSER_INFORMATION pbi,
	_In_ BOOLEAN is_busy,
	_In_ BOOLEAN is_forced
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

VOID _app_cleanup_system_changes (VOID);

PR_STRING _app_get_shortcut_path (
	_In_ PBROWSER_INFORMATION pbi
);

BOOLEAN _app_is_instance_configured (
	_In_ LONG instance_id
);

VOID _app_clear_browser_info_references (
	_Inout_ PBROWSER_INFORMATION pbi
);
