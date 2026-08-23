/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "Watchdog.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace love
{
namespace lh
{

static std::atomic<unsigned> kicks(0);

void kickWatchdog()
{
	kicks.fetch_add(1);
}

#ifdef _WIN32

// The main thread may be stuck inside the allocator (holding the heap
// lock), so nothing here may touch the heap or the C runtime's stdio: the
// walk uses the kernel unwinder (x64) and the report is written with
// WriteFile from stack buffers, as module base + offset. Resolve the
// offsets afterwards with scripts/symbolize.ps1 against the .pdb files.
static void writeRaw(const char *text)
{
	DWORD written = 0;
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, (DWORD) strlen(text), &written, nullptr);
}

static void writeHex(char *out, unsigned long long value)
{
	const char *digits = "0123456789abcdef";
	char buffer[17];
	int n = 0;
	do
	{
		buffer[n++] = digits[value & 15];
		value >>= 4;
	} while (value != 0);
	int at = 0;
	out[at++] = '0';
	out[at++] = 'x';
	while (n > 0)
		out[at++] = buffer[--n];
	out[at] = '\0';
}

static void dumpThread(HANDLE thread)
{
	DWORD64 pcs[64];
	int depth = 0;

	SuspendThread(thread);
	CONTEXT context;
	memset(&context, 0, sizeof(context));
	context.ContextFlags = CONTEXT_FULL;
	bool got = GetThreadContext(thread, &context) != 0;
#ifdef _M_X64
	while (got && depth < 64)
	{
		pcs[depth++] = context.Rip;
		DWORD64 base = 0;
		RUNTIME_FUNCTION *entry = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
		if (entry == nullptr)
		{
			// A leaf function: the return address is at the stack pointer.
			context.Rip = *(DWORD64 *) context.Rsp;
			context.Rsp += 8;
		}
		else
		{
			void *handler = nullptr;
			DWORD64 establisher = 0;
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, entry, &context, &handler, &establisher, nullptr);
		}
		if (context.Rip == 0)
			break;
	}
#endif
	ResumeThread(thread);

	if (!got)
	{
		writeRaw("[watchdog] GetThreadContext failed\n");
		return;
	}
	writeRaw("[watchdog] main thread stack (module base + offset):\n");
	for (int i = 0; i < depth; i++)
	{
		void *moduleBase = nullptr;
		RtlPcToFileHeader((void *) pcs[i], &moduleBase);
		char line[96];
		char hex[20];
		strcpy(line, "  ");
		writeHex(hex, (unsigned long long) moduleBase);
		strcat(line, hex);
		strcat(line, " + ");
		writeHex(hex, (unsigned long long) (pcs[i] - (DWORD64) moduleBase));
		strcat(line, hex);
		strcat(line, "\n");
		writeRaw(line);
	}
}

void startWatchdog()
{
	const char *setting = getenv("LHATOVE_WATCHDOG");
	if (setting == nullptr)
		return;
	int seconds = atoi(setting);
	if (seconds <= 0)
		return;

	HANDLE main = nullptr;
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main, 0, FALSE, DUPLICATE_SAME_ACCESS);

	// The modules the offsets will be taken against, while it is still safe
	// to ask.
	HMODULE exe = GetModuleHandleA(nullptr);
	HMODULE self = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) &startWatchdog, &self);
	fprintf(stderr, "[watchdog] armed for %d seconds; exe at %p, love library at %p\n", seconds, (void *) exe, (void *) self);

	std::thread([main, seconds]() {
		unsigned seen = kicks.load();
		int silent = 0;
		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			unsigned now = kicks.load();
			if (now != seen)
			{
				seen = now;
				silent = 0;
				continue;
			}
			if (++silent < seconds)
				continue;
			// Two samples a second apart tell a loop from a block.
			writeRaw("[watchdog] no frame for a while\n");
			dumpThread(main);
			std::this_thread::sleep_for(std::chrono::seconds(1));
			dumpThread(main);
			TerminateProcess(GetCurrentProcess(), 3);
		}
	}).detach();
}

#else

void startWatchdog()
{
}

#endif

} // lh
} // love
