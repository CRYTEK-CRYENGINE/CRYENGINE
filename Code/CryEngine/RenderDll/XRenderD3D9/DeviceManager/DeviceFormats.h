// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

enum
{
	FMTSUPPORT_BUFFER                      = BIT(0),
	FMTSUPPORT_TEXTURE1D                   = BIT(1),
	FMTSUPPORT_TEXTURE2D                   = BIT(2),
	FMTSUPPORT_TEXTURE3D                   = BIT(3),
	FMTSUPPORT_TEXTURECUBE                 = BIT(4),

	// BUFFER
	FMTSUPPORT_IA_VERTEX_BUFFER            = BIT(5),
	FMTSUPPORT_IA_INDEX_BUFFER             = BIT(6),
	FMTSUPPORT_SO_BUFFER                   = BIT(7),

	// TEXTURE-ND
	FMTSUPPORT_MIP                         = BIT(8),
	FMTSUPPORT_SRGB                        = BIT(9),

	// TEXTURE-ND reads
	FMTSUPPORT_SHADER_LOAD                 = BIT(10),
	FMTSUPPORT_SHADER_GATHER               = BIT(11),
	FMTSUPPORT_SHADER_GATHER_COMPARISON    = BIT(12),
	FMTSUPPORT_SHADER_SAMPLE               = BIT(13),
	FMTSUPPORT_SHADER_SAMPLE_COMPARISON    = BIT(14),

	FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW = BIT(15),

	// TEXTURE-ND writes
	FMTSUPPORT_DEPTH_STENCIL               = BIT(16),
	FMTSUPPORT_RENDER_TARGET               = BIT(17),
	FMTSUPPORT_BLENDABLE                   = BIT(18),
	FMTSUPPORT_DISPLAYABLE                 = BIT(19),

	FMTSUPPORT_MULTISAMPLE_LOAD            = BIT(20),
	FMTSUPPORT_MULTISAMPLE_RESOLVE         = BIT(21),
	FMTSUPPORT_MULTISAMPLE_RENDERTARGET    = BIT(22),
};

struct SPixFormat
{
	SPixFormat* Next;

	// Pixel format info.
	D3DFormat   DeviceFormat; // Pixel format from Direct3D.
	const char* Desc;         // Stat: Human readable name for stats.
	int8        BitsPerPixel; // Total bits per pixel (negative to accomodate maximum size of 128 bits)

	UINT64      Options;

	int16       MaxWidth;
	int16       MaxHeight;

	SPixFormat()
	{
		Init();
	}

	void Init()
	{
		BitsPerPixel = 0;
		DeviceFormat = (D3DFormat)0;
		Desc = NULL;
		Next = NULL;
		Options = 0;
	}

	bool IsValid(UINT64 Option = 0) const
	{
		if (Option)
			return !!(Options & Option);

		return (BitsPerPixel < 0);
	}

	bool CheckSupport(D3DFormat Format, const char* szDescr);
};

struct SPixFormatSupport
{
	SPixFormat  m_FormatR1;
	SPixFormat  m_FormatA8;                          // 8 bit alpha
	SPixFormat  m_FormatR8;
	SPixFormat  m_FormatR8S;
	SPixFormat  m_FormatR8G8;                        // 16 bit
	SPixFormat  m_FormatR8G8S;                       // 16 bit
	SPixFormat  m_FormatR8G8B8A8;                    // 32 bit
	SPixFormat  m_FormatR8G8B8A8S;                   // 32 bit
	SPixFormat  m_FormatR10G10B10A2;                 // 32 bit

	SPixFormat  m_FormatR16;                         // 16 bit
	SPixFormat  m_FormatR16S;                        // 16 bit
	SPixFormat  m_FormatR16F;                        // 16 bit
	SPixFormat  m_FormatR32F;                        // 32 bit
	SPixFormat  m_FormatR16G16;                      // 32 bit
	SPixFormat  m_FormatR16G16S;                     // 32 bit
	SPixFormat  m_FormatR16G16F;                     // 32 bit
	SPixFormat  m_FormatR32G32F;                     // 64 bit
	SPixFormat  m_FormatR11G11B10F;                  // 32 bit
	SPixFormat  m_FormatR16G16B16A16F;               // 64 bit
	SPixFormat  m_FormatR16G16B16A16;                // 64 bit
	SPixFormat  m_FormatR16G16B16A16S;               // 64 bit
	SPixFormat  m_FormatR32G32B32A32F;               // 128 bit

	SPixFormat  m_FormatBC1;                         // Compressed RGB
	SPixFormat  m_FormatBC2;                         // Compressed RGBA
	SPixFormat  m_FormatBC3;                         // Compressed RGBA
	SPixFormat  m_FormatBC4U;                        // ATI1, single channel compression, unsigned
	SPixFormat  m_FormatBC4S;                        // ATI1, single channel compression, signed
	SPixFormat  m_FormatBC5U;                        // 
	SPixFormat  m_FormatBC5S;                        // 
	SPixFormat  m_FormatBC6UH;                       // Compressed RGB
	SPixFormat  m_FormatBC6SH;                       // Compressed RGB
	SPixFormat  m_FormatBC7;                         // Compressed RGBA

	SPixFormat  m_FormatR9G9B9E5;                    // Shared exponent RGB

#if CRY_RENDERER_OPENGL || CRY_RENDERER_VULKAN
	SPixFormat  m_FormatEAC_R11;                     // EAC compressed single channel for mobile, unsigned
	SPixFormat  m_FormatEAC_R11S;                    // EAC compressed single channel for mobile, signed
	SPixFormat  m_FormatEAC_RG11;                    // EAC compressed dual channel for mobile, unsigned
	SPixFormat  m_FormatEAC_RG11S;                   // EAC compressed dual channel for mobile, signed
	SPixFormat  m_FormatETC2;                        // ETC2 compressed RGB for mobile
	SPixFormat  m_FormatETC2A;                       // ETC2a compressed RGBA for mobile

	SPixFormat  m_FormatASTC_LDR;                    // compressed 8bit LDR for tablets/smartphones
	SPixFormat  m_FormatASTC_HDR;                    // compressed up to 16bit HDR for tablets/smartphones
#endif

	SPixFormat  m_FormatS8;                          // 8bit stencil
	SPixFormat  m_FormatD16;                         // 16bit fixed point depth
	SPixFormat  m_FormatD16S8;                       // 16bit fixed point depth + 8bit stencil
	SPixFormat  m_FormatD24;                         // 24bit fixed point depth
	SPixFormat  m_FormatD24S8;                       // 24bit fixed point depth + 8bit stencil
	SPixFormat  m_FormatD32F;                        // 32bit floating point depth
	SPixFormat  m_FormatD32FS8;                      // 32bit floating point depth + 8bit stencil

#if CRY_RENDERER_VULKAN
	SPixFormat  m_FormatR4G4;                        //  8 bit
	SPixFormat  m_FormatR4G4B4A4;                    // 16 bit
#endif

	SPixFormat  m_FormatB5G6R5;                      // 16 bit
	SPixFormat  m_FormatB5G5R5;                      // 16 bit
	SPixFormat  m_FormatB4G4R4A4;                    // 16 bit
	SPixFormat  m_FormatB8G8R8X8;                    // 32 bit
	SPixFormat  m_FormatB8G8R8A8;                    // 32 bit

	SPixFormat* m_FirstPixelFormat;

public:
	bool IsFormatSupported(ETEX_Format eTFDst)
	{
		return m_FormatSupportedCache[eTFDst];
	}

	ETEX_Format GetLessPreciseFormatSupported(ETEX_Format eTFDst)
	{
		return m_FormatLessPreciseCache[eTFDst];
	}

	ETEX_Format GetClosestFormatSupported(ETEX_Format eTFDst)
	{
		return m_FormatClosestCacheEnm[eTFDst];
	}

	ETEX_Format GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF)
	{
		pPF  = m_FormatClosestCachePtr[eTFDst];
		return m_FormatClosestCacheEnm[eTFDst];
	}

	const SPixFormat* GetPixFormat(ETEX_Format eTFDst)
	{
		return m_FormatClosestCachePtr[eTFDst];
	}

	void CheckFormatSupport();

private:
	bool              m_FormatSupportedCache  [eTF_MaxFormat];
	const SPixFormat* m_FormatClosestCachePtr [eTF_MaxFormat];
	ETEX_Format       m_FormatClosestCacheEnm [eTF_MaxFormat];
	ETEX_Format       m_FormatLessPreciseCache[eTF_MaxFormat];

	bool        _IsFormatSupported(ETEX_Format eTFDst);
	ETEX_Format _GetLessPreciseFormatSupported(ETEX_Format eTFDst);
	ETEX_Format _GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF);
};

struct SClearValue
{
	D3DFormat Format;

	union
	{
		float Color[4];

		struct
		{
			float Depth;
			uint8 Stencil;
		}
		DepthStencil;
	};

	static SClearValue GetDefaults(D3DFormat format, bool depth);
};

namespace DeviceFormats
{
	bool IsTypeless                  (D3DFormat nFormat  );
	bool IsDepthStencil              (D3DFormat nFormat  );
	bool IsSRGBReadable              (D3DFormat nFormat  );

	D3DFormat ConvertFromTexFormat   (ETEX_Format eTF    );
	uint32 GetWriteMask              (ETEX_Format eTF    );
	uint32 GetWriteMask              (D3DFormat nFormat  );
	ETEX_Format ConvertToTexFormat   (D3DFormat nFormat  );

	D3DFormat ConvertToDepthStencil  (D3DFormat nFormat  );
	D3DFormat ConvertToStencilOnly   (D3DFormat nFormat  );
	D3DFormat ConvertToDepthOnly     (D3DFormat nFormat  );
	D3DFormat ConvertToSRGB          (D3DFormat nFormat  );
	D3DFormat ConvertToSigned        (D3DFormat nFormat  );
	D3DFormat ConvertToUnsigned      (D3DFormat nFormat  );
	D3DFormat ConvertToFloat         (D3DFormat nFormat  );
	D3DFormat ConvertToTypeless      (D3DFormat nFormat  );

	inline ETEX_Format ConvertToDepthStencil(ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToDepthStencil  (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToStencilOnly (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToStencilOnly   (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToDepthOnly   (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToDepthOnly     (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToSRGB        (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToSRGB          (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToSigned      (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToSigned        (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToUnsigned    (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToUnsigned      (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToFloat       (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToFloat         (ConvertFromTexFormat(nFormat))); };
	inline ETEX_Format ConvertToTypeless    (ETEX_Format nFormat) { return ConvertToTexFormat(ConvertToTypeless      (ConvertFromTexFormat(nFormat))); };

	UINT GetStride                   (D3DFormat format   );
}
