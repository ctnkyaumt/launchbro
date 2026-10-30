// launchbro
// 2026 ctnkyaumt

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "CpuArch.h"

#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zWindows.h"

#include "miniz.h"

extern R_QUEUED_LOCK lock_download;

VOID _app_setstatus (
	_In_ HWND hwnd,
	_In_opt_ HWND htaskbar,
	_In_opt_ PBROWSER_INFORMATION pbi,
	_In_opt_ LPCWSTR string,
	_In_opt_ ULONG64 total_read,
	_In_opt_ ULONG64 total_length
);

static BOOLEAN _app_is_entry_path_safe (
	_In_ LPCWSTR path
)
{
	if (!path || !*path)
		return FALSE;

	if (path[0] == L'\\' || path[0] == L'/' || wcschr (path, L':') != NULL)
		return FALSE;

	for (SIZE_T i = 0; path[i] != L'\0'; i++)
	{
		if (path[i] == L'.' && path[i + 1] == L'.')
		{
			BOOLEAN is_start = (i == 0 || path[i - 1] == L'\\' || path[i - 1] == L'/');
			BOOLEAN is_end = (path[i + 2] == L'\0' || path[i + 2] == L'\\' || path[i + 2] == L'/');

			if (is_start && is_end)
				return FALSE;
		}
	}

	return TRUE;
}

SRes _app_unpack_7zip (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_In_ PR_STRINGREF bin_name
)
{
#define kInputBufSize ((ULONG_PTR)1 << 18)

	static R_STRINGREF separator_sr = PR_STRINGREF_INIT (L"\\");
	static const ISzAlloc g_Alloc = {SzAlloc, SzFree};

	ISzAlloc alloc_imp = g_Alloc;
	ISzAlloc alloc_temp_imp = g_Alloc;
	CFileInStream archive_stream = {0};
	CLookToRead2 look_stream;
	CSzArEx db;
	ULONG_PTR temp_size = 0;
	LPWSTR temp_buff = NULL;

	// if you need cache, use these 3 variables.
	// if you use external function, you can make these variable as static.

	UInt32 block_index = UINT32_MAX; // it can have any value before first call (if out_buffer = 0)
	Byte *out_buffer = NULL; // it must be 0 before first call for each new archive.
	ULONG_PTR out_buffer_size = 0; // it can have any value before first call (if out_buffer = 0)
	R_STRINGREF path;
	PR_STRING root_dir_name = NULL;
	PR_STRING dest_path;
	PR_STRING sub_dir;
	CSzFile out_file;
	ULONG_PTR offset;
	ULONG_PTR out_size_processed;
	UInt32 attrib;
	UInt64 total_size = 0;
	UInt64 total_read = 0;
	LONG last_percent = -1;
	ULONG_PTR processed_size;
	ULONG_PTR length;
	BOOLEAN is_success = FALSE;
	LONG status;

	status = InFile_OpenW (&archive_stream.file, pbi->cache_path->buffer);

	if (status != ERROR_SUCCESS)
	{
		_r_show_errormessage (hwnd, NULL, status, pbi->cache_path->buffer, ET_WINDOWS);

		return status;
	}

	FileInStream_CreateVTable (&archive_stream);
	LookToRead2_CreateVTable (&look_stream, 0);

	SzArEx_Init (&db);

	look_stream.buf = (PUCHAR)ISzAlloc_Alloc (&alloc_imp, kInputBufSize);

	if (!look_stream.buf)
	{
		_r_show_errormessage (hwnd, NULL, STATUS_NO_MEMORY, L"ISzAlloc_Alloc", ET_NATIVE);

		goto CleanupExit;
	}

	look_stream.bufSize = kInputBufSize;
	look_stream.realStream = &archive_stream.vt;

	LookToRead2_INIT (&look_stream);

	CrcGenerateTable ();

	status = SzArEx_Open (&db, &look_stream.vt, &alloc_imp, &alloc_temp_imp);

	if (status != SZ_OK)
	{
		_r_show_errormessage (hwnd, NULL, status, L"SzArEx_Open", ET_NONE);

		goto CleanupExit;
	}

	// find root directory which contains main executable
	for (ULONG_PTR i = 0; i < db.NumFiles; i++)
	{
		if (SzArEx_IsDir (&db, i))
			continue;

		length = SzArEx_GetFileNameUtf16 (&db, i, NULL);
		total_size += SzArEx_GetFileSize (&db, i);

		if (length > temp_size)
		{
			temp_size = length;

			if (temp_buff)
			{
				temp_buff = _r_mem_reallocate (temp_buff, temp_size * sizeof (UInt16));
			}
			else
			{
				temp_buff = _r_mem_allocate (temp_size * sizeof (UInt16));
			}
		}

		if (!root_dir_name)
		{
			length = SzArEx_GetFileNameUtf16 (&db, i, temp_buff);

			if (!length)
				continue;

			_r_obj_initializestringref_ex (&path, temp_buff, (length - 1) * sizeof (WCHAR));

			_r_str_replacechar (&path, OBJ_NAME_ALTPATH_SEPARATOR, OBJ_NAME_PATH_SEPARATOR);

			length = path.length - bin_name->length - separator_sr.length;

			if (path.length > bin_name->length && _r_str_isendsswith (&path, bin_name, TRUE) && path.buffer[length / sizeof (WCHAR)] == OBJ_NAME_PATH_SEPARATOR)
			{
				_r_obj_movereference ((PVOID_PTR)&root_dir_name, _r_obj_createstring_ex (path.buffer, path.length - bin_name->length));

				_r_str_trimstring (&root_dir_name->sr, &separator_sr, 0);
			}
		}
	}

	for (ULONG_PTR i = 0; i < db.NumFiles; i++)
	{
		length = SzArEx_GetFileNameUtf16 (&db, i, temp_buff);

		if (!length)
			continue;

		if (!_app_is_entry_path_safe (temp_buff))
		{
			status = SZ_ERROR_DATA;
			_r_show_errormessage (hwnd, NULL, status, L"Invalid archive path", ET_NONE);
			break;
		}

		_r_obj_initializestringref_ex (&path, temp_buff, (length - 1) * sizeof (WCHAR));

		_r_str_replacechar (&path, OBJ_NAME_ALTPATH_SEPARATOR, OBJ_NAME_PATH_SEPARATOR);

		_r_str_trimstring (&path, &separator_sr, 0);

		// skip non-root dirs
		if (!_r_obj_isstringempty (root_dir_name) && (path.length <= root_dir_name->length || !_r_str_isstartswith (&path, &root_dir_name->sr, TRUE)))
			continue;

		if (root_dir_name)
			_r_str_skiplength (&path, root_dir_name->length + separator_sr.length);

		dest_path = _r_obj_concatstringrefs (
			3,
			&pbi->binary_dir->sr,
			&separator_sr,
			&path
		);

		if (SzArEx_IsDir (&db, i))
		{
			_r_fs_createdirectory (&dest_path->sr);
		}
		else
		{
			total_read += SzArEx_GetFileSize (&db, i);

			LONG percent = total_size ? (LONG)((total_read * 100) / total_size) : 0;

			if (percent != last_percent)
			{
				last_percent = percent;
				_app_setstatus (hwnd, pbi->htaskbar, pbi, _r_locale_getstring (IDS_STATUS_INSTALL), total_read, total_size);
			}

			// create directory if not-exist
			sub_dir = _r_path_getbasedirectory (&dest_path->sr);

			if (sub_dir)
			{
				if (!_r_fs_exists (&sub_dir->sr))
					_r_fs_createdirectory (&sub_dir->sr);

				_r_obj_dereference (sub_dir);
			}

			offset = 0;
			out_size_processed = 0;

			status = SzArEx_Extract (&db, &look_stream.vt, (UINT)i, &block_index, &out_buffer, &out_buffer_size, &offset, &out_size_processed, &alloc_imp, &alloc_temp_imp);

			if (status != SZ_OK)
			{
				_r_show_errormessage (hwnd, NULL, status, L"SzArEx_Extract", ET_NONE);
				_r_obj_dereference (dest_path);
				break;
			}

			status = OutFile_OpenW (&out_file, dest_path->buffer);

			if (status != SZ_OK)
			{
				if (status != SZ_ERROR_CRC)
					_r_show_errormessage (hwnd, NULL, status, L"OutFile_OpenW", ET_NONE);
				_r_obj_dereference (dest_path);
				break;
			}

			processed_size = out_size_processed;

			status = File_Write (&out_file, out_buffer + offset, &processed_size);

			if (status != SZ_OK || processed_size != out_size_processed)
			{
				if (status == SZ_OK)
					status = SZ_ERROR_WRITE;

				_r_show_errormessage (hwnd, NULL, status, L"File_Write", ET_NONE);
				File_Close (&out_file);
				_r_obj_dereference (dest_path);
				break;
			}

			if (SzBitWithVals_Check (&db.Attribs, i))
			{
				attrib = db.Attribs.Vals[i];

				//	p7zip stores posix attributes in high 16 bits and adds 0x8000 as marker.
				//	We remove posix bits, if we detect posix mode field
				if ((attrib & 0xF0000000) != 0)
					attrib &= 0x7FFF;

				_r_fs_setattributes (dest_path->buffer, NULL, attrib);
			}

			File_Close (&out_file);
		}

		_r_obj_dereference (dest_path);

		if (!is_success)
			is_success = TRUE;
	}

CleanupExit:

	if (root_dir_name)
		_r_obj_dereference (root_dir_name);

	if (out_buffer)
		ISzAlloc_Free (&alloc_imp, out_buffer);

	if (look_stream.buf)
		ISzAlloc_Free (&alloc_imp, look_stream.buf);

	if (temp_buff)
		_r_mem_free (temp_buff);

	SzArEx_Free (&db, &alloc_imp);

	File_Close (&archive_stream.file);

	return status;
}

BOOLEAN _app_unpack_zip (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_In_ PR_STRINGREF bin_name,
	_Out_opt_ PBOOLEAN is_not_zip_ptr
)
{
	static R_STRINGREF separator_sr = PR_STRINGREF_INIT (L"\\");

	mz_zip_archive_file_stat file_stat;
	mz_zip_archive zip_archive = {0};
	PR_STRING root_dir_name = NULL;
	R_BYTEREF path_sr;
	PR_STRING path;
	PR_STRING dest_path;
	PR_STRING sub_dir;
	ULONG64 total_size = 0;
	ULONG64 total_read = 0;
	LONG last_percent = -1;
	ULONG_PTR length;
	UINT total_files;
	BOOLEAN is_success = FALSE;
	NTSTATUS status;

	if (!mz_zip_reader_init_file_v2 (&zip_archive, pbi->cache_path->buffer, 0, 0, 0))
	{
		if (is_not_zip_ptr)
			*is_not_zip_ptr = TRUE;

		return FALSE;
	}

	if (is_not_zip_ptr)
		*is_not_zip_ptr = FALSE;

	total_files = mz_zip_reader_get_num_files (&zip_archive);

	// find root directory which contains main executable
	for (UINT i = 0; i < total_files; i++)
	{
		if (mz_zip_reader_is_file_a_directory (&zip_archive, i) || !mz_zip_reader_file_stat (&zip_archive, i, &file_stat))
			continue;

		if (file_stat.m_is_directory)
			continue;

		// count total size of unpacked files
		total_size += file_stat.m_uncomp_size;

		if (!root_dir_name)
		{
			_r_obj_initializebyteref (&path_sr, file_stat.m_filename);

			status = _r_str_multibyte2unicode (&path_sr, &path);

			if (!NT_SUCCESS (status))
				continue;

			_r_str_replacechar (&path->sr, OBJ_NAME_ALTPATH_SEPARATOR, OBJ_NAME_PATH_SEPARATOR);

			length = path->length - bin_name->length - separator_sr.length;

			if (path->length > bin_name->length && _r_str_isendsswith (&path->sr, bin_name, TRUE) && path->buffer[length / sizeof (WCHAR)] == OBJ_NAME_PATH_SEPARATOR)
			{
				_r_obj_movereference ((PVOID_PTR)&root_dir_name, _r_obj_createstring_ex (path->buffer, path->length - bin_name->length));

				_r_str_trimstring (&root_dir_name->sr, &separator_sr, 0);
			}

			_r_obj_dereference (path);
		}
	}

	is_success = (total_files > 0);

	for (UINT i = 0; i < total_files; i++)
	{
		if (!mz_zip_reader_file_stat (&zip_archive, i, &file_stat))
		{
			_r_show_errormessage (hwnd, NULL, STATUS_UNSUCCESSFUL, L"mz_zip_reader_file_stat", ET_NATIVE);
			is_success = FALSE;
			break;
		}

		_r_obj_initializebyteref (&path_sr, file_stat.m_filename);

		status = _r_str_multibyte2unicode (&path_sr, &path);

		if (!NT_SUCCESS (status))
		{
			_r_show_errormessage (hwnd, NULL, status, L"_r_str_multibyte2unicode", ET_NATIVE);
			is_success = FALSE;
			break;
		}

		if (!_app_is_entry_path_safe (path->buffer))
		{
			_r_show_errormessage (hwnd, NULL, STATUS_DATA_ERROR, L"Invalid archive path", ET_NATIVE);
			_r_obj_dereference (path);
			is_success = FALSE;
			break;
		}

		_r_str_replacechar (&path->sr, OBJ_NAME_ALTPATH_SEPARATOR, OBJ_NAME_PATH_SEPARATOR);

		_r_str_trimstring (&path->sr, &separator_sr, 0);

		// skip non-root dirs
		if (!_r_obj_isstringempty (root_dir_name) && (path->length <= root_dir_name->length || !_r_str_isstartswith (&path->sr, &root_dir_name->sr, TRUE)))
		{
			_r_obj_dereference (path);
			continue;
		}

		if (root_dir_name)
			_r_str_skiplength (&path->sr, root_dir_name->length + separator_sr.length);

		dest_path = _r_obj_concatstringrefs (
			3,
			&pbi->binary_dir->sr,
			&separator_sr,
			&path->sr
		);

		_r_obj_dereference (path);

		if (mz_zip_reader_is_file_a_directory (&zip_archive, i))
		{
			_r_fs_createdirectory (&dest_path->sr);
		}
		else
		{
			sub_dir = _r_path_getbasedirectory (&dest_path->sr);

			if (sub_dir)
			{
				if (!_r_fs_exists (&sub_dir->sr))
					_r_fs_createdirectory (&sub_dir->sr);

				_r_obj_dereference (sub_dir);
			}

			if (!mz_zip_reader_extract_to_file (&zip_archive, i, dest_path->buffer, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
			{
				_r_show_errormessage (hwnd, NULL, STATUS_UNSUCCESSFUL, dest_path->buffer, ET_NATIVE);
				_r_obj_dereference (dest_path);
				is_success = FALSE;
				break;
			}

			total_read += file_stat.m_uncomp_size;

			LONG percent = total_size ? (LONG)((total_read * 100) / total_size) : 0;

			if (percent != last_percent)
			{
				last_percent = percent;
				_app_setstatus (hwnd, pbi->htaskbar, pbi, _r_locale_getstring (IDS_STATUS_INSTALL), total_read, total_size);
			}
		}

		_r_obj_dereference (dest_path);
	}

	if (root_dir_name)
		_r_obj_dereference (root_dir_name);

	mz_zip_reader_end (&zip_archive);

	return is_success;
}

static BOOLEAN _app_swap_directory (
	_In_ LPCWSTR source_dir,
	_In_ LPCWSTR target_dir
)
{
	if (!source_dir || !target_dir)
		return FALSE;

	return MoveFileExW (source_dir, target_dir, MOVEFILE_WRITE_THROUGH) ? TRUE : FALSE;
}

BOOLEAN _app_installupdate (
	_In_ HWND hwnd,
	_In_ PBROWSER_INFORMATION pbi,
	_Out_ PBOOLEAN is_error_ptr
)
{
	R_STRINGREF bin_name;
	PR_STRING buffer1 = NULL;
	PR_STRING buffer2 = NULL;
	PR_STRING backup_dir = NULL;
	PR_STRING staged_dir = NULL;
	PR_STRING parent_dir = NULL;
	BROWSER_INFORMATION install_info;
	BOOLEAN had_existing_dir = FALSE;
	ULONG win32_error = ERROR_SUCCESS;
	NTSTATUS status;

	_r_queuedlock_acquireshared (&lock_download);

	_r_path_getpathinfo (&pbi->binary_path->sr, NULL, &bin_name);

	_r_sys_setthreadexecutionstate (ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

	backup_dir = _r_format_string (L"%s.old", pbi->binary_dir->buffer);
	staged_dir = _r_format_string (L"%s.new", pbi->binary_dir->buffer);

	if (!backup_dir || !staged_dir)
	{
		status = STATUS_NO_MEMORY;
		goto CleanupExit;
	}

	// (0) if bin does not exist but bin.old does (interrupted earlier run), rename bin.old back to bin first
	if (!_r_fs_exists (&pbi->binary_dir->sr) && _r_fs_exists (&backup_dir->sr))
	{
		_app_swap_directory (backup_dir->buffer, pbi->binary_dir->buffer);
	}

	// (1) delete stale bin.new/bin.old
	status = _r_fs_deletedirectory (&backup_dir->sr, TRUE);

	if (!NT_SUCCESS (status) && status != STATUS_OBJECT_NAME_NOT_FOUND)
		_r_log (LOG_LEVEL_ERROR, NULL, L"_r_fs_deletedirectory", status, backup_dir->buffer);

	status = _r_fs_deletedirectory (&staged_dir->sr, TRUE);

	if (!NT_SUCCESS (status) && status != STATUS_OBJECT_NAME_NOT_FOUND)
		_r_log (LOG_LEVEL_ERROR, NULL, L"_r_fs_deletedirectory", status, staged_dir->buffer);

	// (2) extract into bin.new
	parent_dir = _r_path_getbasedirectory (&pbi->binary_dir->sr);

	if (parent_dir)
	{
		if (!_r_fs_exists (&parent_dir->sr))
			_r_fs_createdirectory (&parent_dir->sr);

		_r_obj_dereference (parent_dir);
		parent_dir = NULL;
	}

	_r_fs_createdirectory (&staged_dir->sr);

	install_info = *pbi;
	install_info.binary_dir = staged_dir;

	{
		BOOLEAN is_not_zip = FALSE;

		if (_app_unpack_zip (hwnd, &install_info, &bin_name, &is_not_zip))
		{
			status = SZ_OK;
		}
		else if (is_not_zip)
		{
			status = _app_unpack_7zip (hwnd, &install_info, &bin_name);
		}
		else
		{
			status = SZ_ERROR_DATA;
		}
	}

	// (3) verify the binary exists in bin.new
	if (status == SZ_OK)
	{
		PR_STRING staged_binary = _r_format_string (L"%s\\%s", staged_dir->buffer, bin_name.buffer);

		if (staged_binary)
		{
			if (!_r_fs_exists (&staged_binary->sr))
			{
				_r_show_errormessage (hwnd, NULL, STATUS_OBJECT_NAME_NOT_FOUND, staged_binary->buffer, ET_NATIVE);
				status = STATUS_OBJECT_NAME_NOT_FOUND;
			}

			_r_obj_dereference (staged_binary);
		}
	}

	// (4) rename bin -> bin.old (if bin exists)
	if (status == SZ_OK)
	{
		had_existing_dir = _r_fs_exists (&pbi->binary_dir->sr);

		if (had_existing_dir)
		{
			if (!_app_swap_directory (pbi->binary_dir->buffer, backup_dir->buffer))
			{
				win32_error = GetLastError ();
				_r_show_errormessage (hwnd, NULL, win32_error, pbi->binary_dir->buffer, ET_WINDOWS);
				status = (NTSTATUS)win32_error;
			}
		}
	}

	// (5) rename bin.new -> bin; if (5) fails, rename bin.old back
	if (status == SZ_OK)
	{
		if (!_app_swap_directory (staged_dir->buffer, pbi->binary_dir->buffer))
		{
			win32_error = GetLastError ();
			_r_show_errormessage (hwnd, NULL, win32_error, staged_dir->buffer, ET_WINDOWS);
			status = (NTSTATUS)win32_error;

			if (had_existing_dir)
			{
				if (!_app_swap_directory (backup_dir->buffer, pbi->binary_dir->buffer))
				{
					ULONG rollback_err = GetLastError ();
					_r_log (LOG_LEVEL_ERROR, NULL, L"MoveFileExW", rollback_err, backup_dir->buffer);
				}
			}
		}
	}

	// get new version
	if (status == SZ_OK)
		_r_obj_movereference ((PVOID_PTR)&pbi->current_version, _r_res_queryversionstring (pbi->binary_path->buffer));

	if (status == SZ_OK && _app_get_browser_kind (pbi->browser_type) == BROWSER_KIND_DEV_OFFICIAL && !_r_obj_isstringempty (pbi->new_version))
	{
		PR_STRING rev_key = NULL;

		if (pbi->instance_id >= 2 && pbi->instance_id <= 4)
			rev_key = _r_format_string (L"ChromiumSnapshotRevision%" TEXT (PR_LONG), pbi->instance_id);

		_r_config_setstring (rev_key ? rev_key->buffer : L"ChromiumSnapshotRevision", pbi->new_version->buffer);

		if (rev_key)
			_r_obj_dereference (rev_key);
	}

	// remove cache file when zip cannot be opened
	_r_fs_deletefile (&pbi->cache_path->sr, NULL);

	if (status == SZ_OK && !_r_obj_isstringempty (pbi->chrome_plus_dir) && _r_fs_exists (&pbi->chrome_plus_dir->sr))
	{
		buffer1 = _r_format_string (L"%s\\version.dll", pbi->chrome_plus_dir->buffer);
		buffer2 = _r_format_string (L"%s\\version.dll", pbi->binary_dir->buffer);

		if (_r_fs_exists (&buffer1->sr))
			_r_fs_copyfile (&buffer1->sr, &buffer2->sr, FALSE);

		_r_obj_movereference ((PVOID_PTR)&buffer1, _r_format_string (L"%s\\chrome++.ini", pbi->chrome_plus_dir->buffer));
		_r_obj_movereference ((PVOID_PTR)&buffer2, _r_format_string (L"%s\\chrome++.ini", pbi->binary_dir->buffer));

		if (_r_fs_exists (&buffer1->sr))
			_r_fs_copyfile (&buffer1->sr, &buffer2->sr, FALSE);

		_r_obj_dereference (buffer1);
		_r_obj_dereference (buffer2);
	}

CleanupExit:

	if (status == SZ_OK)
	{
		if (backup_dir)
			_r_fs_deletedirectory (&backup_dir->sr, TRUE);
	}
	else
	{
		if (staged_dir)
			_r_fs_deletedirectory (&staged_dir->sr, TRUE);

		if (had_existing_dir && backup_dir && !_r_fs_exists (&pbi->binary_dir->sr))
		{
			if (!_app_swap_directory (backup_dir->buffer, pbi->binary_dir->buffer))
			{
				win32_error = GetLastError ();
				_r_log (LOG_LEVEL_ERROR, NULL, L"MoveFileExW", win32_error, backup_dir->buffer);
			}
		}
	}

	if (backup_dir)
		_r_obj_dereference (backup_dir);

	if (staged_dir)
		_r_obj_dereference (staged_dir);

	if (parent_dir)
		_r_obj_dereference (parent_dir);

	*is_error_ptr = (status != SZ_OK);

	_r_queuedlock_releaseshared (&lock_download);

	_r_sys_setthreadexecutionstate (ES_CONTINUOUS);

	_app_setstatus (hwnd, pbi->htaskbar, pbi, NULL, 0, 0);

	return (status == SZ_OK) ? TRUE : FALSE;
}

