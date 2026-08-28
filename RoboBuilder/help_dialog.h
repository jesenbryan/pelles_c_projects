#pragma once

#include <windows.h>

// Shows the "Robot Controls" reference dialog -- a proper-looking modal
// popup (title bar, section headers, two-column key/description rows, an
// OK button) rather than a single-string MessageBox. Blocks (runs its own
// local message loop, disabling "owner" for the duration) until the user
// closes it, same call-and-return contract a real DialogBox would have.
// See help_dialog.c for the content list and layout.
void showControlsHelpDialog(HWND owner);
