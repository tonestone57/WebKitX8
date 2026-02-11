# WebKit2 on Haiku Status Report

This document outlines the current state of the WebKit2 port for Haiku OS, identifying completed components, partially implemented features, and missing functionality required for a fully working browser engine.

## Overview

The WebKit2 port for Haiku is in an **early to intermediate stage**. While the basic build infrastructure and some core components are in place, significant parts of the UI process and platform integration are missing or stubbed out. The port currently relies heavily on generic or shared implementations (e.g., `CoordinatedGraphics`, `curl` for networking) and uses placeholders (e.g., `PlayStation` files for layer tree hosting).

## Component Analysis

### 1. Build System (COMPLETE)
- **CMake Configuration**: The build system is well-defined in `Source/cmake/OptionsHaiku.cmake` and `Source/WebKit/PlatformHaiku.cmake`.
- **Options**: `ENABLE_WEBKIT` (WebKit2) is set to `ON`. `USE_COORDINATED_GRAPHICS`, `USE_TEXTURE_MAPPER`, and `USE_NICOSIA` are enabled.
- **Dependencies**: External dependencies like `curl`, `libxml2`, `sqlite3`, `zlib`, `png`, `jpeg`, `webp` are correctly found and linked.

### 2. IPC (MOSTLY COMPLETE)
- **Implementation**: Uses a combination of Haiku-native semaphores (`Source/WebKit/Platform/IPC/haiku/IPCSemaphoreHaiku.cpp`) and generic Unix socket implementations (`Source/WebKit/Platform/IPC/unix/ConnectionUnix.cpp`).
- **Status**: The semaphore implementation uses `create_sem`, `acquire_sem`, `release_sem` correctly. The Unix socket implementation handles connection establishment.
- **Gaps**: Robustness and performance tuning might be needed. The lack of `socketpair` on some older Haiku versions might be an issue, but modern Haiku supports it.

### 3. Process Launching (PARTIALLY DONE)
- **Implementation**: Uses `posix_spawn` in `Source/WebKit/UIProcess/Launcher/haiku/ProcessLauncherHaiku.cpp`.
- **Status**: It sets up a socket pair and passes the file descriptor to the child process.
- **Issues**:
    -   Environment variables are cleared (`envp` is `{ nullptr }`), which might break functionality relying on specific environment settings.
    -   Error handling is basic.
    -   It uses `BString` locking which is slightly unconventional but functional.

### 4. Networking (PARTIALLY DONE)
- **Current State**: Relies on `curl` (via `USE_CURL` in `OptionsHaiku.cmake`).
- **Native Implementation**: There is a native Haiku network implementation in `Source/WebKit/NetworkProcess/haiku/`, but it is currently **disabled** in `PlatformHaiku.cmake` because `USE_CURL` is enabled.
- **Missing**: The native implementation needs to be completed and tested if `curl` is to be replaced.

### 5. Graphics & Rendering (PARTIALLY DONE)
- **Architecture**: Uses `CoordinatedGraphics` and `TextureMapper`.
- **Layer Tree Host**: Uses `Source/WebKit/WebProcess/WebPage/CoordinatedGraphics/LayerTreeHostPlayStation.cpp` as a generic implementation. This works but should eventually be renamed or adapted specifically for Haiku.
- **Drawing Area**: `DrawingAreaProxyCoordinatedGraphics` is used.
- **Painting**: `WebViewBase.cpp` defers painting to the main run loop (`callOnMainRunLoop`), which effectively serializes painting on the main thread. This is a potential performance bottleneck.

### 6. UI Process & API (MISSING / STUBBED)
- **WebView**: `BWebView` and `WebViewBase` provide a basic `BView`-based container.
- **Page Client**: `PageClientImplHaiku.cpp` is mostly **empty stubs**.
    -   **Missing**:
        -   `createPopupMenuProxy` (Menus)
        -   `createContextMenuProxy` (Context Menus)
        -   `createColorPicker`
        -   `createDateTimePicker`
        -   `setCursor` / `cursor` handling
        -   `toolTipChanged`
        -   `enterAcceleratedCompositingMode` / `exitAcceleratedCompositingMode`
        -   `dragAndDrop` support
        -   `startDrag`
        -   `handleKeyboardEvent` / `handleMouseEvent` (partially implemented in `WebViewBase` but needs refinement).

### 7. WebCore Platform Support (PARTIALLY DONE)
- **Files**: `Source/WebCore/platform/haiku/` contains many files.
- **Stubs**: A significant number of functions in `DragDataHaiku.cpp`, `PasteboardHaiku.cpp`, `SearchPopupMenuHaiku.cpp`, `ThemeHaiku.cpp`, and others call `notImplemented()`.
- **Missing**:
    -   Clipboard/Pasteboard integration (`BClipboard`).
    -   Drag and Drop (`BMessage` dragging).
    -   Native theme drawing (`BControlLook`).
    -   System sound / beep.
    -   MIME type registry integration (partially done).

### 8. Run Loop Integration (MISSING)
- **Issue**: `BWebView` constructor calls `WTF::RunLoop::run()`, which blocks the calling thread. This prevents proper integration with the `BApplication` main loop.
- **Requirement**: WebKit's run loop needs to be integrated with Haiku's `BLooper` / `BMessageRunner` mechanism to allow the application to process its own messages while WebKit runs.

## Required Steps to Complete

1.  **Fix Run Loop Integration**:
    -   Implement a `RunLoop::Timer` and `RunLoop` for Haiku that integrates with `BMessageRunner` or uses a separate thread that communicates with the `BApplication` looper.
    -   Remove the blocking `WTF::RunLoop::run()` call from `BWebView`.

2.  **Implement Page Client**:
    -   Implement `PageClientImpl::createPopupMenuProxy` using `BPopUpMenu`.
    -   Implement `PageClientImpl::createContextMenuProxy` using `BPopUpMenu`.
    -   Implement cursor handling (`BCursor`).
    -   Implement tooltips (`BToolTip`).

3.  **Implement WebCore Platform Features**:
    -   **Pasteboard**: Implement `PasteboardHaiku.cpp` using `BClipboard`.
    -   **Drag and Drop**: Implement `DragDataHaiku.cpp` using `BView` drag-and-drop messages.
    -   **Theme**: Implement `ThemeHaiku.cpp` using `BControlLook` to make web controls look like native Haiku controls.

4.  **Improve Graphics Performance**:
    -   Review `WebViewBase::Draw` and `LayerTreeHost` to reduce main thread blocking.
    -   Investigate if `DirectWindow` or `BGLView` can be used for more efficient compositing.

5.  **Enable Native Networking (Optional but Recommended)**:
    -   Finish the implementation in `Source/WebKit/NetworkProcess/haiku/`.
    -   Disable `USE_CURL` and enable the native network process.

6.  **Cleanup**:
    -   Rename/Refactor `LayerTreeHostPlayStation.cpp` to `LayerTreeHostHaiku.cpp` or ensure the generic usage is intentional and correct.
    -   Ensure `ProcessLauncher` passes necessary environment variables.

## Conclusion

The Haiku port of WebKit2 is functional enough to build and likely show a basic page, but it lacks essential desktop features (menus, clipboard, drag-and-drop) and has potential architectural issues (run loop blocking) that prevent it from being a usable browser engine. Focusing on the **Page Client** implementation and **Run Loop** integration should be the immediate priority.
