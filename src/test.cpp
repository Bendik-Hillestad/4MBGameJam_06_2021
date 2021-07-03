#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

extern "C" __declspec(noreturn) void __cdecl _main()
{
    ExitProcess(0);
}
