// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CRYTEXTURESQUISHER_H
#define CRYTEXTURESQUISHER_H

class CryTextureSquisher
{
public:
	enum EBufferType
	{
		eBufferType_uint8,      // native support: BC1-5/7,CTX1
		eBufferType_sint8,      // native support: BC4-5
		eBufferType_uint16,     // native support: BC1-7,CTX1
		eBufferType_sint16,     // native support: BC4-6
		eBufferType_ufloat,     // native support: BC1-7,CTX1
		eBufferType_sfloat,     // native support: BC4-6
	};

	enum EQualityProfile
	{
		eQualityProfile_Low = 0,// as-fast-as-possible
		eQualityProfile_Medium, // not so bad (nightly builds)
		eQualityProfile_High,   // relative good (weekly builds)
		eQualityProfile_Best,   // as-best-as-possible (final build for release)

		eQualityProfile_Num
	};

	enum ECodingPreset
	{
		eCompressorPreset_BC1U = 0,
		eCompressorPreset_BC2U,
		eCompressorPreset_BC3U,
		eCompressorPreset_BC4U,   // r-channel from RGBA
		eCompressorPreset_BC5U,   // rg-channels from RGBA
		eCompressorPreset_BC6UH,
		eCompressorPreset_BC7U,

		eCompressorPreset_BC4S,   // r-channel from RGBA
		eCompressorPreset_BC5S,   // rg-channels from RGBA

		// normal vectors -> unit metric
		eCompressorPreset_BC1Un,
		eCompressorPreset_BC2Un,
		eCompressorPreset_BC3Un,
		eCompressorPreset_BC4Un,  // z-channel from XYZD
		eCompressorPreset_BC5Un,  // xy-channels from XYZD, xyz must be a valid unit-vector
		eCompressorPreset_BC6UHn,
		eCompressorPreset_BC7Un,

		eCompressorPreset_BC4Sn,  // z-channel from XYZD
		eCompressorPreset_BC5Sn,  // xy-channels from XYZD, xyz must be a valid unit-vector

		// transparency -> weighted alpha
		eCompressorPreset_BC1Ua,
		eCompressorPreset_BC2Ut,
		eCompressorPreset_BC3Ut,
		eCompressorPreset_BC4Ua,  // a-channel from RGBA
		eCompressorPreset_BC7Ut,

		eCompressorPreset_BC4Sa,  // a-channel from RGBA

		// grey-scale -> 12+ bits of precision
		eCompressorPreset_BC7Ug,

		// special ones
		eCompressorPreset_CTX1U,  // rg-channels from RGBA
		eCompressorPreset_CTX1Un, // xy-channels from XYZD, xyz must be a valid unit-vector

		eCompressorPreset_Num
	};

	struct CompressorParameters
	{
		// source's parameters
		EBufferType srcType;
		const void* srcBuffer;
		unsigned int width;
		unsigned int height;
		unsigned int pitch;

		// coding preset
		ECodingPreset preset;
		EQualityProfile quality;

		// either if "srgb==1" or if "rgbweights!=uniform"
		bool perceptual;
		float weights[4];

		void* userPtr;
		int userInt;

		void (*userOutputFunction)(const CompressorParameters& compress, const void* compressedData, unsigned int compressedSize, unsigned int oy, unsigned int ox);
	};

	struct DecompressorParameters
	{
		// destination's parameters
		EBufferType dstType;
		void* dstBuffer;
		unsigned int width;
		unsigned int height;
		unsigned int pitch;

		// coding preset
		ECodingPreset preset;

		void* userPtr;
		int userInt;

		void (*userInputFunction)(const DecompressorParameters& decompress, void* compressedData, unsigned int compressedSize, unsigned int oy, unsigned int ox);
	};

public:
	static void Compress(const CompressorParameters& compress);
	static void Decompress(const DecompressorParameters& decompress);

};

#endif
