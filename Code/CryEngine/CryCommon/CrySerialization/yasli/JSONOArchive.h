/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <memory>
#include <CrySerialization/yasli/Archive.h>
#include "Pointers.h"

namespace yasli{

class MemoryWriter;

class JSONOArchive : public Archive{
public:
	// header = 0 - default header, use "" to omit
	YASLI_INLINE JSONOArchive(int textWidth = 80, const char* header = 0);
	YASLI_INLINE ~JSONOArchive();

	YASLI_INLINE bool save(const char* fileName);

	YASLI_INLINE const char* c_str() const;
	const char* buffer() const { return c_str(); }
	YASLI_INLINE size_t length() const;

	YASLI_INLINE bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(char& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(float& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(double& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i8& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u8& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i16& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u16& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i32& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u32& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(i64& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(u64& value, const char* name = "", const char* label = 0) override;

	YASLI_INLINE bool operator()(StringInterface& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(WStringInterface& value, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(const Serializer& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(ContainerInterface& ser, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(KeyValueInterface& keyValue, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(KeyValueDictionaryInterface& keyValue, const char* name = "", const char* label = 0) override;
	YASLI_INLINE bool operator()(PointerInterface& ser, const char* name = "", const char* label = 0) override;

	using Archive::operator();
private:
	YASLI_INLINE void openBracket();
	YASLI_INLINE void closeBracket();
	YASLI_INLINE void openContainerBracket();
	YASLI_INLINE void closeContainerBracket();
	YASLI_INLINE void placeName(const char* name);
	YASLI_INLINE void placeIndent(bool putComma = true);
	YASLI_INLINE void placeIndentCompact(bool putComma = true);

	YASLI_INLINE bool joinLinesIfPossible();

	struct Level{
		Level(bool _isContainer, std::size_t position, int column)
		: isContainer(_isContainer)
		, isKeyValue(false)
		, isDictionary(false)
		, startPosition(position)
		, indentCount(-column)
		, elementIndex(0)
		, nameIndex(0)
		{}
		bool isKeyValue;
		bool isContainer;
		bool isDictionary;
		std::size_t startPosition;
		int nameIndex;
		int elementIndex;
		int indentCount;
	};

	typedef std::vector<Level> Stack;
	Stack stack_;
	std::unique_ptr<MemoryWriter> buffer_;
	const char* header_;
	int textWidth_;
	string fileName_;
	int compactOffset_;
	bool isKeyValue_;
};

}


#if YASLI_INLINE_IMPLEMENTATION
#include "JSONOArchiveImpl.h"
#endif