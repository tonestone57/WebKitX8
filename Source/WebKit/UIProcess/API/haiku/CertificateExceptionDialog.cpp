#include "config.h"
#include "CertificateExceptionDialog.h"

#include <String.h>

// run() is a static method
bool CertificateExceptionDialog::run(const char* host, const char* error)
{
    BString text;
    text << "Security Warning\n\n";
    text << "The website \"" << host << "\" has an invalid security certificate.\n";
    text << "Error: " << error << "\n\n";
    text << "Do you want to add an exception for this site?";

    BAlert* alert = new BAlert("Certificate Error", text.String(), "Deny", "Allow", NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);

    // Go() blocks and handles the deletion of the BAlert object when the window is closed.
    int32 buttonIndex = alert->Go();

    return buttonIndex == 1; // "Allow" is the second button (index 1)
}
