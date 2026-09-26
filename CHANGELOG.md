# Changelog

## 1.0 - 2026-09-26

Initial open-source release of an independent MainUI implementation for Onion on the Miyoo Mini, Mini Plus and Mini Flip.

Reimplements features from the [patched MainUI project](https://github.com/robcodedev/onionos-mainui-patcher) in an open-source launcher.

### What it does

* Console and ROM browsing against the existing `*_cache6.db` SQLite caches, including cache creation, refresh and folder navigation.
* Recents and Favorites, with folder creation, renaming, reordering and removal.
* Search, the Apps list, game launch and return, and the stock settings screens.
* Wi-Fi settings with signal strength, a marker on the connected network, and automatic rescanning while the menu is open, as in stock.
* Theme loading with fallback, and `miyoogamelist.xml` metadata for display names and box art.

### Beyond the stock launcher

* Auto-scrolling game titles.
* Configurable row count in game lists.
* Genre, rating and description from `gamelist.xml` in game details.
* Configurable main menu through `main-menu.json`, including hidden Recents and Expert sections.
* Letter jump in game lists, and a leading `..` row for going up a folder.
* Recents marks favourite games with the theme's favourite icon, as game lists do. Stock shows no icon in Recents. Display only: `recentlist.json` and `favourite.json` are unchanged.
* Sort A-Z for Favorites folders.
* Custom context menus.
* Special characters and spaces in Wi-Fi SSIDs and passwords, except double quotes in SSIDs.
* Weak networks in the Wi-Fi list show the correct signal icon; stock shows full bars for them.
* Preloads thumbnails for the two games before and after the selection.
* The selected game's thumbnail appears together with the list when opening a console, Recents or Favorites, or when returning from a game. An image that takes longer than 80 ms to load never delays opening; it appears when ready.
* Much faster ROM cache rebuilds. The ROM folder is read without checking each file individually, and the image folder is never scanned. A 5,000-ROM arcade folder with 16,000 images rebuilds in under a second, against over 5 minutes in stock MainUI and about a minute in the patched MainUI.
* Faster list navigation.
* Lower CPU use for better battery life: scrolling titles redraw at about 30 fps, the screen is not redrawn while background work such as a ROM refresh or Wi-Fi scan runs, battery and Wi-Fi status are polled every 5 s outside Settings, and each frame is presented in a single pass.
* Basic device information in Settings / About device.

### Reliability and recovery

* Missing emulator directories and blank settings files are handled gracefully. Nonempty malformed settings and existing caches are preserved on failure.
* ROM lists remain browsable when a missing cache cannot be written, with clearer errors for missing ROM folders and invalid gamelists.
* Wi-Fi power changes finish during shutdown, within at most 5 seconds. Background Wi-Fi services cannot retain MainUI's file locks and can be stopped normally.
* Memory limits for theme artwork (32 MiB) and fonts (64 MiB per file) protect against excessively large assets; rejected assets are logged.
* Long Favorites names and folder paths no longer prevent game launches.
* A console with an unreadable `config.json` is skipped instead of hiding every console.
* FAT32-compatible ROM deletion, with recovery after interrupted deletes across reboots and SD-card remounts. If recovery cannot tell which file to keep, it keeps both, and Refresh roms clears the pending state.
* Console ROM paths are confined to the SD card, with additional checks before deleting files.

### Implementation notes

* Links Onion's SQLite on the device rather than bundling a copy, which is a large part of why the binary is around 200 KB against the original's 1.4 MB.
* GPL-3.0-only, matching Onion. See [third-party notices](THIRD_PARTY_NOTICES.md) for cJSON and SQLite.

### Known limitations

* Invalid UTF-8 in miyoogamelist.xml prevents importing the file; save it as UTF-8 before rebuilding the cache.
* ROM paths containing dollar signs ($) or backticks are currently rejected for launch.
* The Max resolution row in About device can read "unknown" on some devices.
* The status bar doesn't show the hotspot icon while Wi-Fi hotspot mode is active (stock does). Onion mostly uses the hotspot during netplay, so this rarely shows.
* A single ROM folder can hold at most 65,536 entries; split larger folders into subfolders. Consoles have been tested with around 5,000 ROMs; much larger libraries (tens of thousands) may run out of memory while the cache is built.
* Testing has covered the Mini Plus more thoroughly than the Mini or Mini Flip.
* Newer labels (folder actions, Tweaks, the About device rows) appear in English whatever language is selected, because their translation IDs do not exist in Onion's language files yet. See [lang/README.md](lang/README.md).
