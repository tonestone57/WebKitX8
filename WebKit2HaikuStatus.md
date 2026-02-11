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

### 4. Networking (PARTIALLY DONE)
- **Current State**: Relies on `curl` (`USE_CURL`).
- **Native Implementation**: Native Haiku network implementation exists but is currently disabled.

### 5. Graphics & Rendering (COMPLETE)
- **Architecture**: `CoordinatedGraphics` is fully integrated via `LayerTreeHostHaiku.cpp`.
- **Drawing Area**: `DrawingAreaProxyCoordinatedGraphics` is used with a Haiku-specific `BackingStore` implementation that ensures thread safety (`m_backingStoreLock`) when painting to `BView`.
- **Context**: `GraphicsContextHaiku.cpp` implements drawing operations using `BView`.

### 6. UI Process & API (MOSTLY COMPLETE)
- **WebView**: `BWebView` and `WebViewBase` provide the hosting view.
- **Page Client**: `PageClientImplHaiku.cpp` implements:
    -   `createPopupMenuProxy` (Menus)
    -   `createContextMenuProxy` (Context Menus)
    -   `setCursor`
    -   `toolTipChanged`
- **Input Events**: `PlatformKeyboardEvent`, `PlatformMouseEvent`, and `PlatformWheelEvent` are implemented and mapped from `BMessage`.

### 7. WebCore Platform Support (MOSTLY COMPLETE)
- **Clipboard**: `PasteboardHaiku.cpp` implemented using `BClipboard` (Read/Write for Text/HTML).
- **Drag and Drop**: `DragDataHaiku.cpp` implemented using `BMessage` inspection.
- **Theme**: `ThemeHaiku.cpp` and `ScrollbarThemeHaiku.cpp` implemented using `BControlLook`.
- **Cursors**: `CursorHaiku.cpp` implemented using standard `BCursor` types.
- **Resources**: `LocalizedStringsHaiku.cpp` provides default English strings. `MIMETypeRegistryHaiku.cpp` uses `BMimeType`.
- **Fonts**: `FontHaiku.cpp` implements basic glyph drawing.

### 8. Remaining Tasks & Improvements
- **Native Networking**: Finish and enable the native network process to remove `curl` dependency.
- **Media Support**: Ensure video/audio playback works reliably.
- **Inspector**: Fully verify Web Inspector integration.
- **Printing**: Implement printing support.
- **Advanced UI**: Implement Color Picker, DateTime Picker, and sophisticated drag-and-drop visuals.

## Conclusion

The port has crossed the threshold from "building" to "running". The browser view should now be able to load pages, render content, handle input, show menus, and interact with the clipboard.
