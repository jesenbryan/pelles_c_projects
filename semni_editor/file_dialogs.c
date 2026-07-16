#undef UNICODE
#undef _UNICODE

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include "file_dialogs.h"

int openFileDialog(char *filename, int maxLen, int defaultFilterIndex)
{
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    filename[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = maxLen;

    ofn.lpstrFilter =
        "Bitmap Files\0*.bmp\0"
        "CSV Files\0*.csv\0"
        "Text Files\0*.txt\0"
        "All Files\0*.*\0";

    ofn.nFilterIndex = defaultFilterIndex;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileNameA(&ofn);
}

int saveFileDialog(char *filename, int maxLen, const char *defaultExt, int defaultFilterIndex)
{
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    filename[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = maxLen;

    ofn.lpstrFilter =
        "Bitmap Files\0*.bmp\0"
        "CSV Files\0*.csv\0"
        "Text Files\0*.txt\0"
        "All Files\0*.*\0";

    ofn.nFilterIndex = defaultFilterIndex;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    ofn.lpstrDefExt = defaultExt;

    return GetSaveFileNameA(&ofn);
}

void openFileDialogOrExit(char *filename, int maxLen, int defaultFilterIndex)
{
    if (!openFileDialog(filename, maxLen, defaultFilterIndex)) {
        printf("No input selected\n");
        exit(1);
    }
}

void saveFileDialogOrExit(char *filename, int maxLen, const char *defaultExt, int defaultFilterIndex)
{
    if (!saveFileDialog(filename, maxLen, defaultExt, defaultFilterIndex)) {
        printf("No output selected\n");
        exit(1);
    }
}
