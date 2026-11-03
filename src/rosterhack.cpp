// XML2 32 Roster Hack Beta v0.02
//
// Raises the hero limit of the original PC X-Men Legends II (2005 retail XMen2.exe)
// from 21 playable heroes to as many as 32 playable heroes plus the default entry
// (33 hero slots). It is an ASI plugin (for Ultimate ASI Loader): it patches the
// running game in memory only. XMen2.exe on disk is never modified. To undo
// everything, remove rosterhack.asi.
//
// What it changes:
//
//  1. Hero record pool. The pool (0x44 bytes per hero) is moved into memory this plugin
//     owns, laid out for 33 slots (the default entry plus up to 32 heroes), and every number in
//     the pool's code that depended on the old 22-slot layout is rewritten.
//  2. Playable index lookup. After herostat.xmlb is loaded, the game builds a table
//     (character ID -> playable index) in a loop that runs 21 times (loop at 0x44BB00).
//     The loop now follows the real number of herostat entries, so nothing has to be
//     changed when heroes are added.
//  3. Roster screen list. The roster screen builds a sorted list of hero names in a
//     fixed 21-entry table (20 bytes each) at 0x8B1010 and silently drops the 22nd
//     hero. The list is moved to plugin memory (33 entries), the function that
//     fills it (0x5DB590) is replaced by an equivalent without the 21 limit, and the
//     loops that read the list are pointed at the new table.
//  4. Portrait fix. Lets additional character heads (the 22nd hero and later) show their portraits
//     without having to update the character heads package files.
//  5. Character manager. The character pool (31 slots) is enlarged (CharPool=on: 64 slots)
//     and so is the name table (297 entries; NameTable=on: 480 entries). The manager object
//     grows and every offset, mask, shift and name-number limit that depends on those
//     sizes is rewritten, including the name-indexed tables outside the manager.
//     This keeps NPC models (cutscene characters and the "<Name>Simple" NPC versions of
//     heroes) from not showing because of the increased roster size.
//  6. Team bonus limits ([Limits] TeamBonus). The game keeps 17 team
//     bonuses of up to 6 heroes and silently drops the rest. The team bonus object is rebuilt
//     larger (TeamBonus=on: up to 100 teams of 23 heroes) and the code that depends on its layout is patched.
//  7. Cel-shading option ([Visual] CelShading=on|off). "off" hides the separate outline scene
//     nodes so models draw without outlines. Nodes are found by name: "<name>_outline", the
//     misspellings "_outine" and "_outlline", "outline", "804", "3902", "5002", and the
//     hand-fixed names "outl" and "_outl". Needs libIGSg.dll.
//     See the xml2_hex_edit_instructions.txt next to this for a list of identified
//     incompatible skins and hex editing fixes to make them compatible.
//  8. Diagnostics. The log records pool and name-table high-water marks and writes
//     "NAME TABLE FULL" when a name cannot be registered. A crash note logs
//     first-chance access violations (address, module+offset, registers, stack).
//
// With Roster, CharPool, NameTable and TeamBonus all off the game's code is the original code
// (apart from the moves).
//
// Settings: rosterhack.ini, section [Limits]: Roster, CharPool, NameTable,
// TeamBonus (each on or off); [Visual]: CelShading.
// The ini is looked for beside this .asi first (for example the plugins folder), then in
// the game folder. If it is in neither place, defaults are written beside the .asi.
// The log (xml2_roster.log) is written in the same folder as the ini.
//
// Safety: every patch site is checked against the bytes that were studied. If
// anything does not match, NOTHING is patched.
//
// Build (x86 Native Tools Command Prompt):
//   cl /nologo /LD /O2 /MT src/rosterhack.cpp /Fe:rosterhack.asi
//
// Released into the public domain (Unlicense). See LICENSE.
//
// Project design and testing: viktorsosiktor.
// Reverse engineering and code written with Claude (Anthropic).

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

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

// ---------------------------------------------------------------- layout

static unsigned g_cap      = 22;
static unsigned g_playable = 21;
static bool     g_playAuto = true;   // Playable=auto: follow the real number of herostat entries
static unsigned g_pool     = 0;

struct Layout
{
    unsigned recEnd;   // end of the records, start of the first mask      (was 0x5D8)
    unsigned list;     // free-slot list                                    (was 0x5DC)
    unsigned tail;     //                                                   (was 0x638)
    unsigned head;     //                                                   (was 0x63C)
    unsigned freec;    // number of free slots                              (was 0x640)
    unsigned mask2;    // second mask                                       (was 0x644)
    unsigned used;     // number of used slots                              (was 0x648)
    unsigned total;    // total bytes                                       (was 0x64C)
};
static Layout g_lay;

static Layout MakeLayout(unsigned c)
{
    Layout L;
    // both masks are bit arrays (slot >> 5 picks the dword, slot & 31 the bit), so each
    // needs one dword per 32 slots. One dword was enough up to 32 slots; 33 needs two.
    unsigned W = (c + 31) / 32;
    L.recEnd = c * 0x44;
    L.list   = L.recEnd + 4 * W;
    L.tail   = L.list + 4 * (c + 1);   // the original layout has one spare dword here
    L.head   = L.tail + 4;
    L.freec  = L.head + 4;
    L.mask2  = L.freec + 4;
    L.used   = L.mask2 + 4 * W;
    L.total  = L.used + 4;
    return L;
}

// Named after the ORIGINAL offsets they replace.
enum Field { F5D8, F5DC, F638, F63C, F640, F644, F648 };

static unsigned OldOff(Field f)
{
    static const unsigned t[] = { 0x5D8, 0x5DC, 0x638, 0x63C, 0x640, 0x644, 0x648 };
    return t[f];
}

static unsigned NewOff(Field f)
{
    switch (f)
    {
    case F5D8: return g_lay.recEnd;
    case F5DC: return g_lay.list;
    case F638: return g_lay.tail;
    case F63C: return g_lay.head;
    case F640: return g_lay.freec;
    case F644: return g_lay.mask2;
    default:   return g_lay.used;
    }
}

// ---------------------------------------------------------------- the six base sites
// (the code that finds the pool)

struct Base
{
    unsigned      addr;
    int           len;
    unsigned char orig[8];
    int           kind;
    const char*   what;
};

static const Base kBase[] =
{
    { 0x44B35C, 6, { 0x8D, 0xBB, 0x18, 0x12, 0x01, 0x00 },       1, "release path: pool base" },
    { 0x44B44B, 7, { 0x83, 0xBE, 0x60, 0x18, 0x01, 0x00, 0x16 }, 4, "full check" },
    { 0x44B454, 6, { 0x8D, 0x8E, 0x18, 0x12, 0x01, 0x00 },       2, "allocate: pool base" },
    { 0x44B462, 7, { 0x8D, 0x84, 0x30, 0x18, 0x12, 0x01, 0x00 }, 3, "allocate: record address" },
    { 0x44B4D0, 6, { 0x8D, 0x8E, 0x18, 0x12, 0x01, 0x00 },       2, "cleanup: pool base" },
    { 0x44B5D7, 6, { 0x8D, 0x8E, 0x18, 0x12, 0x01, 0x00 },       2, "setup: pool base" },
};

static void MakeBase(const Base& s, unsigned char* out)
{
    memset(out, 0x90, s.len);
    switch (s.kind)
    {
    case 1: out[0] = 0xBF; memcpy(out + 1, &g_pool, 4); break;           // mov edi, pool
    case 2: out[0] = 0xB9; memcpy(out + 1, &g_pool, 4); break;           // mov ecx, pool
    case 3: out[0] = 0x8D; out[1] = 0x80; memcpy(out + 2, &g_pool, 4); break;  // lea eax,[eax+pool]
    case 4:                                                                // cmp [pool+used], cap
    {
        unsigned c = g_pool + g_lay.used;
        out[0] = 0x83; out[1] = 0x3D; memcpy(out + 2, &c, 4); out[6] = (unsigned char)g_cap;
        break;
    }
    }
}

// ---------------------------------------------------------------- offset sites
// Each is the 4-byte displacement of an instruction that reads or writes a pool field.

struct DSite { unsigned addr; Field f; };

static const DSite kD[] =
{
    // contains()  448F90
    { 0x448FCA, F644 },
    // take a slot  449730
    { 0x449734, F63C }, { 0x44973B, F5DC }, { 0x449748, F644 }, { 0x44975D, F63C },
    { 0x449769, F63C }, { 0x449771, F63C }, { 0x44977B, F640 }, { 0x449781, F648 },
    { 0x449789, F648 },
    // setup  44A2A0
    { 0x44A2A6, F638 }, { 0x44A2AC, F63C }, { 0x44A2B2, F640 }, { 0x44A2C2, F638 },
    { 0x44A2C8, F640 }, { 0x44A2D3, F638 }, { 0x44A2D9, F640 }, { 0x44A2E1, F638 },
    { 0x44A2F0, F5DC },
    // give a slot back  44AE90
    { 0x44AEAF, F5D8 }, { 0x44AEBA, F5D8 }, { 0x44AEC1, F644 }, { 0x44AEC7, F638 },
    { 0x44AECD, F640 }, { 0x44AED8, F638 }, { 0x44AEDE, F640 }, { 0x44AEE6, F638 },
    { 0x44AEF9, F5DC }, { 0x44AEFF, F648 }, { 0x44AF07, F648 },
    // walk all records  44AF10
    { 0x44AF12, F5D8 }, { 0x44AF19, F5D8 },
    // allocate  44B3E0
    { 0x44B3FE, F5D8 },
    // zero the fields at startup  44B5D7
    { 0x44B5DF, F5D8 }, { 0x44B5E5, F638 }, { 0x44B5EB, F63C }, { 0x44B5F1, F640 },
    { 0x44B5F7, F644 }, { 0x44B5FD, F648 },
};

// ---------------------------------------------------------------- limit sites
// Each is an immediate 22 (0x16) or 21 (0x15) that belongs to this pool.

struct ISite { unsigned addr; int size; unsigned oldv; bool last; };   // last: value is capacity-1

static const ISite kI[] =
{
    { 0x448FB5, 1, 0x16, false },                                         // contains()
    { 0x449766, 1, 0x16, false },                                         // take
    { 0x44A2D0, 1, 0x16, false }, { 0x44A2E6, 4, 0x15, true  },
    { 0x44A2F7, 1, 0x16, false },                                         // setup
    { 0x44AED5, 1, 0x16, false }, { 0x44AEEF, 4, 0x15, true  },           // give back
    { 0x44AF2B, 1, 0x16, false }, { 0x44AF6C, 1, 0x16, false },
    { 0x44AF73, 1, 0x16, false }, { 0x44AF98, 1, 0x16, false },
    { 0x44AFD9, 1, 0x16, false },                                         // walk
};

// ---------------------------------------------------------------- playable-count site
// 0044BB13:  83 FE 15  cmp esi,15      7C E8  jl 44BB00
// The loop at 44BB00 runs once per playable hero (0..20) and records its index.
static const unsigned kPlayAddr = 0x44BB15;   // the immediate of that cmp
static const unsigned kPlayOld  = 0x15;


// ---------------------------------------------------------------- roster name list
// Original table: 008B1010 .. 008B11B4 (21 names x 20 bytes), then the screen's own fields.
static const unsigned kListOld = 0x8B1010;
static const unsigned kListEnd = 0x8B11B4;

// Dwords that mean "the list" (start or end). The same constants at 005DB871 and 005DB8AD
// are the screen object itself (its address) and are left alone.
struct LSite { unsigned addr; bool isEnd; };
static const LSite kL[] =
{
    { 0x5DB4A5, false }, { 0x5DB4DC, true },                       // loop over the names
    { 0x5DB85F, false }, { 0x5DB87C, false }, { 0x5DB888, true },  // building the roster entries
    { 0x5DB9AF, true },
};

// start of the function that fills the list:  push ecx / push ebx / push ebp / mov ebp,ecx
static const unsigned      kFillAddr = 0x5DB590;
static const unsigned char kFillOrig[5] = { 0x51, 0x53, 0x55, 0x8B, 0xE9 };

static unsigned char* g_list = NULL;     // new table
static unsigned       g_listN = 0;       // entries

typedef void* (__cdecl*   FnGetMgr)();
typedef int   (__thiscall* FnCount)(void*);
typedef int   (__thiscall* FnItem)(void*, int);
typedef int   (__thiscall* FnCheck)(void*, int);
typedef unsigned char* (__thiscall* FnObj)(void*, int);
typedef void  (__cdecl*   FnQsort)(void*, unsigned, unsigned, void*);

static unsigned VT(void* obj, unsigned off) { return *(unsigned*)(*(unsigned*)obj + off); }

// Same job as the game's own 005DB590, without the 21 limit.
static void __fastcall FillList(void* self, void*)
{
    unsigned char* flags = (unsigned char*)self + 0x1A4;
    if (*flags & 8) return;

    memset(g_list, 0, g_listN * 0x14);

    FnGetMgr getMgr = (FnGetMgr)0x44B8F0;
    unsigned n = 0;
    unsigned char* wp = g_list;

    void* mgr = getMgr();
    int count = ((FnCount)VT(mgr, 0x48))(mgr);
    for (int i = 0; i < count; i = i + 1)
    {
        mgr = getMgr();
        int item = ((FnItem)VT(mgr, 0x4C))(mgr, i);
        mgr = getMgr();
        if (!(((FnCheck)VT(mgr, 0x78))(mgr, item) & 0xFF))
        {
            mgr = getMgr();
            count = ((FnCount)VT(mgr, 0x48))(mgr);
            continue;
        }
        mgr = getMgr();
        unsigned char* obj = ((FnObj)VT(mgr, 0x74))(mgr, item);
        if (n < g_listN && *(unsigned*)(obj + 0x2A0) == 0x1D)
        {
            const char* name = (const char*)(obj + 0x150);
            if (strlen(name) < 0x13)
            {
                strcpy((char*)wp, name);
                n++;
                wp += 0x14;
            }
        }
        mgr = getMgr();
        count = ((FnCount)VT(mgr, 0x48))(mgr);
    }

    ((FnQsort)0x672520)(g_list, g_listN, 0x14, (void*)0x5DB320);
    *flags |= 8;
}

// ---------------------------------------------------------------- helpers

static unsigned D32(unsigned a) { return *(const unsigned*)a; }
static unsigned char B8(unsigned a) { return *(const unsigned char*)a; }

static bool WriteMem(unsigned addr, const void* data, int n)
{
    DWORD old = 0;
    if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy((void*)addr, data, n);
    VirtualProtect((void*)addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, n);
    return true;
}

// A few snapshots of the new pool, so the log shows the game really uses it.
static DWORD WINAPI Watcher(LPVOID)
{
    for (int i = 0; i < 6; i++)
    {
        Sleep(i == 0 ? 20000 : 30000);

        const unsigned char* p = (const unsigned char*)g_pool;
        unsigned built = 0;
        for (unsigned k = 0; k < g_cap; k++)
            if (D32((unsigned)p + k * 0x44) == 0x685294) built++;

        Log("snapshot %d: records built=%u used=%u free=%u head=%u tail=%u mask1=%08X mask2=%08X",
            i + 1, built, D32((unsigned)p + g_lay.used), D32((unsigned)p + g_lay.freec),
            D32((unsigned)p + g_lay.head), D32((unsigned)p + g_lay.tail),
            D32((unsigned)p + g_lay.recEnd), D32((unsigned)p + g_lay.mask2));

        if (g_list && *(unsigned char*)(kListOld + 0x1A4) & 8)
        {
            char line[900]; int len = 0; int shown = 0;
            for (unsigned k = 0; k < g_listN; k++)
                if (g_list[k * 0x14])
                {
                    len += sprintf_s(line + len, sizeof(line) - len, " %s", (const char*)(g_list + k * 0x14));
                    shown++;
                }
            Log("  roster name list (%d names):%s", shown, line);
        }
    }
    return 0;
}


// ---- portrait fix --------------------------------------------------
// The team screen asks "does ui/models/characters/<NN>01 exist?" before loading a hero's
// portrait. The game answers from an internal list with room for only 21 heroes + default, so
// the 22nd hero is refused even though his file is on disk. This lets the LAST attempt for a
// portrait (the "...01" file) go through to the real load.
static char g_ppath[260] = "";
static int  g_pforced = 0;

extern "C" void __cdecl PortraitSavePath(const char* p)
{
    g_ppath[0] = 0;
    __try { strncpy_s(g_ppath, sizeof(g_ppath), p, _TRUNCATE); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_ppath[0] = 0; }
}

extern "C" unsigned __cdecl PortraitFix(unsigned ok)
{
    if (ok) return ok;
    size_t n = strlen(g_ppath);
    if (n > 2 && strstr(g_ppath, "ui/models/characters/") && strcmp(g_ppath + n - 2, "01") == 0)
    {
        if (g_pforced < 50) Log("portrait fix: forced yes for %s", g_ppath);
        g_pforced++;
        return 1;
    }
    return ok;
}

static const unsigned kPortraitBack = 0x5F50C0;

__declspec(naked) void PortraitHook()
{
    __asm
    {
        pushad
        mov eax, [esp + 0x20]
        push eax
        call PortraitSavePath
        add esp, 4
        popad
        call dword ptr [edx + 0xC]
        pushad
        movzx eax, al
        push eax
        call PortraitFix
        add esp, 4
        mov [esp + 0x1C], eax
        popad
        test al, al
        jmp kPortraitBack
    }
}

static void InstallPortraitFix()
{
    static const unsigned char want[7] = { 0xFF, 0x52, 0x0C, 0x84, 0xC0, 0x75, 0x23 };
    if (memcmp((void*)0x5F50BB, want, 7) != 0)
    {
        Log("portrait fix: bytes at 005F50BB are not the ones we studied (is portraitfix.asi or portraitprobe.asi still installed?). Portrait fix not installed.");
        return;
    }
    unsigned char jmp[5];
    jmp[0] = 0xE9;
    unsigned rel = (unsigned)&PortraitHook - (0x5F50BB + 5);
    memcpy(jmp + 1, &rel, 4);
    if (WriteMem(0x5F50BB, jmp, 5)) Log("portrait fix installed (22nd+ hero portraits are no longer refused)");
    else Log("portrait fix: write failed at 005F50BB");
}


// ================================================================ character manager (steps 6 and 7)
//
// The game keeps every character that can appear in a level (heroes, NPCs, enemies) in
// a "character manager" with two fixed-size tables:
//
//   * an object pool of 31 slots (CharPool=N below), and
//   * a name table of 297 entries (NameTable=N below). Every character name the game
//     registers at startup takes one entry; when it is full, later names are silently
//     refused and those characters fall back to the "default" model.
//
// Both are enlarged here. The pool slots are inside the manager, so everything after them
// moves up (each offset in the code is adjusted). The small tables that must hold more
// entries are moved to new space at the end of the (now larger) manager. The name
// lookup tree is moved to memory the plugin owns, and the few tables elsewhere in the
// game that are indexed by name number are enlarged too.

enum { KS, KM1, KL, KM2, KH, KT, KW1, KW2, KTREE, KP };
struct CSite { unsigned addr; unsigned oldv; int kind; };

// instruction offsets inside the manager (address of the 4-byte displacement, old value, kind)
static const CSite kC[] =
{
    { 0x448C62, 0x09A98, KM2 }, // mov    eax,DWORD PTR [ecx+0x9a98]
    { 0x448C83, 0x09A98, KM2 }, // mov    eax,DWORD PTR [ecx+eax*4+0x9a98]
    { 0x449055, 0x09A08, KM1 }, // mov    eax,DWORD PTR [ebx+0x9a08]
    { 0x44905C, 0x09A08, KM1 }, // lea    edi,[ebx+0x9a08]
    { 0x449542, 0x120D8, KS }, // mov    eax,DWORD PTR [ecx+0x120d8]
    { 0x449550, 0x11E84, KW1 }, // lea    esi,[ecx+0x11e84]
    { 0x449569, 0x09B40, KT }, // mov    bl,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x449570, 0x09B40, KT }, // lea    eax,[eax+ecx*1+0x9b40]
    { 0x44957B, 0x120D8, KS }, // mov    eax,DWORD PTR [ecx+0x120d8]
    { 0x44958B, 0x120D8, KS }, // mov    DWORD PTR [ecx+0x120d8],0x0
    { 0x4495B3, 0x09B40, KT }, // mov    dl,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x4495C1, 0x09B38, KT }, // mov    edx,DWORD PTR [eax+0x9b38]
    { 0x4495CB, 0x09B40, KT }, // or     BYTE PTR [eax+0x9b40],cl
    { 0x4496D4, 0x09A90, KS }, // mov    eax,DWORD PTR [edx+0x9a90]
    { 0x4496DB, 0x09A0C, KL }, // mov    eax,DWORD PTR [edx+eax*4+0x9a0c]
    { 0x4496E8, 0x09A98, KM2 }, // lea    esi,[edx+ecx*4+0x9a98]
    { 0x4496FD, 0x09A90, KS }, // mov    esi,DWORD PTR [edx+0x9a90]
    { 0x449709, 0x09A90, KS }, // mov    DWORD PTR [edx+0x9a90],esi
    { 0x449711, 0x09A90, KS }, // mov    DWORD PTR [edx+0x9a90],0x0
    { 0x44971B, 0x09A94, KS }, // dec    DWORD PTR [edx+0x9a94]
    { 0x449721, 0x09A9C, KS }, // mov    ecx,DWORD PTR [edx+0x9a9c]
    { 0x449729, 0x09A9C, KS }, // mov    DWORD PTR [edx+0x9a9c],ecx
    { 0x449913, 0x09B38, KT }, // mov    ecx,DWORD PTR [eax+esi*1+0x9b38]
    { 0x449940, 0x09B20, KS }, // mov    eax,DWORD PTR [esi+0x9b20]
    { 0x449967, 0x12330, KS }, // mov    ecx,DWORD PTR [esi+0x12330]
    { 0x44997E, 0x120DC, KW2 }, // lea    edi,[esi+0x120dc]
    { 0x44998B, 0x09B38, KT }, // mov    ecx,DWORD PTR [eax+esi*1+0x9b38]
    { 0x449991, 0x09B1C, KS }, // and    ecx,DWORD PTR [ebp+0x9b1c]
    { 0x4499BE, 0x12330, KS }, // mov    eax,DWORD PTR [esi+0x12330]
    { 0x4499E7, 0x12330, KS }, // mov    ecx,DWORD PTR [esi+0x12330]
    { 0x4499FE, 0x120DC, KW2 }, // lea    edi,[esi+0x120dc]
    { 0x449A07, 0x09B1C, KS }, // mov    ecx,DWORD PTR [ebp+0x9b1c]
    { 0x449A11, 0x09B38, KT }, // mov    eax,DWORD PTR [eax+esi*1+0x9b38]
    { 0x449A49, 0x12330, KS }, // mov    eax,DWORD PTR [esi+0x12330]
    { 0x449A67, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x449A86, 0x09B40, KT }, // mov    cl,BYTE PTR [eax+esi*1+0x9b40]
    { 0x449A92, 0x09B38, KT }, // mov    edx,DWORD PTR [eax+0x9b38]
    { 0x449A9C, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x449AB5, 0x0BBA8, KS }, // mov    ecx,DWORD PTR [esi+0xbba8]
    { 0x449AD6, 0x11E84, KW1 }, // lea    eax,[esi+0x11e84]
    { 0x449AF3, 0x11215, KS }, // lea    ecx,[esi+0x11215]
    { 0x449B4E, 0x09B38, KT }, // lea    edi,[ebx+0x9b38]
    { 0x449B54, 0x0BBA8, KS }, // cmp    esi,DWORD PTR [ebx+0xbba8]
    { 0x449B68, 0x09B20, KS }, // mov    ecx,DWORD PTR [ebx+0x9b20]
    { 0x449BAE, 0x09B38, KT }, // lea    edi,[ebx+0x9b38]
    { 0x449BB4, 0x0BBA8, KS }, // cmp    esi,DWORD PTR [ebx+0xbba8]
    { 0x449BC8, 0x09B20, KS }, // mov    ecx,DWORD PTR [ebx+0x9b20]
    { 0x449C17, 0x09B40, KT }, // mov    bl,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x449C21, 0x09B40, KT }, // lea    eax,[eax+ecx*1+0x9b40]
    { 0x449C2E, 0x120D8, KS }, // mov    eax,DWORD PTR [ecx+0x120d8]
    { 0x449C36, 0x11E84, KW1 }, // mov    WORD PTR [ecx+eax*2+0x11e84],dx
    { 0x449C3C, 0x120D8, KS }, // inc    DWORD PTR [ecx+0x120d8]
    { 0x449C66, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x449C73, 0x09B38, KT }, // lea    edi,[esi+0x9b38]
    { 0x449C81, 0x09B20, KS }, // mov    eax,DWORD PTR [esi+0x9b20]
    { 0x449C9B, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x449CB5, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x449CD6, 0x09B40, KT }, // mov    cl,BYTE PTR [eax+esi*1+0x9b40]
    { 0x449CE3, 0x09B38, KT }, // mov    eax,DWORD PTR [eax+0x9b38]
    { 0x449CED, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x449D07, 0x0BBA8, KS }, // mov    ecx,DWORD PTR [esi+0xbba8]
    { 0x449D2B, 0x09B40, KT }, // mov    dl,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x449D35, 0x09B28, KT }, // lea    eax,[eax+ecx*1+0x9b28]
    { 0x449D47, 0x09B1C, KS }, // mov    ecx,DWORD PTR [eax+0x9b1c]
    { 0x449D75, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x449D88, 0x09B38, KT }, // lea    edi,[ecx+esi*1+0x9b38]
    { 0x449D98, 0x09B20, KS }, // mov    eax,DWORD PTR [esi+0x9b20]
    { 0x449E7D, 0x09B38, KT }, // mov    eax,DWORD PTR [edx+esi*1+0x9b38]
    { 0x449E8C, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x449ECD, 0x09B38, KT }, // mov    eax,DWORD PTR [edx+esi*1+0x9b38]
    { 0x449EE3, 0x09B1C, KS }, // mov    ecx,DWORD PTR [edx+0x9b1c]
    { 0x449F05, 0x0BBA8, KS }, // mov    eax,DWORD PTR [edi+0xbba8]
    { 0x449F18, 0x09B38, KT }, // lea    esi,[ecx+edi*1+0x9b38]
    { 0x449F28, 0x09B20, KS }, // mov    ecx,DWORD PTR [edi+0x9b20]
    { 0x449F56, 0x0BBA8, KS }, // mov    eax,DWORD PTR [edi+0xbba8]
    { 0x449F64, 0x09B38, KT }, // lea    ebp,[edi+0x9b38]
    { 0x449F77, 0x09B20, KS }, // mov    esi,DWORD PTR [edi+0x9b20]
    { 0x449FC3, 0x0BBA8, KS }, // mov    eax,DWORD PTR [edi+0xbba8]
    { 0x449FE6, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
    { 0x449FFF, 0x09B38, KT }, // lea    ebx,[ebp+0x9b38]
    { 0x44A011, 0x09B20, KS }, // mov    esi,DWORD PTR [ebp+0x9b20]
    { 0x44A079, 0x0BBA8, KS }, // mov    ecx,DWORD PTR [ebp+0xbba8]
    { 0x44A0A6, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x44A0B8, 0x09B38, KT }, // lea    edi,[esi+0x9b38]
    { 0x44A0CE, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x44A0F2, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x44A1A6, 0x09A8C, KS }, // mov    DWORD PTR [ecx+0x9a8c],esi
    { 0x44A1AC, 0x09A90, KS }, // mov    DWORD PTR [ecx+0x9a90],esi
    { 0x44A1B2, 0x09A94, KS }, // mov    DWORD PTR [ecx+0x9a94],esi
    { 0x44A1C2, 0x09A8C, KS }, // mov    eax,DWORD PTR [ecx+0x9a8c]
    { 0x44A1C8, 0x09A94, KS }, // mov    edi,DWORD PTR [ecx+0x9a94]
    { 0x44A1D3, 0x09A8C, KS }, // mov    DWORD PTR [ecx+0x9a8c],eax
    { 0x44A1D9, 0x09A94, KS }, // mov    DWORD PTR [ecx+0x9a94],edi
    { 0x44A1E1, 0x09A8C, KS }, // mov    DWORD PTR [ecx+0x9a8c],esi
    { 0x44A1F0, 0x09A0C, KL }, // mov    DWORD PTR [ecx+eax*4+0x9a0c],edx
    { 0x44A231, 0x09A08, KM1 }, // mov    ecx,DWORD PTR [eax+esi*1+0x9a08]
    { 0x44A23C, 0x09A08, KM1 }, // mov    DWORD PTR [eax+esi*1+0x9a08],ecx
    { 0x44A243, 0x09A98, KM2 }, // and    DWORD PTR [eax+esi*1+0x9a98],edx
    { 0x44A249, 0x09A8C, KS }, // mov    eax,DWORD PTR [esi+0x9a8c]
    { 0x44A24F, 0x09A94, KS }, // mov    edx,DWORD PTR [esi+0x9a94]
    { 0x44A25A, 0x09A8C, KS }, // mov    DWORD PTR [esi+0x9a8c],eax
    { 0x44A260, 0x09A94, KS }, // mov    DWORD PTR [esi+0x9a94],edx
    { 0x44A268, 0x09A8C, KS }, // mov    DWORD PTR [esi+0x9a8c],0x0
    { 0x44A27B, 0x09A0C, KL }, // mov    DWORD PTR [esi+eax*4+0x9a0c],edi
    { 0x44A281, 0x09A9C, KS }, // mov    eax,DWORD PTR [esi+0x9a9c]
    { 0x44A289, 0x09A9C, KS }, // mov    DWORD PTR [esi+0x9a9c],eax
    { 0x44A77C, 0x0D588, KS }, // lea    ecx,[eax+esi*1+0xd588]
    { 0x44A78B, 0x11214, KS }, // mov    BYTE PTR [edi+esi*1+0x11214],0x1
    { 0x44A7B4, 0x11214, KS }, // mov    al,BYTE PTR [edi+esi*1+0x11214]
    { 0x44A7C7, 0x0D588, KS }, // lea    ecx,[eax+esi*1+0xd588]
    { 0x44A7D6, 0x11214, KS }, // mov    BYTE PTR [edi+esi*1+0x11214],0x0
    { 0x44AB21, 0x09A08, KM1 }, // lea    eax,[esi+ecx*4+0x9a08]
    { 0x44AB3D, 0x09AA0, KH }, // mov    eax,DWORD PTR [esi+edi*4+0x9aa0]
    { 0x44AB58, 0x09A08, KM1 }, // mov    DWORD PTR [esi+0x9a08],edi
    { 0x44AB5E, 0x09A8C, KS }, // mov    DWORD PTR [esi+0x9a8c],edi
    { 0x44AB64, 0x09A90, KS }, // mov    DWORD PTR [esi+0x9a90],edi
    { 0x44AB6A, 0x09A94, KS }, // mov    DWORD PTR [esi+0x9a94],edi
    { 0x44AB70, 0x09A98, KM2 }, // mov    DWORD PTR [esi+0x9a98],edi
    { 0x44AB76, 0x09A9C, KS }, // mov    DWORD PTR [esi+0x9a9c],edi
    { 0x44AB83, 0x09B1C, KS }, // mov    DWORD PTR [esi+0x9b1c],edi
    { 0x44AB89, 0x09B20, KS }, // mov    DWORD PTR [esi+0x9b20],edi
    { 0x44ABA2, 0x09B1C, KS }, // mov    DWORD PTR [esi+0x9b1c],eax
    { 0x44ABA8, 0x09B20, KS }, // mov    DWORD PTR [esi+0x9b20],edx
    { 0x44ABB0, 0x09AA0, KH }, // lea    edx,[esi+0x9aa0]
    { 0x44ABC4, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x44AD23, 0x0BBB0, KTREE }, // mov    eax,DWORD PTR [esi+0xbbb0]
    { 0x44AD29, 0x0BBAC, KTREE }, // lea    edi,[esi+0xbbac]
    { 0x44AD41, 0x0D584, KS }, // mov    DWORD PTR [esi+0xd584],edi
    { 0x44AD47, 0x0D580, KS }, // mov    DWORD PTR [esi+0xd580],eax
    { 0x44AD55, 0x0D580, KS }, // mov    edx,DWORD PTR [esi+0xd580]
    { 0x44AD5B, 0x0D584, KS }, // mov    eax,DWORD PTR [esi+0xd584]
    { 0x44ADA8, 0x09A9C, KS }, // mov    DWORD PTR [esi+0x9a9c],edi
    { 0x44ADAE, 0x09A98, KM2 }, // mov    DWORD PTR [esi+0x9a98],edi
    { 0x44ADC2, 0x09AA0, KH }, // lea    eax,[esi+0x9aa0]
    { 0x44ADD2, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x44ADE5, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x44AE06, 0x09B1C, KS }, // mov    esi,DWORD PTR [edi+0x9b1c]
    { 0x44AE18, 0x09B20, KS }, // mov    ecx,DWORD PTR [edi+0x9b20]
    { 0x44AE26, 0x09AA0, KH }, // add    DWORD PTR [edi+esi*4+0x9aa0],eax
    { 0x44AE2F, 0x09AA0, KH }, // mov    DWORD PTR [edi+esi*4+0x9aa0],esi
    { 0x44AE35, 0x09B20, KS }, // mov    ecx,DWORD PTR [edi+0x9b20]
    { 0x44AE45, 0x09AA0, KH }, // mov    DWORD PTR [edi+esi*4+0x9aa0],edx
    { 0x44B096, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x44B0A4, 0x09B28, KT }, // lea    ebx,[esi+0x9b28]
    { 0x44B0EA, 0x09B28, KT }, // lea    edi,[eax+esi*1+0x9b28]
    { 0x44B10B, 0x0BBA8, KS }, // mov    eax,DWORD PTR [esi+0xbba8]
    { 0x44B21C, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
    { 0x44B23A, 0x09B28, KT }, // lea    ebx,[ecx+ebp*1+0x9b28]
    { 0x44B247, 0x09B20, KS }, // mov    esi,DWORD PTR [ebp+0x9b20]
    { 0x44B25A, 0x09B20, KS }, // mov    ecx,DWORD PTR [edi+0x9b20]
    { 0x44B261, 0x09AA0, KH }, // mov    eax,DWORD PTR [edi+esi*4+0x9aa0]
    { 0x44B271, 0x09AA0, KH }, // mov    DWORD PTR [edi+esi*4+0x9aa0],eax
    { 0x44B27A, 0x09AA0, KH }, // mov    DWORD PTR [edi+esi*4+0x9aa0],esi
    { 0x44B280, 0x09B20, KS }, // mov    ecx,DWORD PTR [edi+0x9b20]
    { 0x44B290, 0x09AA0, KH }, // mov    DWORD PTR [edi+esi*4+0x9aa0],eax
    { 0x44B2C3, 0x0BBA8, KS }, // mov    DWORD PTR [ebp+0xbba8],esi
    { 0x44B2C9, 0x0BBB0, KTREE }, // mov    DWORD PTR [ebp+0xbbb0],0x3fffffff
    { 0x44B2D3, 0x0D328, KTREE }, // mov    DWORD PTR [ebp+0xd328],esi
    { 0x44B2DB, 0x0BBB8, KTREE }, // lea    ecx,[ebp+0xbbb8]
    { 0x44B309, 0x12330, KS }, // mov    DWORD PTR [ebp+0x12330],esi
    { 0x44B30F, 0x120D8, KS }, // mov    DWORD PTR [ebp+0x120d8],esi
    { 0x44B397, 0x11864, KS }, // lea    esi,[ebx+0x11864]
    { 0x44B46F, 0x11E80, KS }, // cmp    DWORD PTR [esi+0x11e80],0xb
    { 0x44B478, 0x11864, KS }, // lea    ecx,[esi+0x11864]
    { 0x44B48A, 0x11864, KS }, // lea    eax,[eax+esi*1+0x11864]
    { 0x44B4BF, 0x11864, KS }, // lea    ecx,[esi+0x11864]
    { 0x44B4E4, 0x0D588, KS }, // lea    eax,[esi+0xd588]
    { 0x44B554, 0x09B30, KT }, // lea    eax,[esi+0x9b30]
    { 0x44B573, 0x0BBA8, KS }, // mov    DWORD PTR [esi+0xbba8],edi
    { 0x44B579, 0x0BBB8, KTREE }, // lea    ecx,[esi+0xbbb8]
    { 0x44B57F, 0x0BBB0, KTREE }, // mov    DWORD PTR [esi+0xbbb0],0x3fffffff
    { 0x44B589, 0x0BBB4, KTREE }, // mov    DWORD PTR [esi+0xbbb4],0xffffffff
    { 0x44B593, 0x0BBAC, KTREE }, // mov    DWORD PTR [esi+0xbbac],0x685418
    { 0x44B5B3, 0x0D588, KS }, // lea    eax,[esi+0xd588]
    { 0x44B5B9, 0x0BBAC, KTREE }, // mov    DWORD PTR [esi+0xbbac],0x685430
    { 0x44B5C4, 0x0D580, KS }, // mov    DWORD PTR [esi+0xd580],0x3fffffff
    { 0x44B5CE, 0x0D584, KS }, // mov    DWORD PTR [esi+0xd584],edi
    { 0x44B608, 0x11864, KS }, // lea    ecx,[esi+0x11864]
    { 0x44B63B, 0x120D8, KS }, // mov    DWORD PTR [esi+0x120d8],edi
    { 0x44B641, 0x12330, KS }, // mov    DWORD PTR [esi+0x12330],edi
    { 0x44B648, 0x11214, KS }, // mov    WORD PTR [esi+0x11214],di
    { 0x44B6A2, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ecx+0xbba8]
    { 0x44B6B2, 0x12330, KS }, // mov    eax,DWORD PTR [ecx+0x12330]
    { 0x44B6C2, 0x120D8, KS }, // mov    eax,DWORD PTR [ecx+0x120d8]
    { 0x44B6E8, 0x120DC, KW2 }, // mov    ax,WORD PTR [ecx+eax*2+0x120dc]
    { 0x44B6F8, 0x11E84, KW1 }, // mov    ax,WORD PTR [ecx+eax*2+0x11e84]
    { 0x44B70B, 0x09B28, KT }, // lea    eax,[eax+ecx*1+0x9b28]
    { 0x44B72B, 0x09B2C, KT }, // lea    eax,[eax+ecx*1+0x9b2c]
    { 0x44B74B, 0x09B30, KT }, // lea    eax,[eax+ecx*1+0x9b30]
    { 0x44B78C, 0x09B3D, KT }, // movzx  eax,BYTE PTR [eax+ecx*1+0x9b3d]
    { 0x44B7AC, 0x09B38, KT }, // mov    esi,DWORD PTR [eax+ecx*1+0x9b38]
    { 0x44B7CB, 0x09B40, KT }, // mov    al,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x44B7EB, 0x09B40, KT }, // mov    al,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x44B80B, 0x09B40, KT }, // mov    al,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x44B82B, 0x09B40, KT }, // mov    al,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x44B84C, 0x09B3E, KT }, // mov    ax,WORD PTR [eax+ecx*1+0x9b3e]
    { 0x44B86B, 0x09B40, KT }, // mov    dl,BYTE PTR [eax+ecx*1+0x9b40]
    { 0x44B872, 0x09B40, KT }, // lea    eax,[eax+ecx*1+0x9b40]
    { 0x44B89B, 0x09B38, KT }, // mov    eax,DWORD PTR [eax+ecx*1+0x9b38]
    { 0x44B8A4, 0x09B1C, KS }, // and    eax,DWORD PTR [edx+0x9b1c]
    { 0x44B987, 0x0BBA8, KS }, // mov    ecx,DWORD PTR [esi+0xbba8]
    { 0x44B9A6, 0x09B40, KT }, // mov    cl,BYTE PTR [eax+esi*1+0x9b40]
    { 0x44B9B6, 0x09B40, KT }, // mov    BYTE PTR [edi+0x9b40],cl
    { 0x44B9BE, 0x09B38, KT }, // mov    eax,DWORD PTR [edi+0x9b38]
    { 0x44B9D5, 0x09B20, KS }, // mov    ecx,DWORD PTR [esi+0x9b20]
    { 0x44B9DB, 0x09B38, KT }, // mov    edx,DWORD PTR [edi+0x9b38]
    { 0x44B9F4, 0x0BBA8, KS }, // mov    ecx,DWORD PTR [esi+0xbba8]
    { 0x44BA15, 0x11E84, KW1 }, // lea    edi,[esi+0x11e84]
    { 0x44BA23, 0x120D8, KS }, // mov    eax,DWORD PTR [esi+0x120d8]
    { 0x44BA39, 0x09B40, KT }, // mov    bl,BYTE PTR [eax+esi*1+0x9b40]
    { 0x44BA40, 0x09B40, KT }, // lea    eax,[eax+esi*1+0x9b40]
    { 0x44BA4B, 0x120D8, KS }, // mov    eax,DWORD PTR [esi+0x120d8]
    { 0x44BA62, 0x11215, KS }, // lea    ecx,[esi+0x11215]
    { 0x44C05C, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
    { 0x44C06C, 0x0BBA8, KS }, // mov    DWORD PTR [ebp+0xbba8],eax
    { 0x44C1A3, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
    { 0x44C1C1, 0x09B0C, KT }, // lea    esi,[eax+ebp*1+0x9b0c]
    { 0x44C1CB, 0x0BBA8, KS }, // mov    DWORD PTR [ebp+0xbba8],edx
    { 0x44C243, 0x12330, KS }, // mov    eax,DWORD PTR [ebp+0x12330]
    { 0x44C250, 0x120DC, KW2 }, // mov    WORD PTR [ebp+eax*2+0x120dc],cx
    { 0x44C256, 0x12330, KS }, // inc    DWORD PTR [ebp+0x12330]
    { 0x44C3B3, 0x0BBAC, KTREE }, // lea    ecx,[ebp+0xbbac]
    { 0x44C3D2, 0x09B28, KT }, // lea    edi,[eax+ebp*1+0x9b28]
    { 0x44C3FD, 0x09AA0, KS }, // cmp    DWORD PTR [ebp+0x9aa0],0x1f
    { 0x44C431, 0x09B1C, KS }, // mov    esi,DWORD PTR [ebp+0x9b1c]
    { 0x44C4CC, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
    { 0x44C4DB, 0x09B5C, KT }, // lea    esi,[ebp+0x9b5c]
    { 0x44C4EC, 0x09B20, KS }, // mov    ecx,DWORD PTR [ebp+0x9b20]
    { 0x44C507, 0x0BBA8, KS }, // mov    eax,DWORD PTR [ebp+0xbba8]
};

// tree-pool template: offsets relative to the tree's own storage
static const CSite kTP[] =
{
    { 0x449854, 0x1740, KP }, // mov    eax,DWORD PTR [edx+0x1740]
    { 0x44985B, 0x1294, KP }, // mov    eax,DWORD PTR [edx+eax*4+0x1294]
    { 0x449868, 0x1748, KP }, // lea    esi,[edx+ecx*4+0x1748]
    { 0x44987D, 0x1740, KP }, // mov    esi,DWORD PTR [edx+0x1740]
    { 0x44988C, 0x1740, KP }, // mov    DWORD PTR [edx+0x1740],esi
    { 0x449894, 0x1740, KP }, // mov    DWORD PTR [edx+0x1740],0x0
    { 0x44989E, 0x1744, KP }, // dec    DWORD PTR [edx+0x1744]
    { 0x4498A4, 0x1770, KP }, // mov    ecx,DWORD PTR [edx+0x1770]
    { 0x4498AC, 0x1770, KP }, // mov    DWORD PTR [edx+0x1770],ecx
    { 0x44A366, 0x173C, KP }, // mov    DWORD PTR [ecx+0x173c],esi
    { 0x44A36C, 0x1740, KP }, // mov    DWORD PTR [ecx+0x1740],esi
    { 0x44A372, 0x1744, KP }, // mov    DWORD PTR [ecx+0x1744],esi
    { 0x44A382, 0x173C, KP }, // mov    eax,DWORD PTR [ecx+0x173c]
    { 0x44A388, 0x1744, KP }, // mov    edi,DWORD PTR [ecx+0x1744]
    { 0x44A395, 0x173C, KP }, // mov    DWORD PTR [ecx+0x173c],eax
    { 0x44A39B, 0x1744, KP }, // mov    DWORD PTR [ecx+0x1744],edi
    { 0x44A3A3, 0x173C, KP }, // mov    DWORD PTR [ecx+0x173c],esi
    { 0x44A3B2, 0x1294, KP }, // mov    DWORD PTR [ecx+eax*4+0x1294],edx
    { 0x44A3EE, 0x1294, KP }, // lea    ecx,[esi+0x1294]
    { 0x44A420, 0x1770, KP }, // mov    DWORD PTR [esi+0x1770],eax
    { 0x44AD63, 0x1780, KP }, // mov    ax,WORD PTR [eax+edx*2+0x1780]
    { 0x44B00F, 0x1748, KP }, // lea    edx,[eax+ecx*4+0x1748]
    { 0x44B02A, 0x173C, KP }, // mov    ecx,DWORD PTR [eax+0x173c]
    { 0x44B030, 0x1744, KP }, // mov    edi,DWORD PTR [eax+0x1744]
    { 0x44B03E, 0x173C, KP }, // mov    DWORD PTR [eax+0x173c],ecx
    { 0x44B044, 0x1744, KP }, // mov    DWORD PTR [eax+0x1744],edi
    { 0x44B04C, 0x173C, KP }, // mov    DWORD PTR [eax+0x173c],0x0
    { 0x44B05F, 0x1294, KP }, // mov    DWORD PTR [eax+ecx*4+0x1294],esi
    { 0x44B065, 0x1770, KP }, // mov    ecx,DWORD PTR [eax+0x1770]
    { 0x44B06D, 0x1770, KP }, // mov    DWORD PTR [eax+0x1770],ecx
    { 0x44B1A2, 0x1780, KP }, // mov    WORD PTR [esi+ecx*2+0x1780],dx
    { 0x449236, 0x04A8, KP }, // mov    DWORD PTR [eax+0x4a8],ecx
    { 0x44923C, 0x04AC, KP }, // mov    DWORD PTR [eax+0x4ac],ecx
    { 0x449242, 0x04B0, KP }, // mov    DWORD PTR [eax+0x4b0],ecx
};

// pool limits: immediate 1Fh (capacity) or 1Eh (capacity-1)
struct CImm { unsigned addr; unsigned char oldv; };
static const CImm kCI[] =
{
    { 0x448C78, 0x1F }, { 0x448CB9, 0x1F }, { 0x448CBD, 0x1F },
    { 0x44906E, 0x1F }, { 0x4490AF, 0x1F }, { 0x4490D6, 0x1F }, { 0x4490F8, 0x1F }, { 0x44913A, 0x1F },
    { 0x449706, 0x1F },
    { 0x44A1D0, 0x1F }, { 0x44A1E6, 0x1E }, { 0x44A1F7, 0x1F },
    { 0x44A257, 0x1F }, { 0x44A271, 0x1E },
    { 0x44AB8E, 0x1E }, { 0x44ABDA, 0x1F },
    { 0x44ADFA, 0x1F },
    { 0x44C401, 0x1F },
};

// name limits: immediate dword 129h (297) or 128h (296)
struct CNum { unsigned addr; unsigned oldv; };
static const CNum kNI[] =
{
    { 0x449886, 0x129 }, // cmp    ecx,0x129
    { 0x44A38F, 0x129 }, // cmp    eax,0x129
    { 0x44A3A8, 0x128 }, // mov    eax,0x128
    { 0x44A3B9, 0x129 }, // cmp    edx,0x129
    { 0x44A3D7, 0x129 }, // mov    ecx,0x129
    { 0x44B038, 0x129 }, // cmp    ecx,0x129
    { 0x44B055, 0x128 }, // mov    ecx,0x128
    { 0x44B559, 0x129 }, // mov    ecx,0x129
    { 0x44C1A8, 0x129 }, // cmp    eax,0x129
    { 0x48F74C, 0x129 }, // cmp    ecx,0x129
    { 0x48F77D, 0x129 }, // cmp    edx,0x129
    { 0x48FAE1, 0x129 }, // cmp    ecx,0x129
    { 0x4D0F41, 0x129 }, // mov    ecx,0x129
};

// the table of 297 dwords at 00783F80 (inside the global object at 00782728)
struct CExt { unsigned addr; int sib; };
static const CExt kEX[] =
{
    { 0x4CC142, 1 }, { 0x4CC165, 1 }, { 0x4CC3B2, 1 }, { 0x4CC3D1, 1 }, { 0x4D0F3C, 0 },
};

static unsigned g_charN = 31, g_charD = 0, g_nameN = 297;
static unsigned g_tXB = 0, g_tW = 10;       // tree bitmap (offset inside the tree's storage, words)

static void* __cdecl CharAlloc(size_t sz, int a, int b)
{
    typedef void* (__cdecl *Fn)(size_t, int, int);
    void* p = ((Fn)0x5605E0)(sz, a, b);
    if (p) memset(p, 0, sz);
    return p;
}

// ---- tree bitmap clearing (replaces 10 unrolled stores; the new tree has more words)
__declspec(naked) static void ZeroTreeEsi()
{
    __asm
    {
        pushad
        mov  edi, esi
        add  edi, g_tXB
        mov  ecx, g_tW
        xor  eax, eax
        cld
        rep  stosd
        popad
        ret
    }
}
__declspec(naked) static void ZeroTreeEcx()
{
    __asm
    {
        pushad
        mov  edi, ecx
        add  edi, g_tXB
        mov  ecx, g_tW
        xor  eax, eax
        cld
        rep  stosd
        popad
        ret
    }
}

// ---- name table overflow logger
static int g_nameFails = 0;
static void __cdecl LogNameFail(const char* name, unsigned count)
{
    g_nameFails++;
    if (g_nameFails <= 300)
        Log("NAME TABLE FULL (%u entries): could not register \"%s\"", count, (name && !IsBadReadPtr(name, 1)) ? name : "(unreadable)");
}
__declspec(naked) static void NameFailHook()
{
    __asm
    {
        pushad
        push eax                    // count
        mov  eax, [esp + 0x38]      // the name string (the game's own [esp+14h])
        push eax
        call LogNameFail
        add  esp, 8
        popad
        push 0x44C481
        ret
    }
}
static void InstallNameFailLog()
{
    static const unsigned char want[6] = { 0x0F, 0x84, 0xCF, 0x02, 0x00, 0x00 };
    if (memcmp((void*)0x44C1AC, want, 6) != 0) { Log("name overflow log: bytes at 0044C1AC are not the ones we studied, not installed."); return; }
    unsigned char jmp[6] = { 0x0F, 0x84, 0, 0, 0, 0 };
    unsigned rel = (unsigned)NameFailHook - (0x44C1AC + 6);
    memcpy(jmp + 2, &rel, 4);
    if (WriteMem(0x44C1AC, jmp, 6)) Log("name overflow log installed");
    else Log("name overflow log: write failed");
}

// ---------------------------------------------------------------------------------------------
// Team bonus table (tested by the author with 24 teams of 10 heroes).
// The game keeps its team bonuses (data/team_bonus.xmlb) in one object: a vtable pointer, then 17 records
// of 0x38 bytes (6 name and description fields, up to 6 hero ids, a spare number, the hero count), then
// two more numbers: how many teams were read, and the index of the team being read or shown.
// The loader stops at 17 teams and at 6 heroes per team; whatever is beyond is silently dropped
// (confirmed by testing for teams).
// Here the object is rebuilt with 100 records of 23 heroes each (TeamBonus=on). The game's own getter
// (0x4C2370) is replaced so it hands out the bigger object (the game's own record constructor is run on every
// record), and the places that depend on the layout are patched:
//   * the record size (7 places) and the hero-slot stride (1 place),
//   * the hero count, which sits after the last hero slot (4 places),
//   * the limit of 6 heroes (1 place) and the limit of 17 teams (1 place),
//   * the two numbers after the records, which move behind the last record (14 places).
// TeamBonus=off leaves the game as it was (17 teams of 6 heroes).
// ---------------------------------------------------------------------------------------------
static unsigned g_tbN = 17;     // teams
static unsigned g_tbH = 6;      // heroes per team
static unsigned g_tbS = 0x38;   // record size in bytes
static void*    g_tbObj = NULL;

static void* __cdecl TeamBonusGetter()
{
    if (g_tbObj) return g_tbObj;
    unsigned sz = 4 + g_tbN * g_tbS + 8;
    char* p = (char*)VirtualAlloc(NULL, (sz + 0xFFF) & ~0xFFFu, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);   // zero filled
    if (!p) { Log("team bonus: out of memory"); return (void*)0x778680; }
    *(unsigned*)p = 0x68EDF0;            // the object's vtable, as the game sets it
    typedef void (__stdcall *VecCtor)(void*, unsigned, unsigned, void*, void*);
    ((VecCtor)0x6722D4)(p + 4, g_tbS, g_tbN, (void*)0x4C1E50, (void*)0x48EF90);   // same call the game's getter makes
    *(unsigned*)(p + 4 + g_tbN * g_tbS) = 0;                                      // teams read so far
    *(unsigned*)(p + 8 + g_tbN * g_tbS) = 0xFFFFFFFF;                             // current team index (none)
    g_tbObj = p;
    Log("team bonus object built: room for %u teams of %u heroes", g_tbN, g_tbH);
    return p;
}

// An on/off setting in rosterhack.ini. A missing setting is written as "on". Only "off" turns it
// off; anything else (including a number left over from an older version) counts as on.
static bool IniOn(const char* section, const char* key, const char* iniPath)
{
    char v[16] = "";
    GetPrivateProfileStringA(section, key, "", v, sizeof(v), iniPath);
    if (v[0] == 0)
    {
        WritePrivateProfileStringA(section, key, "on", iniPath);
        Log("no %s in rosterhack.ini: wrote %s=on", key, key);
        return true;
    }
    return _stricmp(v, "off") != 0;
}

static void InstallTeamBonus(const char* iniPath)
{
    // TeamBonus=on: up to 100 teams of up to 23 heroes. off: the original 17 teams of 6 heroes.
    bool on = IniOn("Limits", "TeamBonus", iniPath);
    unsigned n = on ? 100 : 17;
    unsigned h = on ? 23 : 6;
    if (!on) { Log("TeamBonus=off: team bonus table left as in the original game"); return; }

    static const unsigned kCountSites[5] = { 0x4C1E8C, 0x4C1EB2, 0x4C1F3B, 0x4C21D2, 0x4C21EB };   // [object + 0x3C0]
    static const unsigned kIdxSites[9]   = { 0x4C1E82, 0x4C1EFF, 0x4C1F07, 0x4C1FFE, 0x4C203E, 0x4C207E, 0x4C20BE, 0x4C20FE, 0x4C2155 };   // [object + 0x3C4]
    static const unsigned kSizeSites[7]  = { 0x4C1F4A, 0x4C2004, 0x4C2044, 0x4C2084, 0x4C20C4, 0x4C2104, 0x4C2204 };   // imul reg, reg, 0x38 (the 0x38 byte)
    static const unsigned kHeroCnt[4]    = { 0x4C1F9C, 0x4C1FC7, 0x4C22C2, 0x4C22DD };               // [record + 0x38]: hero count
    static const unsigned char getterWant[6] = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    bool ok = memcmp((void*)0x4C2370, getterWant, 6) == 0 && B8(0x4C21D6) == 0x83 && B8(0x4C21D7) == 0xF8 && B8(0x4C21D8) == 0x11;
    for (int i = 0; i < 5; i++) if (D32(kCountSites[i]) != 0x3C0) ok = false;
    for (int i = 0; i < 9; i++) if (D32(kIdxSites[i]) != 0x3C4) ok = false;
    for (int i = 0; i < 7; i++) if (B8(kSizeSites[i]) != 0x38 || B8(kSizeSites[i] - 2) != 0x6B) ok = false;
    for (int i = 0; i < 4; i++) if (B8(kHeroCnt[i]) != 0x38) ok = false;
    if (B8(0x4C22C5) != 0x06 || B8(0x4C22C3) != 0x83) ok = false;      // cmp eax, 6
    if (B8(0x4C22EA) != 0x0E || B8(0x4C22E8) != 0x6B) ok = false;      // imul ecx, ecx, 0xE (hero slot stride)
    if (!ok) { Log("team bonus: the game's code is not what we studied, TeamBonus not applied."); return; }

    g_tbN = n; g_tbH = h;
    g_tbS = 0x38 + 4 * (h - 6);
    int fails = 0;
    unsigned newOff = 4 + n * g_tbS, newIdx = newOff + 4;
    for (int i = 0; i < 5; i++) if (!WriteMem(kCountSites[i], &newOff, 4)) fails++;
    for (int i = 0; i < 9; i++) if (!WriteMem(kIdxSites[i], &newIdx, 4)) fails++;
    unsigned char sz8 = (unsigned char)g_tbS;
    for (int i = 0; i < 7; i++) if (!WriteMem(kSizeSites[i], &sz8, 1)) fails++;
    unsigned char hc = (unsigned char)(0x38 + 4 * (h - 6));      // the hero count moves back by the extra hero slots
    for (int i = 0; i < 4; i++) if (!WriteMem(kHeroCnt[i], &hc, 1)) fails++;
    unsigned char lim = (unsigned char)n, hl = (unsigned char)h, st = (unsigned char)(g_tbS / 4);
    if (!WriteMem(0x4C21D8, &lim, 1)) fails++;
    if (!WriteMem(0x4C22C5, &hl, 1)) fails++;
    if (!WriteMem(0x4C22EA, &st, 1)) fails++;
    unsigned char jmp[5] = { 0xE9, 0, 0, 0, 0 };
    unsigned rel = (unsigned)&TeamBonusGetter - (0x4C2370 + 5);
    memcpy(jmp + 1, &rel, 4);
    if (!WriteMem(0x4C2370, jmp, 5)) fails++;
    Log("team bonus table raised to %u teams of %u heroes (was 17 of 6), %d write failure(s). The object is built when the game first asks for it.", n, h, fails);
}

static DWORD WINAPI CharWatcher(LPVOID)
{
    unsigned lastUsed = 0xFFFFFFFF, maxUsed = 0, maxNames = 0;
    unsigned lastHero = 0xFFFFFFFF, heroStable = 0, heroLogged = 0xFFFFFFFF;
    bool announced = false;
    for (int i = 0; i < 36000; i++)        // up to ~1 hour
    {
        Sleep(100);
        unsigned mgr = *(volatile unsigned*)0x71770C;
        if (!mgr) continue;
        if (!announced) { Log("character manager created at %08X (%u pool slots, %u names)", mgr, g_charN, g_nameN); announced = true; }
        unsigned names = *(volatile unsigned*)(mgr + 0xBBA8 + g_charD);
        if (names > maxNames && names <= 600) { maxNames = names; Log("name table: %u of %u entries in use (new high)", names, g_nameN); }
        {
            // number of herostat entries the manager registered (same field its getter returns)
            unsigned heroes = *(volatile unsigned*)(mgr + *(volatile unsigned*)0x44B6B2);
            if (heroes <= 64 && heroes != 0)
            {
                if (heroes == lastHero) heroStable++; else { lastHero = heroes; heroStable = 0; }
                if (heroStable == 20 && heroes != heroLogged)
                {
                    heroLogged = heroes;
                    Log("herostat entries: %u%s", heroes, g_playAuto ? " (the playable lookup runs once per entry minus the default)" : "");
                }
            }
        }
        unsigned used = *(volatile unsigned*)(mgr + 0x9AA0 + g_charD);
        if (used != lastUsed)
        {
            lastUsed = used;
            if (used > maxUsed)
            {
                maxUsed = used;
                Log("character pool: %u of %u slots in use (new high)", used, g_charN);
            }
        }
    }
    return 0;
}

// rewrite an instruction that addresses [register + disp32] (or [register + index*scale + disp32])
// so that it addresses the absolute address instead. The instruction keeps its length.
static bool PatchAbs(unsigned dispAddr, int sib, unsigned absAddr)
{
    unsigned char m = B8(dispAddr - (sib ? 2 : 1));
    unsigned char s = sib ? B8(dispAddr - 1) : 0;
    unsigned char buf[6];
    int start;
    if (sib)
    {
        if ((m & 0xC0) != 0x80 || (m & 7) != 4) return false;
        buf[0] = (unsigned char)(m & 0x3F);
        buf[1] = (unsigned char)((s & 0xF8) | 5);
        start = 2;
    }
    else
    {
        if ((m & 0xC0) != 0x80 || (m & 7) == 4) return false;
        buf[0] = (unsigned char)((m & 0x38) | 5);
        start = 1;
    }
    memcpy(buf + start, &absAddr, 4);
    return WriteMem(dispAddr - start, buf, start + 4);
}

static bool InstallCharManager(const char* iniPath)
{
    // CharPool=on: 64 character slots. NameTable=on: 480 names. off: the original 31 and 297.
    unsigned n  = IniOn("Limits", "CharPool", iniPath) ? 64 : 31;
    unsigned nm = IniOn("Limits", "NameTable", iniPath) ? 480 : 297;

    if (n == 31 && nm == 297) { Log("CharPool=off and NameTable=off: character manager left as in the original game"); return true; }

    if (B8(0x44B35C) != 0xBF)
    {
        Log("character manager: the hero pool patch is not in place, so the character manager is left alone.");
        return false;
    }

    // ---- verify every byte we are about to touch
    int bad = 0;
    for (unsigned i = 0; i < sizeof(kC) / sizeof(kC[0]); i++)
        if (D32(kC[i].addr) != kC[i].oldv) { Log("MISMATCH at %08X: expected %05X, found %08X", kC[i].addr, kC[i].oldv, D32(kC[i].addr)); bad++; }
    for (unsigned i = 0; i < sizeof(kTP) / sizeof(kTP[0]); i++)
        if (D32(kTP[i].addr) != kTP[i].oldv) { Log("MISMATCH at %08X: expected %04X, found %08X", kTP[i].addr, kTP[i].oldv, D32(kTP[i].addr)); bad++; }
    for (unsigned i = 0; i < sizeof(kCI) / sizeof(kCI[0]); i++)
        if (B8(kCI[i].addr) != kCI[i].oldv) { Log("MISMATCH at %08X: expected %02X, found %02X", kCI[i].addr, kCI[i].oldv, B8(kCI[i].addr)); bad++; }
    for (unsigned i = 0; i < sizeof(kNI) / sizeof(kNI[0]); i++)
        if (D32(kNI[i].addr) != kNI[i].oldv) { Log("MISMATCH at %08X: expected %03X, found %08X", kNI[i].addr, kNI[i].oldv, D32(kNI[i].addr)); bad++; }
    for (unsigned i = 0; i < sizeof(kEX) / sizeof(kEX[0]); i++)
        if (D32(kEX[i].addr) != 0x1858) { Log("MISMATCH at %08X: expected 1858, found %08X", kEX[i].addr, D32(kEX[i].addr)); bad++; }
    for (unsigned i = 0; i < sizeof(kC) / sizeof(kC[0]); i++)
        if (kC[i].kind == KTREE)
        {
            unsigned char m = B8(kC[i].addr - 1);
            if ((m & 0xC0) != 0x80 || (m & 7) == 4) { Log("MISMATCH at %08X: unexpected instruction form %02X", kC[i].addr, m); bad++; }
        }
    static const unsigned char cave1[6] = { 0x8D, 0x8E, 0x48, 0x17, 0x00, 0x00 };   // lea ecx,[esi+1748]
    static const unsigned char cave2[6] = { 0x8D, 0x81, 0x48, 0x17, 0x00, 0x00 };   // lea eax,[ecx+1748]
    if (memcmp((void*)0x44A3F9, cave1, 6)) { Log("MISMATCH at 0044A3F9"); bad++; }
    if (memcmp((void*)0x44B2DF, cave2, 6)) { Log("MISMATCH at 0044B2DF"); bad++; }
    if (D32(0x44B916) != 0x12334) { Log("MISMATCH at 0044B916 (manager size)"); bad++; }
    if (D32(0x449B01) != 0xF488)  { Log("MISMATCH at 00449B01"); bad++; }
    if (D32(0x44BA70) != 0xF488)  { Log("MISMATCH at 0044BA70"); bad++; }
    if (D32(0x673260) != 0xD588)  { Log("MISMATCH at 00673260"); bad++; }
    if (B8(0x44B91A) != 0xE8 || D32(0x44B91B) + 0x44B91F != 0x5605E0) { Log("MISMATCH at 0044B91A (allocation call)"); bad++; }
    if (bad) { Log("character manager: %d site(s) did not match. Nothing patched.", bad); return false; }

    // ---- layout
    g_charN = n; g_charD = (n - 31) * 0x4F8; g_nameN = nm;
    const unsigned D  = g_charD;
    const bool bigPool  = (n > 31);
    const bool bigNames = (nm > 297);
    const unsigned N2 = nm;
    const unsigned E0 = 0x12334 + D;              // end of the moved manager (manager-relative)
    const unsigned XM1 = E0 - 4;                  // pool tables (relative to the pool = manager + 4)
    const unsigned XL  = XM1 + 8;
    const unsigned XM2 = XL + 0x100;
    const unsigned XH  = XM2 + 8;
    const unsigned TOFF  = E0 + 0x210;            // new name table (manager-relative)
    const unsigned W1OFF = (TOFF + 0x1C * (N2 + 2) + 3) & ~3u;
    const unsigned W2OFF = (W1OFF + 2 * (N2 + 4) + 3) & ~3u;
    const unsigned newSize = (W2OFF + 2 * (N2 + 4) + 15) & ~15u;

    // the name lookup tree (own memory)
    const unsigned tXP   = 16 * N2 + 4;
    const unsigned tTAIL = tXP + 4 * (N2 + 1);
    const unsigned tXB   = tTAIL + 0xC;
    const unsigned tW    = (N2 + 1 + 31) / 32;
    const unsigned tUSED = tXB + 4 * tW;
    const unsigned tXW   = tUSED + 4 + 0xC;        // relative to the tree object (the container starts 0Ch in)
    const unsigned tSize = (tXW + 2 * (N2 + 4) + 0x40 + 15) & ~15u;
    g_tXB = tXB; g_tW = tW;

    unsigned char* tree = NULL;
    unsigned* ext = NULL;
    if (bigNames)
    {
        tree = (unsigned char*)VirtualAlloc(NULL, tSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        ext  = (unsigned*)VirtualAlloc(NULL, 4 * (N2 + 8), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!tree || !ext) { Log("character manager: could not allocate memory. Nothing patched."); return false; }
    }
    Log("character manager: pool %u slots (fields move up by %X), names %u, manager grows to %X bytes", n, D, N2, newSize);
    if (bigNames) Log("  name table at +%X, tree at %08X (%X bytes), global table at %08X", TOFF, (unsigned)tree, tSize, (unsigned)ext);

    int fails = 0;
    #define WR32(a, val) do { unsigned _v = (val); if (!WriteMem((a), &_v, 4)) { Log("write failed at %08X", (unsigned)(a)); fails++; } } while (0)

    for (unsigned i = 0; i < sizeof(kC) / sizeof(kC[0]); i++)
    {
        unsigned a = kC[i].addr, o = kC[i].oldv;
        switch (kC[i].kind)
        {
        case KM1: WR32(a, bigPool ? XM1 : o); break;
        case KL:  WR32(a, bigPool ? XL  : o); break;
        case KM2: WR32(a, bigPool ? XM2 : o); break;
        case KH:  WR32(a, bigPool ? XH  : o); break;
        case KT:
            if (bigNames) WR32(a, TOFF + (o - 0x9B28)); else WR32(a, o + D);
            break;
        case KW1: if (bigNames) WR32(a, W1OFF); else WR32(a, o + D); break;
        case KW2: if (bigNames) WR32(a, W2OFF); else WR32(a, o + D); break;
        case KTREE:
            if (bigNames)
            {
                unsigned off = (o == 0xD328) ? (tUSED + 0xC) : (o - 0xBBAC);
                if (!PatchAbs(a, 0, (unsigned)tree + off)) { Log("could not patch %08X", a); fails++; }
            }
            else WR32(a, o + D);
            break;
        default: WR32(a, o + D); break;
        }
    }
    if (bigNames)
    {
        for (unsigned i = 0; i < sizeof(kTP) / sizeof(kTP[0]); i++)
        {
            unsigned a = kTP[i].addr, o = kTP[i].oldv, v = 0;
            switch (o)
            {
            case 0x1294: v = tXP; break;
            case 0x173C: v = tTAIL; break;
            case 0x1740: v = tTAIL + 4; break;
            case 0x1744: v = tTAIL + 8; break;
            case 0x1748: v = tXB; break;
            case 0x1770: v = tUSED; break;
            case 0x1780: v = tXW; break;
            case 0x04A8: v = 4 * (N2 + 1); break;
            case 0x04AC: v = 4 * (N2 + 1) + 4; break;
            case 0x04B0: v = 4 * (N2 + 1) + 8; break;
            }
            WR32(a, v);
        }
        for (unsigned i = 0; i < sizeof(kNI) / sizeof(kNI[0]); i++)
            WR32(kNI[i].addr, kNI[i].oldv == 0x129 ? N2 : N2 - 1);
        for (unsigned i = 0; i < sizeof(kEX) / sizeof(kEX[0]); i++)
            if (!PatchAbs(kEX[i].addr, kEX[i].sib, (unsigned)ext)) { Log("could not patch %08X", kEX[i].addr); fails++; }

        // the tree's bitmap clears (35 bytes each: call + nops)
        unsigned char blk[35];
        memset(blk, 0x90, sizeof(blk));
        blk[0] = 0xE8; unsigned rel = (unsigned)ZeroTreeEsi - (0x44A3F9 + 5); memcpy(blk + 1, &rel, 4);
        if (!WriteMem(0x44A3F9, blk, 35)) { Log("write failed at 0044A3F9"); fails++; }
        rel = (unsigned)ZeroTreeEcx - (0x44B2DF + 5); memcpy(blk + 1, &rel, 4);
        if (!WriteMem(0x44B2DF, blk, 35)) { Log("write failed at 0044B2DF"); fails++; }
    }
    else
    {
        // names unchanged: the two unrolled clears stay as they are
    }
    for (unsigned i = 0; i < sizeof(kCI) / sizeof(kCI[0]); i++)
    {
        unsigned char v = (kCI[i].oldv == 0x1F) ? (unsigned char)n : (unsigned char)(n - 1);
        if (!WriteMem(kCI[i].addr, &v, 1)) { Log("write failed at %08X", kCI[i].addr); fails++; }
    }
    WR32(0x44B916, newSize);
    WR32(0x449B01, 0xF488 + D);
    WR32(0x44BA70, 0xF488 + D);
    WR32(0x673260, 0xD588 + D);
    WR32(0x44B91B, (unsigned)CharAlloc - 0x44B91F);
    #undef WR32

    Log("character manager patched: %u + %u + %u offset sites, %d write failure(s)",
        (unsigned)(sizeof(kC) / sizeof(kC[0])), (unsigned)(sizeof(kTP) / sizeof(kTP[0])), (unsigned)(sizeof(kCI) / sizeof(kCI[0]) + sizeof(kNI) / sizeof(kNI[0]) + sizeof(kEX) / sizeof(kEX[0])), fails);
    HANDLE h = CreateThread(NULL, 0, CharWatcher, NULL, 0, NULL);
    if (h) CloseHandle(h);
    return fails == 0;
}


// ---------------------------------------------------------------- cel shading off (experimental)
// [Visual] CelShading=off hides the black cel-shading outline of models. The outline is a separate
// model node named "<something>_outline". Every node goes through igNode::postFileRead (libIGSg.dll)
// when a model finishes loading. We hook that, and when the node's name ends in "_outline" we set the
// node's IS_INVISIBLE flag (0x20 in the flags word at +0x14), so it is simply not drawn.
// Nothing on disk is changed. CelShading=on (default) installs nothing.

static volatile LONG g_celHits = 0;

// Names of cel-shading shell nodes. Most end in "_outline". A few skins in the stock game name the
// shell differently (found by scanning every file in the actors folder): "804" (Iceman 0801, 0803 and
// 7107), the typos "_outine" (0321, 11801, 15384 and extra parts in 4101, 7801 to 7805, 13101) and
// "_outlline" (2502, 2507), and a bare "outline" (0204).
static bool IsOutlineName(const char* nm)
{
    size_t n = strlen(nm);
    if (n >= 8 && _stricmp(nm + n - 8, "_outline") == 0) return true;
    if (n >= 7 && _stricmp(nm + n - 7, "_outine") == 0) return true;
    if (n >= 9 && _stricmp(nm + n - 9, "_outlline") == 0) return true;
    if (_stricmp(nm, "outline") == 0) return true;
    if (strcmp(nm, "804") == 0) return true;
    // Number-named shells in skins 3901 ("3902"), 3504 and 5001 ("5002"). There is no skin file 3902 or 5002, so these
    // names can never be a body. (The shells in 2401 "2402" and 7401/7403-7405 "7402" are NOT listed: skins 2402 and
    // 7402 name their real body the same, and would lose their body. Those skins need renaming instead.)
    if (strcmp(nm, "3902") == 0 || strcmp(nm, "5002") == 0) return true;
    // "outl": the shell name used in the hand-fixed skins 2401, 7401 to 7405 and 8601. Their shells were named after
    // another skin or after the body ("2402", "7402", "8601"), which cannot be matched by name without also hiding
    // real bodies. The fix renames the shell to this four-letter name, same length so the file layout is unchanged.
    if (strcmp(nm, "outl") == 0) return true;
    // "_outl": same idea for five-digit skin numbers (15412), where the shell was named after the body ("15412").
    if (strcmp(nm, "_outl") == 0) return true;
    return false;
}

static void __cdecl CelNode(void* node)
{
    __try
    {
        const char* nm = *(const char**)((unsigned char*)node + 8);
        if (!nm) return;
        if (IsOutlineName(nm))
        {
            *(unsigned*)((unsigned char*)node + 0x14) |= 0x20;
            LONG k = InterlockedIncrement(&g_celHits);
            if (k <= 60) Log("cel shading off: hid outline node '%s' (#%ld)", nm, k);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static DWORD WINAPI CelWatcher(LPVOID)
{
    HMODULE m = NULL;
    for (int i = 0; i < 6000 && !(m = GetModuleHandleA("libIGSg.dll")); i++) Sleep(10);
    if (!m) { Log("cel shading off: libIGSg.dll never loaded, nothing done"); return 0; }
    unsigned base = (unsigned)m;
    unsigned at = base + 0x56D60;                       // igNode::postFileRead
    // push esi / mov esi,ecx / call dword ptr [abs]  (9 bytes), then mov eax,[esi]
    static const unsigned char head[5] = { 0x56, 0x8B, 0xF1, 0xFF, 0x15 };
    static const unsigned char tail[2] = { 0x8B, 0x06 };
    if (memcmp((const void*)at, head, 5) != 0 || memcmp((const void*)(at + 9), tail, 2) != 0)
    {
        Log("cel shading off: libIGSg.dll code does not match (base %08X), not installed", base);
        return 0;
    }
    unsigned char* cave = (unsigned char*)VirtualAlloc(NULL, 0x40, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) { Log("cel shading off: no memory for the hook"); return 0; }
    unsigned char* p = cave;
    *p++ = 0x60;                                        // pushad
    *p++ = 0x51;                                        // push ecx
    *p++ = 0xE8; { int r = (int)((unsigned)&CelNode - ((unsigned)p + 4)); memcpy(p, &r, 4); p += 4; }
    *p++ = 0x83; *p++ = 0xC4; *p++ = 0x04;              // add esp,4
    *p++ = 0x61;                                        // popad
    memcpy(p, (const void*)at, 9); p += 9;              // the original 9 bytes, as relocated in memory
    *p++ = 0xE9; { int r = (int)((at + 9) - ((unsigned)p + 4)); memcpy(p, &r, 4); p += 4; }
    FlushInstructionCache(GetCurrentProcess(), cave, 0x40);
    unsigned char j[9] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 };
    int rel = (int)((unsigned)cave - (at + 5));
    memcpy(j + 1, &rel, 4);
    if (WriteMem(at, j, 9)) Log("cel shading off: hook installed in libIGSg.dll (base %08X)", base);
    else Log("cel shading off: write failed");
    return 0;
}


// ---------------------------------------------------------------- crash note
// If the game hits an access violation, write where it happened into the log, so a crash report
// (for example under Mod Organizer 2) says which module and address failed. Logs the first few only.

static volatile LONG g_avN = 0;

static LONG CALLBACK CrashNote(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord || ep->ExceptionRecord->ExceptionCode != 0xC0000005) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_avN) > 8) return EXCEPTION_CONTINUE_SEARCH;
    const EXCEPTION_RECORD* r = ep->ExceptionRecord;
    const CONTEXT* c = ep->ContextRecord;
    char mod[MAX_PATH] = "?";
    HMODULE hm = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)r->ExceptionAddress, &hm) && hm)
    {
        GetModuleFileNameA(hm, mod, MAX_PATH);
        char* sl = strrchr(mod, '\\'); if (sl) memmove(mod, sl + 1, strlen(sl));
    }
    Log("first-chance access violation #%ld (the game may handle this): at %08X (%s+%X), %s address %08X", (long)g_avN, (unsigned)r->ExceptionAddress, mod,
        hm ? (unsigned)r->ExceptionAddress - (unsigned)hm : 0,
        (r->NumberParameters > 0 && r->ExceptionInformation[0] == 1) ? "writing" : "reading",
        r->NumberParameters > 1 ? (unsigned)r->ExceptionInformation[1] : 0);
    Log("  eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    __try
    {
        const unsigned* st = (const unsigned*)c->Esp;
        Log("  stack: %08X %08X %08X %08X %08X %08X %08X %08X", st[0], st[1], st[2], st[3], st[4], st[5], st[6], st[7]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return EXCEPTION_CONTINUE_SEARCH;
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    char exe[MAX_PATH], asiDir[MAX_PATH], logPath[MAX_PATH], iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) *(slash + 1) = 0;
    // folder this .asi lives in (game folder, scripts or plugins)
    asiDir[0] = 0;
    HMODULE self = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&InitializeASI, &self) && self)
    {
        GetModuleFileNameA(self, asiDir, MAX_PATH);
        char* s2 = strrchr(asiDir, '\\');
        if (s2) *(s2 + 1) = 0; else asiDir[0] = 0;
    }
    if (!asiDir[0]) strcpy_s(asiDir, exe);
    // ini: beside the .asi if it is there, else the game folder if it is there, else beside the .asi
    const char* dir = asiDir;
    char tryPath[MAX_PATH];
    strcpy_s(tryPath, asiDir); strcat_s(tryPath, "rosterhack.ini");
    if (GetFileAttributesA(tryPath) == INVALID_FILE_ATTRIBUTES)
    {
        strcpy_s(tryPath, exe); strcat_s(tryPath, "rosterhack.ini");
        if (GetFileAttributesA(tryPath) != INVALID_FILE_ATTRIBUTES) dir = exe;
    }
    strcpy_s(logPath, dir); strcat_s(logPath, "xml2_roster.log");
    strcpy_s(iniPath, dir); strcat_s(iniPath, "rosterhack.ini");

    fopen_s(&g_log, logPath, "w");
    AddVectoredExceptionHandler(1, CrashNote);
    Log("---- XML2 32 Roster Hack Beta v0.02 ----");

    if ((unsigned long)GetModuleHandleA(NULL) != 0x400000)
    {
        Log("exe is not at the expected base 00400000. Nothing patched.");
        return;
    }

    // ---- settings from the ini (written with defaults when missing) ----
    // Roster=on: up to 32 heroes plus the default entry (33 hero records). off: the original
    // limit of 21 heroes plus the default entry (22 hero records).
    unsigned cap = IniOn("Limits", "Roster", iniPath) ? 33 : 22;
    g_cap = cap;
    g_lay = MakeLayout(cap);

    // Playable always follows the real number of herostat entries, so nothing has to be
    // changed when heroes are added.
    g_playAuto = true;
    g_playable = 21;

    Log("capacity %u, playable %s. layout: records 0..%03X, mask1 %03X, list %03X, tail %03X, head %03X, free %03X, mask2 %03X, used %03X, total %03X",
        g_cap, g_playAuto ? "auto" : "set by hand", g_lay.recEnd, g_lay.recEnd, g_lay.list, g_lay.tail, g_lay.head, g_lay.freec, g_lay.mask2, g_lay.used, g_lay.total);

    const unsigned nSites = (unsigned)(sizeof(kBase) / sizeof(kBase[0]) + sizeof(kD) / sizeof(kD[0]) + sizeof(kI) / sizeof(kI[0])) + 1;

    // ---- 1. check every site first; patch nothing unless all match ----
    int bad = 0;

    for (size_t i = 0; i < sizeof(kBase) / sizeof(kBase[0]); i++)
    {
        const Base& s = kBase[i];
        if (memcmp((const void*)s.addr, s.orig, s.len) != 0)
        {
            Log("MISMATCH at %08X (%s)", s.addr, s.what);
            bad++;
        }
    }
    for (size_t i = 0; i < sizeof(kD) / sizeof(kD[0]); i++)
    {
        const DSite& s = kD[i];
        if (D32(s.addr) != OldOff(s.f))
        {
            Log("MISMATCH at %08X: expected offset %03X, found %08X", s.addr, OldOff(s.f), D32(s.addr));
            bad++;
        }
    }
    for (size_t i = 0; i < sizeof(kI) / sizeof(kI[0]); i++)
    {
        const ISite& s = kI[i];
        unsigned found = (s.size == 1) ? B8(s.addr) : D32(s.addr);
        if (found != s.oldv)
        {
            Log("MISMATCH at %08X: expected limit %02X, found %X", s.addr, s.oldv, found);
            bad++;
        }
    }
    // the playable-count loop: 83 FE 15 7C E8
    {
        static const unsigned char want[] = { 0x46, 0x83, 0xFE, 0x15, 0x7C, 0xE8 };
        if (memcmp((const void*)0x44BB12, want, sizeof(want)) != 0)
        {
            Log("MISMATCH at 0044BB13 (playable loop)");
            bad++;
        }
    }
    if (memcmp((const void*)kFillAddr, kFillOrig, 5) != 0)
    {
        Log("MISMATCH at %08X (roster list filler)", kFillAddr);
        bad++;
    }
    for (size_t i = 0; i < sizeof(kL) / sizeof(kL[0]); i++)
    {
        unsigned want = kL[i].isEnd ? kListEnd : kListOld;
        if (D32(kL[i].addr) != want)
        {
            Log("MISMATCH at %08X: expected %08X, found %08X", kL[i].addr, want, D32(kL[i].addr));
            bad++;
        }
    }
    if (bad)
    {
        Log("%d site(s) did not match. Nothing was patched.", bad);
        return;
    }
    Log("all %u sites match the bytes we studied", nSites);

    // ---- 2. memory for the pool (zero-filled by Windows) ----
    unsigned char* mem = (unsigned char*)VirtualAlloc(NULL, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem)
    {
        Log("could not allocate memory for the pool. Nothing patched.");
        return;
    }
    g_pool = (unsigned)mem;
    g_list = (unsigned char*)VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_list)
    {
        Log("could not allocate memory for the roster list. Nothing patched.");
        return;
    }
    g_listN = g_cap;
    Log("roster name list at %08X (%u entries)", (unsigned)g_list, g_listN);
    Log("pool at %08X (%u bytes used)", g_pool, g_lay.total);

    // ---- 3. patch ----
    int fails = 0;

    for (size_t i = 0; i < sizeof(kBase) / sizeof(kBase[0]); i++)
    {
        unsigned char nb[8];
        MakeBase(kBase[i], nb);
        if (!WriteMem(kBase[i].addr, nb, kBase[i].len)) { Log("write failed at %08X", kBase[i].addr); fails++; }
    }
    for (size_t i = 0; i < sizeof(kD) / sizeof(kD[0]); i++)
    {
        unsigned v = NewOff(kD[i].f);
        if (!WriteMem(kD[i].addr, &v, 4)) { Log("write failed at %08X", kD[i].addr); fails++; }
    }
    for (size_t i = 0; i < sizeof(kI) / sizeof(kI[0]); i++)
    {
        unsigned v = kI[i].last ? (g_cap - 1) : g_cap;
        if (!WriteMem(kI[i].addr, &v, kI[i].size)) { Log("write failed at %08X", kI[i].addr); fails++; }
    }
    if (g_playAuto)
    {
        // Replace "inc esi / cmp esi,15 / jl" with a small routine that asks the character
        // manager how many herostat entries it registered (its own getter, vtable +0x48) and
        // loops (entries - 1) times: the same relation the original game has (21 of 22).
        unsigned char* cave = (unsigned char*)VirtualAlloc(NULL, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!cave) { Log("could not allocate the playable-loop routine"); fails++; }
        else
        {
            unsigned char* p = cave;
            *p++ = 0x46;                                   // inc esi
            *p++ = 0x8B; *p++ = 0x07;                      // mov eax,[edi]
            *p++ = 0x8B; *p++ = 0xCF;                      // mov ecx,edi
            *p++ = 0xFF; *p++ = 0x50; *p++ = 0x48;         // call [eax+48]  (number of herostat entries)
            *p++ = 0x48;                                   // dec eax
            *p++ = 0x3B; *p++ = 0xF0;                      // cmp esi,eax
            *p++ = 0x0F; *p++ = 0x8C;                      // jl 44BB00
            { int rel = (int)(0x44BB00 - ((unsigned)p + 4)); memcpy(p, &rel, 4); p += 4; }
            *p++ = 0xE9;                                   // jmp 44BB18
            { int rel = (int)(0x44BB18 - ((unsigned)p + 4)); memcpy(p, &rel, 4); p += 4; }
            FlushInstructionCache(GetCurrentProcess(), cave, 0x100);
            unsigned char jmp[5] = { 0xE9, 0, 0, 0, 0 };
            int rel = (int)((unsigned)cave - (0x44BB12 + 5));
            memcpy(jmp + 1, &rel, 4);
            if (!WriteMem(0x44BB12, jmp, 5)) { Log("write failed at 0044BB12"); fails++; }
            else Log("playable loop at 0044BB13 is automatic (runs once per herostat entry minus the default)");
        }
    }
    else
    {
        unsigned char v = (unsigned char)g_playable;
        if (!WriteMem(kPlayAddr, &v, 1)) { Log("write failed at %08X", kPlayAddr); fails++; }
        else Log("playable loop at 0044BB13 now runs %u times (was %u)", g_playable, kPlayOld);
    }

    // roster name list: point the readers at the new table, then replace the filler
    for (size_t i = 0; i < sizeof(kL) / sizeof(kL[0]); i++)
    {
        unsigned v = (unsigned)g_list + (kL[i].isEnd ? g_listN * 0x14 : 0);
        if (!WriteMem(kL[i].addr, &v, 4)) { Log("write failed at %08X", kL[i].addr); fails++; }
    }
    {
        unsigned char jmp[5];
        jmp[0] = 0xE9;
        unsigned rel = (unsigned)&FillList - (kFillAddr + 5);
        memcpy(jmp + 1, &rel, 4);
        if (!WriteMem(kFillAddr, jmp, 5)) { Log("write failed at %08X", kFillAddr); fails++; }
        else Log("roster list filler replaced: %u names now fit (was 21)", g_listN);
    }

    Log("patched %u sites, %d write failure(s).", nSites + 7, fails);

    InstallPortraitFix();
    {
        char cel[16] = "";
        GetPrivateProfileStringA("Visual", "CelShading", "", cel, sizeof(cel), iniPath);
        if (cel[0] == 0) { WritePrivateProfileStringA("Visual", "CelShading", "on", iniPath); strcpy_s(cel, "on"); }
        if (_stricmp(cel, "off") == 0)
        {
            HANDLE ch = CreateThread(NULL, 0, CelWatcher, NULL, 0, NULL);
            if (ch) CloseHandle(ch);
            Log("CelShading=off: waiting for libIGSg.dll to hide outline nodes");
        }
        else Log("CelShading=on (original look)");
    }
    InstallCharManager(iniPath);
    InstallTeamBonus(iniPath);
    InstallNameFailLog();

    HANDLE h = CreateThread(NULL, 0, Watcher, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
