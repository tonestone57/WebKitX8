# WebKit Haiku Port Analysis

## Executive Summary

The WebKit port for Haiku is in an **advanced functional state**. It successfully implements the multi-process architecture (UIProcess, WebProcess, NetworkProcess) and provides a stable browsing experience for general web content. Core features like networking, rendering, and input handling are mapped to native Haiku APIs (`BHttpRequest`, `BView`, `BMessage`).

However, the port lacks support for several modern web standards (WebAudio, WebGL, WebRTC) and has some usability gaps regarding security exceptions and advanced media handling.

## Detailed Feature Audit

### 1. Networking
*   **Status:** **Functional / Incomplete**
*   **Implementation:** Native `BHttpRequest` (Haiku Service Kit) is used in `NetworkDataTaskHaiku.cpp`.
*   **Strengths:** Native integration, supports HTTP/1.1, redirects, and basic authentication.
*   **Deficiencies:**
    *   **HTTP/2:** Not implemented. The current backend relies on `BHttpRequest` which is HTTP/1.1 only.
    *   **SSL Exceptions:** Certificate verification is strict. Exceptions are read from a flat text file (`WebKit/certificate_exceptions`) with no user interface to add them. Users must manually edit this file.
    *   **Authentication:** Basic and Digest auth are handled, but the parsing logic is custom and potentially fragile.

### 2. Graphics & Rendering
*   **Status:** **Functional**
*   **Implementation:** Uses `CoordinatedGraphics` for compositing. 2D rendering maps to `BView` drawing commands.
*   **Strengths:** Efficient 2D drawing (rects, gradients) using native server-side calls.
*   **Deficiencies:**
    *   **WebGL:** Explicitly disabled (`ENABLE_WEBGL` is `OFF`).
    *   **Hardware Acceleration:** No hardware-accelerated Canvas or 3D context.
    *   **Wide Gamut/HDR:** Not supported by the platform abstraction.

### 3. Multimedia
*   **Status:** **Partial**
*   **Implementation:** `MediaPlayerPrivateHaiku` uses `BMediaKit`.
*   **Strengths:** Video playback works for formats supported by system codecs.
*   **Deficiencies:**
    *   **WebAudio:** Disabled (`ENABLE_WEB_AUDIO` is `OFF`). No support for complex audio synthesis or processing.
    *   **WebRTC/MediaStream:** Disabled (`ENABLE_MEDIA_STREAM` is `OFF`). No camera/microphone access.
    *   **EME:** No Encrypted Media Extensions support.

### 4. User Interface & Input
*   **Status:** **Mostly Complete**
*   **Implementation:** `PageClientImplHaiku` and `WebViewBase`.
*   **Strengths:** Full mouse (including wheel) and keyboard support. Native context menus and tooltips.
*   **Deficiencies:**
    *   **Touch Events:** Configuration is contradictory (`ENABLE_TOUCH_EVENTS` is `OFF` in Options but `1` in Platform definitions), likely effectively disabled or untested.
    *   **Printing:** Viewport printing works, but full-page pagination is missing.
    *   **Accessibility:** Disabled. No integration with Haiku's accessibility APIs (if available).

## Codebase Health & Quality

*   **Stubs:** `TemporaryLinkStubs.cpp` is very small, indicating most required platform symbols are properly implemented.
*   **Memory Management:** `MemoryPressureHandlerHaiku.cpp` implements a basic polling mechanism (every 10s) to free caches. It correctly accounts for Haiku's aggressive file caching.
*   **Build System:** CMake configuration is clean and well-maintained.

## Critical Missing Functionality (To-Do List)

The following areas require significant work to achieve feature parity with other ports:

1.  **Certificate Exception UI:**
    *   *Required:* Implement a dialog in UIProcess to prompt the user when a certificate error occurs, and write the exception to the settings file.
    *   *Current Workaround:* Manual text file editing.

2.  **HTTP/2 Support:**
    *   *Required:* Update `NetworkDataTaskHaiku` to support HTTP/2, potentially by updating the underlying Haiku Service Kit usage or evaluating `curl` (currently disabled) as an alternative backend.

3.  **WebAudio Support:**
    *   *Required:* Enable `ENABLE_WEB_AUDIO` and implement the necessary audio bus transformers using `ffmpeg` or native `BSoundPlayer` streams effectively.

4.  **WebRTC / Media Capture:**
    *   *Required:* Implement `RealtimeMediaSource` classes using `BMediaRoster` to capture audio/video input.

5.  **WebGL:**
    *   *Required:* Implement `GraphicsContextGL` using EGL/GL context compatible with `BGLView`.

## Recommendations

1.  **Immediate Priority:** Implement the **Certificate Exception UI**. This is a major usability hurdle for browsing the modern web where SSL errors are common on legacy/niche platforms due to root store issues.
2.  **Performance:** Refine the `MemoryPressureHandler` thresholds. 10% free memory might be too aggressive or too lenient depending on the system RAM.
3.  **Stability:** Audit the custom HTTP Authentication header parsing in `NetworkDataTaskHaiku.cpp` for robustness.
