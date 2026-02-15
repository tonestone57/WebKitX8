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

## Phase 2: Robust Certificate Management (PARTIALLY IMPLEMENTED)

### Goal
Provide a complete UI for managing SSL exceptions and inspecting certificates.

### Tasks
1.  **Certificate Inspection UI (IMPLEMENTED):**
    -   `CertificateInfoDialog` created.
    -   `SHOW_CERTIFICATE_INFO` message and handler stub in `WebViewBase`.
    -   *Pending:* Plumbing the actual `BCertificate` data from `NetworkDataTaskHaiku` -> `WebPage` -> `WebViewBase` to populate the dialog with real data instead of placeholders.
2.  **Persistent Storage (UI IMPLEMENTED):**
    -   `CertificateExceptionDialog` implemented and integrated.
    -   Exceptions are stored as a flat list of hostnames (legacy format).
    -   *Upgrade:* Store the specific certificate fingerprint (SHA-256) alongside the hostname to prevent MITM attacks where a different invalid cert is presented for an allowed host.
    -   *Format:* JSON or BMessage flattened file: `{ "host": "example.com", "fingerprint": "..." }`.

## Phase 3: Cookie Management

### Goal
Ensure cookie persistence and same-site policy compliance.

### Tasks
1.  **Cookie Jar Integration:**
    -   Review `CookieJarHaiku.cpp`. Currently it relies on `BPathMonitor` to watch a cookie file.
    -   *Issue:* This mechanism might be racy or incomplete for session cookies vs. persistent cookies.
    -   *Action:* If moving to `curl`, use `curl`'s cookie engine or WebKit's `NetworkStorageSession` with a SQLite backend.
2.  **SameSite Support:**
    -   Verify `NetworkDataTaskHaiku` correctly respects `SameSite` attributes in `Set-Cookie` headers. (Currently likely ignored if handled by `BHttpRequest` transparently).

## Phase 4: WebSocket Support

### Goal
Full WebSocket support (currently a stub).

### Tasks
1.  **Implementation:**
    -   Implement `WebSocketTaskHaiku` using `BSocket` or `curl`.
    -   Ensure it handles the upgrade handshake and masking correctly.

## Recommendation

**Strongly recommend evaluating `USE_CURL`.** Porting `NetworkDataTaskCurl` to build on Haiku is likely significantly less effort than re-implementing HTTP/2 and advanced cookie policies on top of the Haiku Service Kit, and provides better long-term maintenance parity with other WebKit ports (GTK/WPE).
