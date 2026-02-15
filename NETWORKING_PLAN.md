# Plan to Complete Networking Functionality

The WebKit Haiku port currently uses the native `BHttpRequest` API (Service Kit). While functional for basic browsing, it lacks modern features required for the full web experience. This plan outlines the steps to upgrade the networking stack.

## Phase 1: HTTP/2 Implementation

### Goal
Enable HTTP/2 support to improve page load performance and compatibility with modern servers.

### Tasks
1.  **Investigate Backend Feasibility:**
    -   Assess if Haiku's `BHttpRequest` supports HTTP/2 negotiation (ALPN).
    -   *Constraint:* If `BHttpRequest` is strictly HTTP/1.1, we must switch backends.
2.  **Evaluate `curl` Backend:**
    -   The `USE_CURL` option exists in CMake but is currently disabled.
    -   *Action:* Enable `USE_CURL=ON` in `OptionsHaiku.cmake` and verify if `NetworkDataTaskCurl` compiles and links against Haiku's `libcurl` port.
    -   *Action:* If `curl` is viable, replace `NetworkDataTaskHaiku` with the standard `NetworkDataTaskCurl` implementation, which brings robust HTTP/2, cookies, and caching for free.
3.  **Alternative: Enhance `NetworkDataTaskHaiku`:**
    -   If keeping the native backend is prioritized, implement HTTP/2 framing and stream management on top of raw `BSocket` instead of `BHttpRequest`. (High Effort).

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
