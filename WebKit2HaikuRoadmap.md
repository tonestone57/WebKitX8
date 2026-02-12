# WebKit2 Porting Roadmap for Haiku OS

## Project Status Summary

The WebKit2 port for Haiku OS has achieved significant milestones, successfully implementing the core process model (UIProcess, WebProcess, NetworkProcess) and utilizing the Haiku API for window management, input handling, and basic drawing. The port is currently capable of rendering web pages, handling user input, and managing multiple processes.

However, several critical areas remain incomplete or rely on non-native implementations, preventing the port from being a fully-featured, production-ready browser engine. The primary missing components are native networking integration, media playback support, and advanced desktop integration features like printing and inspection tools.

## Areas for Completion

The remaining work is divided into three key areas to facilitate focused development and tracking.

### Area 1: Core Networking & Media Integration

This area focuses on replacing temporary solutions with native Haiku implementations and enabling rich media content support.

**Tasks:**
1.  **Native Networking Implementation (`NetworkDataTaskHaiku.cpp`)**
    -   Complete the implementation of `NetworkDataTaskHaiku` using Haiku's Service Kit (`BUrlRequest`, `BHttpRequest`).
    -   Implement proper data reception and handling in `BytesWritten`.
    -   Securely implement certificate verification in `CertificateVerificationFailed`.
    -   Implement request completion logic in `RequestCompleted`.
    -   Ensure authentication challenges are correctly processed.
2.  **Remove Curl Dependency**
    -   Once the native networking implementation is robust, disable `USE_CURL` and rely solely on the native Haiku network stack to improve integration and performance.
3.  **Media Playback Support**
    -   Implement media player backends using `BMediaKit` to support `<audio>` and `<video>` elements.
    -   Ensure hardware acceleration for video decoding where possible.

### Area 2: Desktop Integration & Advanced UI Features

This area aims to provide a seamless and feature-rich user experience by integrating deeper with the Haiku desktop environment.

**Tasks:**
1.  **Web Inspector**
    -   Implement `RemoteWebInspectorProxyHaiku` to enable the Web Inspector for debugging web content.
2.  **Printing Support**
    -   Implement printing capabilities using Haiku's native printing system (`BPrintJob`) to allow printing web pages.
3.  **Spell Checking**
    -   Implement `TextCheckerHaiku` using Haiku's spell check services (if available) or a suitable library to provide spell checking in text fields.
4.  **Advanced Input Controls**
    -   Implement `ColorChooser` for `<input type="color">`.
    -   Implement `DateTimeChooser` for date and time input types.
5.  **Enhanced Drag and Drop**
    -   Improve drag and drop visuals and interaction with the Haiku Tracker (file manager) for a more native feel.

### Area 3: Stability, Performance & Optimization

This area ensures the port is stable, performant, and compliant with web standards.

**Tasks:**
1.  **Layout Tests & Regression Fixes**
    -   Systematically run the WebKit layout test suite to identify and fix rendering and behavioral regressions.
2.  **Memory Management**
    -   Implement memory pressure handlers to release caches (e.g., `PageCache`, `FontCache`) when the system is low on memory.
3.  **Graphics Optimization**
    -   Optimize the `CoordinatedGraphics` implementation and `BackingStoreHaiku` to reduce drawing overhead and improve scrolling/animation smoothness.
4.  **Crash Investigation**
    -   Investigate and resolve any crashes or hangs reported during stress testing or extended usage.
