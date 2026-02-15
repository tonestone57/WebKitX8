# WebKit Haiku Port: Next Steps

Following the networking modernization (switching to `libcurl`) and the implementation of the Certificate Exception UI, the following tasks are required to achieve full functionality and security parity.

## 1. Wire SSL Exceptions to Curl (Critical)
*   **Context:** The `CertificateExceptionDialog` currently persists user exceptions to `~/config/settings/WebKit/certificate_exceptions`. However, the newly enabled `libcurl` backend (`NetworkDataTaskCurl`) does not yet read or respect this file.
*   **Action:** Modify the `libcurl` session initialization (likely in `NetworkProcess/curl/NetworkDataTaskCurl.cpp` or `NetworkSessionCurl.cpp`) to:
    1.  Read the `certificate_exceptions` file.
    2.  Configure the SSL context (via `CURLOPT_SSL_CTX_FUNCTION`) to bypass verification *only* for the listed hosts.

## 2. Upgrade Exception Storage Security
*   **Context:** The current exception file stores simple hostnames (e.g., `example.com`), which is vulnerable to Man-in-the-Middle (MITM) attacks if an attacker presents a different invalid certificate for an allowed host.
*   **Action:** Upgrade the storage format to include the certificate fingerprint (SHA-256).
    *   *New Format:* JSON or structured text: `{ "host": "example.com", "fingerprint": "sha256:..." }`
    *   *Logic:* Only allow the connection if the hostname *and* the certificate fingerprint match.

## 3. Implement WebSocket Support
*   **Context:** `WebSocketTaskHaiku` is currently a stub, meaning WebSockets do not work.
*   **Action:** Implement the WebSocket protocol using:
    *   Option A: `libcurl`'s WebSocket capabilities (if available in the linked version).
    *   Option B: Native Haiku `BSocket` implementation.

## 4. Modernize Cookie Storage
*   **Context:** `CookieJarHaiku` currently monitors a text file for cookie updates, which is prone to race conditions and does not support complex policies (SameSite, etc.) robustly.
*   **Action:** Migrate to WebKit's cross-platform `NetworkStorageSession` with the SQLite backend. This ensures standard compliance and reliable persistence.

## 5. Enable Multimedia Features
*   **WebAudio:** Implement `AudioDestinationHaiku` using `BSoundPlayer` to enable audio synthesis and processing.
*   **WebGL:** Implement `GraphicsContextGL` using `BGLView` to enable 3D graphics.
*   **Media Capture:** Implement `RealtimeMediaSource` using `BMediaRoster` to enable camera/microphone access (WebRTC).
