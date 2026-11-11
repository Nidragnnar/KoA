// Nos testes, uma falha de memória (0xc0000005) imprime a pilha com nomes de função e linhas,
// já que não há depurador de linha de comando instalado.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>

namespace {

LONG WINAPI imprimirPilha(EXCEPTION_POINTERS* info) {
    const DWORD codigo = info->ExceptionRecord->ExceptionCode;
    if (codigo != EXCEPTION_ACCESS_VIOLATION && codigo != EXCEPTION_STACK_OVERFLOW && codigo != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(proc, nullptr, TRUE);
    CONTEXT ctx = *info->ContextRecord;
    STACKFRAME64 q{};
    q.AddrPC.Offset = ctx.Rip;
    q.AddrFrame.Offset = ctx.Rbp;
    q.AddrStack.Offset = ctx.Rsp;
    q.AddrPC.Mode = q.AddrFrame.Mode = q.AddrStack.Mode = AddrModeFlat;
    std::fprintf(stderr, "\n=== falha 0x%08lx; pilha: ===\n", codigo);
    for (int i = 0; i < 25 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &q, &ctx, nullptr,
                                          SymFunctionTableAccess64, SymGetModuleBase64, nullptr); ++i) {
        char buf[sizeof(SYMBOL_INFO) + 256]{};
        auto* s = reinterpret_cast<SYMBOL_INFO*>(buf);
        s->SizeOfStruct = sizeof(SYMBOL_INFO);
        s->MaxNameLen = 255;
        DWORD64 desl = 0;
        IMAGEHLP_LINE64 linha{sizeof(IMAGEHLP_LINE64)};
        DWORD deslLinha = 0;
        const bool temNome = SymFromAddr(proc, q.AddrPC.Offset, &desl, s);
        const bool temLinha = SymGetLineFromAddr64(proc, q.AddrPC.Offset, &deslLinha, &linha);
        std::fprintf(stderr, "  %2d %s  %s:%lu\n", i, temNome ? s->Name : "?", temLinha ? linha.FileName : "?",
                     temLinha ? linha.LineNumber : 0);
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

const bool instalado = [] {
    AddVectoredExceptionHandler(1, imprimirPilha);
    return true;
}();

}
