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

// love.data for L^: encoding, hashing and compression over strings. The
// reference is wrap_DataModule.cpp beside this file. ByteData, DataView
// and the Data containers come with the modules that need them.

#include "DataModule.h"
#include "CompressedData.h"
#include "lh/lh.h"

#include <string>

namespace love
{
namespace data
{

struct DataBinding
{
	lh::Errors *errors;
};

static DataBinding binding;

static bool bytesOf(LhatMachine *machine, const LhatValue *arguments, size_t count, size_t index, const char *&bytes, size_t &size)
{
	bytes = index < count ? lh::stringOf(arguments[index], &size) : nullptr;
	if (bytes == nullptr)
	{
		lh::raise(machine, "Expected a string");
		return false;
	}
	return true;
}

// encode(format, text) -> string; format is "base64" or "hex".
static LhatValue lh_encode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string formatstr = lh::optString(arguments, count, 0, "");
	EncodeFormat format;
	if (!getConstant(formatstr.c_str(), format))
		return lh::raise(machine, "Invalid encode format: " + formatstr);
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
		return lhat_nil();
	size_t linelen = (size_t) lh::optNumber(arguments, count, 2, 0);
	return lh::guard(machine, [&]() {
		size_t dstlen = 0;
		char *dst = encode(format, bytes, size, dstlen, linelen);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(dst != nullptr ? dst : "", dstlen), &out);
		delete[] dst;
		return out;
	});
}

static LhatValue lh_decode(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string formatstr = lh::optString(arguments, count, 0, "");
	EncodeFormat format;
	if (!getConstant(formatstr.c_str(), format))
		return lh::raise(machine, "Invalid encode format: " + formatstr);
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		size_t dstlen = 0;
		char *dst = decode(format, bytes, size, dstlen);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(dst != nullptr ? dst : "", dstlen), &out);
		delete[] dst;
		return out;
	});
}

// hash(function, text) -> the raw digest bytes, as a string.
static LhatValue lh_hash(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string funcstr = lh::optString(arguments, count, 0, "");
	HashFunction::Function function;
	if (!HashFunction::getConstant(funcstr.c_str(), function))
		return lh::raise(machine, "Invalid hash function: " + funcstr);
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		LhatValue out = lhat_nil();
		lh::makeString(machine, hash(function, bytes, size), &out);
		return out;
	});
}

// compress(format, text[, level]) -> the compressed bytes, as a string.
static LhatValue lh_compress(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string formatstr = lh::optString(arguments, count, 0, "");
	Compressor::Format format;
	if (!Compressor::getConstant(formatstr.c_str(), format))
		return lh::raise(machine, "Invalid compressed data format: " + formatstr);
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
		return lhat_nil();
	int level = (int) lh::optNumber(arguments, count, 2, -1);
	return lh::guard(machine, [&]() {
		StrongRef<CompressedData> data(compress(format, bytes, size, level), Acquire::NORETAIN);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string((const char *) data->getData(), data->getSize()), &out);
		return out;
	});
}

static LhatValue lh_decompress(LhatMachine *machine, void *context, const LhatValue *arguments, size_t count)
{
	(void) context;
	std::string formatstr = lh::optString(arguments, count, 0, "");
	Compressor::Format format;
	if (!Compressor::getConstant(formatstr.c_str(), format))
		return lh::raise(machine, "Invalid compressed data format: " + formatstr);
	const char *bytes = nullptr;
	size_t size = 0;
	if (!bytesOf(machine, arguments, count, 1, bytes, size))
		return lhat_nil();
	return lh::guard(machine, [&]() {
		size_t rawsize = 0;
		char *raw = decompress(format, bytes, size, rawsize);
		LhatValue out = lhat_nil();
		lh::makeString(machine, std::string(raw != nullptr ? raw : "", rawsize), &out);
		delete[] raw;
		return out;
	});
}

} // data

namespace lh
{

bool lhopen_love_data(Context &ctx)
{
	if (ctx.types())
		return true;

	using namespace love::data;
	binding.errors = ctx.errors;
	const char *m = "love.data";
	return ctx.func(m, "encode", "f^string^, string^ -> string^;", lh_encode, nullptr)
		&& ctx.func(m, "encode", "f^string^, string^, number^ -> string^;", lh_encode, nullptr)
		&& ctx.func(m, "decode", "f^string^, string^ -> string^;", lh_decode, nullptr)
		&& ctx.func(m, "hash", "f^string^, string^ -> string^;", lh_hash, nullptr)
		&& ctx.func(m, "compress", "f^string^, string^ -> string^;", lh_compress, nullptr)
		&& ctx.func(m, "compress", "f^string^, string^, number^ -> string^;", lh_compress, nullptr)
		&& ctx.func(m, "decompress", "f^string^, string^ -> string^;", lh_decompress, nullptr);
}

} // lh
} // love
