# WebKit2 Haiku Port: Outstanding Task List and Audit

This document provides a detailed breakdown of the current status of the WebKit2 port for Haiku, acting as the authoritative source for feature status, roadmap, and outstanding tasks.

## 1. Project Status Summary

The WebKit2 port for Haiku is in an **advanced functional state**. It successfully implements the multi-process architecture (UIProcess, WebProcess, NetworkProcess) and provides a stable browsing experience for general web content. The networking stack has been fully modernized to use `libcurl`, enabling HTTP/2 and robust security features.

## 2. Outstanding High Priority Tasks

### Multimedia & Video Playback

### Optimization & Stability
*   **Memory Management**: Refine `MemoryPressureHandlerHaiku` thresholds based on real-world usage to prevent OOM kills on low-memory systems.
*   **Graphics Optimization**: Further optimize `CoordinatedGraphics` and `BackingStoreHaiku` for high-DPI screens and complex animations.
*   **Networking Performance**: Tune Curl buffer sizes and investigate specific edge cases in complex proxy environments.

## 3. Deferred / Future Considerations

*   **Hardware Acceleration**:
    *   **Status**: Deferred.
    *   **Strategy**: Prioritize standard software fallback to `app_server` to ensure stability before enabling acceleration.
*   **Peripheral & Hardware APIs**: Gamepad API, Battery Status API, and Touch Events are deferred.
*   **Media Stream & WebRTC**: Partially enabled (`ENABLE_MEDIA_STREAM` is ON). Basic infrastructure exists, but full WebRTC support requires porting `libwebrtc` and implementing robust camera/microphone access.
*   **Accessibility**: True screen reader support is deferred until a mature, system-wide accessibility API exists in Haiku.
*   **Encrypted Media Extensions (EME)**: Indefinitely deferred (requires proprietary binaries like Widevine).
*   **Wide Gamut & HDR**: Deferred until Haiku's `app_server` gains system-level color-management infrastructure.

## 4. Completed Features

These components are considered functional and stable for general browsing.

### Networking (Modernized)
*   **Backend**: Fully migrated to `libcurl` (`USE_CURL=ON`) supporting HTTP/2.
*   **Security**:
    *   Robust certificate verification with SHA-256 fingerprint storage.
    *   `CertificateExceptionDialog` implemented for user override of SSL errors.
*   **Cookies**: Persistent storage via SQLite (`CookieJarDB`).
*   **WebSockets**: Supported via `WebSocketTaskCurl` and `curl`.
*   **Proxy**: System proxy settings are propagated to the Network Process.

### Multimedia
*   **Haiku Media Pipeline Integration**:
    *   **Native Backend**: `MediaPlayerPrivateHaiku` implements full playback using `BMediaFile` and `BMediaTrack` with a dedicated video decoding thread.
    *   **A/V Synchronization**: Robust synchronization achieved using `std::atomic` and precise time tracking against the audio clock (or system time).
*   **Media Source Extensions (MSE)**:
    *   **Status**: Enabled (`ENABLE_MEDIA_SOURCE=ON`).
    *   **Implementation**: Utilizes `StreamingDataController` to feed `BMediaFile` via a custom `BPositionIO` bridge.
*   **Web Audio**: Enabled using native Haiku `BSoundPlayer` and `BMediaFile` backend. `AudioFileReader` implemented for decoding.
*   **Speech Synthesis**: Stub implementation (`PlatformSpeechSynthesizerHaiku`) added and enabled.

### Graphics & Rendering
*   **WebGL**: Enabled via `ENABLE_WEBGL` and `GraphicsContextGLTextureMapperANGLE` with `PlatformDisplayHaiku` utilizing EGL.
*   **2D Canvas**: Supported via software rendering using `ImageBufferHaikuSurfaceBackend` and `GraphicsContextHaiku`.
*   **Printing**: Full-page pagination support implemented via `AsyncPrinter`.
*   **Compositing**: `CoordinatedGraphics` is fully integrated via `LayerTreeHostHaiku`.

### UI Process & Integration
*   **Core WebView API**: `BWebView` and `WebViewBase` provide a functional view for embedding, URL loading, and navigation.
*   **Process Model**: Successful separation of UIProcess, WebProcess, and NetworkProcess using `posix_spawn`.
*   **Input Handling**: Full support for Mouse (Down, Up, Moved, Wheel) and Keyboard events.
*   **Preferences & Storage**: `B_USER_SETTINGS_DIRECTORY` usage for saving recent searches and certificate exceptions.
*   **Web Inspector**: Local Web Inspector is fully functional using a socket-based connection.
*   **Native UI**: Context Menus, Popup Menus, Color Picker, and DateTime Picker are implemented using native controls.
*   **Drag and Drop**: Support for dragging Text, Colors, and URLs.
*   **Geolocation**: Stub implementation (`GeolocationProviderHaiku`) added and enabled.

### Build System
*   **CMake**: Configuration (`PlatformHaiku.cmake`, `OptionsHaiku.cmake`) is established and maintained.
