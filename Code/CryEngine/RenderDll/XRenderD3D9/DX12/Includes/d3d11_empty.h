// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if CRY_PLATFORM_WINAPI || _MSC_EXTENSIONS
	#if _MSC_VER
		#define DX11_STRINGIFY(_STRING) # _STRING

		#define DX11_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
			  DX11_STRINGIFY(_D0 ## - ## _W0 ## - ## _W1 ## - ## _B0 ## _B1 ## - ## _B2 ## _B3 ## _B4 ## _B5 ## _B6 ## _B7)

		#define DX11_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
			  _CLASS __declspec(uuid(DX11_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7))) _TYPE;
	#else
		#define DX11_GUID_QUOTE(_STRING) # _STRING
		#define DX11_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
				  DX11_GUID_QUOTE(_D0) "-"	\
				  DX11_GUID_QUOTE(_W0) "-" DX11_GUID_QUOTE(_W1) "-"	\
				  DX11_GUID_QUOTE(_B0) DX11_GUID_QUOTE(_B1) "-"	\
				  DX11_GUID_QUOTE(_B2) DX11_GUID_QUOTE(_B3) DX11_GUID_QUOTE(_B4) DX11_GUID_QUOTE(_B5) DX11_GUID_QUOTE(_B6) DX11_GUID_QUOTE(_B7)
		#define DX11_DEFINE_TYPE_GUID(_CLASS, _TYPE, _D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7) \
				  _CLASS __declspec(uuid(DX11_GUID_STRING(_D0, _W0, _W1, _B0, _B1, _B2, _B3, _B4, _B5, _B6, _B7))) _TYPE;
	#endif
#else
	//implement GUID for other platforms
	#error "NOT IMPLEMENTED!"
#endif

#if !DX11_COM_INTERFACES
	#ifndef __unknwnbase_h__
	struct IUnknown;

	#if CRY_PLATFORM_WINAPI || _MSC_EXTENSIONS
		DX11_DEFINE_TYPE_GUID(struct, IUnknown,                    00000000, 0000, 0000, C0, 00, 00, 00, 00, 00, 00, 46)
	#endif

	struct IUnknown
	{
	public:
		virtual ~IUnknown() {};

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
		virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
		virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
	};
	#endif

	class IEmptyRefCountable;
	class IEmptyDeviceChild;
	class IEmptyDevice1;
	class IEmptyDeviceContext1;
	class IEmptyState;
	class IEmptyResource;
	class IEmptyView;
	class IEmptyAsynchronous;
	class IEmptyInputLayout;

	class IEmptyOutput4;
	class IEmptyAdapter2;
	class IEmptyAdapter3;
	class IEmptyFactory4;
	class IEmptySwapChain1;
	class IEmptySwapChain3;

	#if CRY_PLATFORM_WINAPI || _MSC_EXTENSIONS
		// *INDENT-OFF*
		DX11_DEFINE_TYPE_GUID(class , IEmptyDevice1,               DB6F6DDB, AC77, 4E88, 82, 53, 81, 9D, F9, BB, F1, 40)
		DX11_DEFINE_TYPE_GUID(class , IEmptyDeviceContext1,        C0BFA96C, E089, 44FB, 8E, AF, 26, F8, 79, 61, 90, DA)
		DX11_DEFINE_TYPE_GUID(class , IEmptyState,                 1841E5C8, 16B0, 489B, BC, C8, 44, CF, B0, D5, DE, AE)
		DX11_DEFINE_TYPE_GUID(class , IEmptyResource,              DC8E63F3, D12B, 4952, B4, 7B, 5E, 45, 02, 6A, 86, 2D)
		DX11_DEFINE_TYPE_GUID(class , IEmptyView,                  839D1216, BB2E, 412B, B7, F4, A9, DB, EB, E0, 8E, D1)
		DX11_DEFINE_TYPE_GUID(class , IEmptyDeviceChild,           1841E5C8, 16B0, 489B, BC, C8, 44, CF, B0, D5, DE, AE)
		DX11_DEFINE_TYPE_GUID(class , IEmptyAsynchronous,          4B35D0CD, 1E15, 4258, 9C, 98, 1B, 13, 33, F6, DD, 3B)
		DX11_DEFINE_TYPE_GUID(class , IEmptyInputLayout,           E4819DDC, 4CF0, 4025, BD, 26, 5D, E8, 2A, 3E, 07, B7)
								    
		DX11_DEFINE_TYPE_GUID(class , IEmptyOutput4,               DC7DCA35, 2196, 414D, 9F, 53, 61, 78, 84, 03, 2A, 60)
		DX11_DEFINE_TYPE_GUID(class , IEmptyAdapter2,              0AA1AE0A, FA0E, 4B84, 86, 44, E0, 5F, F8, E5, AC, B5)
		DX11_DEFINE_TYPE_GUID(class , IEmptyAdapter3,              645967A4, 1392, 4310, A7, 98, 80, 53, CE, 3E, 93, FD)
		DX11_DEFINE_TYPE_GUID(class , IEmptyFactory4,              1BC6EA02, EF36, 464F, BF, 0C, 21, CA, 39, E5, 16, 8A)
		DX11_DEFINE_TYPE_GUID(class , IEmptySwapChain1,            50C83A1C, E072, 4C48, 87, B0, 36, 30, FA, 36, A6, D0)
		DX11_DEFINE_TYPE_GUID(class , IEmptySwapChain3,            94D99BDB, F1F8, 4AB0, B2, 36, 7D, A0, 17, 0E, DA, B1)
		// *INDENT-ON*
	#endif

	#if !CRY_PLATFORM_DURANGO
		#define VIRTUALGFX virtual
		#define FINALGFX final
		#define IID_GFX_ARGS IID_PPV_ARGS
		typedef IUnknown IGfxUnknown;
		class IEmptyGfxRefCountable : public IUnknown { /*public: virtual ULONG STDMETHODCALLTYPE AddRef() = 0; virtual ULONG STDMETHODCALLTYPE Release() = 0;*/ };
	#else
		#define VIRTUALGFX 
		#define FINALGFX 
		#define IID_GFX_ARGS IID_GRAPHICS_PPV_ARGS
		typedef IGraphicsUnknown IGfxUnknown;
		class IEmptyGfxRefCountable : public IGraphicsUnknown { public: IEmptyGfxRefCountable() { m_RefCount = 0; m_pFunction = nullptr; } };
	#endif

	// *INDENT-OFF*
	class IEmptyDeviceChild    : public IEmptyGfxRefCountable { };
	class IEmptyDevice1        : public IEmptyGfxRefCountable { };
	class IEmptyDeviceContext1 : public IEmptyDeviceChild  { };
	class IEmptyState          : public IEmptyDeviceChild  { };
	class IEmptyResource       : public IEmptyDeviceChild  { public: virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) = 0; };
	class IEmptyView           : public IEmptyDeviceChild  { /*public: virtual void GetResource(IEmptyResource **ppResource) = 0;*/ };
	class IEmptyAsynchronous   : public IEmptyDeviceChild  { };
	class IEmptyInputLayout    : public IEmptyDeviceChild  { };

//	class IEmptyRefCountable   : public IUnknown           { public: virtual ULONG STDMETHODCALLTYPE AddRef() = 0; virtual ULONG STDMETHODCALLTYPE Release() = 0; };
	class IEmptyOutput4        : public IEmptyGfxRefCountable { };
	class IEmptyAdapter2       : public IEmptyGfxRefCountable { };
	class IEmptyAdapter3       : public IEmptyGfxRefCountable { };
	class IEmptyFactory4       : public IEmptyGfxRefCountable { };
	class IEmptySwapChain1     : public IEmptyGfxRefCountable { };
	class IEmptySwapChain3     : public IEmptyGfxRefCountable { };
	// *INDENT-ON*
#endif
