// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryCore/BaseTypes.h>                // For uint8, uint16, uint32
namespace Unicode
{
//! Supported encoding/conversion types.
enum EEncoding
{
	//! UTF-8 encoding, see http://www.unicode.org/resources/utf8.html.
	//! Input and output are supported.
	//! \note This format maps the entire UCS, where each code-point can take [1, 4] 8-bit code-units.
	//! \note This is a strict super-set of Latin1/ISO-885901 as well as ASCII.
	eEncoding_UTF8,

	//! UTF-16 encoding, see http://tools.ietf.org/html/rfc2781.
	//! Input and output are supported.
	//! \note This format maps the entire UCS, where each code-point can take [1, 2] 16-bit code-units.
	eEncoding_UTF16,

	//! UTF-32 encoding, see http://www.unicode.org/reports/tr17/.
	//! Input and output are supported.
	//! \note This format maps the entire UCS, each code-point is stored in a single 32-bit code-unit.
	eEncoding_UTF32,

	//! ASCII encoding, see http://! en.wikipedia.org/wiki/ASCII.
	//! Input and output are supported (any output UCS values out of supported range are mapped to question mark).
	//! \note Only values [U+0000, U+007F] can be mapped.
	eEncoding_ASCII,

	//! Latin1, aka ISO-8859-1 encoding, see http://en.wikipedia.org/wiki/ISO/IEC_8859-1.
	//! Only input is supported.
	//! \note This is a strict super-set of ASCII, it additionally maps [U+00A0, U+00FF].
	eEncoding_Latin1,

	//! Windows ANSI codepage 1252, see http://en.wikipedia.org/wiki/Windows-1252.
	//! Only input is supported.
	//! \note This is a strict super-set of ASCII and Latin1/ISO-8859-1, it maps some code-units from [0x80, 0x9F].
	eEncoding_Win1252,
};

//! Methods of recovery from invalid encoded sequences.
enum EErrorRecovery
{
	//! No attempt to detect invalid encoding is performed, the input is assumed to be valid.
	//! If the input is not valid, the output is undefined (in debug, this condition will cause an assert to trigger).
	eErrorRecovery_None,

	//! When an invalidly encoded sequence is detected, the sequence is discarded (will not be part of the output).
	//! Typically used for logic/hashing purposes when the input is almost certainly valid.
	eErrorRecovery_Discard,

	//! When an invalidly encoded sequence is detected, the sequence is replaced with the replacement-character (U+FFFD).
	//! Typically used when the output sequence is used for UI display purposes.
	eErrorRecovery_Replace,

	//! When an invalidly encoded sequence is detected, the sequence is replaced with the eEncoding_Latin1 equivalent.
	//! If the sequence is also not valid Latin1 encoded, the sequence is discarded.
	//! Typically used when reading generic text files with 1-byte code-units.
	//! \note This recovery method can only be used when decoding UTF-8.
	eErrorRecovery_FallbackLatin1ThenDiscard,

	//! When an invalidly encoded sequence is detected, the sequence is replaced with the eEncoding_Win1252 equivalent.
	//! If the sequence is also not valid codepage 1252 encoded, the sequence is discarded.
	//! Typically used when reading text files generated on Windows with 1-byte code-units.
	//! \note This recovery method can only be used when decoding UTF-8.
	eErrorRecovery_FallbackWin1252ThenDiscard,

	//! When an invalidly encoded sequence is detected, the sequence is replaced with the eEncoding_Latin1 equivalent.
	//! If the sequence is also not valid Latin1 encoded, it is replaced with the replacement-character (U+FFFD).
	//! Typically used when reading generic text files with 1-byte code-units.
	//! \note This recovery method can only be used when decoding UTF-8.
	eErrorRecovery_FallbackLatin1ThenReplace,

	//! When an invalidly encoded sequence is detected, the sequence is replaced with the eEncoding_Win1252 equivalent.
	//! If the sequence is also not valid codepage 1252 encoded, it is replaced with the replacement-character (U+FFFD).
	//! Typically used when reading text files generated on Windows with 1-byte code-units.
	//! \note This recovery method can only be used when decoding UTF-8.
	eErrorRecovery_FallbackWin1252ThenReplace,
};

namespace Detail
{
//! Decode a single code-unit of an encoding into an UCS code-point.
//! When Safe flag is set, encoding errors are detected so a fall-back encoding or other recovery method can be used.
//! \retval <0x001FFFFF  Decoded codepoint (== return value), call again with next code-unit and clear state.
//! \retval <0x80000000  Intermediate state returned, call again with next code-unit and the returned state.
//! \retval >=0x80000000 Bad encoding detected, up to 16 bits (UTF-16) or 24 bits (UTF-8, last in lower bits) contain previous consumed values (does not happen if Safe == false).
template<EEncoding InputEncoding, bool Safe>
inline uint32 Decode(uint32 state, uint32 unit);

//! Some constant values used when encoding/decoding.
enum
{
	cDecodeShiftRemaining = 26,                                   //!< Where to store the remaining count in the state.
	cDecodeOneRemaining   = 1 << cDecodeShiftRemaining,           //!< Remaining value of one.
	cDecodeMaskRemaining  = 3 << cDecodeShiftRemaining,           //!< All possible remaining bits that can be used.
	cDecodeLeadBit        = 1 << 22,                              //!< All bits up to and including this one are reserved.
	cDecodeErrorBit       = 1 << 31,                              //!< Set if an error occurs during decoding.
	cDecodeOverlongBit    = 1 << 30,                              //!< Set if overlong sequence was used.
	cDecodeSurrogateBit   = 1 << 29,                              //!< Set if surrogate code-point decoded in UTF-8.
	cDecodeInvalidBit     = 1 << 28,                              //!< Set if invalid code-point decoded (U+FFFE/FFFF).
	cDecodeSuccess        = 0,                                    //!< Placeholder to indicate no error occurred.
	cCodepointMax         = 0x10FFFF,                             //!< The maximum value of an UCS code-point.
	cLeadSurrogateFirst   = 0xD800,                               //!< The first valid UTF-16 lead-surrogate value.
	cLeadSurrogateLast    = 0xDBFF,                               //!< The last valid UTF-16 lead-surrogate value.
	cTrailSurrogateFirst  = 0xDC00,                               //!< The first valid UTF-16 trail-surrogate value.
	cTrailSurrogateLast   = 0xDFFF,                               //!< The last valid UTF-16 trail-surrogate value.
	cReplacementCharacter = 0xFFFD,                               //!< The default replacement character.
};

//! Validate the UTF-8 state of a multi-byte sequence.
//! The safe decoder of UTF-8 will call this function when a full potential code-point has been decoded.
//! This function is (at most) called for 50% of the decoded UTF-8 code-units, but likely at much lower frequency.
inline uint32 DecodeValidate8(uint32 state)
{
	uint32 errorbits = (state >> 8) | cDecodeErrorBit;
	state ^= (state & 0x400000) >> 1;     // For 3-byte sequences, bit 5 of the lead byte needs to be cleared.
	const uint32 cp =
	  (state & 0x3F) |
	  ((state & 0x3F00) >> 2) |
	  ((state & 0x3F0000) >> 4) |
	  ((state & 0x07000000) >> 6);
	if (cp <= cCodepointMax)
	{
		if (cp >= cLeadSurrogateFirst && cp <= cTrailSurrogateLast)
		{
			errorbits += cDecodeSurrogateBit;     // CESU-8 encoding might have been used.
		}
		else
		{
			uint32 minval = 0x80;
			minval += (0x00400000 & state) ? 0x800 - 0x80 : 0;
			minval += (0x40000000 & state) ? 0x10000 - 0x80 : 0;
			if (cp >= minval)
			{
				if ((cp & 0xFFFFFFFEU) != 0xFFFEU)
				{
					return cp;     // Valid code-point.
				}
				errorbits += cDecodeInvalidBit;     // Invalid character used.
			}
			errorbits += cDecodeOverlongBit;     // Overlong encoding used.
		}
	}
	return errorbits;
}

//! Decode UTF-8, unsafe.
template<> inline uint32 Decode<eEncoding_UTF8, false>(uint32 state, uint32 unit)
{
	if (state == 0)     // First byte.
	{
		unit = unit & 0xFF;
		if (unit < 0xC0) return unit;     // Single-unit (ASCII).
		uint32 remaining = (unit >> 4) - 0xC;
		remaining += (remaining == 0);
		return (unit & 0x1F) + (remaining << cDecodeShiftRemaining);     // Lead byte of multi-byte.
	}
	state = (state << 6) + (unit & 0x3F) + (state & cDecodeMaskRemaining) - cDecodeOneRemaining; // Apply c-byte.
	return state & ~cDecodeLeadBit;                                                              // Mask off the lead bits of a 4-byte sequence.
}

//! Decode UTF-8, safe
template<> inline uint32 Decode<eEncoding_UTF8, true>(uint32 state, uint32 unit)
{
	if (unit <= 0xF4)     // Discard out-of-range values immediately.
	{
		if (state == 0)     // First byte.
		{
			if (unit < 0x80) return unit;             // Single-byte.
			if (unit < 0xC2) return cDecodeErrorBit;  // Invalid continuation byte (or illegal 0xC0/0xC1).
			uint32 remaining = (unit >> 4) - 0xC;
			remaining += (remaining == 0);
			return unit + (remaining << cDecodeShiftRemaining);     // Multi-byte.
		}
		if ((unit & 0xC0) == 0x80)
		{
			const uint32 remaining = (state & cDecodeMaskRemaining) - cDecodeOneRemaining;
			state = (state << 8) + unit;
			if (remaining != 0) return state | remaining;  // Intermediate byte of a multi-byte sequence.
			return DecodeValidate8(state);                 // Final byte of a multi-byte sequence.
		}
	}
	return cDecodeErrorBit | state;
}

//! Decode UTF-16, unsafe.
template<> inline uint32 Decode<eEncoding_UTF16, false>(uint32 state, uint32 unit)
{
	const bool bLead = (unit >= cLeadSurrogateFirst) && (unit <= cLeadSurrogateLast);
	const uint32 initial = unit + (bLead << cDecodeShiftRemaining);
	const uint32 pair = 0x10000 + ((state & 0x3FF) << 10) + (unit & 0x3FF);
	return state == 0 ? initial : pair;
}

//! Decode UTF-16, safe.
template<> inline uint32 Decode<eEncoding_UTF16, true>(uint32 state, uint32 unit)
{
	const bool bTrail = (unit >= cTrailSurrogateFirst) && (unit <= cTrailSurrogateLast);
	if (state != 0 && !bTrail) return cDecodeErrorBit + (state & 0xFFFF);     // Lead surrogate without trail surrogate
	uint32 result = Decode<eEncoding_UTF16, false>(state, unit);
	bool bValid = (result & 0xFFFFFFFEU) != 0xFFFEU;
	return bValid ? result : result + cDecodeErrorBit + cDecodeInvalidBit;
}

//! Decode UTF-32, unsafe.
template<> inline uint32 Decode<eEncoding_UTF32, false>(uint32 state, uint32 unit)
{
	return unit;
}

//! Decode UTF-32, safe.
template<> inline uint32 Decode<eEncoding_UTF32, true>(uint32 state, uint32 unit)
{
	if (unit > cCodepointMax) return cDecodeErrorBit;
	if (unit >= cLeadSurrogateFirst && unit <= cTrailSurrogateLast) return cDecodeErrorBit | cDecodeSurrogateBit;
	if ((unit & 0xFFFEU) == 0xFFFEU) return cDecodeErrorBit | cDecodeInvalidBit;
	return unit;
}

//! Decode ASCII, unsafe.
template<> inline uint32 Decode<eEncoding_ASCII, false>(uint32 state, uint32 unit)
{
	return unit;
}

//! Decode ASCII, safe.
template<> inline uint32 Decode<eEncoding_ASCII, true>(uint32 state, uint32 unit)
{
	if (unit > 0x7F) return cDecodeErrorBit;
	return unit;
}

//! Decode Latin1, unsafe.
template<> inline uint32 Decode<eEncoding_Latin1, false>(uint32 state, uint32 unit)
{
	return unit;
}

//! Decode Latin1, safe.
template<> inline uint32 Decode<eEncoding_Latin1, true>(uint32 state, uint32 unit)
{
	if ((unit >= 0x80 && unit <= 0x9F) || (unit > 0xFF)) return cDecodeErrorBit;
	return unit;
}

//! Decode Windows CP-1252, unsafe.
template<> inline uint32 Decode<eEncoding_Win1252, false>(uint32 state, uint32 unit)
{
	static const uint16 cp1252[] =
	{
		0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
		0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
	};
	return (unit < 0x80 || unit > 0x9F) ? unit : cp1252[unit - 0x80];
}

//! Decode Windows CP-1252, safe.
template<> inline uint32 Decode<eEncoding_Win1252, true>(uint32 state, uint32 unit)
{
	if (unit > 0xFF) return cDecodeErrorBit;
	uint32 result = Decode<eEncoding_Win1252, false>(state, unit);
	if (!(unit < 0x80 || unit > 0x9F) && (result == unit)) return cDecodeErrorBit;     // Not defined in codepage 1252.
	return result;
}

//! Utility to apply empty-base-optimization on type T.
//! Will fall back to a member if T is a reference type.
template<typename T, int Tag = 0>
struct SBase : T
{
	SBase(T base) : T(base) {}
	T&       GetBase()       { return *this; }
	const T& GetBase() const { return *this; }
};
template<typename T, int Tag>
struct SBase<T&, Tag>
{
	T& base;
	SBase(T& base) : base(base) {}
	T&       GetBase()       { return base; }
	const T& GetBase() const { return base; }
};

//! Functor to decode UCS code-points from an input range.
//! Recovery functor will be invoked as a fall-back if decoding fails.
//! This allows ensuring all the output is valid (even if the input isn't).
//! \note The destructor will automatically flush any remaining (erroneous) state, you can also call Finalize().
template<EEncoding InputEncoding, typename Sink, typename Recovery = void>
struct SDecoder : SBase<Sink, 1>, SBase<Recovery, 2>
{
	uint32 state;
	SDecoder(Sink sink, Recovery recovery = Recovery()) : SBase<Sink, 1>(sink), SBase<Recovery, 2>(recovery), state(0) {}
	SDecoder() { Finalize(); }
	Recovery& recovery() { return SBase<Recovery, 2>::GetBase(); }
	Sink&     sink()     { return SBase<Sink, 1>::GetBase(); }
	void      operator()(uint32 unit)
	{
		state = Detail::Decode<InputEncoding, true>(state, unit);
		if (state <= 0x1FFFFF)
		{
			sink()(state);
			state = 0;
		}
		else if (state & Detail::cDecodeErrorBit)
		{
			recovery()(sink(), state, unit);
			state = 0;
		}
	}
	void Finalize()
	{
		if (state)
		{
			recovery()(sink(), state, 0);
			state = 0;
		}
	}
};

//! Functor to decode to UCS code-points from an input range.
//! No attempt to discover or recover from encoding errors is made, can only safely be used with known-valid input.
template<EEncoding InputEncoding, typename Sink>
struct SDecoder<InputEncoding, Sink, void> : SBase<Sink>
{
	uint32 state;
	SDecoder(Sink sink) : SBase<Sink>(sink), state(0) {}
	Sink& sink() { return SBase<Sink>::GetBase(); }
	void  operator()(uint32 unit)
	{
		state = Detail::Decode<InputEncoding, false>(state, unit);
		if (state <= 0x1FFFFF)
		{
			sink()(state);
			state = 0;
		}
	}
	void Finalize() {}
};

//! Generic Unicode encoder functor.
//! Encoding must be one an encoding type for which output is supported.
//! The Sink type must have HintSequence member for UTF-8 and UTF-16 (although it may be a no-op).
//! In general, you feed operator() with UCS code-points and it will emit code-units.
template<EEncoding OutputEncoding, typename Sink>
struct SEncoder
{
	static const bool value = false;
};

//! Specialization of ASCII encoder functor.
//! \note Any out-of-range character is mapped to question mark.
template<typename Sink>
struct SEncoder<eEncoding_ASCII, Sink> : SBase<Sink>
{
	static const bool value = true;
	typedef uint8 value_type;
	SEncoder(Sink sink) : SBase<Sink>(sink) {}
	void operator()(uint32 cp)
	{
		cp = cp < 0x80 ? cp : (uint32)'?';
		SBase<Sink>::GetBase()(value_type(cp));
	}
};

//! Specialization of UTF-8 encoder functor.
template<typename Sink>
struct SEncoder<eEncoding_UTF8, Sink> : SBase<Sink>
{
	static const bool value = true;
	typedef uint8 value_type;
	SEncoder(Sink sink) : SBase<Sink>(sink) {}
	Sink& sink() { return SBase<Sink>::GetBase(); }
	void  operator()(uint32 cp)
	{
		if (cp < 0x80)
		{
			// Single byte sequence.
			sink()(value_type(cp));
		}
		else
		{
			// Expand 21-bit value to 32-bit.
			uint32 bits =
			  (cp & 0x00003F) +
			  ((cp & 0x000FC0) << 2) +
			  ((cp & 0x03F000) << 4) +
			  ((cp & 0x1C0000) << 6);

			// Type of sequence.
			const bool bSeq4 = (cp >= 0x10000);
			const bool bSeq3 = (cp >= 0x800);

			// Mask lead-bytes and continuation-bytes.
			uint32 mask = 0xEFE0C080;
			mask ^= (bSeq3 << 14);
			mask += (bSeq4 ? 0xA00000 : 0);
			bits |= mask;

			// Length of the sequence.
			const uint32 length = (uint32)bSeq4 + (uint32)bSeq3 + 1;
			sink().HintSequence(length);

			// Sink the multi-byte sequence.
			if (bSeq4) sink()(value_type(bits >> 24));
			if (bSeq3) sink()(value_type(bits >> 16));
			sink()(value_type(bits >> 8));
			sink()(value_type(bits));
		}
	}
};

//! Specialization of UTF-16 encoder functor.
template<typename Sink>
struct SEncoder<eEncoding_UTF16, Sink> : SBase<Sink>
{
	static const bool value = true;
	typedef uint16 value_type;
	SEncoder(Sink sink) : SBase<Sink>(sink) {}
	Sink& sink() { return SBase<Sink>::GetBase(); }
	void  operator()(uint32 cp)
	{
		if (cp < 0x10000)
		{
			// Single unit
			sink()(value_type(cp));
		}
		else
		{
			// We will generate two-element sequence
			sink().HintSequence(2);

			// Surrogate pair
			cp -= 0x10000;
			uint32 lead = ((cp >> 10) & 0x3FF) + Detail::cLeadSurrogateFirst;
			uint32 trail = (cp & 0x3FF) + Detail::cTrailSurrogateFirst;
			sink()(value_type(lead));
			sink()(value_type(trail));
		}
	}
};

//! Specialization of UTF-32 encoder functor.
//! \note This is a no-op, but we want to be able to express UTF-32 just like the other encodings.
template<typename Sink>
struct SEncoder<eEncoding_UTF32, Sink> : SBase<Sink>
{
	static const bool value = true;
	typedef uint32 value_type;
	SEncoder(Sink sink) : SBase<Sink>(sink) {}
	void operator()(uint32 cp)
	{
		SBase<Sink>::GetBase()(value_type(cp));
	}
};

//! Specialization for unsafe no-op transcoding.
//! Since the conversion is a no-op, no need to keep any state or do any computation.
//! \note For a decoding with a fallback, this is not possible since we can't guarantee the input is valid.
template<EEncoding SameEncoding, typename Sink>
struct SDecoder<SameEncoding, SEncoder<SameEncoding, Sink>, void>
{
	Sink sink;
	SDecoder(Sink sink) : sink(sink) {}
	void operator()(uint32 unit)
	{
		sink(unit);
	}
	void Finalize() {}
};

//! Recovery handler that, on encoding error, discards the offending sequence.
template<typename Sink>
struct SRecoveryDiscard
{
	SRecoveryDiscard() {}
	void operator()(Sink& sink, uint32 error, uint32 unit) {}
};

//! Recovery handler that, on encoding error, replaces the sequence with replacement-character (U+FFFD).
//! \note This implementation matches a whole invalid sequence, it could be changed to emit for every code-unit.
template<typename Sink>
struct SRecoveryReplace
{
	SRecoveryReplace() {}
	void operator()(Sink& sink, uint32 error, uint32 unit) { sink(cReplacementCharacter); }
};

//! Recovery handler that, on encoding error, falls back to another encoding.
//! The fallback encoding must be stateless (ie: ASCII, Latin1 or Win1252).
//! This type assumes an 8-bit primary encoding since the only viable fallback encodings are 8-bit.
template<typename Sink, EEncoding FallbackEncoding, typename NextFallback>
struct SRecoveryFallback : NextFallback
{
	SRecoveryFallback() : NextFallback() {}
	void operator()(Sink& sink, uint32 error, uint32 unit)
	{
		SDecoder<FallbackEncoding, Sink&, NextFallback&> fallback(sink, *static_cast<NextFallback*>(this));
		uint8 byte1(error >> 16);
		uint8 byte2(error >> 8);
		uint8 byte3(error);
		uint8 byte4(unit);
		if (byte1) fallback(byte1);
		if (byte1 | byte2) fallback(byte2);
		if (byte1 | byte2 | byte3) fallback(byte3);
		fallback(byte4);
	}
};

//! Helper to pick a SRecoveryFallback instantiation based on RecoveryMethod.
template<EEncoding OutputEncoding, typename Sink, EErrorRecovery RecoveryMethod>
struct SRecoveryFallbackHelper
{
	//! A compilation error here means RecoveryMethod value was unexpected here.
	static_assert(
	  RecoveryMethod == eErrorRecovery_FallbackLatin1ThenDiscard ||
	  RecoveryMethod == eErrorRecovery_FallbackLatin1ThenReplace ||
	  RecoveryMethod == eErrorRecovery_FallbackWin1252ThenDiscard ||
	  RecoveryMethod == eErrorRecovery_FallbackWin1252ThenReplace, "Invalid recovery method!");
	typedef SEncoder<OutputEncoding, Sink> SinkType;
	static const EEncoding FallbackEncoding =
	  RecoveryMethod == eErrorRecovery_FallbackLatin1ThenDiscard ||
	  RecoveryMethod == eErrorRecovery_FallbackLatin1ThenReplace
	  ? eEncoding_Latin1 : eEncoding_Win1252;
	template<typename Dummy, bool WithDiscard>
	struct Pick { typedef SRecoveryDiscard<SinkType>               type; };
	template<typename Dummy>
	struct Pick<Dummy, false> { typedef SRecoveryReplace<SinkType> type; };
	typedef typename Pick<Sink,
	                      RecoveryMethod == eErrorRecovery_FallbackLatin1ThenDiscard ||
	                      RecoveryMethod == eErrorRecovery_FallbackWin1252ThenDiscard>::type NextFallback;
	typedef SRecoveryFallback<SinkType, FallbackEncoding, NextFallback> RecoveryType;
	typedef SDecoder<eEncoding_UTF8, SinkType, RecoveryType>            FullType;
};

//! Derives a chained decoder/encoder pair that performs code-unit -> code-unit transform.
//! The RecoveryMethod template parameter determines the behavior during encoding.
//! This is the basic way to perform trans-coding, and is the type instantiated by the higher-level functions.
template<EEncoding InputEncoding, EEncoding OutputEncoding, typename Sink, EErrorRecovery RecoveryMethod>
struct STranscoderSelect;
template<EEncoding InputEncoding, EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<InputEncoding, OutputEncoding, Sink, eErrorRecovery_None>
	: SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, void>
{
	typedef SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, void> TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding InputEncoding, EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<InputEncoding, OutputEncoding, Sink, eErrorRecovery_Discard>
	: SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, SRecoveryDiscard<SEncoder<OutputEncoding, Sink>>>
{
	typedef SRecoveryDiscard<SEncoder<OutputEncoding, Sink>>                      RecoveryType;
	typedef SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, RecoveryType> TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding InputEncoding, EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<InputEncoding, OutputEncoding, Sink, eErrorRecovery_Replace>
	: SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, SRecoveryReplace<SEncoder<OutputEncoding, Sink>>>
{
	typedef SRecoveryReplace<SEncoder<OutputEncoding, Sink>>                      RecoveryType;
	typedef SDecoder<InputEncoding, SEncoder<OutputEncoding, Sink>, RecoveryType> TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<eEncoding_UTF8, OutputEncoding, Sink, eErrorRecovery_FallbackLatin1ThenDiscard>
	: SRecoveryFallbackHelper<OutputEncoding, Sink, eErrorRecovery_FallbackLatin1ThenDiscard>::FullType
{
	static const EErrorRecovery RecoveryMethod = eErrorRecovery_FallbackLatin1ThenDiscard;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::RecoveryType RecoveryType;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::FullType     TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<eEncoding_UTF8, OutputEncoding, Sink, eErrorRecovery_FallbackLatin1ThenReplace>
	: SRecoveryFallbackHelper<OutputEncoding, Sink, eErrorRecovery_FallbackLatin1ThenReplace>::FullType
{
	static const EErrorRecovery RecoveryMethod = eErrorRecovery_FallbackLatin1ThenReplace;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::RecoveryType RecoveryType;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::FullType     TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<eEncoding_UTF8, OutputEncoding, Sink, eErrorRecovery_FallbackWin1252ThenDiscard>
	: SRecoveryFallbackHelper<OutputEncoding, Sink, eErrorRecovery_FallbackWin1252ThenDiscard>::FullType
{
	static const EErrorRecovery RecoveryMethod = eErrorRecovery_FallbackWin1252ThenDiscard;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::RecoveryType RecoveryType;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::FullType     TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};
template<EEncoding OutputEncoding, typename Sink>
struct STranscoderSelect<eEncoding_UTF8, OutputEncoding, Sink, eErrorRecovery_FallbackWin1252ThenReplace>
	: SRecoveryFallbackHelper<OutputEncoding, Sink, eErrorRecovery_FallbackWin1252ThenReplace>::FullType
{
	static const EErrorRecovery RecoveryMethod = eErrorRecovery_FallbackWin1252ThenReplace;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::RecoveryType RecoveryType;
	typedef typename SRecoveryFallbackHelper<OutputEncoding, Sink, RecoveryMethod>::FullType     TranscoderType;
	STranscoderSelect(Sink sink) : TranscoderType(sink) {}
};

//! Check if the given recovery mode is safe.
//! This is used for SFINAE checks in higher-level functions.
template<EErrorRecovery R>
struct SIsSafeEncoding
{
	PREFAST_SUPPRESS_WARNING(6285)
	static const bool value =
	  R == eErrorRecovery_Discard ||
	  R == eErrorRecovery_Replace ||
	  R == eErrorRecovery_FallbackLatin1ThenDiscard ||
	  R == eErrorRecovery_FallbackLatin1ThenReplace ||
	  R == eErrorRecovery_FallbackWin1252ThenDiscard ||
	  R == eErrorRecovery_FallbackWin1252ThenReplace;
};

//! Check if data in one encoding can be copied directly to another encoding.
//! This is the basis for block-copy and string-assign optimizations in un-safe conversion functions.
//! \note There are more valid combinations, they are left out since those can't occur with the output encodings supported.
//! \note Only used for un-safe functions since it doesn't account for potential invalid sequences (they would be copied over).
template<EEncoding InputEncoding, EEncoding OutputEncoding>
struct SIsCopyableEncoding
{
	static const bool value =
	  InputEncoding == eEncoding_ASCII ||                                       // ASCII and Latin1 values don't change in any encoding.
	  (InputEncoding == eEncoding_Latin1 && OutputEncoding != eEncoding_ASCII); // Except Latin1 -> ASCII is lossy.
};

//! If the input and output encodings are the same, then it's copyable.
template<EEncoding SameEncoding>
struct SIsCopyableEncoding<SameEncoding, SameEncoding>
{
	static const bool value = true;
};
}
}
