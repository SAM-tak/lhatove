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

#ifndef LOVE_THREAD_LHTHREAD_H
#define LOVE_THREAD_LHTHREAD_H

// STL
#include <string>
#include <vector>

// LOVE
#include "common/Object.h"
#include "common/Variant.h"
#include "threads.h"
#include "lh/lh.h"

namespace love
{
namespace thread
{

// The successor of LuaThread: an L^ proto run to completion on an OS thread
// of its own, on a machine of its own made from the shared program
// (std.thread's shape -- stdlib/thread.c). The proto is either a unit the
// program checked (a file the loader can read) or a script loaded from
// text, which this owns. Arguments and channel values cross as Variants:
// see lh::variantOf.
class LhThread : public Threadable
{
public:

	static love::Type type;

	// `proto` is run; `owned` (may be the same pointer, or nullptr) is freed
	// with this. `registry` converts the arguments on the worker.
	LhThread(const std::string &name, LhatProgram *program, const LhatProto *proto, LhatProto *owned, const lh::TypeRegistry *registry);
	virtual ~LhThread();

	void threadFunction() override;

	const std::string &getError() const;
	bool hasError() const { return haserror; }

	bool start(const std::vector<Variant> &args);

private:

	void onError();

	LhatProgram *program;
	const LhatProto *proto;
	LhatProto *owned;
	const lh::TypeRegistry *registry;
	std::string name;
	std::string error;
	bool haserror;
	std::vector<Variant> args;

}; // LhThread

} // thread
} // love

#endif // LOVE_THREAD_LHTHREAD_H
