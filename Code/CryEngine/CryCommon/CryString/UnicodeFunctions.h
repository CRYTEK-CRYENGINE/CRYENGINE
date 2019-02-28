// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once
#include "UnicodeBinding.h"
namespace Unicode
{
//! Results of analysis of an input range of code-units (in any encoding).
//! This is returned by calling Analyze() function.
struct SAnalysisResult
{
	//! The type to use for counting units.
	//! Can be changed to uint64_t for dealing with 4GB+ of string data.
	typedef uint32 size_type;

	size_type inputUnits;       //!< The number of input units analyzed.
	size_type outputUnits8;     //!< The number of output units when encoded with UTF-8.
	size_type outputUnits16;    //!< The number of output units when encoded with UTF-16.
	size_type outputUnits32;    //!< The number of output units when encoded with UTF-32 (aka number of UCS code-points).
	size_type cpNonAscii;       //!< The number of non-ASCII UCS code-points encountered.
	size_type cpInvalid;        //!< The number of invalid UCS code-point encountered (or 0xFFFFFFFF if not available).

	//! Default constructor, initialize everything to zero.
	SAnalysisResult() : inputUnits(0), outputUnits8(0), outputUnits16(0), outputUnits32(0), cpInvalid(0), cpNonAscii(0) {}

	//! Check if the input range was empty.
	bool IsEmpty() const { return inputUnits == 0; }

	//! Check if the input range only contained ASCII characters.
	bool IsAscii() const { return cpNonAscii == 0; }

	//! Check if the input range was valid (has no encoding errors).
	//! \note This returns false if an unsafe decoder was used for analysis.
	bool IsValid() const { return cpInvalid == 0; }

	//! Get the length of the input range, in source code-units.
	size_type LengthInSourceUnits() const { return inputUnits; }

	//! Get the length of the input range, in UCS code-points.
	size_type LengthInUCS() const { return outputUnits32; }

	//! Get the length of the input range when encoded with the given encoding, in code-units.
	//! \note If the encoding is not supported for output, the function returns 0.
	size_type LengthInEncodingUnits(EEncoding encoding) const
	{
		switch (encoding)
		{
		case eEncoding_ASCII:
		case eEncoding_UTF32:
			return outputUnits32;
		case eEncoding_UTF16:
			return outputUnits16;
		case eEncoding_UTF8:
			return outputUnits8;
		default:
			return 0;
		}
	}

	//! Get the length of the input range when encoded with the given encoding, in bytes.
	//! \note If the encoding is not supported for output, the function returns 0.
	size_type LengthInEncodingBytes(EEncoding encoding) const
	{
		size_type units = LengthInEncodingUnits(encoding);
		switch (encoding)
		{
		case eEncoding_UTF32:
			return units << 2;
		case eEncoding_UTF16:
			return units << 1;
		default:
			return units;
		}
	}
};

namespace Detail
{
//! A sink implementation that does nothing.
struct SDummySink
{
	void operator()(uint32 unit)     {}
	void HintSequence(uint32 length) {}
};

//! A sink that counts the number of units of output.
struct SCountingSink
{
	size_t result;

	SCountingSink() : result(0) {}
	void operator()(uint32 unit)     { ++result; }
	void HintSequence(uint32 length) {}
};

//! A sink that updates analysis statistics.
struct SAnalysisSink
{
	SAnalysisResult& result;

	SAnalysisSink(SAnalysisResult& result) : result(result) {}
	void operator()(uint32 cp)
	{
		const bool isCat2 = cp >= 0x80;
		const bool isCat3 = cp >= 0x800;
		const bool isCat4 = cp >= 0x10000;
		result.outputUnits32 += 1;
		result.outputUnits16 += (1 + isCat4);
		result.outputUnits8 += (1 + isCat4 + isCat3 + isCat2);
		result.cpNonAscii += isCat2;
	}
	void HintSequence(uint32 length) {}
};

//! A recovery helper for analysis that counts invalid sequences.
struct SAnalysisRecovery
{
	SAnalysisRecovery() {}
	void operator()(SAnalysisSink& sink, uint32 error, uint32 unit)
	{
		sink.result.cpInvalid += 1;
	}
};

//! A recovery helper for validation, it tracks if there is any invalid sequence.
struct SValidationRecovery
{
	bool isValid;

	SValidationRecovery() : isValid(true) {}
	void operator()(SDummySink& sink, uint32 error, uint32 unit)
	{
		isValid = false;
	}
};

//! Helper to perform analysis, counts the input for a given encoding.
template<EEncoding InputEncoding>
struct SAnalyzer
{
	SDecoder<InputEncoding, SAnalysisSink, SAnalysisRecovery> decoder;

	SAnalyzer(SAnalysisResult& result) : decoder(result) {}
	void operator()(uint32 item)
	{
		decoder.sink().result.inputUnits += 1;
		decoder(item);
	}
};

//! Analyze string and store analysis result.
//! This is the generic function called by other Analyze overloads.
template<EEncoding InputEncoding, typename InputStringType>
inline void Analyze(SAnalysisResult& target, const InputStringType& source)
{
	// Bind methods.
	const EBind bindMethod = SBindObject<InputStringType, false>::value;
	integral_constant<EBind, bindMethod> tag;

	// Analyze using helper.
	SAnalyzer<InputEncoding> analyzer(target);
	Feed(source, analyzer, tag);
}

//! Tests that the string is valid encoding.
//! This is the generic function called by other Validate overloads.
template<EEncoding InputEncoding, typename InputStringType>
inline bool Validate(const InputStringType& source)
{
	// Bind methods.
	const EBind bindMethod = SBindObject<InputStringType, false>::value;
	integral_constant<EBind, bindMethod> tag;

	// Validate using helper.
	SDummySink sink;
	SDecoder<InputEncoding, SDummySink, SValidationRecovery> validator(sink);
	Feed(source, validator, tag);
	return validator.recovery().isValid;
}

//! Find length of a string (in code-units) after trans-coding from InputEncoding to OutputEncoding.
//! This is the generic function called by the other Length overloads.
template<EEncoding InputEncoding, EEncoding OutputEncoding, typename InputStringType>
inline size_t Length(const InputStringType& source)
{
	// If this assert hits, consider using LengthSafe.
	assert((Detail::Validate<InputEncoding, InputStringType>(source)) && "Length was used with non-safe input");

	// Bind methods.
	const EBind bindMethod = SBindObject<InputStringType, false>::value;
	integral_constant<EBind, bindMethod> tag;

	// All copyable encodings have the property that the number of input encoding units equals the output units.
	// In addition, this also holds for UTF-32 (always 1) -> ASCII (always 1), even though it's lossy.
	const bool isCopyable = SIsCopyableEncoding<InputEncoding, OutputEncoding>::value;
	const bool isCountable = isCopyable || (InputEncoding == eEncoding_UTF32 && OutputEncoding == eEncoding_ASCII);

	if (isCountable)
	{
		// Optimization: The number of input units is equal to the number of output units.
		return EncodedLength(source, tag);
	}
	else
	{
		// We need to perform the conversion.
		SCountingSink sink;
		STranscoderSelect<InputEncoding, OutputEncoding, SCountingSink&, eErrorRecovery_None> transcoder(sink);
		Feed(source, transcoder, tag);
		return sink.result;
	}
}

//! Find length of a string (in code-units) after trans-coding from InputEncoding to OutputEncoding.
//! This is the generic function called by the other LengthSafe overloads.
//! \note The Recovery type used during conversion may influence the result, so this needs to match if you use the length information.
template<EEncoding InputEncoding, EEncoding OutputEncoding, EErrorRecovery Recovery, typename InputStringType>
inline size_t LengthSafe(const InputStringType& source)
{
	// SRequire a safe recovery method.
	static_assert(SIsSafeEncoding<Recovery>::value, "Unsafe encoding!");

	// Bind methods.
	const EBind bindMethod = SBindObject<InputStringType, false>::value;
	integral_constant<EBind, bindMethod> tag;

	// We can't optimize here, since we cannot assume the input is validly encoded
	SCountingSink sink;
	STranscoderSelect<InputEncoding, OutputEncoding, SCountingSink&, Recovery> transcoder(sink);
	Feed(source, transcoder, tag);
	return sink.result;
}

//! Helper for block-copying entire string at once (as an optimization)
//! This optimization will effectively try to memcpy or assign the whole string at once.
//! \note We need to do some partial specialization here to find out if the optimization is valid, so we can't use a function in C++98.
template<bool Enable, typename SinkType, bool Append, EBind OutputMethod, typename InputStringType, typename OutputStringType>
struct SBlockCopy
{
	static const EBind bindMethod = SBindObject<InputStringType, false>::value;
	typedef integral_constant<EBind, bindMethod> TagType;
	size_t operator()(OutputStringType& target, const InputStringType& source)
	{
		// Optimization: Use block copying for these types.
		TagType tag;
		const size_t length = EncodedLength(source, tag);
		SinkType sink(target, length);
		if (sink.CanWrite())
		{
			const void* const dataPtr = EncodedPointer(source, tag);
			sink(dataPtr, length);
		}
		return length;
	}
};

//! Specialization that will use direct string assignment.
//! \note This optimization is not selected when appending, this could be a future optimization if this is common.
//! Reason for this is that the += operator is not present on all supported types (ie, std::vector)
template<typename SinkType, typename SameStringType>
struct SBlockCopy<true, SinkType, false, eBind_Data, SameStringType, SameStringType>
{
	size_t operator()(SameStringType& target, const SameStringType& source)
	{
		// Optimization: Use copy assignment.
		target = source;
		return source.size();
	}
};

//! Fall-back specialization for Enable == false.
//! \note This specialization has to exist for the linker, but should never be called (and optimized away).
template<typename SinkType, bool Append, EBind OutputMethod, typename InputStringType, typename OutputStringType>
struct SBlockCopy<false, SinkType, Append, OutputMethod, InputStringType, OutputStringType>
{
	size_t operator()(OutputStringType& target, const InputStringType& source)
	{
		assert(false && "Should never be called");
		return 0;
	}
};

//! Trans-code a string from InputEncoding to OutputEncoding.
//! This is the generic function that is called by Convert and Append overloads.
//! \rReturn the number of code-units required for full output (excluding any terminators).
template<EEncoding InputEncoding, EEncoding OutputEncoding, bool Append, typename InputStringType, typename OutputStringType>
inline size_t Convert(OutputStringType& target, const InputStringType& source)
{
	// If this assert hits, consider using ConvertSafe.
	assert((Detail::Validate<InputEncoding, InputStringType>(source)) && "Convert was used with non-safe input");

	// Bind methods.
	const EBind inputBindMethod = SBindObject<InputStringType, false>::value;
	const EBind outputBindMethod = SBindOutput<OutputStringType, false>::value;
	integral_constant<EBind, inputBindMethod> tag;
	typedef SWriteSink<OutputStringType, Append, outputBindMethod> SinkType;

	// Check if we can optimize this.
	const bool isCopyable = SIsCopyableEncoding<InputEncoding, OutputEncoding>::value;
	const bool isBlocks = SIsBlockCopyable<SBindObject<InputStringType, false>, SBindOutput<OutputStringType, false>>::value;
	const bool useBlockCopy = isCopyable && isBlocks;
	size_t length;
	if (useBlockCopy)
	{
		// Use optimized path.
		SBlockCopy<useBlockCopy, SinkType, Append, outputBindMethod, InputStringType, OutputStringType> blockCopy;
		length = blockCopy(target, source);
	}
	else
	{
		// We need to perform the conversion code-unit by code-unit.
		length = Detail::Length<InputEncoding, OutputEncoding, InputStringType>(source);
		SinkType sink(target, length);
		if (sink.CanWrite())
		{
			STranscoderSelect<InputEncoding, OutputEncoding, SinkType&, eErrorRecovery_None> transcoder(sink);
			Feed(source, transcoder, tag);
		}
	}
	return length;
}

//! Safely trans-code a string from InputEncoding to OutputEncoding using the specified Recovery to handle encoding errors.
//! This is the generic function called by ConvertSafe and AppendSafe overloads.
template<EEncoding InputEncoding, EEncoding OutputEncoding, bool Append, EErrorRecovery Recovery, typename InputStringType, typename OutputStringType>
inline size_t ConvertSafe(OutputStringType& target, const InputStringType& source)
{
	// SRequire a safe recovery method.
	static_assert(SIsSafeEncoding<Recovery>::value, "Unsafe encoding!");

	// Bind methods.
	const EBind inputBindMethod = SBindObject<InputStringType, false>::value;
	const EBind outputBindMethod = SBindOutput<OutputStringType, false>::value;
	integral_constant<EBind, inputBindMethod> tag;
	typedef SWriteSink<OutputStringType, Append, outputBindMethod> SinkType;

	// We can't optimize with block-copy here, since we cannot assume the input is validly encoded.
	const size_t length = Detail::LengthSafe<InputEncoding, OutputEncoding, Recovery, InputStringType>(source);
	SinkType sink(target, length);
	if (sink.CanWrite())
	{
		STranscoderSelect<InputEncoding, OutputEncoding, SinkType&, Recovery> transcoder(sink);
		Feed(source, transcoder, tag);
	}
	return length;
}

//! Require that T is usable as input object, with automatic encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename T, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAutoObj : SRequire<
	                     SBindObject<T, true>::value != eBind_Impossible&&
	                     SEncoder<E, SDummySink>::value&&
	                     SIsSafeEncoding<R>::value
	                     > {};

//! Require that T is usable as input iterator, with automatic encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename T, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAutoIts : SRequire<
	                     SBindIterator<T, true>::value != eBind_Impossible&&
	                     SEncoder<E, SDummySink>::value&&
	                     SIsSafeEncoding<R>::value
	                     > {};

//! Require that T is usable as input object, with user-specified encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename T, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAnyObj : SRequire<
	                    SBindObject<T, false>::value != eBind_Impossible&&
	                    SEncoder<E, SDummySink>::value&&
	                    SIsSafeEncoding<R>::value
	                    > {};

//! Require that T is usable as input iterator, with user-specified encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename T, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAnyIts : SRequire<
	                    SBindIterator<T, false>::value != eBind_Impossible&&
	                    SEncoder<E, SDummySink>::value&&
	                    SIsSafeEncoding<R>::value
	                    > {};

//! Require that I is usable as input object, and O as output object, with automatic encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename I, typename O, bool S, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAutoObjOut : SRequire<
	                        SBindObject<I, true>::value != eBind_Impossible&&
	                        SBindOutput<typename conditional<S, SPackedBuffer<O>, O>::type, true>::value != eBind_Impossible&&
	                        SEncoder<E, SDummySink>::value&&
	                        SIsSafeEncoding<R>::value
	                        > {};

//! Require that I is usable as input iterator, and O as output object, with automatic encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename I, typename O, bool S, EEncoding E = eEncoding_UTF8, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAutoItsOut : SRequire<
	                        SBindIterator<I, true>::value != eBind_Impossible&&
	                        SBindOutput<typename conditional<S, SPackedBuffer<O>, O>::type, true>::value != eBind_Impossible&&
	                        SEncoder<E, SDummySink>::value&&
	                        SIsSafeEncoding<R>::value
	                        > {};

//! Require that I is usable as input object, and O as output object, with user-specified encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename I, typename O, bool S, EEncoding E, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAnyObjOut : SRequire<
	                       SBindObject<I, false>::value != eBind_Impossible&&
	                       SBindOutput<typename conditional<S, SPackedBuffer<O>, O>::type, false>::value != eBind_Impossible&&
	                       SEncoder<E, SDummySink>::value&&
	                       SIsSafeEncoding<R>::value
	                       > {};

//! Require that I is usable as input object, and O as output object, with user-specified encoding.
//! This is used as a SFINAE argument for overload resolution of the main functions.
template<typename I, typename O, bool S, EEncoding E, EErrorRecovery R = eErrorRecovery_Discard>
struct SReqAnyItsOut : SRequire<
	                       SBindIterator<I, false>::value != eBind_Impossible&&
	                       SBindOutput<typename conditional<S, SPackedBuffer<O>, O>::type, false>::value != eBind_Impossible&&
	                       SEncoder<E, SDummySink>::value&&
	                       SIsSafeEncoding<R>::value
	                       > {};
}

//! Analyze the given string with the given encoding, providing information on validity and encoding length.
template<EEncoding InputEncoding, typename InputStringType>
inline SAnalysisResult Analyze(const InputStringType& str,
                               typename Detail::SReqAnyObj<InputStringType>::type* = 0)
{
	SAnalysisResult result;
	Detail::Analyze<InputEncoding, InputStringType>(result, str);
	return result;
}

//! Analyze the (assumed) Unicode string input, providing information on validity and encoding length.
//! The Unicode encoding is picked automatically depending on the input type.
template<typename InputStringType>
inline SAnalysisResult Analyze(const InputStringType& str,
                               typename Detail::SReqAutoObj<InputStringType>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	SAnalysisResult result;
	Detail::Analyze<InputEncoding, InputStringType>(result, str);
	return result;
}

//! Analyze the given range with the given encoding, providing information on validity and encoding length.
template<EEncoding InputEncoding, typename InputIteratorType>
inline SAnalysisResult Analyze(InputIteratorType begin, InputIteratorType end,
                               typename Detail::SReqAnyIts<InputIteratorType>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	SAnalysisResult result;
	Detail::Analyze<InputEncoding, InputStringType>(result, its);
	return result;
}

//! Analyze the given (assumed) Unicode range, providing information on validity and encoding length.
//! The Unicode encoding is picked automatically depending on the input type.
template<typename InputIteratorType>
inline SAnalysisResult Analyze(InputIteratorType begin, InputIteratorType end,
                               typename Detail::SReqAutoIts<InputIteratorType>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	SAnalysisResult result;
	Detail::Analyze<InputEncoding, InputStringType>(result, its);
	return result;
}

//! Checks if the given string is valid in the given encoding.
template<EEncoding InputEncoding, typename InputStringType>
inline bool Validate(const InputStringType& str,
                     typename Detail::SReqAnyObj<InputStringType>::type* = 0)
{
	return Detail::Validate<InputEncoding, InputStringType>(str);
}

//! Checks if the given string is a valid Unicode string.
//! The Unicode encoding is picked automatically depending on the input type.
template<typename InputStringType>
inline bool Validate(const InputStringType& str,
                     typename Detail::SReqAutoObj<InputStringType>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	return Detail::Validate<InputEncoding, InputStringType>(str);
}

//! Checks if the given range is valid in the given encoding.
template<EEncoding InputEncoding, typename InputIteratorType>
inline bool Validate(InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAnyIts<InputIteratorType>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::Validate<InputEncoding, InputStringType>(its);
}

//! Checks if the given range is valid Unicode.
//! The Unicode encoding is picked automatically depending on the input type.
template<typename InputIteratorType>
inline bool Validate(InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAutoIts<InputIteratorType>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::Validate<InputEncoding, InputStringType>(its);
}

//! Get the length (in OutputEncoding) of the given known-valid string with the given InputEncoding.
//! \note Length assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use LengthSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename InputStringType>
inline size_t Length(const InputStringType& str,
                     typename Detail::SReqAnyObj<InputStringType, OutputEncoding>::type* = 0)
{
	return Detail::Length<InputEncoding, OutputEncoding, InputStringType>(str);
}

//! Get the length (in OutputEncoding) of the given known-valid Unicode string.
//! The Unicode encoding is picked automatically depending on the input type.
//! \note Length assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use LengthSafe.
template<EEncoding OutputEncoding, typename InputStringType>
inline size_t Length(const InputStringType& str,
                     typename Detail::SReqAutoObj<InputStringType, OutputEncoding>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	return Detail::Length<InputEncoding, OutputEncoding, InputStringType>(str);
}

//! Get the length (in OutputEncoding) of the known-valid range with the given InputEncoding.
//! \note Length assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use LengthSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename InputIteratorType>
inline size_t Length(InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAnyIts<InputIteratorType, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::Length<InputEncoding, OutputEncoding, InputStringType>(its);
}

//! Get the length (in OutputEncoding) of the known-valid Unicode range.
//! The Unicode encoding is picked automatically depending on the input type.
//! \note Length assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use LengthSafe.
template<EEncoding OutputEncoding, typename InputIteratorType>
inline size_t Length(InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAutoIts<InputIteratorType>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::Length<InputEncoding, OutputEncoding, InputStringType>(its);
}

//! Get the length (in OutputEncoding) of the given string with the given InputEncoding.
//! \note LengthSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Length.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputStringType>
inline size_t LengthSafe(const InputStringType& str,
                         typename Detail::SReqAnyObj<InputStringType, OutputEncoding, Recovery>::type* = 0)
{
	return Detail::LengthSafe<InputEncoding, OutputEncoding, Recovery, InputStringType>(str);
}

//! Get the length (in OutputEncoding) of the given Unicode string.
//! The Unicode encoding is picked automatically depending on the input type.
//! \note LengthSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Length.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, typename InputStringType>
inline size_t LengthSafe(const InputStringType& str,
                         typename Detail::SReqAutoObj<InputStringType, OutputEncoding, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	return Detail::LengthSafe<InputEncoding, OutputEncoding, Recovery, InputStringType>(str);
}

//! Get the length (in OutputEncoding) of the range with the given InputEncoding.
//! \note LengthSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Length.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputIteratorType>
inline size_t LengthSafe(InputIteratorType begin, InputIteratorType end,
                         typename Detail::SReqAnyIts<InputIteratorType, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::LengthSafe<InputEncoding, OutputEncoding, Recovery, InputStringType>(its);
}

//! Get the length (in OutputEncoding) of the Unicode range.
//! The Unicode encoding is picked automatically depending on the input type.
//! \note LengthSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Length.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, typename InputIteratorType>
inline size_t LengthSafe(InputIteratorType begin, InputIteratorType end,
                         typename Detail::SReqAutoIts<InputIteratorType, OutputEncoding, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	return Detail::LengthSafe<InputEncoding, OutputEncoding, Recovery, InputStringType>(its);
}

//! Converts the given string in the given input encoding and stores into the result string with the given output encoding.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputStringType>
inline OutputStringType& Convert(OutputStringType& result, const InputStringType& str,
                                 typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the (assumed) Unicode string input and stores into the result Unicode string.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, typename InputStringType>
inline OutputStringType& Convert(OutputStringType& result, const InputStringType& str,
                                 typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the given range in the given input encoding and stores into the result string with the given output encoding.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& Convert(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                 typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the (assumed) Unicode range and stores into the result Unicode string.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, typename InputIteratorType>
inline OutputStringType& Convert(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                 typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the given string in the given input encoding and stores into the result buffer with the given output encoding.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputStringType>
inline size_t Convert(OutputCharType* buffer, size_t length, const InputStringType& str,
                      typename Detail::SReqAnyObjOut<InputStringType, OutputCharType*, true, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str) + 1;
}

//! Converts the (assumed) Unicode string input and stores into the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the buffer type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<typename OutputCharType, typename InputStringType>
inline size_t Convert(OutputCharType* buffer, size_t length, const InputStringType& str,
                      typename Detail::SReqAutoObjOut<InputStringType, OutputCharType*, true>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str) + 1;
}

//! Converts the given range in the given input encoding and stores into the result buffer with the given output encoding.
//! \note: Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputIteratorType>
inline size_t Convert(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                      typename Detail::SReqAnyItsOut<InputIteratorType, OutputCharType*, true, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its) + 1;
}

//! Converts the (assumed) Unicode range and stores into the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
//! \returns The required length of the output buffer, in code-units, including the null-terminator.
template<typename OutputCharType, typename InputIteratorType>
inline size_t Convert(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                      typename Detail::SReqAutoItsOut<InputIteratorType, OutputCharType*, true>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its) + 1;
}

//! Converts the given string in the given input encoding to a new string of the given type and output encoding.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputStringType>
inline OutputStringType Convert(const InputStringType& str,
                                typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	OutputStringType result;
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the (assumed) Unicode string input to a new Unicode string of the given type.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, typename InputStringType>
inline OutputStringType Convert(const InputStringType& str,
                                typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	OutputStringType result;
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the given range in the given input encoding to a new string of the given type and output encoding.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputIteratorType>
inline OutputStringType Convert(InputIteratorType begin, InputIteratorType end,
                                typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	OutputStringType result;
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the (assumed) Unicode range to a new Unicode string of the given type.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note Convert assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use ConvertSafe.
template<typename OutputStringType, typename InputIteratorType>
inline OutputStringType Convert(InputIteratorType begin, InputIteratorType end,
                                typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	OutputStringType result;
	Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the given string in the given input encoding and stores into the result string with the given output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputStringType>
inline OutputStringType& ConvertSafe(OutputStringType& result, const InputStringType& str,
                                     typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the (assumed) Unicode string input and stores into the result Unicode string.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputStringType>
inline OutputStringType& ConvertSafe(OutputStringType& result, const InputStringType& str,
                                     typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the given range in the given input encoding and stores into the result string with the given output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& ConvertSafe(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                     typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the (assumed) Unicode range and stores into the result Unicode string.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& ConvertSafe(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                     typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the given string in the given input encoding and stores into the result buffer with the given output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputStringType>
inline size_t ConvertSafe(OutputCharType* buffer, size_t length, const InputStringType& str,
                          typename Detail::SReqAnyObjOut<InputStringType, OutputCharType*, true, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str) + 1;
}

//! Converts the (assumed) Unicode string input and stores into the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the buffer type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, typename OutputCharType, typename InputStringType>
inline size_t ConvertSafe(OutputCharType* buffer, size_t length, const InputStringType& str,
                          typename Detail::SReqAutoObjOut<InputStringType, OutputCharType*, true, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str) + 1;
}

//! Converts the given range in the given input encoding and stores into the result buffer with the given output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputIteratorType>
inline size_t ConvertSafe(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                          typename Detail::SReqAnyItsOut<InputIteratorType, OutputCharType*, true, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, its) + 1;
}

//! Converts the (assumed) Unicode range and stores into the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, typename OutputCharType, typename InputIteratorType>
inline size_t ConvertSafe(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                          typename Detail::SReqAutoItsOut<InputIteratorType, OutputCharType*, true, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, false, InputStringType, OutputStringType>(result, its) + 1;
}

//! Converts the given string in the given input encoding to a new string of the given type and output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputStringType>
inline OutputStringType ConvertSafe(const InputStringType& str,
                                    typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	OutputStringType result;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the (assumed) Unicode string input to a new Unicode string of the given type.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputStringType>
inline OutputStringType ConvertSafe(const InputStringType& str,
                                    typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	OutputStringType result;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Converts the given range in the given input encoding to a new string of the given type and output encoding.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, EEncoding OutputEncoding, EEncoding InputEncoding, typename InputIteratorType>
inline OutputStringType ConvertSafe(InputIteratorType begin, InputIteratorType end,
                                    typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	OutputStringType result;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Converts the (assumed) Unicode range to a new Unicode string of the given type.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note ConvertSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Convert.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputIteratorType>
inline OutputStringType ConvertSafe(InputIteratorType begin, InputIteratorType end,
                                    typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	OutputStringType result;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, false, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Appends the given string in the given input encoding and stores at the end of the result string with the given output encoding.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputStringType>
inline OutputStringType& Append(OutputStringType& result, const InputStringType& str,
                                typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Appends the (assumed) Unicode string input and stores at the end of the result Unicode string.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
template<typename OutputStringType, typename InputStringType>
inline OutputStringType& Append(OutputStringType& result, const InputStringType& str,
                                typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Appends the given range in the given input encoding and stores at the end of the result string with the given output encoding.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& Append(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Appends the (assumed) Unicode range and stores at the end of the result Unicode string.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
template<typename OutputStringType, typename InputIteratorType>
inline OutputStringType& Append(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Appends the given string in the given input encoding to the result buffer with the given output encoding.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputStringType>
inline size_t Append(OutputCharType* buffer, size_t length, const InputStringType& str,
                     typename Detail::SReqAnyObjOut<InputStringType, OutputCharType*, true, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, str) + 1;
}

//! Appends the (assumed) Unicode string input to the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the buffer type and output type.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<typename OutputCharType, typename InputStringType>
inline size_t Append(OutputCharType* buffer, size_t length, const InputStringType& str,
                     typename Detail::SReqAutoObjOut<InputStringType, OutputCharType*, true>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, str) + 1;
}

//! Appends the given range in the given input encoding to the result buffer with the given output encoding.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputIteratorType>
inline size_t Append(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAnyItsOut<InputIteratorType, OutputCharType*, true, OutputEncoding>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, its) + 1;
}

//! Appends the (assumed) Unicode range to the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note Append assumes the input is valid encoded, if this is not guaranteed (ie, user-input), use AppendSafe.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<typename OutputCharType, typename InputIteratorType>
inline size_t Append(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                     typename Detail::SReqAutoItsOut<InputIteratorType, OutputCharType*, true>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, its) + 1;
}

//! Appends the given string in the given input encoding and stores at the end of the result string with the given output encoding.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputStringType>
inline OutputStringType& AppendSafe(OutputStringType& result, const InputStringType& str,
                                    typename Detail::SReqAnyObjOut<InputStringType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Appends the (assumed) Unicode string input and stores at the end of the result Unicode string.
//! The Unicode encodings are picked automatically depending on the input type and output type.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputStringType>
inline OutputStringType& AppendSafe(OutputStringType& result, const InputStringType& str,
                                    typename Detail::SReqAutoObjOut<InputStringType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, str);
	return result;
}

//! Appends the given range in the given input encoding and stores at the end of the result string with the given output encoding.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& AppendSafe(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                    typename Detail::SReqAnyItsOut<InputIteratorType, OutputStringType, false, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Appends the (assumed) Unicode range and stores at the end of the result Unicode string.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
template<EErrorRecovery Recovery, typename OutputStringType, typename InputIteratorType>
inline OutputStringType& AppendSafe(OutputStringType& result, InputIteratorType begin, InputIteratorType end,
                                    typename Detail::SReqAutoItsOut<InputIteratorType, OutputStringType, false, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputStringType, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	const InputStringType its(begin, end);
	Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, its);
	return result;
}

//! Appends the given string in the given input encoding to the result buffer with the given output encoding.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputStringType>
inline size_t AppendSafe(OutputCharType* buffer, size_t length, const InputStringType& str,
                         typename Detail::SReqAnyObjOut<InputStringType, OutputCharType*, true, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, str) + 1;
}

//! Appends the (assumed) Unicode string input to the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the buffer type and output type.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, typename OutputCharType, typename InputStringType>
inline size_t AppendSafe(OutputCharType* buffer, size_t length, const InputStringType& str,
                         typename Detail::SReqAutoObjOut<InputStringType, OutputCharType*, true, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputStringType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedBuffer<OutputCharType*> OutputStringType;
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, str) + 1;
}

//! Appends the given range in the given input encoding to the result buffer with the given output encoding.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, EEncoding OutputEncoding, EEncoding InputEncoding, typename OutputCharType, typename InputIteratorType>
inline size_t AppendSafe(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                         typename Detail::SReqAnyItsOut<InputIteratorType, OutputCharType*, true, OutputEncoding, Recovery>::type* = 0)
{
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::ConvertSafe<InputEncoding, OutputEncoding, true, Recovery, InputStringType, OutputStringType>(result, its) + 1;
}

//! Appends the (assumed) Unicode range to the result Unicode buffer.
//! The Unicode encodings are picked automatically depending on the range type and output type.
//! \note AppendSafe uses the specified Recovery parameter to fix encoding errors, if the input is known-valid, use Append.
//! \return The required length of the output buffer, in code-units, including the null-terminator.
template<EErrorRecovery Recovery, typename OutputCharType, typename InputIteratorType>
inline size_t AppendSafe(OutputCharType* buffer, size_t length, InputIteratorType begin, InputIteratorType end,
                         typename Detail::SReqAutoItsOut<InputIteratorType, OutputCharType*, true, eEncoding_UTF8, Recovery>::type* = 0)
{
	const EEncoding InputEncoding = Detail::SInferEncoding<InputIteratorType, true>::value;
	const EEncoding OutputEncoding = Detail::SInferEncoding<OutputCharType*, false>::value;
	typedef Detail::SPackedIterators<InputIteratorType> InputStringType;
	typedef Detail::SPackedBuffer<OutputCharType*>      OutputStringType;
	const InputStringType its(begin, end);
	OutputStringType result(buffer, length);
	return Detail::Convert<InputEncoding, OutputEncoding, true, InputStringType, OutputStringType>(result, its) + 1;
}
}

//! \endcond