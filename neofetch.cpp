// neofetch.cpp - Neofetch minimalista de arquivo unico para Windows
//
// Compilar (MinGW):  g++ -O2 -static neofetch.cpp -o neofetch.exe -luser32 -ladvapi32
// Compilar (MSVC):   cl /EHsc /O2 /utf-8 neofetch.cpp
//
// Requer Windows 10+ (usa sequencias ANSI no console).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// ---------- Cores ANSI ----------
static const char* C_RESET = "\x1b[0m";
static const char* C_ACCENT = "\x1b[1;94m";  // azul claro em negrito
static const char* C_LOGO = "\x1b[94m";

// ---------- Utilidades ----------
static std::string narrow(const wchar_t* w) {
    if (!w || !*w) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::string trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    s = s.substr(a, b - a + 1);
    // colapsa espacos duplicados (comum no nome da CPU)
    std::string out;
    for (char c : s) {
        if (c == ' ' && !out.empty() && out.back() == ' ') continue;
        out += c;
    }
    return out;
}

static std::string regString(HKEY root, const wchar_t* path, const wchar_t* name) {
    wchar_t buf[512];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, path, name, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
                     nullptr, buf, &size) == ERROR_SUCCESS)
        return trim(narrow(buf));
    return "";
}

static DWORD regDword(HKEY root, const wchar_t* path, const wchar_t* name) {
    DWORD v = 0, size = sizeof(v);
    RegGetValueW(root, path, name, RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
                 nullptr, &v, &size);
    return v;
}

static std::string fmt(const char* f, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

static std::string gib(unsigned long long bytes) {
    return fmt("%.2f GiB", bytes / (1024.0 * 1024.0 * 1024.0));
}

// ---------- Coletores de informacao ----------
static std::string getUser() {
    wchar_t b[256]; DWORD n = 256;
    return GetUserNameW(b, &n) ? narrow(b) : "user";
}

static std::string getHost() {
    wchar_t b[256]; DWORD n = 256;
    return GetComputerNameW(b, &n) ? narrow(b) : "pc";
}

static const wchar_t* CV = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

static std::string getOS() {
    std::string name = regString(HKEY_LOCAL_MACHINE, CV, L"ProductName");
    std::string build = regString(HKEY_LOCAL_MACHINE, CV, L"CurrentBuildNumber");
    // O registro ainda diz "Windows 10" no Windows 11; corrige pelo build
    if (!build.empty() && atoi(build.c_str()) >= 22000) {
        size_t p = name.find("Windows 10");
        if (p != std::string::npos) name.replace(p, 10, "Windows 11");
    }
    SYSTEM_INFO si; GetNativeSystemInfo(&si);
    const char* arch = "x86";
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x86_64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   arch = "arm"; break;
    }
    return name + " " + arch;
}

static std::string getKernel() {
    std::string build = regString(HKEY_LOCAL_MACHINE, CV, L"CurrentBuildNumber");
    DWORD ubr = regDword(HKEY_LOCAL_MACHINE, CV, L"UBR");
    std::string disp = regString(HKEY_LOCAL_MACHINE, CV, L"DisplayVersion");
    std::string s = "NT " + build + "." + std::to_string(ubr);
    if (!disp.empty()) s += " (" + disp + ")";
    return s;
}

static std::string getHostModel() {
    const wchar_t* p = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    std::string m = regString(HKEY_LOCAL_MACHINE, p, L"SystemManufacturer");
    std::string n = regString(HKEY_LOCAL_MACHINE, p, L"SystemProductName");
    std::string s = trim(m + " " + n);
    return s.empty() ? "Desconhecido" : s;
}

static std::string getUptime() {
    unsigned long long ms = GetTickCount64();
    unsigned long long s = ms / 1000;
    unsigned long long d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    std::string out;
    if (d) out += std::to_string(d) + (d == 1 ? " dia, " : " dias, ");
    if (d || h) out += std::to_string(h) + (h == 1 ? " hora, " : " horas, ");
    out += std::to_string(m) + (m == 1 ? " min" : " mins");
    return out;
}

static std::string getParentProcess() {
    DWORD pid = GetCurrentProcessId(), ppid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return "";
    PROCESSENTRY32W pe;
    std::string name;
    memset(&pe, 0, sizeof(pe)); pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) { ppid = pe.th32ParentProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    if (ppid) {
        memset(&pe, 0, sizeof(pe)); pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == ppid) { name = narrow(pe.szExeFile); break; }
            } while (Process32NextW(snap, &pe));
        }
    }
    CloseHandle(snap);
    return name;
}

static std::string getShell() {
    std::string p = getParentProcess();
    if (p.empty()) return "Desconhecido";
    // remove ".exe"
    if (p.size() > 4 && _stricmp(p.c_str() + p.size() - 4, ".exe") == 0)
        p.resize(p.size() - 4);
    return p;
}

static std::string getTerminal() {
    if (getenv("WT_SESSION")) return "Windows Terminal";
    if (const char* t = getenv("TERM_PROGRAM")) return t;
    if (getenv("ConEmuPID")) return "ConEmu";
    return "Console do Windows";
}

static std::string getResolution() {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    DEVMODEW dm; memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
    int hz = 0;
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) hz = (int)dm.dmDisplayFrequency;
    std::string s = fmt("%dx%d", w, h);
    if (hz > 1) s += fmt(" @ %dHz", hz);
    return s;
}

static std::string getCPU() {
    std::string name = regString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD mhz = regDword(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz");
    std::string s = name.empty() ? "Desconhecida" : name;
    s += fmt(" (%lu)", (unsigned long)si.dwNumberOfProcessors);
    if (mhz) s += fmt(" @ %.2fGHz", mhz / 1000.0);
    return s;
}

static std::string getGPU() {
    DISPLAY_DEVICEW dd; 
    std::string first;
    for (DWORD i = 0;; ++i) {
        memset(&dd, 0, sizeof(dd)); dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesW(nullptr, i, &dd, 0)) break;
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        std::string n = trim(narrow(dd.DeviceString));
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) return n;
        if (first.empty()) first = n;
    }
    return first.empty() ? "Desconhecida" : first;
}

static std::string getMemory() {
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return "Desconhecida";
    unsigned long long used = ms.ullTotalPhys - ms.ullAvailPhys;
    return fmt("%s / %s (%lu%%)", gib(used).c_str(), gib(ms.ullTotalPhys).c_str(),
               (unsigned long)ms.dwMemoryLoad);
}

static std::string getDisk() {
    wchar_t sysdir[MAX_PATH];
    wchar_t root[] = L"C:\\";
    if (GetSystemDirectoryW(sysdir, MAX_PATH) >= 3) { root[0] = sysdir[0]; }
    ULARGE_INTEGER freeB, total, totalFree;
    if (!GetDiskFreeSpaceExW(root, &freeB, &total, &totalFree)) return "Desconhecido";
    unsigned long long used = total.QuadPart - totalFree.QuadPart;
    int pct = total.QuadPart ? (int)(used * 100 / total.QuadPart) : 0;
    return fmt("(%c:) %s / %s (%d%%)", (char)root[0], gib(used).c_str(),
               gib(total.QuadPart).c_str(), pct);
}

static std::string getLocale() {
    wchar_t b[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(b, LOCALE_NAME_MAX_LENGTH)) return narrow(b);
    return "Desconhecido";
}

// ---------- Logo ----------
static std::vector<std::string> makeLogo(int& width) {
    const std::string B = "\xE2\x96\x88";  // U+2588 FULL BLOCK em UTF-8
    auto rep = [&](int n) { std::string s; for (int i = 0; i < n; ++i) s += B; return s; };
    const int pane = 14;
    width = pane * 2 + 2;
    std::vector<std::string> L;
    for (int i = 0; i < 7; ++i) L.push_back(rep(pane) + "  " + rep(pane));
    L.push_back("");
    for (int i = 0; i < 7; ++i) L.push_back(rep(pane) + "  " + rep(pane));
    return L;
}

// ---------- Main ----------
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetProcessDPIAware();  // resolucao real em telas com escala

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::string user = getUser(), host = getHost();
    std::string title = std::string(C_ACCENT) + user + C_RESET + "@" + C_ACCENT + host + C_RESET;
    std::string sep(user.size() + host.size() + 1, '-');

    std::vector<std::string> info;
    auto add = [&](const char* label, const std::string& v) {
        info.push_back(std::string(C_ACCENT) + label + C_RESET + ": " + v);
    };

    info.push_back(title);
    info.push_back(sep);
    add("OS", getOS());
    add("Host", getHostModel());
    add("Kernel", getKernel());
    add("Uptime", getUptime());
    add("Shell", getShell());
    add("Resolucao", getResolution());
    add("Terminal", getTerminal());
    add("CPU", getCPU());
    add("GPU", getGPU());
    add("Memoria", getMemory());
    add("Disco", getDisk());
    add("Locale", getLocale());
    info.push_back("");

    // paleta de cores
    std::string pal1, pal2;
    for (int i = 40; i <= 47; ++i) pal1 += "\x1b[" + std::to_string(i) + "m   ";
    for (int i = 100; i <= 107; ++i) pal2 += "\x1b[" + std::to_string(i) + "m   ";
    info.push_back(pal1 + C_RESET);
    info.push_back(pal2 + C_RESET);

    int logoW = 0;
    std::vector<std::string> logo = makeLogo(logoW);

    size_t rows = std::max(logo.size(), info.size());
    printf("\n");
    for (size_t i = 0; i < rows; ++i) {
        if (i < logo.size() && !logo[i].empty())
            printf("  %s%s%s", C_LOGO, logo[i].c_str(), C_RESET);
        else
            printf("  %*s", logoW, "");
        if (i < info.size()) printf("   %s", info[i].c_str());
        printf("\n");
    }
    printf("\n");
    return 0;
}
