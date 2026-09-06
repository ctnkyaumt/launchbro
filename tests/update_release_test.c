#include <assert.h>
#include <stdio.h>
#include "../src/update_release.h"

int main (void)
{
	const wchar_t* x64 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_x64.zip";
	const wchar_t* x86 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_x86.zip";
	const wchar_t* arm64 = L"ungoogled-chromium_152.0.7977.75-1.1_windows_arm64.zip";

	assert (_app_release_asset_score (x64, 64, 1) >= 0);
	assert (_app_release_asset_score (x86, 32, 1) >= 0);
	assert (_app_release_asset_score (x64, 32, 1) == -1);
	assert (_app_release_asset_score (x86, 64, 1) == -1);
	assert (_app_release_asset_score (arm64, 64, 1) == -1);
	assert (_app_release_asset_score (arm64, 32, 1) == -1);
	assert (_app_release_asset_score (x64, 0, 1) == -1);
	assert (_app_release_asset_score (NULL, 64, 1) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_installer_x64.exe", 64, 1) == -1);
	assert (_app_release_asset_score (L"unrelated_windows_x64.zip", 64, 1) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_linux_x64.zip", 64, 1) == -1);
	assert (_app_release_asset_score (L"ungoogled-chromium_152.0_windows_x64.zip.sha256", 64, 1) == -1);

	assert (_app_release_asset_score (L"r3dfox-148.0-win64.7z", 64, 0) >= 0);
	assert (_app_release_asset_score (L"iceweasel-148.0-win32.zip", 32, 0) >= 0);
	assert (_app_release_asset_score (L"r3dfox-148.0-win32.zip", 64, 0) == -1);
	assert (_app_release_asset_score (L"iceweasel-148.0-arm64.zip", 64, 0) == -1);
	assert (_app_release_asset_score (L"source.zip", 64, 0) == -1);
	assert (_app_release_asset_score (L"browser-linux-x64.zip", 64, 0) == -1);
	assert (_app_release_asset_score (L"browser-x86_64.zip", 32, 0) == -1);
	assert (_app_release_asset_score (L"browser-x64-symbols.zip", 64, 0) == -1);

	assert (_app_release_chromium_version_length (L"152.0.7977.75-1.1") == wcslen (L"152.0.7977.75"));
	assert (_app_release_chromium_version_length (L"152.0.7977.75") == wcslen (L"152.0.7977.75"));
	assert (_app_release_chromium_version_length (L"152.0.7977") == 0);
	assert (_app_release_chromium_version_length (L"152..7977.75") == 0);
	assert (_app_release_chromium_version_length (L"152.0.7977.75junk") == 0);
	assert (_app_release_chromium_version_length (L"") == 0);
	assert (_app_release_chromium_version_length (NULL) == 0);

	puts ("27 release asset/version regression checks passed.");
	return 0;
}
