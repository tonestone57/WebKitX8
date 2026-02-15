#pragma once

#include <Alert.h>
#include <String.h>

class CertificateExceptionDialog {
public:
    // Static helper to run the dialog safely without object lifecycle management issues
    static bool run(const char* host, const char* error);
};
