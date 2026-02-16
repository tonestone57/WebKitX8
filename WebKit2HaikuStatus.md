# WebKit2 on Haiku Status Report

This document outlines the current state of the WebKit2 port for Haiku OS.

## Overview

The WebKit2 port for Haiku is in an **advanced functional stage**. Most core components are implemented, allowing for basic browsing, rendering, and interaction.

## Component Analysis

### 1. Build System (COMPLETE)
- **CMake Configuration**: Fully defined in `Source/cmake/OptionsHaiku.cmake` and `Source/WebKit/PlatformHaiku.cmake`.
- **Options**: `ENABLE_WEBKIT` is `ON`. `USE_COORDINATED_GRAPHICS` is enabled.

### 2. IPC & RunLoop (COMPLETE)
- **IPC**: Uses Haiku semaphores and Unix sockets.
- **RunLoop**: `WTF::RunLoop` is fully implemented using `BLooper` and `BMessageRunner`. It correctly handles the main thread (attaching to `be_app`) and secondary threads. Timers use `MonotonicTime` for state tracking.

### 3. Process Launching (COMPLETE)
- **Implementation**: Uses `posix_spawn` in `ProcessLauncherHaiku.cpp`.
- **Environment**: System environment variables are correctly passed to child processes.

### 4. Networking (MOSTLY COMPLETE)
- **Current State**: Networking backend migrated to `libcurl` (`NetworkDataTaskCurl`) to support HTTP/2 and modern standards.
- **Features**: Certificate verification, Cookies (SQLite), and WebSockets are supported.

### 5. Graphics & Rendering (COMPLETE)
- **Architecture**: `CoordinatedGraphics` is fully integrated via `LayerTreeHostHaiku.cpp`.
- **Drawing Area**: `DrawingAreaProxyCoordinatedGraphics` is used with a Haiku-specific `BackingStore` implementation that ensures thread safety (`m_backingStoreLock`) when painting to `BView`.
- **Context**: `GraphicsContextHaiku.cpp` implements drawing operations using `BView`.
- **WebGL**: Implemented via `GraphicsContextGLTextureMapperANGLE` and `PlatformDisplayHaiku` utilizing EGL.

### 6. UI Process & API (COMPLETE)
- **WebView**: `BWebView` and `WebViewBase` provide the hosting view.
- **Page Client**: `PageClientImplHaiku.cpp` implements menus, cursors, and tooltips.
- **Input Events**: `PlatformKeyboardEvent`, `PlatformMouseEvent`, and `PlatformWheelEvent` are implemented and mapped from `BMessage`.
- **Inspector**: Local Web Inspector is implemented via `WebInspectorUIProxyHaiku` and `InspectorWindow`.
- **Advanced UI**: Native Color Picker and DateTime Picker are implemented.

### 7. WebCore Platform Support (COMPLETE)
- **Clipboard**: `PasteboardHaiku.cpp` implemented using `BClipboard` (Read/Write for Text/HTML).
- **Drag and Drop**: `DragDataHaiku.cpp` and `DragControllerHaiku.cpp` implemented for Files, Text, and Colors.
- **Theme**: `ThemeHaiku.cpp` and `ScrollbarThemeHaiku.cpp` implemented using `BControlLook`.
- **Cursors**: `CursorHaiku.cpp` implemented using standard `BCursor` types.
- **Resources**: `LocalizedStringsHaiku.cpp` provides default English strings. `MIMETypeRegistryHaiku.cpp` uses `BMimeType`.
- **Fonts**: `FontHaiku.cpp` implements glyph drawing and half-width font fallbacks.
- **Media**: `AudioFileReaderHaiku` implements audio decoding (WebAudio enabled). `MediaPlayerPrivateHaiku` implements playback and buffering.
- **Features**: Geolocation and Speech Synthesis stubs are implemented.

### 8. Remaining Tasks & Improvements
- **Optimization**: Continued refinement of memory pressure handling and drawing performance.

## Conclusion

The port has crossed the threshold from "building" to "running" and is now feature-complete for general browsing, debugging, and media playback.
