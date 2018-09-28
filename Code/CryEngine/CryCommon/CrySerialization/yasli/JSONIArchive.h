/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include "Config.h"
#include <CrySerialization/yasli/Archive.h>
#include "Token.h"
#include <memory>

namespace yasli{

class MemoryReader;

class JSONIArchive : public Archive{
public:
	YASLI_INLINE JSONIArchive();
	YASLI_INLINE ~JSONIArchive();

	YASLI_INLINE bool load(const char* filename);
	YASLI_INLINE bool open(const char* buffer, size_t length, bool free = false);

	YASLI_INLINE bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(char& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(float& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(double& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i8& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i16& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i32& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i64& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u8& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u16& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u32& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u64& value, const char* name = "", const char* label = 0) override;

	YASLI_INLINE bool operator()(StringInterface& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(WStringInterface& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(const Serializer& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(BlackBox& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(ContainerInterface& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(KeyValueInterface& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(PointerInterface& ser, const char* name = "", const char* label = 0) override;

	YASLI_INLINE bool operator()(CTimeValue& ser, const char* name = "", const char* label = 0) override;
	#define MP_FUNCTION(T)	YASLI_INLINE bool operator()(T& ser, const char* name = "", const char* label = 0) override;
	#include <CrySystem\mpfloat.types>
	#undef MP_FUNCTION

	using Archive::operator();
private:
	YASLI_INLINE bool findName(const char* name, Token* outName = 0);
	YASLI_INLINE bool openBracket();
	YASLI_INLINE bool closeBracket();

	YASLI_INLINE bool openContainerBracket();
	YASLI_INLINE bool closeContainerBracket();

	YASLI_INLINE void checkValueToken();
	YASLI_INLINE bool checkStringValueToken();
	YASLI_INLINE void readToken();
	YASLI_INLINE void putToken();
	YASLI_INLINE int line(const char* position) const;
	YASLI_INLINE bool isName(Token token) const;

	YASLI_INLINE bool expect(char token);
	YASLI_INLINE void skipBlock();

	struct Level{
		const char* start;
		const char* firstToken;
		bool isContainer;
		bool isKeyValue;
		Level() : isContainer(false), isKeyValue(false) {}
	};
	typedef std::vector<Level> Stack;
	Stack stack_;

	std::auto_ptr<MemoryReader> reader_;
	Token token_;
	std::vector<char> unescapeBuffer_;
	string filename_;
	void* buffer_;
};

YASLI_INLINE double parseFloat(const char* s);

}

#if YASLI_INLINE_IMPLEMENTATION
#include "JSONIArchiveImpl.h"
#endif