// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryDXGLGuid.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains the definition of a cross platform unique
//               identifier for DXGL interfaces and names
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLGUID__
	#define __CRYDXGLGUID__

// *INDENT-OFF* - GUIDs should not be indented

#define DXGL_DEFINE_GUID(_NAME, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
	const GUID _NAME = \
	{ \
		0x ## _D0, \
		0x ## _W0, \
		0x ## _W1, \
		{ \
			0x ## _B0, \
			0x ## _B1, \
			0x ## _B2, \
			0x ## _B3, \
			0x ## _B4, \
			0x ## _B5, \
			0x ## _B6, \
			0x ## _B7 \
		} \
	};

#if CRY_PLATFORM_WINDOWS

#define DXGL_GUID_QUOTE(_STRING) #_STRING
#define DXGL_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
	DXGL_GUID_QUOTE(_D0##-##_W0##-##_W1##-##_B0##_B1##-##_B2##_B3##_B4##_B5##_B6##_B7)
#define DXGL_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
	_CLASS __declspec(uuid(DXGL_GUID_STRING(_D0,_W0,_W1,_B0,_B1,_B2,_B3,_B4,_B5,_B6,_B7))) _TYPE;

#else // CRY_PLATFORM_WINDOWS

typedef struct _GUID
{
    uint32 Data1;
    ushort Data2;
    ushort Data3;
    uchar Data4[8];
} GUID;

typedef GUID IID;

inline bool operator==(const GUID& kLeft, const GUID& kRight)
{
	return memcmp(&kLeft, &kRight, sizeof(GUID)) == 0;
}

typedef const GUID& REFGUID;
typedef const GUID& REFIID;

template <typename T>
struct SCryDXGLTypeGuid{};

#define DXGL_TYPE_GUID_NAME(_TYPE) gDXGLTypeGUID__ ## _TYPE
#define DXGL_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
	DXGL_DEFINE_GUID(DXGL_TYPE_GUID_NAME(_TYPE), _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
	template <> struct SCryDXGLTypeGuid<_CLASS _TYPE>{ static const GUID& Get() { return DXGL_TYPE_GUID_NAME(_TYPE); } };

#define __uuidof(_TYPE) SCryDXGLTypeGuid<_TYPE>::Get()

#endif // CRY_PLATFORM_WINDOWS

#if DXGL_FULL_EMULATION

DXGL_DEFINE_TYPE_GUID(struct, IUnknown,                             00000000, 0000, 0000, C0, 00, 00, 00, 00, 00, 00, 46)
DXGL_DEFINE_TYPE_GUID(struct, ID3D10Blob,                           8BA5FB08, 5195, 40e2, AC, 58, 0D, 98, 9C, 3A, 01, 02)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DeviceChild,                    1841E5C8, 16B0, 489B, BC, C8, 44, CF, B0, D5, DE, AE)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DepthStencilState,              03823EFB, 8D8F, 4E1C, 9A, A2, F6, 4B, B2, CB, FD, F1)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11BlendState,                     75B68FAA, 347D, 4159, 8F, 45, A0, 64, 0F, 01, CD, 9A)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11RasterizerState,                9BB4AB81, AB1A, 4D8F, B5, 06, FC, 04, 20, 0B, 6E, E7)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Resource,                       DC8E63F3, D12B, 4952, B4, 7B, 5E, 45, 02, 6A, 86, 2D)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Buffer,                         48570B85, D1EE, 4FCD, A2, 50, EB, 35, 07, 22, B0, 37)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Texture1D,                      F8FB5C27, C6B3, 4F75, A4, C8, 43, 9A, F2, EF, 56, 4C)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Texture2D,                      6F15AAF2, D208, 4E89, 9A, B4, 48, 95, 35, D3, 4F, 9C)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Texture3D,                      037E866E, F56D, 4357, A8, AF, 9D, AB, BE, 6E, 25, 0E)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11View,                           839D1216, BB2E, 412B, B7, F4, A9, DB, EB, E0, 8E, D1)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ShaderResourceView,             B0E06FE0, 8192, 4E1A, B1, CA, 36, D7, 41, 47, 10, B2)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11RenderTargetView,               DFDBA067, 0B8D, 4865, 87, 5B, D7, B4, 51, 6C, C1, 64)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DepthStencilView,               9FDAC92A, 1876, 48C3, AF, AD, 25, B9, 4F, 84, A9, B6)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11UnorderedAccessView,            28ACF509, 7F5C, 48F6, 86, 11, F3, 16, 01, 0A, 63, 80)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11VertexShader,                   3B301D64, D678, 4289, 88, 97, 22, F8, 92, 8B, 72, F3)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11HullShader,                     8E5C6061, 628A, 4C8E, 82, 64, BB, E4, 5C, B3, D5, DD)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DomainShader,                   F582C508, 0F36, 490C, 99, 77, 31, EE, CE, 26, 8C, FA)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11GeometryShader,                 38325B96, EFFB, 4022, BA, 02, 2E, 79, 5B, 70, 27, 5C)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11PixelShader,                    EA82E40D, 51DC, 4F33, 93, D4, DB, 7C, 91, 25, AE, 8C)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ComputeShader,                  4F5B196E, C2BD, 495E, BD, 01, 1F, DE, D3, 8E, 49, 69)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11InputLayout,                    E4819DDC, 4CF0, 4025, BD, 26, 5D, E8, 2A, 3E, 07, B7)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11SamplerState,                   DA6FEA51, 564C, 4487, 98, 10, F0, D0, F9, B4, E3, A5)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Asynchronous,                   4B35D0CD, 1E15, 4258, 9C, 98, 1B, 13, 33, F6, DD, 3B)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Query,                          D6C00747, 87B7, 425E, B8, 4D, 44, D1, 08, 56, 0A, FD)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Predicate,                      9EB576DD, 9F77, 4D86, 81, AA, 8B, AB, 5F, E4, 90, E2)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Counter,                        6E8C49FB, A371, 4770, B4, 40, 29, 08, 60, 22, B7, 41)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ClassInstance,                  A6CD7FAA, B0B7, 4A2F, 94, 36, 86, 62, A6, 57, 97, CB)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ClassLinkage,                   DDF57CBA, 9543, 46E4, A1, 2B, F2, 07, A0, FE, 7F, ED)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11CommandList,                    A24BC4D1, 769E, 43F7, 80, 13, 98, FF, 56, 6C, 18, E2)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DeviceContext,                  C0BFA96C, E089, 44FB, 8E, AF, 26, F8, 79, 61, 90, DA)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Device,                         DB6F6DDB, AC77, 4E88, 82, 53, 81, 9D, F9, BB, F1, 40)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ShaderReflection,               8D536CA1, 0CCA, 4956, A8, 37, 78, 69, 63, 75, 55, 84)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ShaderReflectionType,           6E6FFA6A, 9BAE, 4613, A5, 1E, 91, 65, 2D, 50, 8C, 21)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ShaderReflectionVariable,       51F23923, F3E5, 4BD1, 91, CB, 60, 61, 77, D8, DB, 4C)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11ShaderReflectionConstantBuffer, EB62D63D, 93DD, 4318, 8A, E8, C6, F8, 3A, D3, 71, B8)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11SwitchToRef,                    1EF337E3, 58E7, 4F83, A6, 92, DB, 22, 1F, 5E, D4, 7E)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIObject,                          AEC22FB8, 76F3, 4639, 9B, E0, 28, EB, 43, A6, 7A, 2E)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIDeviceSubObject,                 3D3E0379, F9DE, 4D58, BB, 6C, 18, D6, 29, 92, F1, A6)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIOutput,                          AE02EEDB, C735, 4690, 8D, 52, 5A, 8D, C2, 02, 13, AA)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIAdapter,                         2411E7E1, 12AC, 4CCF, BD, 14, 97, 98, E8, 53, 4D, C0)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIAdapter1,                        29038f61, 3839, 4626, 91, fd, 08, 68, 79, 01, 1a, 05)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIFactory,                         7b7166ec, 21c7, 44ae, b2, 1a, c9, ae, 32, 1a, e3, 69)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIFactory1,                        770AAE78, F26F, 4DBA, A8, 29, 25, 3C, 83, D1, B3, 87)
DXGL_DEFINE_TYPE_GUID(struct, IDXGIDevice,                          54EC77FA, 1377, 44E6, 8C, 32, 88, FD, 5F, 44, C8, 4C)
DXGL_DEFINE_TYPE_GUID(struct, IDXGISwapChain,                       310d36a0, d2e7, 4c0a, aa, 04, 6a, 9d, 23, b8, 88, 6a)

DXGL_DEFINE_GUID(WKPDID_D3DDebugObjectName, 429B8C22, 9188, 4B0C, 87, 42, AC, B0, BF, 85, C2, 00)

#else
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLTexture1D,         637BD3A1, 3507, 4ECA, B0, 24, F4, 5E, 72, 1A, 93, CA)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLTexture2D,         810C3ECB, 11EA, 48C6, 92, EE, FE, 1F, 56, CC, A1, FB)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLTexture3D,         AD18E34A, 1879, 4329, 8A, 38, 47, 3E, 98, 92, 11, F6)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLBuffer,            2FC0ECFE, C29D, 468C, 96, B1, B4, 7E, A0, 02, 6B, AC)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLResource,          2B819A4A, B3DE, 4999, 93, DA, 03, 31, D7, 94, AE, 2E)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLView,              D2D7D83A, 77D1, 4112, A7, 80, 67, 98, 30, 70, 2F, 59)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLQuery,             EF4578BD, D215, 4EF8, 9B, C3, D5, AD, 83, DA, 77, EC)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLDebug,             AAEE26AF, 2E73, 478F, B6, 0C, 8B, 1D, 90, 4D, 5F, 4D)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLShaderReflection,  4B1CFC1E, 4E1E, 4954, A3, BB, E5, 17, 90, E3, 5F, A6)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLDevice,            36525D64, 2382, 4130, 81, 00, 78, DE, 5D, 43, 9F, 33)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLDeviceChild,       E61E0A3E, F6BD, 4998, B0, 1B, 9A, D8, F9, CA, 67, 30)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLSwitchToRef,       AD18E34A, 1879, 4329, 8A, 38, 47, 3E, 98, 92, 11, F6)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLGIFactory,         408D1CF0, 64A9, 4B2D, 99, 53, B0, D8, CD, CE, BA, AF)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLGIAdapter,         BA6BC4F4, 7419, 4CDA, A4, 76, 0C, 89, 35, 6B, 48, 6F)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLGIDevice,          ED665E26, B530, 432F, 83, 19, BB, 2D, 87, FA, 71, C2)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLGIObject,          CB223673, 742A, 458F, 90, ED, AC, 1E, 25, 94, 83, 46)
DXGL_DEFINE_TYPE_GUID(class, CCryDXGLSwapChain,         06cef291, a887, 4f9f, 95, 9c, 0a, ed, 53, 2c, 4c, 7b)
#if DXGL_VIRTUAL_DEVICE_AND_CONTEXT
DXGL_DEFINE_TYPE_GUID(struct, ID3D11Device,             2203D7E1, 1491, 4D0A, BA, E1, 28, F3, AD, 1A, 24, 56)
DXGL_DEFINE_TYPE_GUID(struct, ID3D11DeviceContext,      649E1339, C585, 4F31, 8B, F6, E2, 6C, D4, 20, EA, 82)
#endif //DXGL_VIRTUAL_DEVICE_AND_CONTEXT

DXGL_DEFINE_GUID(WKPDID_D3DDebugObjectName, BD6C7F86, 6C13, 453C, 92, B9, 70, 31, B4, D1, 51, D5)

#endif //DXGL_FULL_EMULATION

#undef DXGL_DEFINE_GUID
#undef DXGL_QUOTE

#endif
