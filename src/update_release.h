// Release asset policy, shared with the dependency-free regression tests.
#pragma once

#include <wchar.h>
#include <wctype.h>

enum
{
	RELEASE_POLICY_GENERIC = 0,
	RELEASE_POLICY_UNGOOGLED,
	RELEASE_POLICY_HIBBIKI,
	RELEASE_POLICY_ROBRICH,
};

static int _app_release_contains (const wchar_t* text, const wchar_t* needle)
{
	size_t length = wcslen (needle);

	for (; *text; text++)
	{
		size_t i = 0;
		while (i < length && text[i] && towlower (text[i]) == towlower (needle[i]))
			i++;
		if (i == length)
			return 1;
	}

	return 0;
}

static int _app_release_equal_nocase (const wchar_t* a, const wchar_t* b)
{
	if (!a || !b)
		return 0;

	while (*a && *b)
	{
		if (towlower (*a) != towlower (*b))
			return 0;
		a++;
		b++;
	}

	return *a == *b;
}

static int _app_release_endswith (const wchar_t* text, const wchar_t* suffix)
{
	size_t length = wcslen (text);
	size_t suffix_length = wcslen (suffix);

	return length >= suffix_length && _app_release_contains (text + length - suffix_length, suffix);
}

static int _app_release_tag_endswith (const wchar_t* tag, const wchar_t* suffix)
{
	size_t tag_length;
	size_t suffix_length;

	if (!tag || !suffix)
		return 0;

	tag_length = wcslen (tag);
	suffix_length = wcslen (suffix);

	if (tag_length < suffix_length)
		return 0;

	return _app_release_equal_nocase (tag + tag_length - suffix_length, suffix);
}

static size_t _app_release_revision_length (const wchar_t* text)
{
	size_t digits = 0;
	size_t i = 0;

	if (!text)
		return 0;

	while (text[i] >= L'0' && text[i] <= L'9')
	{
		digits++;
		i++;
	}

	if (digits == 0)
		return 0;

	while (text[i])
	{
		if (text[i] != L' ' && text[i] != L'\t' && text[i] != L'\r' && text[i] != L'\n')
			return 0;
		i++;
	}

	return digits;
}

static int _app_release_asset_score (const wchar_t* name, int architecture, int policy)
{
	int score;
	int is_x64;
	int is_x86;

	if (!name || (architecture != 32 && architecture != 64))
		return -1;

	if (policy == RELEASE_POLICY_HIBBIKI)
	{
		if (architecture != 64)
			return -1;

		return _app_release_equal_nocase (name, L"chrome.7z") ? 10 : -1;
	}

	if (policy == RELEASE_POLICY_ROBRICH)
	{
		if (architecture != 64)
			return -1;

		return _app_release_equal_nocase (name, L"chrome.zip") ? 10 : -1;
	}

	// Only the upstream portable Windows archive is usable by our extractor.
	// In particular, ARM64 and installer assets must never win by a "64" match.
	if (policy == RELEASE_POLICY_UNGOOGLED)
	{
		if (wcsncmp (name, L"ungoogled-chromium_", 19) != 0)
			return -1;
		return _app_release_endswith (name, architecture == 64 ? L"_windows_x64.zip" : L"_windows_x86.zip") ? 10 : -1;
	}

	if (_app_release_endswith (name, L".zip"))
		score = 10;
	else if (_app_release_endswith (name, L".7z"))
		score = 9;
	else
		return -1;

	if (_app_release_contains (name, L"arm") || _app_release_contains (name, L"aarch64") ||
		_app_release_contains (name, L"linux") || _app_release_contains (name, L"macos") ||
		_app_release_contains (name, L"source") || _app_release_contains (name, L"symbols"))
		return -1;

	is_x64 = _app_release_contains (name, L"win64") || _app_release_contains (name, L"x64") ||
		_app_release_contains (name, L"x86_64") || _app_release_contains (name, L"amd64");
	is_x86 = !is_x64 && (_app_release_contains (name, L"win32") || _app_release_contains (name, L"x86") ||
		_app_release_contains (name, L"i686"));

	if ((architecture == 64 && !is_x64) || (architecture == 32 && !is_x86))
		return -1;

	return score + (_app_release_contains (name, L"portable") ? 2 : 0);
}

// The upstream package revision (e.g. -1.1) is not part of chrome.exe's
// four-component file version. Comparing it causes repeated update offers.
static size_t _app_release_chromium_version_length (const wchar_t* tag)
{
	size_t i = 0;
	int part;

	if (!tag)
		return 0;

	for (part = 0; part < 4; part++)
	{
		size_t start = i;
		while (tag[i] >= L'0' && tag[i] <= L'9')
			i++;
		if (i == start)
			return 0;
		if (part < 3 && tag[i++] != L'.')
			return 0;
	}

	return tag[i] == L'\0' || tag[i] == L'-' ? i : 0;
}
