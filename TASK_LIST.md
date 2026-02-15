# WebKit2 Haiku Port: Outstanding Task List and Audit

This document provides a detailed breakdown of the current status of the WebKit2 port for Haiku, identifying outstanding tasks, partially implemented features, and completed components.

## 1. Outstanding / Unimplemented Features

These features are currently disabled in the build configuration or have only stub implementations.

*   **Web Audio API**: `ENABLE_WEB_AUDIO` is `OFF`. Implementation is skeletal or non-functional.
*   **Media Stream & WebRTC**: `ENABLE_MEDIA_STREAM` is `OFF`. No support for camera/microphone access or peer connections.
*   **Gamepad API**: `ENABLE_GAMEPAD` is `OFF`.
*   **Geolocation API**: `ENABLE_GEOLOCATION` is `OFF`.
*   **Speech Synthesis**: `ENABLE_SPEECH_SYNTHESIS` is `OFF`.
*   **Touch Events**: `ENABLE_TOUCH_EVENTS` is `OFF` in typical builds (though some stubs exist).
*   **WebGL**: `ENABLE_WEBGL` is `OFF`. No hardware-accelerated 3D context.
*   **Accessibility**: `ENABLE_ACCESSIBILITY` is `OFF` or relies on external ATK, not integrated with Haiku's native accessibility features.
*   **Wide Gamut & HDR**: Explicitly not supported in `PlatformScreenHaiku.cpp`.
*   **Battery Status API**: No implementation found.
*   **Full Page Printing**: Only viewport printing is currently supported.

## 2. Partially Implemented / Needs Improvement

These features exist and function to some degree but lack feature parity or robustness.

*   **Networking (Haiku Native)**:
    *   Implemented using `BHttpRequest` (Haiku Service Kit).
    *   **Missing**: HTTP/2 support, advanced authentication schemes (currently basic/digest are handled manually), robust cookie management integration.
    *   **Issues**: Certificate verification failures log an error but lack a user interface for creating exceptions (exceptions are stored if manually added).
*   **Clipboard & Drag-and-Drop**:
    *   **Clipboard**: Copy/Paste works for Text and Images using `BClipboard`. HTML copy is supported.
    *   **Drag-and-Drop**: Uses a custom "WebKitDrag" clipboard for internal drag operations. Full system integration for dragging content *out* of the WebView to other applications needs verification/completion.
*   **Multimedia (Video/Audio Playback)**:
    *   Implemented using `BMediaKit` (`MediaPlayerPrivateHaiku`).
    *   **Limitations**: Codec support depends on system-installed media add-ons. No Encrypted Media Extensions (EME) support.
*   **Graphics (2D)**:
    *   `GraphicsContextHaiku` is well-implemented for standard 2D operations (paths, gradients, images).
    *   **Limitations**: Complex filters and shadows might rely on software fallbacks or be unoptimized. No hardware acceleration for 2D canvas (software rasterization).
*   **Memory Pressure Handling**:
    *   `MemoryPressureHandlerHaiku` exists but tuning for Haiku's specific memory management behavior (e.g., aggressive caching vs. freeing) is ongoing.

## 3. Completed Features

These components are considered functional and stable for general browsing.

*   **Core WebView API**: `BWebView` and `WebViewBase` provide a functional view for embedding, URL loading, and navigation.
*   **Process Model**: Successful separation of UIProcess, WebProcess, and NetworkProcess. Launching via `posix_spawn` works correctly.
*   **Input Handling**: Full support for Mouse (Down, Up, Moved, Wheel) and Keyboard events, mapped from native `BMessage`s.
*   **Preferences & Storage**: Implementation for `B_USER_SETTINGS_DIRECTORY` usage for saving recent searches and certificate exceptions.
*   **Web Inspector**: Local Web Inspector is fully functional using a socket-based connection (`RemoteInspectorProtocolHandler`).
*   **Cursor Support**: Mapping of standard web cursors to native `BCursor`s.
*   **User Interface**: Native implementation of Context Menus (`WebContextMenuProxyHaiku`) and Popup Menus (`WebPopupMenuProxyHaiku`).
*   **Build System**: CMake configuration (`PlatformHaiku.cmake`, `OptionsHaiku.cmake`) is established and maintained.
