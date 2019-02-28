// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once
#include "UnicodeBinding.h"
namespace Unicode
{
namespace Detail
{
//! Moves the iterator to the next UCS code-point in the encoded sequence.
//! Non-specialized version (for 1:1 code-unit to code-point).
template<typename BaseIterator, typename BoundsChecker, EEncoding Encoding>
inline void MoveNext(BaseIterator& it, const BoundsChecker& checker, const integral_constant<EEncoding, Encoding> )
{
	static_assert(
	  Encoding == eEncoding_ASCII ||
	  Encoding == eEncoding_UTF32 ||
	  Encoding == eEncoding_Latin1 ||
	  Encoding == eEncoding_Win1252, "Invalid encoding!");
	assert(!checker.IsEnd(it) && "Attempt to iterate past the end of the sequence");

	// All of these encodings use a single code-unit for each code-point.
	++it;
}

//! Moves the iterator to the next UCS code-point in the encoded sequence.
//! Specialized for UTF-8.
template<typename BaseIterator, typename BoundsChecker>
inline void MoveNext(BaseIterator& it, const BoundsChecker& checker, integral_constant<EEncoding, eEncoding_UTF8> )
{
	assert(!checker.IsEnd(it) && "Attempt to iterate past the end of the sequence");

	// UTF-8: just need to skip up to 3 continuation bytes.
	for (int i = 0; i < 4; ++i)
	{
		++it;
		if (checker.IsEnd(it))
		{
			break;
		}
		uint32 val = static_cast<uint32>(*it);
		if ((val & 0xC0) != 0x80)
		{
			break;
		}
	}
}

//! Moves the iterator to the next UCS code-point in the encoded sequence.
//! Specialized for UTF-16.
template<typename BaseIterator, typename BoundsChecker>
inline void MoveNext(BaseIterator& it, const BoundsChecker& checker, integral_constant<EEncoding, eEncoding_UTF16> )
{
	assert(!checker.IsEnd(it) && "Attempt to iterate past the end of the sequence");

	// UTF-16: just need to skip one lead surrogate.
	++it;
	uint32 val = static_cast<uint32>(*it);
	if (val >= cLeadSurrogateFirst && val <= cLeadSurrogateLast)
	{
		if (!checker.IsEnd(it))
		{
			++it;
		}
	}
}

//! Moves the iterator to the previous UCS code-point in the encoded sequence.
//! Non-specialized version (for 1:1 code-unit to code-point).
template<typename BaseIterator, typename BoundsChecker, EEncoding Encoding>
inline void MovePrev(BaseIterator& it, const BoundsChecker& checker, const integral_constant<EEncoding, Encoding> )
{
	static_assert(
	  Encoding == eEncoding_ASCII ||
	  Encoding == eEncoding_UTF32 ||
	  Encoding == eEncoding_Latin1 ||
	  Encoding == eEncoding_Win1252, "Invalid encoding!");
	assert(!checker.IsBegin(it) && "Attempt to iterate past the beginning of the sequence");

	// All of these encodings use a single code-unit for each code-point.
	--it;
}

//! Moves the iterator to the previous UCS code-point in the encoded sequence.
//! Specialized for UTF-8.
template<typename BaseIterator, typename BoundsChecker>
inline void MovePrev(BaseIterator& it, const BoundsChecker& checker, integral_constant<EEncoding, eEncoding_UTF8> )
{
	assert(!checker.IsBegin(it) && "Attempt to iterate past the beginning of the sequence");

	// UTF-8: just need to skip up to 3 continuation bytes.
	for (int i = 0; i < 4; ++i)
	{
		--it;
		if (checker.IsBegin(it))
		{
			break;
		}
		uint32 val = static_cast<uint32>(*it);
		if ((val & 0xC0) != 0x80)
		{
			break;
		}
	}
}

//! Moves the iterator to the previous UCS code-point in the encoded sequence.
//! Specialized for UTF-16.
template<typename BaseIterator, typename BoundsChecker>
inline void MovePrev(BaseIterator& it, const BoundsChecker& checker, integral_constant<EEncoding, eEncoding_UTF16> )
{
	assert(!checker.IsBegin(it) && "Attempt to iterate past the beginning of the sequence");

	// UTF-16: just need to skip one lead surrogate.
	--it;
	uint32 val = static_cast<uint32>(*it);
	if (val >= cLeadSurrogateFirst && val <= cLeadSurrogateLast)
	{
		if (!checker.IsBegin(it))
		{
			--it;
		}
	}
}

//! Utility to access base iterators properties from CIterator.
//! This is the bounds-checked specialization, the range information is kept to defend against malformed sequences.
template<typename BaseIterator, bool BoundsChecked>
struct SBaseIterators
{
	typedef BaseIterator type;
	type begin, end;
	type it;

	SBaseIterators(const BaseIterator& begin, const BaseIterator& end)
		: begin(begin), end(end), it(begin) {}

	SBaseIterators(const SBaseIterators& other)
		: begin(other.begin), end(other.end), it(other.it) {}

	SBaseIterators& operator=(const SBaseIterators& other)
	{
		begin = other.begin;
		end = other.end;
		it = other.it;
		return *this;
	}

	bool IsBegin(const BaseIterator& it) const
	{
		return begin == it;
	}

	bool IsEnd(const BaseIterator& it) const
	{
		return end == it;
	}

	bool IsEqual(const SBaseIterators& other) const
	{
		return it == other.it
		       && begin == other.begin
		       && end == other.end;
	}

	//! O(N) version; works with any forward-iterator (or better).
	//! \note Only called inside assert.
	bool IsInRange(const BaseIterator& it, std::forward_iterator_tag) const
	{
		for (BaseIterator i = begin; i != end; ++i)
		{
			if (it == i)
			{
				return true;
			}
		}
		return false;
	}

	//! O(1) version; requires random-access-iterator.
	//! \note Only called inside assert.
	bool IsInRange(const BaseIterator& it, std::random_access_iterator_tag) const
	{
		return (begin <= it && it < end);
	}

	//! Dispatches to the O(1) version if a random-access iterator is used (common case).
	//! \note Only called inside assert.
	bool IsInRange(const BaseIterator& it) const
	{
		return IsInRange(it, typename std::iterator_traits<BaseIterator>::iterator_category());
	}
};

//! Utility to access base iterators properties from CIterator.
//! This is the un-checked specialization for known-safe sequences.
template<typename BaseIterator>
struct SBaseIterators<BaseIterator, false>
{
	typedef BaseIterator type;
	type it;

	SBaseIterators(const BaseIterator& begin, const BaseIterator& end)
		: it(begin) {}

	SBaseIterators(const BaseIterator& begin)
		: it(begin) {}

	SBaseIterators(const SBaseIterators& other)
		: it(other.it) {}

	SBaseIterators& operator=(const SBaseIterators& other)
	{
		it = other.it;
		return *this;
	}

	bool IsBegin(const BaseIterator& it) const
	{
		return false;
	}

	bool IsEnd(const BaseIterator& it) const
	{
		return false;
	}

	bool IsEqual(const SBaseIterators& other) const
	{
		return it == other.it;
	}

	bool IsInRange(const BaseIterator& it) const
	{
		return true;
	}
};

//! Helper to store the last code-point and error bit that was decoded.
//! This is the safe specialization for potentially malformed sequences.
template<bool Safe>
struct SIteratorSink
{
	static const uint32 cEmpty = 0xFFFFFFFFU;
	uint32              value;
	bool                error;

	void                Clear()
	{
		value = cEmpty;
		error = false;
	}

	bool IsEmpty() const
	{
		return value == cEmpty;
	}

	bool IsError() const
	{
		return error;
	}

	const uint32& GetValue() const
	{
		return value;
	}

	void MarkDecodingError()
	{
		value = cReplacementCharacter;
		error = true;
	}

	template<EEncoding Encoding, typename BaseIterator, bool BoundsChecked>
	void Decode(const SBaseIterators<BaseIterator, BoundsChecked>& its, integral_constant<EEncoding, Encoding> )
	{
		typedef SDecoder<Encoding, SIteratorSink&, SIteratorSink&> DecoderType;
		DecoderType decoder(*this, *this);
		Clear();
		for (BaseIterator it = its.it; IsEmpty(); ++it)
		{
			uint32 val = static_cast<uint32>(*it);
			decoder(val);
			if (its.IsEnd(it))
			{
				break;
			}
		}
		if (IsEmpty())
		{
			// If we still have neither a new value or an error flag, just treat as error.
			// This can happen if we reached the end of the sequence, but it ends in an incomplete code-sequence.
			MarkDecodingError();
		}
	}

	template<EEncoding Encoding, typename BaseIterator, bool BoundsChecked>
	void DecodeIfEmpty(const SBaseIterators<BaseIterator, BoundsChecked>& its, integral_constant<EEncoding, Encoding> tag)
	{
		if (IsEmpty())
		{
			Decode(its, tag);
		}
	}

	void operator()(uint32 unit)
	{
		value = unit;
	}

	void operator()(SIteratorSink&, uint32, uint32)
	{
		MarkDecodingError();
	}
};

//! Helper to store the last code-point that was decoded.
//! This is the un-safe specialization for known-valid sequences.
//! \note No error-state is tracked since we won't handle that regardless for un-safe CIterator.
template<>
struct SIteratorSink<false>
{
	static const uint32 cEmpty = 0xFFFFFFFFU;
	uint32              value;

	void Clear()
	{
		value = cEmpty;
	}

	bool IsEmpty() const
	{
		return value == cEmpty;
	}

	bool IsError() const
	{
		return false;
	}

	const uint32& GetValue() const
	{
		return value;
	}

	template<EEncoding Encoding, typename BaseIterator, bool BoundsChecked>
	void Decode(const SBaseIterators<BaseIterator, BoundsChecked>& its, integral_constant<EEncoding, Encoding> )
	{
		typedef SDecoder<Encoding, SIteratorSink&, void> DecoderType;
		DecoderType decoder(*this);
		for (BaseIterator it = its.it; IsEmpty(); ++it)
		{
			uint32 val = static_cast<uint32>(*it);
			decoder(val);
		}
	}

	template<EEncoding Encoding, typename BaseIterator, bool BoundsChecked>
	void DecodeIfEmpty(const SBaseIterators<BaseIterator, BoundsChecked>& its, integral_constant<EEncoding, Encoding> tag)
	{
		if (IsEmpty())
		{
			Decode(its, tag);
		}
	}

	void operator()(uint32 unit)
	{
		value = unit;
	}
};
}

//! Helper class that can iterate over an encoded text sequence and read the underlying UCS code-points.
//! If the Safe flag is set, bounds checking is performed inside multi-unit sequences to guard against decoding errors.
//! This requires the user to know where the sequence ends (use the constructor taking two parameters).
//! \note The BaseIterator must be forward-iterator or better when Safe flag is set.
//! If the Safe flag is not set, you must guarantee the sequence is validly encoded, and allows the use of the single argument constructor.
//! In the case of unsafe iterator used for C-style string pointer, look for a U+0000 dereferenced value to end the iteration.
//! Regardless of the Safe flag, the user must ensure that the iterator is never moved past the beginning or end of the range (just like any other STL iterator).
//! Example of typical usage:
//! string utf8 = "foo"; //! UTF-8
//! for (Unicode::CIterator<string::const_iterator> it(utf8.begin(), utf8.end()); it != utf8.end(); ++it)
//! {
//!   uint32 codepoint = *it; //! 32-bit UCS code-point
//! }
//! Example unsafe usage: (for known-valid encoded C-style strings):
//! const char *pValid = "foo"; //! UTF-8
//! for (Unicode::CIterator<const char *, false> it = pValid; *it != 0; ++it)
//! {
//!   uint32 codepoint = *it; //! 32-bit UCS code-point
//! }
template<typename BaseIterator, bool Safe = true, EEncoding Encoding = Detail::SInferEncoding<BaseIterator, true>::value>
class CIterator
{
	//! The iterator value in the encoded sequence.
	//! Optionally provides bounds-checking.
	Detail::SBaseIterators<BaseIterator, Safe> its;

	//! The cached UCS code-point at the current position.
	//! Mutable because dereferencing is conceptually const, but does cache some state in this case.
	mutable Detail::SIteratorSink<Safe> sink;

public:
	// Types for compatibility with STL bidirectional iterator requirements.
	typedef const uint32                    value_type;
	typedef const uint32&                   reference;
	typedef const uint32*                   pointer;
	typedef const ptrdiff_t                 difference_type;
	typedef std::bidirectional_iterator_tag iterator_category;

	//! Construct an iterator for the given range.
	//! The initial position of the iterator as at the beginning of the range.
	CIterator(const BaseIterator& begin, const BaseIterator& end)
		: its(begin, end)
	{
		sink.Clear();
	}

	//! Construct an iterator from a single iterator (typically C-style string pointer).
	//! This can only be used for unsafe iterators.
	template<typename IteratorType>
	CIterator(const IteratorType& it, typename Detail::SRequire<!Safe&& Detail::is_convertible<IteratorType, BaseIterator>::value, IteratorType>::type* = 0)
		: its(static_cast<const BaseIterator&>(it))
	{
		sink.Clear();
	}

	//! Copy-construct an iterator.
	CIterator(const CIterator& other)
		: its(other.its), sink(other.sink) {}

	//! Copy-assign an iterator.
	CIterator& operator=(const CIterator& other)
	{
		its = other.its;
		sink = other.sink;
		return *this;
	}

	//! Test if the iterator points at an encoding error in the underlying encoded sequence.
	//! When using an un-safe iterator, this function always returns true, if a sequence can contain encoding errors, you must use the safe variant.
	//! \note This requires the underlying iterator to be dereferenced, so you cannot use it only while the iterator is inside the valid range.
	//! \return false if there is an encoding error, true otherwise.
	bool IsAtValidCodepoint() const
	{
		assert(!its.IsEnd(its.it) && "Attempt to dereference the past-the-end iterator");
		Detail::integral_constant<EEncoding, Encoding> tag;
		sink.DecodeIfEmpty(its, tag);
		return !sink.IsError();
	}

	//! Gets the current position in the underlying encoded sequence.
	//! If the iterator points to an invalidly encoded sequence (ie, IsError() returns true), the direction of iteration is significant.
	//! In that case the returned position is approximated; to work around this: move all iterators of which the position is compared in the same direction.
	const BaseIterator& GetPosition() const
	{
		return its.it;
	}

	//! Sets the current position in the underlying encoded sequence.
	//! You may not set the position outside the range for which this iterator was constructed.
	void SetPosition(const BaseIterator& it)
	{
		assert(its.IsInRange(it) && "Attempt to set the underlying iterator outside of the supported range");
		its.it = it;
	}

	//! Test if this iterator is equal to another iterator instance.
	//! In the presence of an invalidly encoded sequence (ie, IsError() returns true), the direction of iteration is significant.
	//! To work around this, you can either:
	//! 1) Move all iterators that will be compared in the same direction; or
	//! 2) Compare the dereferenced iterator value(s) instead (if applicable).
	bool operator==(const CIterator& other) const
	{
		return its.IsEqual(other.its);
	}

	//! Test if this iterator is equal to another base iterator.
	//! \note If the provided iterator does not point to the the first code-unit of an UCS code-point, the behavior is undefined.
	bool operator==(const BaseIterator& other) const
	{
		return its.it == other;
	}

	//! Test if this iterator is equal to another iterator instance.
	//! In the presence of an invalidly encoded sequence (ie, IsError() returns true), the direction of iteration is significant.
	//! To work around this, you can either:
	//! 1) Move all iterators that will be compared in the same direction; or
	//! 2) Compare the dereferenced iterator value(s) instead (if applicable).
	bool operator!=(const CIterator& other) const
	{
		return !its.IsEqual(other.its);
	}

	//! Test if this iterator is equal to another base iterator.
	//! \note If the provided iterator does not point to the the first code-unit of an UCS code-point, the behavior is undefined.
	bool operator!=(const BaseIterator& other) const
	{
		return its.it != other;
	}

	//! Get the decoded UCS code-point at the current position in the sequence.
	//! If the iterator points to an invalidly encoded sequence (ie, IsError() returns true) the function returns U+FFFD (replacement character).
	reference operator*() const
	{
		assert(!its.IsEnd(its.it) && "Attempt to dereference the past-the-end iterator");
		Detail::integral_constant<EEncoding, Encoding> tag;
		sink.DecodeIfEmpty(its, tag);
		return sink.GetValue();
	}

	//! Advance the iterator to the next UCS code-point.
	//! You must make sure the iterator is not at the end of the sequence, even in Safe mode.
	//! However, in Safe mode, the iterator will never move past the end of the sequence in the presence of encoding errors.
	CIterator& operator++()
	{
		Detail::integral_constant<EEncoding, Encoding> tag;
		Detail::MoveNext(its.it, its, tag);
		sink.Clear();
		return *this;
	}

	//! Go back to the previous UCS code-point.
	//! You must make sure the iterator is not at the beginning of the sequence, even in Safe mode.
	//! However, in Safe mode, the iterators will never move past the beginning of the sequence in the presence of encoding errors.
	CIterator& operator--()
	{
		Detail::integral_constant<EEncoding, Encoding> tag;
		Detail::MovePrev(its.it, its, tag);
		sink.Clear();
		return *this;
	}

	//! Advance the iterator to the next UCS code-point, return a copy of the iterator position before advancing.
	//! You must make sure the iterator is not at the end of the sequence, even in Safe mode.
	//! However, in Safe mode, the iterator will never move past the end of the sequence in the presence of encoding errors.
	CIterator operator++(int)
	{
		CIterator result = *this;
		++*this;
		return result;
	}

	//! Go back to the previous UCS code-point, return a copy of the iterator position before going back.
	//! You must make sure the iterator is not at the beginning of the sequence, even in Safe mode.
	//! However, in Safe mode, the iterators will never move past the beginning of the sequence in the presence of encoding errors.
	CIterator operator--(int)
	{
		CIterator result = *this;
		--*this;
		return result;
	}
};

namespace Detail
{
//! Specializes the CIterator template to use for a given string type.
//! \note The reason we use this is because MSVC doesn't want to deduce this on the MakeIterator declaration.
template<typename StringType>
struct SIteratorSpecializer
{
	typedef CIterator<typename StringType::const_iterator> type;
};
}

//! Helper function to make an UCS code-point iterator given an Unicode string.
//! Example usage:
//! string utf8 = "foo"; // UTF-8
//! auto it = Unicode::MakeIterator(utf8);
//! while (it != utf8.end())
//! {
//!   uint32 codepoint = *it; // 32-bit UCS code-point
//! }
//! Or, in a for-loop:
//! for (auto it = Unicode::MakeIterator(utf8); it != utf8.end(); ++it) {}
template<typename StringType>
inline typename Detail::SIteratorSpecializer<StringType>::type MakeIterator(const StringType& str)
{
	return typename Detail::SIteratorSpecializer<StringType>::type(str.begin(), str.end());
}
}

//! \endcond