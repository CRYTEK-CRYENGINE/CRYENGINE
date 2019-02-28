// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define VK_DEFINE_GUID(_NAME, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
  const GUID _NAME =                                                                 \
  {                                                                                  \
    0x ## _D0,                                                                       \
    0x ## _W0,                                                                       \
    0x ## _W1,                                                                       \
    {                                                                                \
      0x ## _B0,                                                                     \
      0x ## _B1,                                                                     \
      0x ## _B2,                                                                     \
      0x ## _B3,                                                                     \
      0x ## _B4,                                                                     \
      0x ## _B5,                                                                     \
      0x ## _B6,                                                                     \
      0x ## _B7                                                                      \
    }                                                                                \
  };

#if CRY_PLATFORM_WINAPI || _MSC_EXTENSIONS
	#if _MSC_VER
		#define VK_STRINGIFY(_STRING) # _STRING

		#define VK_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
			  VK_STRINGIFY(_D0 ## - ## _W0 ## - ## _W1 ## - ## _B0 ## _B1 ## - ## _B2 ## _B3 ## _B4 ## _B5 ## _B6 ## _B7)

		#define VK_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
			  _CLASS __declspec(uuid(VK_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7))) _TYPE;
	#else
		#define VK_GUID_QUOTE(_STRING) # _STRING
		#define VK_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
				  VK_GUID_QUOTE(_D0) "-"	\
				  VK_GUID_QUOTE(_W0) "-" VK_GUID_QUOTE(_W1) "-"	\
				  VK_GUID_QUOTE(_B0) VK_GUID_QUOTE(_B1) "-"	\
				  VK_GUID_QUOTE(_B2) VK_GUID_QUOTE(_B3) VK_GUID_QUOTE(_B4) VK_GUID_QUOTE(_B5) VK_GUID_QUOTE(_B6) VK_GUID_QUOTE(_B7)
		#define VK_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
				  _CLASS __declspec(uuid(VK_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7))) _TYPE;
	#endif
#else
	//implement GUID for other platforms
	#error "NOT IMPLEMENTED!"
#endif

#if !DX11_COM_INTERFACES
	class CCryVKGIOutput;
	class CCryVKGIAdapter;
	class CCryVKGIFactory;
	class CCryVKSwapChain;

	#if CRY_PLATFORM_WINAPI || _MSC_EXTENSIONS
		// *INDENT-OFF*
		VK_DEFINE_TYPE_GUID(class, CCryVKGIOutput,            DC7DCA35, 2196, 414D, 9F, 53, 61, 78, 84, 03, 2A, 60)
		VK_DEFINE_TYPE_GUID(class, CCryVKGIAdapter,           645967A4, 1392, 4310, A7, 98, 80, 53, CE, 3E, 93, FD)
		VK_DEFINE_TYPE_GUID(class, CCryVKGIFactory,           1BC6EA02, EF36, 464F, BF, 0C, 21, CA, 39, E5, 16, 8A)
		VK_DEFINE_TYPE_GUID(class, CCryVKSwapChain,           94D99BDB, F1F8, 4AB0, B2, 36, 7D, A0, 17, 0E, DA, B1)
		// *INDENT-ON*
	#endif

	VK_DEFINE_GUID         (WKPDID_D3DDebugObjectName,        BD6C7F86, 6C13, 453C, 92, B9, 70, 31, B4, D1, 51, D5)
#endif
