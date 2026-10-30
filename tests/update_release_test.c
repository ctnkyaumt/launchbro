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

	puts ("43 release asset/version regression checks passed.");
	return 0;
}
