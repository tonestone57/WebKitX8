# WebKit2 Haiku Port: Outstanding Task List and Audit

This document provides a detailed breakdown of the current status of the WebKit2 port for Haiku, organized by priority.

## 1. Multimedia & Video Playback (Priority: High)

*   **Media Source Extensions (MSE)**:
    *   **Goal**: Enable and configure MSE.
    *   **Reason**: Support adaptive streaming protocols like DASH (YouTube) and HLS (news sites).
*   **Codec Backend (FFmpeg)**:
    *   **Goal**: Ensure the FFmpeg backend is properly integrated to handle software decoding for standard web media formats.
    *   **Mandatory Targets**: H.264, VP9, AV1 (Video) and AAC, Opus (Audio).
## 2. Deferred / Future Considerations

*   **Hardware Acceleration**:
    *   **Status**: Deferred.
    *   **Strategy**: Prioritize standard software fallback to `app_server` to ensure stability before enabling acceleration.
*   **Peripheral & Hardware APIs**: Gamepad API, Battery Status API, and Touch Events are deferred.
*   **Media Stream & WebRTC**: Partially enabled (`ENABLE_MEDIA_STREAM` is ON). Full WebRTC support requires porting `libwebrtc`.
*   **Accessibility**: True screen reader support is deferred until a mature, system-wide accessibility API exists in Haiku.

*   **Encrypted Media Extensions (EME)**: Indefinitely deferred (requires proprietary binaries like Widevine).
*   **Wide Gamut & HDR**: Deferred until Haiku's `app_server` gains system-level color-management infrastructure.

## 3. Completed Features

These components are considered functional and stable for general browsing.

*   **Multimedia**:
    *   **Web Audio**: Enabled using native Haiku `BSoundPlayer` and `BMediaFile` backend. `AudioFileReader` implemented for decoding.
    *   **Haiku Media Pipeline (`MediaPlayerPrivate`)**: Implemented. Routes decoded video frames to `BBitmap`/`app_server` and audio to `BSoundPlayer` or `BMediaRoster`.
    *   **Speech Synthesis**: Stub implementation (`PlatformSpeechSynthesizerHaiku`) added and enabled.
*   **Graphics & Rendering**:
    *   **WebGL**: Enabled via `ENABLE_WEBGL` and `GraphicsContextGLTextureMapperANGLE` with `PlatformDisplayHaiku` utilizing EGL.
    *   **2D Canvas**: Supported via software rendering using `ImageBufferHaikuSurfaceBackend` and `GraphicsContextHaiku`.
    *   **Printing**: Full-page pagination support implemented via `AsyncPrinter`.
*   **Networking**:
    *   **Backend**: Fully migrated to `libcurl` (HTTP/2 support).
    *   **Certificates**: Robust exception handling with SHA-256 fingerprint storage.
    *   **Cookies**: Persistent storage via SQLite (`CookieJarDB`).
    *   **WebSockets**: Supported via `WebSocketTaskCurl` and `curl`.
    *   **Proxy**: System proxy settings are propagated to the Network Process.
*   **Core WebView API**: `BWebView` and `WebViewBase` provide a functional view for embedding, URL loading, and navigation.
*   **Process Model**: Successful separation of UIProcess, WebProcess, and NetworkProcess. Launching via `posix_spawn` works correctly.
*   **Input Handling**: Full support for Mouse (Down, Up, Moved, Wheel) and Keyboard events, mapped from native `BMessage`s.
*   **Preferences & Storage**: Implementation for `B_USER_SETTINGS_DIRECTORY` usage for saving recent searches and certificate exceptions.
*   **Web Inspector**: Local Web Inspector is fully functional using a socket-based connection (`RemoteInspectorProtocolHandler`).
*   **Cursor Support**: Mapping of standard web cursors to native `BCursor`s.
*   **User Interface**: Native implementation of Context Menus (`WebContextMenuProxyHaiku`) and Popup Menus (`WebPopupMenuProxyHaiku`).
*   **Geolocation**: Stub implementation (`GeolocationProviderHaiku`) added and enabled.
*   **Build System**: CMake configuration (`PlatformHaiku.cmake`, `OptionsHaiku.cmake`) is established and maintained.
