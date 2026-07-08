v2.9.10 (8 July 2026)
- fixed "Run at end" appearing to do nothing: ChromiumUpdateOnly's shipped/fallback default (true) contradicted its own documented default ("false -> update & start Chromium (default)"), so is_onlyupdate was TRUE on every normal launch and silently blocked every _app_openbrowser call gated on it, regardless of the Run at end setting. Default now matches the documentation (false); the /update command-line flag still forces update-only mode when explicitly requested.
- fixed the scheduled auto-update task potentially leaving a browser closed after updating: the relaunch-with-previous-tabs step skipped reopening entirely when a browser instance's command-line args happened to be empty, instead of still relaunching it with --restore-last-session.
- fixed the taskbar showing two separate icons (pinned + running) instead of unifying them: the shortcut no longer declares a custom AppUserModelID at all. Clicking a taskbar pin launches chrome.exe directly, bypassing launchbro.exe entirely, so nothing launchbro does at launch time can stamp a matching ID onto that window - the only thing that reliably matches in every case (pinned-direct-launch included) is Windows' own default, path-derived AppUserModelID, which the pin and the window now both fall back to since neither overrides it. Each instance still gets its own bin/bin2/bin3/bin4 folder, so that default still keeps separate profile instances visually distinct.
- fixed the browser launching a second, unexpected time whenever launchbro's window was closed after an earlier run had left it open (e.g. after an update install): the end-of-run launch call didn't check ChromiumWaitForDownloadEnd, so with that option at its default (on) both it AND the WM_DESTROY-triggered launch could fire - the second one arriving late, whenever the window actually got closed.
- fixed the uninstaller (installer.nsi) showing "2.7" in Programs and Features - that version string was hardcoded and never tracked the real app version; it's now kept in sync automatically by the build/release workflows, same as the exe's own version.
- fixed the uninstaller briefly flashing a visible PowerShell window with red error text while closing running browser processes - it now runs hidden with broader error suppression (only Stop-Process had -ErrorAction set before; the Get-CimInstance/WMI query itself did not).
- fixed self-update silently failing after installing via setup.exe (downloads/unpacks but never installs, then "path not found" for chrome.exe on exit): the installer put launchbro in Program Files, which requires admin to write to, while launchbro.exe itself intentionally runs unelevated (asInvoker) and needs to keep writing into its own install folder to self-update. The installer now installs per-user under Documents (no admin/UAC needed to install or to keep updating afterward) and its uninstall registry entries moved from HKLM to HKCU to match. Existing Program Files installs need to be removed via their own uninstall.exe first, then reinstalled with the new setup.exe.
- fixed launchbro.exe being unreachable: the Start Menu shortcut used to auto-run the full check/download/launch-browser flow on every open, closing the window (and launching the browser) before you could ever see or use it. A bare launch (no command-line arguments at all) now just shows the window and waits - click Start yourself to check for updates. Anything invoked with arguments (the scheduled task's /taskupdate, a URL routed through launchbro.exe, /update, etc.) keeps auto-running exactly as before. The Desktop shortcut, which launches the browser directly, is unaffected either way.
- audited remaining "chrlauncher" references: all are intentional (migration detection and legacy-install cleanup, which must name the old folder to find it) - no stray branding left.
- audited remaining "henrypp" references: all are legitimate URLs to the actual upstream routine/builder libraries this project depends on - no stray branding, changing them would break the build.

v2.9.9 (8 July 2026)
- fixed the "Chromium needs to run once to register as a browser" dialog reappearing on every single launch: it now asks (and does the launch-and-close registration dance) at most once. The common cause is that Windows' default browser was never actually set to Chromium via Settings - relaunching Chromium cannot fix that by itself, only the user can, so nagging every startup accomplished nothing. The silent, non-prompting registry patch still runs on every launch and self-applies automatically the moment the user does set Chromium as default.

v2.9.8 (8 July 2026)
- fixed default-browser links doing nothing (and the registry patch refusing to apply / re-prompting "run once") after the chrlauncher->launchbro migration: the http/https handler still pointed at the old chrlauncher chrome.exe path. launchbro now recognizes a stale reference to its own moved browser (same exe name, path no longer on disk), repoints the command to the current binary, and injects the profile. A real browser present on disk (e.g. Google Chrome) is never touched.

v2.9.7 (8 July 2026)
- update-check network failures (e.g. DNS error 12007 on startup before the connection is ready) are now logged instead of popping a blocking "Could not download update" dialog; the tray still notifies on the main flow

v2.9.6 (8 July 2026)
- fixed default browser links still not launching: the registry patch now edits HKEY_CLASSES_ROOT\{ProgId}\shell\open\command (and the per-user Classes override), matching the known-working manual fix, and DELETES any DelegateExecute value instead of writing an empty one (the empty value from 2.9.4 itself blocked activation; this repairs those machines on next run)
- uninstall now removes the desktop shortcuts it created (they used to be left behind) and reverts the default-browser --user-data-dir edit so no stale profile path is left in the http/https handler
- build-zip/release workflows now stamp the exe version from CHANGELOG.md automatically, so test builds no longer ship a stale version number

v2.9.5 (8 July 2026)
- fixed "Download update" button doing nothing when the browser was already installed and auto-download was off
- fixed profile export/import scanning the wrong folder (missed the 32/64 arch subfolder), so nothing was ever exported
- desktop shortcut is now created only on first install (and refreshed on update); added CreateShortcut and CreateShortcutOnUpdate options instead of regenerating it on every launch and on every update-check
- surface a clear error and roll back when a downloaded package unpacks without producing the browser executable, instead of leaving an empty bin folder and a misleading "error while checking for updates"

v2.9.4 (8 July 2026)
- fixed default browser links not opening the custom profile on Windows 11 by clearing the Chromium DelegateExecute handler that overrode the patched command
- re-apply the registry patch on machines where a previous build left DelegateExecute intact

v2.9.3 (29 June 2026)
- fixed registry tweak using --single-argument instead of -- for correct URL handling
- fixed http://https// double-scheme issue when opening links via default browser
- bumped version from 2.7 to 2.9.3 (was never updated, causing false update notifications)
- removed donation section from about dialog

v2.9.2 (7 June 2026)
- fixes on reg tweak and taskbar icons unification

v2.9.1 (10 May 2026)
- refactored project to launchbro and added Firefox fork icon support
- implemented application updater, uninstall, and profile export support
- fixed build errors
- added new icons and dynamic icon switching by browser instance type
- changed idle and Explorer executable icon to launchbro.ico
- changed update activity icon to chromium.ico for Chromium-based instances
- changed update activity icon to ff.ico for Firefox-based instances
- resized icons
- updated readme

v2.9 (7 May 2026)
- added registry profile patching for portable default browser integration
- added GitHub API latest release support for r3dfox and iceweasel updaters
- implemented smart release asset selection based on architecture and file type
- added exponential backoff retry logic for failed downloads
- fixed bugs

v2.8 (23 February 2026)
- added firefox forks as instances (add via .ini file)
- added functionality to install multiple instances (up to 4)
- implemented updater logic that updates all instances sequentially
- splitted main.c dor code manageability

v2.7 (21 February 2026)
- added Cromite updater support (ChromiumType=cromite)
- changed defaults: ChromiumType=ungoogled-chromium and ChromiumUpdateOnly=true
- added scheduled auto-update toggle with restart notice
- fixed empty Language submenu after adding new Settings items
- changed default update checking to disabled (ChromiumCheckPeriod=0)
- improved CI artifact naming (.zip extension)
- fixed bugs

v2.6 (14 January 2022)
- set win7sp1 as minimum required version
- prevent system sleep until download complete
- added brave browser launch support
- fixed command line with spaces bug (issue #186)
- fixed cleanup old chromium versions (issue #180)
- fixed opening windows twice (issue #172)
- removed flash player support
- cosmetics fixes
- fixed bugs

v2.5.7 (17 November 2019)
- added updater only mode (issue #84 & #119)
- updated project sdk
- cosmetics fixes
- fixed bugs

v2.5.6 (9 March 2019)
- added better search for browser executables
- fixed working with multiple launchbro copies
- fixed firefox based browsers support

v2.5.5 (10 February 2019)
- added more file formats recognition
- fixed cache file can still available after unpacking (issue #123)
- fixed passing arguments to browser (issue #125)
- fixed window blinking on startup
- updated project sdk
- updated lzma sdk
- cosmetics fixes
- fixed bugs

v2.5.4 (20 December 2018)
- added 7zip packages support (issue #120)
- activate browser window when browser is running
- fixed bugs

v2.5.3 (27 November 2018)
- fixed loading custom configuration (issue #114)
- fixed opening new Chromium window

v2.5.2 (22 November 2018)
- fixed unpacking some zip packages (issue #113)
- updated locale

v2.5.1 (18 November 2018)
- added custom configuration settings and removed mutex (issue #59)
- fixed destroying main window when error occurs
- fixed proxy credentials parsing (issue #110)
- fixed proxy support for win81+ (issue #110)
- updated chromium types description (issue #109)
- updated install script

v2.5 (3 November 2018)
- added localization support (please use example.txt, not launchbro.lng file)
- added proxy support (issue #108)
- fixed unpacking some zip packages (issue #107 & #100)
- fixed wow64 related bugs
- updated windows sdk
- cosmetics fixes
- fixed bugs

v2.4.3 (2 March 2018)
- fixed tls 1.2 certificates recognition (issue #95)
- fixed downloading files
- updated project sdk

v2.4.1 (28 Juny 2017)
- fixed always 32-bit downloading (issue #74)

v2.4 (27 Juny 2017)
+ added ChromiumWaitForDownloadEnd config (issue #68 & #65)
+ added ChromiumUpdateUrl config (issue #69)
+ added set of old configuration (v1.9.4)
- replaced wininet with winhttp library
- removed errors messages
- changed http to https (issue #69)
- updated documentation
- fixed classic ui
- fixed bugs
- ui fixes

v2.3 (14 February 2017)
+ added "ungoogled-chromium" support (issue #33)
+ added forced update checking state into the "ChromiumCheckPeriod" param
- increased startup speed
- removed "--disable-component-update" from arguments (issue #35)
- updated documentation
- updated localization
- fixed opening incorrect urls (issue #52 and #56)
- fixed bugs

v2.2 (21 January 2017)
- fixed OpenZip failure

v2.1 (18 January 2017)
- fixed PPAPI arguments

v2.0 (12 January 2017)
* now download and install update is working in background
+ added tray icon and show it when new version found
+ added download control option
+ added "ChromiumBinary" option to set custom Chromium binary file name
- increased download buffer size
- fixed run Chromium when directory contain spaces
- fixed race conditions on download
- fixed relative/environment path generation
- fixed open links if launchbro set as default browser
- removed PPAPI from package
- updated readme
- code cleanup

v1.9.4 (16 September 2016)
+ output debug strings on silent mode
- fixed localization (issue #32)

v1.9.3 (15 September 2016)
+ command line arguments
+ output debug strings on error
- updated PPAPI to 23.0.0.162
- updated to the latest sdk
- fixed localization

v1.9.2 (16 Juny 2016)
- removed update checking when Chromium is running (issue #16)
- fixed working under proxy (issue #17)
- changed default browser type to "dev-codecs-sync"
- updated PPAPI to 21.0.0.242

v1.9.1 (23 April 2016)
+ now you can pass environment variables via command line
- update checking time don't saved sometimes

v1.9 (19 April 2016)
+ now you can pass arguments to Chromium through launcher command line
- fixed parsing arguments ("blank tab" bug)
- fixed custom builds cleanup
- updated "SetDefaultBrowser" script
- updated PPAPI to 21.0.0.213
- minor bug fixes

v1.8 (19 March 2016)
+ now distributed under MIT license
+ support to download unofficial build with codecs (see launchbro.ini)
- updated PPAPI to 21.0.0.182

v1.7 (23 February 2016)
+ support to use portable version of PPAPI (see "FlashPlayerPath" config)
- fixed "SetDefaultBrowser" script (now used internal win7 feature to set as default browser)
- removed std::regex (saved about 100kb)
- stability improvements

v1.6 (22 January 2016)
+ added script to set launchbro as default browser
- cleanup Chromium package on extract
- removed Mozilla Firefox support (because his have native autoupdater)
- minor bug fixes

v1.5 (17 December 2015)
+ added "Mozilla Firefox" support 
+ added new settings
+ added command line for run specified browser
- fixed CreateProcess current directory parameter

v1.4 (4 December 2015)
+ added "ChromiumDirectory" setting
- fixed statusbar flickering

v1.3 (1 December 2015)
- fixed incorrect version checking

v1.2 (30 November 2015)
+ added checking for installation package is really Chromium archive
+ added exit confirmation when downloading/installing
- fixed architecture detection on various systems
- changed settings description (see launchbro.ini)

v1.1 (29 November 2015)
+ code rewritten on c++
+ added select for downloading architecture
+ added new settings (see launchbro.ini)
+ more information about update

v1.0 (26 November 2015)
- first public version
