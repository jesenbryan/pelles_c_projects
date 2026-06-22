#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include "file_dialogs.h"

int openFileDialog(char *filename, int maxLen)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    filename[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = maxLen;
    ofn.lpstrFilter = "Bitmap Files\0*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&ofn);
}

int saveFileDialog(char *filename, int maxLen)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    filename[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = maxLen;
    ofn.lpstrFilter =
    "CSV Files\0*.csv\0"
    "Text Files\0*.txt\0"
    "All Files\0*.*\0";
    ofn.nFilterIndex = 1;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    ofn.lpstrDefExt = "csv";

    return GetSaveFileName(&ofn);
}

void openFileDialogOrExit(char *filename, int maxLen)
{
    if (!openFileDialog(filename, maxLen)) {
        printf("No input selected\n");
        exit(1);
    }
}

void saveFileDialogOrExit(char *filename, int maxLen)
{
    if (!saveFileDialog(filename, maxLen)) {
        printf("No output selected\n");
        exit(1);
    }
}
