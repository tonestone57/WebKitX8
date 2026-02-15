# Plan to Complete Networking Functionality

The WebKit Haiku port currently uses the native `BHttpRequest` API (Service Kit). While functional for basic browsing, it lacks modern features required for the full web experience. This plan outlines the steps to upgrade the networking stack.

## Phase 1: HTTP/2 Implementation (COMPLETED)

### Goal
Enable HTTP/2 support to improve page load performance and compatibility with modern servers.

### Tasks
1.  **Switch to `curl` Backend:**
    -   `USE_CURL=ON` enabled in `OptionsHaiku.cmake`.
    -   `NetworkDataTaskCurl` is now used for networking.
    -   *Note:* This deprecates `NetworkDataTaskHaiku`.

## Phase 2: Robust Certificate Management (COMPLETED)

### Goal
Provide a complete UI for managing SSL exceptions and inspecting certificates.

### Tasks
1.  **Certificate Inspection UI (IMPLEMENTED):**
    -   `CertificateInfoDialog` created.
    -   `SHOW_CERTIFICATE_INFO` message and handler stub in `WebViewBase`.
    -   *Pending:* Plumbing the actual `BCertificate` data from `NetworkDataTaskHaiku` -> `WebPage` -> `WebViewBase` to populate the dialog with real data instead of placeholders.
2.  **Persistent Storage (COMPLETED):**
    -   `CertificateExceptionDialog` implemented and integrated.
    -   Exceptions are stored as a SHA-256 fingerprint alongside the hostname to prevent MITM attacks.
    -   `CertificateUtilitiesHaiku` created to manage storage and verification.
    -   Wiring the Curl SSL context to respect these exceptions is implemented.

## Phase 3: Cookie Management (Priority: High)

### Goal
Ensure robust cookie persistence and security using industry-standard storage.

### Tasks
1.  **SQLite Integration:**
    -   **Goal:** Migrate cookie storage from the legacy `BPathMonitor`-based `CookieJarHaiku` to WebKit's `NetworkStorageSession` backed by SQLite.
    -   **Reason:** Prevents race conditions, ensures thread safety, and maintains compatibility with the Curl backend's cookie engine.
    -   *Status:* Initial wiring for `cookiePersistentStorageFile` in `WebsiteDataStoreHaiku` is implemented. Needs full verification that `NetworkStorageSessionCurl` uses this path correctly and that the legacy `CookieJarHaiku` is effectively deprecated.
2.  **SameSite Support:**
    -   **Goal:** Verify correct handling of `SameSite` cookie attributes.
    -   **Action:** Test `NetworkDataTaskCurl` behavior against sites requiring SameSite enforcement.

## Phase 4: WebSocket Support

### Goal
Full WebSocket support (currently a stub).

### Tasks
1.  **Implementation:**
    -   **Goal:** Complete the implementation of `WebSocketTaskHaiku` or verify `WebSocketTaskCurl` is fully functional on Haiku.
    -   **Requirements:** Ensure it handles the upgrade handshake, masking, and binary frames correctly.

## Recommendation

**Strongly recommend evaluating `USE_CURL`.** Porting `NetworkDataTaskCurl` to build on Haiku is likely significantly less effort than re-implementing HTTP/2 and advanced cookie policies on top of the Haiku Service Kit, and provides better long-term maintenance parity with other WebKit ports (GTK/WPE).
