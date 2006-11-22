#include <windows.h>
#include <stdio.h>
#include "imageutil.h"

// Returns a pointer to a specified section.
// (returns NULL if section was such name was not found)
IMAGE_SECTION_HEADER* GetSectionHeader(char* name)
{
	HANDLE hMod = GetModuleHandle(NULL);
	IMAGE_DOS_HEADER* p = (IMAGE_DOS_HEADER*)hMod;
	IMAGE_NT_HEADERS* nth = (IMAGE_NT_HEADERS*)((DWORD)hMod + p->e_lfanew);
	IMAGE_FILE_HEADER* fh = &(nth->FileHeader);
	IMAGE_SECTION_HEADER* sec = 
		(IMAGE_SECTION_HEADER*)((DWORD)fh + sizeof(IMAGE_FILE_HEADER) + fh->SizeOfOptionalHeader);

	WORD num = fh->NumberOfSections;
	for (WORD i=0; i<num; i++)
	{
		if (memcmp(sec[i].Name, name, 8)==0) return sec + i;
	}
	return NULL;
}

// Initializes the output "ppDataDirectory" parameter with a pointer to 
// IMAGE_DATA_DIRECTORY structure. Returns the number of directory
// entries.(0 - if module has no optional header)
DWORD GetImageDataDirectory(HMODULE hMod, IMAGE_DATA_DIRECTORY** ppDataDirectory)
{
	IMAGE_DOS_HEADER* p = (IMAGE_DOS_HEADER*)hMod;
	IMAGE_NT_HEADERS* nth = (IMAGE_NT_HEADERS*)((DWORD)hMod + p->e_lfanew);
	IMAGE_FILE_HEADER* fh = &(nth->FileHeader);
	if (fh->SizeOfOptionalHeader == 0) return 0;

	IMAGE_OPTIONAL_HEADER* oh = &(nth->OptionalHeader);
	*ppDataDirectory = oh->DataDirectory;
	return oh->NumberOfRvaAndSizes;
}

// Returns the pointer to the first IMAGE_IMPORT_DESCRIPTOR structure.
// If moduleName is NULL, the main application is assumed (as opposed to DLL).
// Returns NULL if no optional header exists, or if no import descriptors)
IMAGE_IMPORT_DESCRIPTOR* GetImageImportDescriptors(char* moduleName)
{
	HMODULE hMod = GetModuleHandle(moduleName);
	return GetModuleImportDescriptors(hMod);
}

// Returns the pointer to the first IMAGE_IMPORT_DESCRIPTOR structure.
// (returns NULL if no optional header exists, or if no import descriptors)
IMAGE_IMPORT_DESCRIPTOR* GetModuleImportDescriptors(HMODULE hMod)
{
	IMAGE_DATA_DIRECTORY* dataDirectory = NULL;
	DWORD numEntries = GetImageDataDirectory(hMod, &dataDirectory);
	if (numEntries < 2) return NULL;
	DWORD rva = dataDirectory[1].VirtualAddress;
	return (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)hMod + (DWORD)rva);
}

// Position the file at the beginning of specified section
// Returns true if positioning was successful.
bool SeekSectionHeader(FILE* f, char* name)
{
	fseek(f, 0, SEEK_SET);
	IMAGE_DOS_HEADER dh;
	fread(&dh, sizeof(IMAGE_DOS_HEADER), 1, f);
	fseek(f, dh.e_lfanew - sizeof(IMAGE_DOS_HEADER), SEEK_CUR);
	IMAGE_NT_HEADERS nth;
	fread(&nth, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER), 1, f);
	fseek(f, nth.FileHeader.SizeOfOptionalHeader, SEEK_CUR);

	IMAGE_SECTION_HEADER sec;
	WORD num = nth.FileHeader.NumberOfSections;
	for (WORD i=0; i<num; i++)
	{
		fread(&sec, sizeof(IMAGE_SECTION_HEADER), 1, f);
		if (strlen(name)==0 || memcmp(sec.Name, name, 8)==0 || strncmp((char*)sec.Name, name, 8)==0)
		{
			// go back the length of the section, because
			// we want to position at the beginning
			fseek(f, -sizeof(IMAGE_SECTION_HEADER), SEEK_CUR);
			return true;
		}
	}
	return false;
}

// Position the file at the beginning of section virtual address
// Returns true if positioning was successful.
bool SeekSectionVA(FILE* f, char* name)
{
	if (SeekSectionHeader(f, name))
	{
		fseek(f, sizeof(BYTE)*IMAGE_SIZEOF_SHORT_NAME, SEEK_CUR); // section name
		fseek(f, sizeof(DWORD), SEEK_CUR); // misc union
		return true;
	}
	return false;
}

// Position the file at the entry point address in PE-header
// Returns true if positioning was successful
bool SeekEntryPoint(FILE* f)
{
	fseek(f, 0, SEEK_SET);
	IMAGE_DOS_HEADER dh;
	fread(&dh, sizeof(IMAGE_DOS_HEADER), 1, f);
	fseek(f, dh.e_lfanew - sizeof(IMAGE_DOS_HEADER), SEEK_CUR);
	IMAGE_NT_HEADERS nth;
	fread(&nth, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER), 1, f);
	if (nth.FileHeader.SizeOfOptionalHeader > 0)
	{
		fseek(f, sizeof(WORD), SEEK_CUR);  // magic
		fseek(f, sizeof(BYTE), SEEK_CUR);  // major linker version
		fseek(f, sizeof(BYTE), SEEK_CUR);  // minor linker version
		fseek(f, sizeof(DWORD), SEEK_CUR); // size of code
		fseek(f, sizeof(DWORD), SEEK_CUR); // size of initialized data
		fseek(f, sizeof(DWORD), SEEK_CUR); // size of uninitialized data
		return true;
	}
	return false;
}

// Position the file at the image base address in PE-header
// Returns true if positioning was successful
bool SeekImageBase(FILE* f)
{
	if (SeekEntryPoint(f))
	{
		fseek(f, sizeof(DWORD), SEEK_CUR); // address of entry point
		fseek(f, sizeof(DWORD), SEEK_CUR); // base of code
		fseek(f, sizeof(DWORD), SEEK_CUR); // base of data
		return true;
	}
	return false;
}

// Returns a pointer to code section header
IMAGE_SECTION_HEADER* GetCodeSectionHeader()
{
	return GetSectionHeader(".text");
}

// Position the file at the code section flags. 
// Returns true if positioning was successful.
bool SeekCodeSectionFlags(FILE* f)
{
	if (SeekSectionHeader(f, ".text"))
	{
		// read the section except for the last 4 bytes,
		// so that we're positioned at the section flags
		fseek(f, sizeof(IMAGE_SECTION_HEADER) - 4, SEEK_CUR);
		return true;
	}
	return false;
}

