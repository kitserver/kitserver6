#ifndef _JUCE_IMAGEUTIL_
#define _JUCE_IMAGEUTIL_

#include <windows.h>
#include <stdio.h>

// generic utility methods
IMAGE_SECTION_HEADER* GetSectionHeader(char* name);
DWORD GetImageDataDirectory(IMAGE_DATA_DIRECTORY** ppDataDirectory);
IMAGE_IMPORT_DESCRIPTOR* GetImageImportDescriptors(char* moduleName);
IMAGE_IMPORT_DESCRIPTOR* GetModuleImportDescriptors(HMODULE hMod);

bool SeekSectionHeader(FILE* f, char* name);
bool SeekSectionVA(FILE* f, char* name);
bool SeekEntryPoint(FILE* f);
bool SeekImageBase(FILE* f);

// custom utilities to work with code section
IMAGE_SECTION_HEADER* GetCodeSectionHeader();
bool SeekCodeSectionFlags(FILE* f);

#endif
