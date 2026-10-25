// XML2 Roster Hack companion: controller chain (Beta v0.01).
//
// The game gets DirectInput through libIGDisplay.dll, which asks for one function:
// DirectInputCreateEx from "DINPUT.dll". The plugin loader now owns the name dinput.dll,
// so the modern-controller mod can't use it. This plugin loads the controller mod under a
// different file name (xml2_controller.dll) and points libIGDisplay's DirectInputCreateEx
// at the controller mod's version. The controller mod then passes everything on to the
// real Windows DirectInput, the same as it always did.
//
// Nothing on disk is changed. Output: xml2_chain.log next to XMen2.exe.
//
// Build (x86 Native Tools Command Prompt):
//   cl /nologo /LD /O2 /MT src/controllerchain.cpp /Fe:controllerchain.asi
//
// Released into the public domain (Unlicense). See LICENSE.
// Project design and testing: viktorsosiktor.
// Reverse engineering and code written with Claude (Anthropic).

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static FILE* g_log = NULL;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = 0;

    char logPath[MAX_PATH];
    strcpy_s(logPath, dir);
    strcat_s(logPath, "xml2_chain.log");
    fopen_s(&g_log, logPath, "w");
    Log("---- controller chain ----");

    HMODULE disp = GetModuleHandleA("libIGDisplay.dll");
    if (!disp) { Log("libIGDisplay.dll is not loaded yet. Nothing done."); return; }
    Log("libIGDisplay.dll at %p", disp);

    char ctrl[MAX_PATH];
    strcpy_s(ctrl, dir);
    strcat_s(ctrl, "xml2_controller.dll");
    if (GetFileAttributesA(ctrl) == INVALID_FILE_ATTRIBUTES)
    {
        Log("xml2_controller.dll not found beside XMen2.exe. Nothing done (the game runs without the controller mod).");
        return;
    }
    HMODULE mod = LoadLibraryA(ctrl);
    if (!mod) { Log("could not load xml2_controller.dll (error %lu). Nothing done.", GetLastError()); return; }
    FARPROC fn = GetProcAddress(mod, "DirectInputCreateEx");
    if (!fn) { Log("xml2_controller.dll has no DirectInputCreateEx. Nothing done."); return; }
    Log("controller mod loaded at %p, its DirectInputCreateEx at %p", mod, fn);

    // walk libIGDisplay's import table for DINPUT.dll / DirectInputCreateEx
    unsigned char* base = (unsigned char*)disp;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY idd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!idd.VirtualAddress) { Log("libIGDisplay has no import table. Nothing done."); return; }

    int patched = 0;
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + idd.VirtualAddress); d->Name; d++)
    {
        const char* name = (const char*)(base + d->Name);
        if (_stricmp(name, "DINPUT.dll") != 0) continue;
        DWORD* names = (DWORD*)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        FARPROC* iat = (FARPROC*)(base + d->FirstThunk);
        for (int i = 0; names[i]; i++)
        {
            bool match = false;
            if (names[i] & 0x80000000) match = ((names[i] & 0xFFFF) == 1);   // ordinal 1 = DirectInputCreateEx in our copies
            else match = strcmp((const char*)(base + names[i] + 2), "DirectInputCreateEx") == 0;
            if (!match) continue;
            DWORD old;
            if (VirtualProtect(&iat[i], sizeof(void*), PAGE_READWRITE, &old))
            {
                Log("libIGDisplay's DirectInputCreateEx was %p, now %p", iat[i], fn);
                iat[i] = fn;
                VirtualProtect(&iat[i], sizeof(void*), old, &old);
                patched++;
            }
            else Log("could not change the import slot (error %lu)", GetLastError());
        }
    }
    Log(patched ? "chained: the game now talks to the controller mod." : "did not find the DirectInputCreateEx import. Nothing changed.");
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
