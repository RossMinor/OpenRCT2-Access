/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessCrashHandler.h"

#ifdef _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    // dbghelp.h must follow windows.h.
    #include <dbghelp.h>

    #include <cstdint>
    #include <cstdio>
    #include <string>

    #include "AccessUpdate.h"

    #include <openrct2/Version.h>
    #include <openrct2/core/Guard.hpp>

    #pragma comment(lib, "dbghelp.lib")

namespace OpenRCT2::Ui::Accessibility
{
    static LPTOP_LEVEL_EXCEPTION_FILTER _previousFilter = nullptr;
    static bool _installed = false;

    // Copies UTF-16 text to the Windows clipboard. Best-effort: any failure is swallowed because we
    // are already on a crash path and have nothing better to fall back to.
    static void CopyToClipboard(const std::wstring& text)
    {
        if (!OpenClipboard(nullptr))
            return;

        EmptyClipboard();

        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem != nullptr)
        {
            if (void* dst = GlobalLock(hMem); dst != nullptr)
            {
                memcpy(dst, text.c_str(), bytes);
                GlobalUnlock(hMem);
                // On success the clipboard takes ownership of hMem, so we must not free it.
                if (SetClipboardData(CF_UNICODETEXT, hMem) == nullptr)
                    GlobalFree(hMem);
            }
            else
            {
                GlobalFree(hMem);
            }
        }

        CloseClipboard();
    }

    // Appends "module+0xoffset" for the given address to the report.
    static void AppendModuleAndOffset(std::wstring& out, DWORD64 address)
    {
        HMODULE module = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address), &module)
            && module != nullptr)
        {
            wchar_t path[MAX_PATH] = L"";
            GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));

            // Use just the file name, not the full path.
            const wchar_t* name = path;
            for (const wchar_t* p = path; *p != L'\0'; p++)
            {
                if (*p == L'\\' || *p == L'/')
                    name = p + 1;
            }

            const DWORD64 offset = address - reinterpret_cast<DWORD64>(module);
            wchar_t line[MAX_PATH + 64];
            swprintf_s(line, L"%s+0x%llx", name[0] != L'\0' ? name : L"<module>", offset);
            out += line;
        }
        else
        {
            wchar_t line[64];
            swprintf_s(line, L"0x%016llx", address);
            out += line;
        }
    }

    // Walks the faulting thread's stack from the exception context and appends a symbolised call
    // stack to the report. Falls back to module+offset when symbols are unavailable.
    static void AppendCallStack(std::wstring& out, EXCEPTION_POINTERS* info)
    {
        if (info == nullptr || info->ContextRecord == nullptr)
            return;

        const HANDLE process = GetCurrentProcess();
        const HANDLE thread = GetCurrentThread();

        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        const bool symReady = SymInitialize(process, nullptr, TRUE) != FALSE;

        // StackWalk64 mutates the context, so work on a copy.
        CONTEXT context = *info->ContextRecord;

        STACKFRAME64 frame{};
        DWORD machine;
    #if defined(_M_X64) || defined(_M_AMD64)
        machine = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = context.Rip;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrStack.Offset = context.Rsp;
    #elif defined(_M_IX86)
        machine = IMAGE_FILE_MACHINE_I386;
        frame.AddrPC.Offset = context.Eip;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrStack.Offset = context.Esp;
    #elif defined(_M_ARM64)
        machine = IMAGE_FILE_MACHINE_ARM64;
        frame.AddrPC.Offset = context.Pc;
        frame.AddrFrame.Offset = context.Fp;
        frame.AddrStack.Offset = context.Sp;
    #else
        if (symReady)
            SymCleanup(process);
        return;
    #endif
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        out += L"\r\nCall stack:\r\n";

        // Buffer for SymFromAddr: SYMBOL_INFO followed by room for the (undecorated) name.
        alignas(SYMBOL_INFO) uint8_t symbolBuffer[sizeof(SYMBOL_INFO) + 512 * sizeof(char)] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;

        for (int depth = 0; depth < 32; depth++)
        {
            if (!StackWalk64(
                    machine, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64,
                    SymGetModuleBase64, nullptr))
                break;

            const DWORD64 pc = frame.AddrPC.Offset;
            if (pc == 0)
                break;

            out += L"  ";

            DWORD64 disp = 0;
            if (symReady && SymFromAddr(process, pc, &disp, symbol) && symbol->Name[0] != '\0')
            {
                // Symbol names come back as narrow text; widen them for the report.
                wchar_t wname[512];
                const int n = MultiByteToWideChar(CP_UTF8, 0, symbol->Name, -1, wname, static_cast<int>(std::size(wname)));
                if (n > 0)
                {
                    out += wname;
                    wchar_t off[32];
                    swprintf_s(off, L"+0x%llx", disp);
                    out += off;
                }
                else
                {
                    AppendModuleAndOffset(out, pc);
                }

                IMAGEHLP_LINE64 lineInfo{};
                lineInfo.SizeOfStruct = sizeof(lineInfo);
                DWORD lineDisp = 0;
                if (SymGetLineFromAddr64(process, pc, &lineDisp, &lineInfo) && lineInfo.FileName != nullptr)
                {
                    wchar_t wfile[MAX_PATH];
                    if (MultiByteToWideChar(CP_UTF8, 0, lineInfo.FileName, -1, wfile, static_cast<int>(std::size(wfile))) > 0)
                    {
                        wchar_t loc[MAX_PATH + 32];
                        swprintf_s(loc, L" (%s:%lu)", wfile, lineInfo.LineNumber);
                        out += loc;
                    }
                }
            }
            else
            {
                AppendModuleAndOffset(out, pc);
            }

            out += L"\r\n";
        }

        if (symReady)
            SymCleanup(process);
    }

    static std::wstring BuildCrashReport(EXCEPTION_POINTERS* info)
    {
        std::wstring report = L"OpenRCT2 crash report\r\n";

        wchar_t versionLine[256];
        swprintf_s(
            versionLine, L"OpenRCT2: %hs\r\nAccessibility mod: %hs\r\n", gVersionInfoFull, GetAccessModVersion());
        report += versionLine;

        if (info != nullptr && info->ExceptionRecord != nullptr)
        {
            const auto* rec = info->ExceptionRecord;
            wchar_t excLine[160];
            swprintf_s(
                excLine, L"Exception: 0x%08lx at 0x%016llx\r\nFaulting module: ", rec->ExceptionCode,
                reinterpret_cast<DWORD64>(rec->ExceptionAddress));
            report += excLine;
            AppendModuleAndOffset(report, reinterpret_cast<DWORD64>(rec->ExceptionAddress));
            report += L"\r\n";

            // Access-violation records carry the read/write flag and the offending address.
            if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
            {
                const wchar_t* op = rec->ExceptionInformation[0] == 1 ? L"write to" : L"read from";
                wchar_t avLine[96];
                swprintf_s(avLine, L"Access violation %s 0x%016llx\r\n", op, (DWORD64)rec->ExceptionInformation[1]);
                report += avLine;
            }
        }

        if (auto assertMsg = Guard::GetLastAssertMessage(); assertMsg.has_value() && !assertMsg->empty())
        {
            wchar_t wmsg[512];
            if (MultiByteToWideChar(CP_UTF8, 0, assertMsg->c_str(), -1, wmsg, static_cast<int>(std::size(wmsg))) > 0)
            {
                report += L"Assertion: ";
                report += wmsg;
                report += L"\r\n";
            }
        }

        AppendCallStack(report, info);
        return report;
    }

    static LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info)
    {
        const std::wstring report = BuildCrashReport(info);
        CopyToClipboard(report);

        // Keep the dialog short so a screen reader does not read the whole call stack aloud; the
        // full report is on the clipboard, ready to paste into a GitHub issue.
        const wchar_t* message = L"OpenRCT2 has crashed.\r\n\r\nThe crash details have been copied to your "
                                 L"clipboard. Press Enter to close, then paste them (Ctrl+V) into a new bug "
                                 L"report at\r\nhttps://github.com/RossMinor/OpenRCT2-Access/issues";
        MessageBoxW(nullptr, message, L"OpenRCT2 has crashed", MB_OK | MB_ICONERROR);

        // If another handler (e.g. Breakpad) was installed before us, let it run too so its dump is
        // still produced; otherwise terminate the process.
        if (_previousFilter != nullptr)
            return _previousFilter(info);

        return EXCEPTION_EXECUTE_HANDLER;
    }

    void CrashHandlerInit()
    {
        if (_installed)
            return;
        _installed = true;
        _previousFilter = SetUnhandledExceptionFilter(OnUnhandledException);
    }
} // namespace OpenRCT2::Ui::Accessibility

#else

namespace OpenRCT2::Ui::Accessibility
{
    void CrashHandlerInit()
    {
    }
} // namespace OpenRCT2::Ui::Accessibility

#endif
