# v2.0.0 (Revived)
Revived fork by **Mifuu**. Original mod by **dogotrigger**.
## Technical Changes
- Updated to **Geode 5.7.1** and **GD 2.2081** (all platforms)
- Networking rewritten for the Geode 5 web API (`WebFuture` + `TaskHolder`); the old `EventListener<WebTask>` API no longer exists
- Mod ID changed to `mifu.level_history` to avoid conflicting with the original mod
- Dependencies bumped: `hjfod.gmd-api >= 1.5.0`, `geode.node-ids >= 1.23.0`
- All API requests now send a polite identifying User-Agent and use canonical trailing-slash API URLs
## Fixes
- **Level descriptions now display**: the API field is `description`, not `level_description`
- **JSON parsing can no longer crash the game**: malformed API responses are handled gracefully (new error `-12`)
- Search results now read `cache_user_id` from each hit instead of the response root
- Restored search query text was gated on the level ID field being non-empty
- Single-record levels / single search hits now get the action buttons too
- Levels trimmed off by the "level array size" setting are no longer leaked
- Fixed "No levels cannot be found" typo and initial "Page 0" label
## New Features
- **Save .gmd**: archive the shown record as a `.gmd` file into the mod's save folder (`exports/`)
- **History Page** button: open the level's GDHistory page in your browser
- **Estimated upload date** shown for level ID searches (GDHistory date estimation API)
- **Archive statistics** (levels, level strings, songs, users...) in the provider info popup
- **Cloudflare detection** (new error `-11`): when the download route is challenged, a clear error is shown with an "Open Page" fallback
- **Mod settings**: default instance URL, request timeout, cache lifetime, date estimation toggle
- Last used level ID / search query are remembered across sessions
- Per-record date labels now include the level version and record source, e.g. `2015-11-21 (v1) [glm_03]`

# v1.1.1
## GDHistory Provider
- Fixed level download issue
## UI
- Fixed a typo
## Technical Changes
- Geode version has been switched to `4.0.1`
- Added mod tags

# v1.1.0
## GDHistory Provider
- All requests are now cached
  - This feature would decrease loading time in some cases and would make GDHistory's server load a bit lower.
## UI
- Added info button for all level providers
- Fixed a bug when 'Go to Level' button wasn't able to gray out *(reported by Reapavon)*
- URL for a level provider now can be changed on runtime
## Technical Changes
- Geode version has been switched to `3.2.0`

# v1.0.4
## GDHistory Provider
- Networking has been redone
## Technical Changes
- Geode version has been switched to `3.0.0-beta.1`

# v1.0.3
## GDHistory Provider
- Now levels can be copied
## UI
- 'Copy ID' button is renamed to 'Go to Level'
  - This button now goes into Level Info menu, not to the level itself.
- 'Get It' button has been removed
- Level Thumbnails are now removed when they get appeared on the screen
## Error Handlers
- Possible error would be printed after going into the Level Info page instead of immediately returning
### GDHistory Provider
- New descriptive errors:
  - `-7` - GD History's api is down.
  - `-8` - API did not return record data.
  - `-9` - API returned record data with invalid data type.
  - `-10` - API returned record array with zero levels in it.

# v1.0.2
## GDHistory Provider
- Fixed typo
- Feature score is now parsed properly
## UI
- Fixed button placement in the Online layer
- 'Play Level' is now faded if level/record does not have level data
- 'Copy ID' button now is not shown on Android (due to crashes)
- Level thumbnails are now removed automatically
- Level ratings are now removed automatically
## Error Handlers
- '`gmd api error`' now also dumps level string
### GDHistory Provider
- Added error handler for specific private level records

# v1.0.1
### GDHistory
- Fixed HTTP bug
### Base
- Initial release on Android

# v1.0.0
- Initial release for Windows