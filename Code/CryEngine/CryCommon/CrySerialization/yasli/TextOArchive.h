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
#include "Archive.h"
#include "Pointers.h"

namespace yasli{

class MemoryWriter;

class TextOArchive : public Archive{
public:
	// header = 0 - default header, use "" to omit
	TextOArchive(int textWidth = 80, const char* header = 0);
	~TextOArchive();

	bool save(const char* fileName);

	const char* c_str() const;    
	size_t length() const;    

	// from Archive:
	bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	bool operator()(char& value, const char* name = "", const char* label = 0) override;
	bool operator()(StringInterface& value, const char* name = "", const char* label = 0) override;
	bool operator()(WStringInterface& value, const char* name = "", const char* label = 0) override;
	bool operator()(float& value, const char* name = "", const char* label = 0) override;
	bool operator()(double& value, const char* name = "", const char* label = 0) override;
	bool operator()(i8& value, const char* name = "", const char* label = 0) override;
	bool operator()(u8& value, const char* name = "", const char* label = 0) override;
	bool operator()(i16& value, const char* name = "", const char* label = 0) override;
	bool operator()(u16& value, const char* name = "", const char* label = 0) override;
	bool operator()(i32& value, const char* name = "", const char* label = 0) override;
	bool operator()(u32& value, const char* name = "", const char* label = 0) override;
	bool operator()(i64& value, const char* name = "", const char* label = 0) override;
	bool operator()(u64& value, const char* name = "", const char* label = 0) override;


	bool operator()(const Serializer& ser, const char* name = "", const char* label = 0) override;
	bool operator()(ContainerInterface& ser, const char* name = "", const char* label = 0) override;
	// ^^^

	using Archive::operator();
private:
	void openBracket();
	void closeBracket();
	void openContainerBracket();
	void closeContainerBracket();
	void placeName(const char* name);
	void placeIndent();
	void placeIndentCompact();

	bool joinLinesIfPossible();

	struct Level{
		Level(bool _isContainer, std::size_t position, int column)
		: isContainer(_isContainer)
		, startPosition(position)
		, indentCount(-column)
		{}
		bool isContainer;
		std::size_t startPosition;
		int indentCount;
	};

	typedef std::vector<Level> Stack;
	Stack stack_;
	std::unique_ptr<MemoryWriter> buffer_;
	const char* header_;
	int textWidth_;
	string fileName_;
	int compactOffset_;
};

}
