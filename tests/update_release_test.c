#include <assert.h>
#include <stdio.h>
#include "../src/update_release.h"

int main (void)
{
	const wchar_t* x64 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_x64.zip";
	const wchar_t* x86 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_x86.zip";
	const wchar_t* arm64 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_arm64.zip";

	assert (_app_release_asset_score (x64, 64, RELEASE_POLICY_UNGOOGLED) >= 0);
	assert (_app_release_asset_score (x86, 32, RELEASE_POLICY_UNGOOGLED) >= 0);
	assert (_app_release_asset_score (x64, 32, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (x86, 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (arm64, 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (arm64, 32, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (x64, 0, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (NULL, 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_installer_x64.exe", 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (L"unrelated_windows_x64.zip", 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_linux_x64.zip", 64, RELEASE_POLICY_UNGOOGLED) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_windows_x64.zip.sha256", 64, RELEASE_POLICY_UNGOOGLED) == -1);

	assert (_app_release_asset_score (L"r3dfox-148.0-win64.7z", 64, RELEASE_POLICY_GENERIC) >= 0);
	assert (_app_release_asset_score (L"iceweasel-148.0-win32.zip", 32, RELEASE_POLICY_GENERIC) >= 0);
	assert (_app_release_asset_score (L"r3dfox-148.0-win32.zip", 64, RELEASE_POLICY_GENERIC) == -1);
	assert (_app_release_asset_score (L"iceweasel-148.0-arm64.zip", 64, RELEASE_POLICY_GENERIC) == -1);
	assert (_app_release_asset_score (L"source.zip", 64, RELEASE_POLICY_GENERIC) == -1);
	assert (_app_release_asset_score (L"browser-linux-x64.zip", 64, RELEASE_POLICY_GENERIC) == -1);
	assert (_app_release_asset_score (L"browser-x86_64.zip", 32, RELEASE_POLICY_GENERIC) == -1);
	assert (_app_release_asset_score (L"browser-x64-symbols.zip", 64, RELEASE_POLICY_GENERIC) == -1);

	assert (_app_release_asset_score (L"chrome.7z", 64, RELEASE_POLICY_HIBBIKI) == 10);
	assert (_app_release_asset_score (L"chrome.7z", 32, RELEASE_POLICY_HIBBIKI) == -1);
	assert (_app_release_asset_score (L"mini_installer.exe", 64, RELEASE_POLICY_HIBBIKI) == -1);
	assert (_app_release_asset_score (L"policy_templates.zip", 64, RELEASE_POLICY_HIBBIKI) == -1);

	assert (_app_release_asset_score (L"chrome.zip", 64, RELEASE_POLICY_ROBRICH) == 10);
	assert (_app_release_asset_score (L"chrome.zip", 32, RELEASE_POLICY_ROBRICH) == -1);
	assert (_app_release_asset_score (L"mini_installer.exe", 64, RELEASE_POLICY_ROBRICH) == -1);

	assert (_app_release_tag_endswith (L"v155.0.8051.0-r1695130-win64-avx2", L"-win64-avx2") == 1);
	assert (_app_release_tag_endswith (L"v155.0.8051.0-r1695130-win64-avx512", L"-win64-avx2") == 0);
	assert (_app_release_tag_endswith (L"v155.0.8051.0-r1695130-linux64-deb-avx2", L"-win64-avx2") == 0);

	assert (_app_release_chromium_version_length (L"152.0.7977.75-1.1") == wcslen (L"152.0.7977.75"));
	assert (_app_release_chromium_version_length (L"152.0.7977.75") == wcslen (L"152.0.7977.75"));
	assert (_app_release_chromium_version_length (L"152.0.7977") == 0);
	assert (_app_release_chromium_version_length (L"152..7977.75") == 0);
	assert (_app_release_chromium_version_length (L"152.0.7977.75junk") == 0);
	assert (_app_release_chromium_version_length (L"") == 0);
	assert (_app_release_chromium_version_length (NULL) == 0);
	assert (_app_release_chromium_version_length (L"156.0.8078.74-r1706539") == wcslen (L"156.0.8078.74"));

	assert (_app_release_revision_length (L"1731498") == 7);
	assert (_app_release_revision_length (L"1731498\n") == 7);
	assert (_app_release_revision_length (L"") == 0);
	assert (_app_release_revision_length (L"12a") == 0);
	assert (_app_release_revision_length (L"<html>") == 0);

	assert (_app_download_is_complete (11335889, 200807142) == 0);
	assert (_app_download_is_complete (200807142, 200807142) == 1);
	assert (_app_download_is_complete (11335889, 0) == 1);
	assert (_app_download_is_complete (0, 0) == 1);

	const unsigned char zip_hdr[] = { 'P', 'K', 0x03, 0x04, 0x14, 0x00 };
	const unsigned char zip_empty_hdr[] = { 'P', 'K', 0x05, 0x06, 0x00, 0x00 };
	const unsigned char sz_hdr[] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
	const unsigned char doctype_hdr[] = "<!DOCTYPE html>";
	const unsigned char short_hdr[] = { 'P', 'K', 0x03 };

	assert (_app_archive_detect_format (zip_hdr, sizeof (zip_hdr)) == APP_ARCHIVE_FORMAT_ZIP);
	assert (_app_archive_detect_format (zip_empty_hdr, sizeof (zip_empty_hdr)) == APP_ARCHIVE_FORMAT_ZIP);
	assert (_app_archive_detect_format (sz_hdr, sizeof (sz_hdr)) == APP_ARCHIVE_FORMAT_7Z);
	assert (_app_archive_detect_format (doctype_hdr, sizeof (doctype_hdr) - 1) == APP_ARCHIVE_FORMAT_UNKNOWN);
	assert (_app_archive_detect_format (NULL, 0) == APP_ARCHIVE_FORMAT_UNKNOWN);
	assert (_app_archive_detect_format (doctype_hdr, 0) == APP_ARCHIVE_FORMAT_UNKNOWN);
	assert (_app_archive_detect_format (short_hdr, sizeof (short_hdr)) == APP_ARCHIVE_FORMAT_UNKNOWN);

	puts ("54 release asset/version/download regression checks passed.");
	return 0;
}
