#pragma once

int openFileDialog(char *filename, int maxLen, int defaultFilterIndex);
int saveFileDialog(char *filename, int maxLen, const char *defaultExt, int defaultFilterIndex);

void openFileDialogOrExit(char *filename, int maxLen, int defaultFilterIndex);
void saveFileDialogOrExit(char *filename, int maxLen, const char *defaultExt, int defaultFilterIndex);
