# WebKit Haiku Port: Next Steps

Following the networking modernization (switching to `libcurl`) and the implementation of the Certificate Exception UI, the following tasks are required to achieve full functionality and security parity.

## 1. Wire SSL Exceptions to Curl (COMPLETED)
*   **Status:** Implemented in `CurlSSLVerifier` to read `~/config/settings/WebKit/certificate_exceptions` on Haiku.

## 2. Upgrade Exception Storage Security (COMPLETED)
*   **Status:** SHA-256 fingerprint storage implemented.

## 3. Implement WebSocket Support (COMPLETED)
*   **Status:** Implemented via `WebSocketTaskCurl` using `libcurl`.

## 4. Modernize Cookie Storage (COMPLETED)
*   **Status:** Migrated to `NetworkStorageSession` using the SQLite backend (`CookieJarDB`).

## 5. Enable Multimedia Features
*   **WebAudio (COMPLETED):** Implemented `AudioDestinationHaiku` using `BSoundPlayer`.
*   **WebGL (COMPLETED):** Implemented `GraphicsContextGL` using `GraphicsContextGLTextureMapperANGLE` and `PlatformDisplayHaiku` utilizing EGL.
*   **Media Capture (PARTIAL):** Basic infrastructure enabled (`ENABLE_MEDIA_STREAM`). Further work needed for robust camera/microphone access.
