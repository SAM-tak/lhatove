// Resolves the "module base + offset" lines the LHATOVE_WATCHDOG report
// writes, against a module's .pdb.
//
//   cl /nologo symbolize.c dbghelp.lib
//   symbolize.exe build\love\RelWithDebInfo\liblove.dll 0x9427d 0x5cd7fa ...
//
// The offsets are relative to the module's load base, so any module whose
// pdb sits beside it works (lovec.exe, liblove.dll).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: symbolize <module> <offset>...\n");
        return 1;
    }
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEBUG);
    if (!SymInitialize(process, NULL, FALSE))
    {
        fprintf(stderr, "SymInitialize failed (%lu)\n", GetLastError());
        return 1;
    }
    DWORD64 base = 0x10000000;
    char dir[MAX_PATH];
    strncpy(dir, argv[1], MAX_PATH - 1);
    dir[MAX_PATH - 1] = 0;
    char *slash = strrchr(dir, '\\');
    if (slash != NULL)
        *slash = 0;
    else
        strcpy(dir, ".");
    // The module's own directory first, then whatever _NT_SYMBOL_PATH says
    // (a symbol server, for the system's DLLs).
    char search[4096];
    strcpy(search, dir);
    const char *more = getenv("_NT_SYMBOL_PATH");
    if (more != NULL && strlen(more) + strlen(search) + 2 < sizeof(search))
    {
        strcat(search, ";");
        strcat(search, more);
    }
    SymSetSearchPath(process, search);
    DWORD64 loaded = SymLoadModuleEx(process, NULL, argv[1], NULL, base, 0, NULL, 0);
    IMAGEHLP_MODULE64 info;
    memset(&info, 0, sizeof(info));
    info.SizeOfStruct = sizeof(info);
    if (SymGetModuleInfo64(process, loaded, &info))
        fprintf(stderr, "symbols: type %d, pdb %s\n", (int) info.SymType, info.LoadedPdbName);
    if (loaded == 0)
    {
        fprintf(stderr, "SymLoadModuleEx failed (%lu)\n", GetLastError());
        return 1;
    }
    for (int i = 2; i < argc; i++)
    {
        DWORD64 offset = strtoull(argv[i], NULL, 0);
        DWORD64 address = loaded + offset;
        char room[sizeof(SYMBOL_INFO) + 512];
        SYMBOL_INFO *symbol = (SYMBOL_INFO *) room;
        memset(room, 0, sizeof(room));
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        DWORD64 displacement = 0;
        const char *name = symbol->Name;
        if (!SymFromAddr(process, address, &displacement, symbol))
        {
            fprintf(stderr, "SymFromAddr failed (%lu)\n", GetLastError());
            name = "?";
        }
        IMAGEHLP_LINE64 line;
        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
            printf("%s  %s + 0x%llx  (%s:%lu)\n", argv[i], name, (unsigned long long) displacement, line.FileName, line.LineNumber);
        else
            printf("%s  %s + 0x%llx\n", argv[i], name, (unsigned long long) displacement);
    }
    SymCleanup(process);
    return 0;
}
