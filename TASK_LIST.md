# WebKit2 Haiku Port: Outstanding Task List and Audit

This document provides a detailed breakdown of the current status of the WebKit2 port for Haiku, organized by priority.

## 1. Networking (Priority: High)

*   **Cookie Management - SQLite Integration**:
    *   **Goal**: Migrate cookie storage from the legacy `BPathMonitor`-based `CookieJarHaiku` to WebKit's `NetworkStorageSession` backed by SQLite.
    *   **Reason**: Prevents race conditions and ensures robust persistence compatible with the Curl backend.
    *   *Status*: Initial wiring for `cookiePersistentStorageFile` in `WebsiteDataStoreHaiku` is implemented, but full migration and verification are needed.
*   **Cookie Management - SameSite Support**:
    *   **Goal**: Verify correct handling of `SameSite` cookie attributes.
    *   **Reason**: Essential for modern web compatibility and security.
*   **WebSocket Support**:
    *   **Goal**: Complete the implementation of `WebSocketTaskHaiku` (or verify `WebSocketTaskCurl` is fully functional).
    *   **Status**: Stubs exist; needs full implementation using `curl` or `BSocket`.

## 2. Graphics & Rendering (Priority: Medium-High)

*   **WebGL**:
    *   **Goal**: Enable `ENABLE_WEBGL` and implement `GraphicsContextGL`.
    *   **Implementation**: Use an EGL/GL context compatible with `BGLView`.
*   **Hardware Acceleration**:
    *   **Goal**: Investigate hardware acceleration for 2D canvas and compositing.
    *   **Strategy**: Prioritize standard software fallback to `app_server` to ensure stability before enabling acceleration.

## 3. Multimedia & Video Playback (Priority: Medium)

*   **Media Source Extensions (MSE)**:
    *   **Goal**: Enable and configure MSE.
    *   **Reason**: Support adaptive streaming protocols like DASH (YouTube) and HLS (news sites).
*   **Codec Backend (FFmpeg)**:
    *   **Goal**: Ensure the FFmpeg backend is properly integrated to handle software decoding for standard web media formats.
    *   **Mandatory Targets**: H.264, VP9, AV1 (Video) and AAC, Opus (Audio).
*   **Haiku Media Pipeline (`MediaPlayerPrivate`)**:
    *   **Goal**: Implement the Haiku-specific media player backend.
    *   **Role**: Glue code that routes decoded video frames to `BBitmap`/`app_server` and audio to `BSoundPlayer` or `BMediaRoster`, while maintaining strict A/V synchronization.

## 4. Other Missing Features (Priority: Low)

*   **Printing**:
    *   **Goal**: Implement full-page pagination support (currently only viewport printing works).
*   **Geolocation API**:
    *   **Goal**: Implement a basic IP-based fallback (due to lack of OS-level location services).
*   **Speech Synthesis**:
    *   **Goal**: Enable by porting a third-party library (e.g., eSpeak) as a dependency.

## 5. Deferred / Future Considerations

*   **Web Audio API**: Deferred. Focus will remain on standard HTML5 `<audio>`/`<video>` and MSE for standard media playback rather than complex audio synthesis.
*   **Peripheral & Hardware APIs**: Gamepad API, Battery Status API, and Touch Events are deferred.
*   **Media Stream & WebRTC**: Deferred due to the massive size of the Google WebRTC library dependency and complex OS-level audio/video capture integrations.
*   **Accessibility**: True screen reader support is deferred until a mature, system-wide accessibility API exists in Haiku.
*   **Encrypted Media Extensions (EME)**: Indefinitely deferred (requires proprietary binaries like Widevine).
*   **Wide Gamut & HDR**: Deferred until Haiku's `app_server` gains system-level color-management infrastructure.

## 6. Completed Features

These components are considered functional and stable for general browsing.

*   **Networking - Certificate Exceptions**: Robust certificate exception handling with SHA-256 fingerprint storage is implemented and integrated with the Curl backend.
*   **Core WebView API**: `BWebView` and `WebViewBase` provide a functional view for embedding, URL loading, and navigation.
*   **Process Model**: Successful separation of UIProcess, WebProcess, and NetworkProcess. Launching via `posix_spawn` works correctly.
*   **Input Handling**: Full support for Mouse (Down, Up, Moved, Wheel) and Keyboard events, mapped from native `BMessage`s.
*   **Preferences & Storage**: Implementation for `B_USER_SETTINGS_DIRECTORY` usage for saving recent searches and certificate exceptions.
*   **Web Inspector**: Local Web Inspector is fully functional using a socket-based connection (`RemoteInspectorProtocolHandler`).
*   **Cursor Support**: Mapping of standard web cursors to native `BCursor`s.
*   **User Interface**: Native implementation of Context Menus (`WebContextMenuProxyHaiku`) and Popup Menus (`WebPopupMenuProxyHaiku`).
*   **Build System**: CMake configuration (`PlatformHaiku.cmake`, `OptionsHaiku.cmake`) is established and maintained.
