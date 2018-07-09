/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

// Tags are 16-bit xor-hashes, checked for uniqueness in debug.
// Block is automatic: 8, 16 or 32-bits

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/MemoryWriter.h> 
#include <vector>

namespace yasli{

inline unsigned short calcHash(const char* str)
{
	unsigned int hash = 1315423911;

	while(*str)
		hash ^= (hash << 5) + *str++ + (hash >> 2);

	return static_cast<unsigned short>(hash);
}

class BinOArchive : public Archive{
public:

	YASLI_INLINE BinOArchive();
	YASLI_INLINE ~BinOArchive() {}

	YASLI_INLINE void clear();
	YASLI_INLINE size_t length() const;
	const char* buffer() const { return stream_.buffer(); }
	YASLI_INLINE bool save(const char* fileName);

	YASLI_INLINE bool operator()(bool& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(StringInterface& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(WStringInterface& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(float& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(double& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i8& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i16& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i32& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i64& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u8& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u16& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u32& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u64& value, const char* name, const char* label) override;

	YASLI_INLINE bool operator()(char& value, const char* name, const char* label) override;

	YASLI_INLINE bool operator()(const Serializer &ser, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(ContainerInterface &ser, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(PointerInterface &ptr, const char* name, const char* label) override;

	using Archive::operator();

private:
	YASLI_INLINE void openContainer(const char* name, int size, const char* typeName);
	YASLI_INLINE void openNode(const char* name, bool size8 = true);
	YASLI_INLINE void closeNode(const char* name, bool size8 = true);

	std::vector<unsigned int> blockSizeOffsets_;
	MemoryWriter stream_;

#ifdef YASLI_BIN_ARCHIVE_CHECK_EMPTY_NAME_MIX
	enum BlockType { UNDEFINED, POD, NON_POD };
	std::vector<BlockType> blockTypes_;
#endif
};

//////////////////////////////////////////////////////////////////////////

class BinIArchive : public Archive{
public:

	YASLI_INLINE BinIArchive();
	YASLI_INLINE ~BinIArchive();

	YASLI_INLINE bool load(const char* fileName);
	YASLI_INLINE bool open(const char* buffer, size_t length); // Do not copy buffer!
	YASLI_INLINE bool open(const BinOArchive& ar) { return open(ar.buffer(), ar.length()); }
	YASLI_INLINE void close();

	YASLI_INLINE bool operator()(bool& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(char& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(float& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(double& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i8& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u8& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i16& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u16& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i32& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u32& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(i64& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(u64& value, const char* name, const char* label) override;


	YASLI_INLINE bool operator()(StringInterface& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(WStringInterface& value, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(const Serializer& ser, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(ContainerInterface& ser, const char* name, const char* label) override;
	YASLI_INLINE bool operator()(PointerInterface& ptr, const char* name, const char* label) override;

	using Archive::operator();

private:
	class Block
	{
	public:
		Block(const char* data, int size) : 
		  begin_(data), curr_(data), end_(data + size), complex_(false) {}

		  YASLI_INLINE bool get(const char* name, Block& block);

		  void read(void *data, int size)
		  {
			  if(curr_ + size <= end_){
					memcpy(data, curr_, size);
					curr_ += size;	
			  }
			  else
				  YASLI_ASSERT(0);
		  }

		  template<class T>
		  void read(T& x){ read(&x, sizeof(x)); }

		  void read(string& s)
		  {
			  if(curr_ + strlen(curr_) < end_){
				s = curr_;
				curr_ += strlen(curr_) + 1;
			  }
			  else{
				  YASLI_ASSERT(0);
				  curr_ = end_;
			  }
		  }
		  void read(wstring& s)
		  {
			  // make sure that accessed wchar_t is always aligned
			  const char* strEnd = curr_;
			  const wchar_t nullChar = L'\0';
			  while (strEnd < end_) {
				  if (memcmp(strEnd, &nullChar, sizeof(nullChar)) == 0)
					  break;
				  strEnd += sizeof(wchar_t);
			  }
			  int len = int((strEnd - curr_) / sizeof(wchar_t));

			  s.resize(len);
			  if (len)
				  memcpy((void*)&s[0], curr_, len * sizeof(wchar_t));

			  curr_ = curr_ + (len + 1) * sizeof(wchar_t);
			  if (curr_ > end_){
				  YASLI_ASSERT(0);
				  curr_ = end_;
			  }
		  }

		  YASLI_INLINE unsigned int readPackedSize();

		  void resetBegin() { begin_ = curr_; }  // Set block wrapping point to current point, after custom block data

		  bool validToClose() const { return complex_ || curr_ == end_; }    // Simple blocks must be read exactly
	
	private:
		const char* begin_;
		const char* end_;
		const char* curr_;
		bool complex_;

#ifdef YASLI_BIN_ARCHIVE_CHECK_HASH_COLLISION
		typedef std::map<unsigned short, string> HashMap;
		HashMap hashMap_;
#endif
	};

	typedef std::vector<Block> Blocks;
	Blocks blocks_;
	const char* loadedData_;
	string stringBuffer_;
	wstring wstringBuffer_;

	YASLI_INLINE bool openNode(const char* name);
	YASLI_INLINE void closeNode(const char* name, bool check = true);
	Block& currentBlock() { return blocks_.back(); }
	template<class T>
	void read(T& t) { currentBlock().read(t); }
};

}


#if YASLI_INLINE_IMPLEMENTATION
#include "BinArchiveImpl.h"
#endif