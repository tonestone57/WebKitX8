# Plan to Complete Networking Functionality

The WebKit Haiku port previously used the native `BHttpRequest` API (Service Kit). It has now been successfully migrated to the `libcurl` backend (`USE_CURL=ON`) to support modern web features.

## Phase 1: HTTP/2 Implementation (COMPLETED)

### Goal
Enable HTTP/2 support to improve page load performance and compatibility with modern servers.

### Status
*   **Completed:** `USE_CURL=ON` is enabled in `OptionsHaiku.cmake`. `NetworkDataTaskCurl` handles networking.

## Phase 2: Robust Certificate Management (COMPLETED)

### Goal
Provide a complete UI for managing SSL exceptions and inspecting certificates.

### Status
*   **Completed:** `CertificateExceptionDialog` implemented.
*   **Completed:** SHA-256 fingerprint storage implemented in `CertificateUtilitiesHaiku`.
*   **Completed:** `NetworkDataTaskCurl` and `NavigationClient` wired to use the exception list.

## Phase 3: Cookie Management (COMPLETED)

### Goal
Ensure robust cookie persistence and security using industry-standard storage.

### Status
*   **Completed:** `WebsiteDataStoreHaiku` configures `cookiePersistentStorageFile` for `NetworkSessionCurl`.
*   **Completed:** `NetworkStorageSession` uses SQLite (`CookieJarDB`) via the Curl backend, ensuring robust persistence and avoiding legacy `BPathMonitor` race conditions.
*   **Completed:** SameSite support is handled inherently by the shared WebKit Curl implementation.

## Phase 4: WebSocket Support (COMPLETED)

### Goal
Full WebSocket support.

### Status
*   **Completed:** `WebSocketTaskCurl` is enabled via `USE_CURL`. It handles the handshake, framing, and masking using `libcurl` and `NetworkSessionCurl`.
*   **Completed:** Proxy settings are propagated from `WebsiteDataStore` to `NetworkProcess`, ensuring WebSockets work behind proxies.

## Conclusion

The core networking stack modernization for WebKit on Haiku is **Complete**. Future work may involve:
*   Refining performance (tuning Curl buffer sizes).
*   Investigating specific edge cases in complex proxy environments.
