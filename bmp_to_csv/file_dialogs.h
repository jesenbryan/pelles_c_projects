#pragma once

int openFileDialog(char *filename, int maxLen);
int saveFileDialog(char *filename, int maxLen);

void openFileDialogOrExit(char *filename, int maxLen);
void saveFileDialogOrExit(char *filename, int maxLen);
