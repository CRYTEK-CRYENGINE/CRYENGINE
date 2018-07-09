// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Math.h>

#include <memory>

struct IFlashVariableObject;
struct IFlashPlayerBootStrapper;
struct IFSCommandHandler;
struct IExternalInterfaceHandler;
struct IActionScriptFunction;
struct IScaleformPlayback;

struct SFlashVarValue;
struct SFlashCxform;
struct SFlashDisplayInfo;
struct SFlashCursorEvent;
struct SFlashKeyEvent;
struct SFlashCharEvent;

//! Currently arrays of SFlashVarValue are not supported as it would require runtime conversion from / to Scaleform internal variant type.
//! GFxValue unless we enforce binary compatibility!.
enum EFlashVariableArrayType
{
	FVAT_Int,
	FVAT_Double,
	FVAT_Float,
	FVAT_ConstStrPtr,
	FVAT_ConstWstrPtr
};

struct IFlashPlayer
{
	enum EOptions
	{
		//LOG_FLASH_LOADING   = 0x01, // Logs loading of flash file.
		//LOG_ACTION_SCRIPT   = 0x02, // Logs action script.
		RENDER_EDGE_AA       = 0x04,  //!< Enables edge anti-aliased flash rendering.
		INIT_FIRST_FRAME     = 0x08,  //!< Init objects of first frame when creating instance of flash file.
		ENABLE_MOUSE_SUPPORT = 0x10,  //!< Enable mouse input support.

		DEFAULT              = RENDER_EDGE_AA | INIT_FIRST_FRAME | ENABLE_MOUSE_SUPPORT,
		DEFAULT_NO_MOUSE     = RENDER_EDGE_AA | INIT_FIRST_FRAME
	};

	enum ECategory
	{
		eCat_RequestMeshCacheResetBit = 0x80000000,

		eCat_Default                  = 0,
		eCat_Temp                     = 1,
		eCat_Temp_TessHeavy           = 1 | eCat_RequestMeshCacheResetBit,

		eCat_User                     = 3
	};

	enum EScaleModeType
	{
		eSM_NoScale,
		eSM_ShowAll,
		eSM_ExactFit,
		eSM_NoBorder
	};

	enum EAlignType
	{
		eAT_Center,
		eAT_TopCenter,
		eAT_BottomCenter,
		eAT_CenterLeft,
		eAT_CenterRight,
		eAT_TopLeft,
		eAT_TopRight,
		eAT_BottomLeft,
		eAT_BottomRight
	};
	
	//! Initialization.
	virtual bool Load(const char* pFilePath, unsigned int options = DEFAULT, unsigned int cat = eCat_Default) = 0;

	// Rendering
	virtual void           SetBackgroundColor(const ColorB& color) = 0;
	virtual void           SetBackgroundAlpha(float alpha) = 0;
	virtual float          GetBackgroundAlpha() const = 0;
	virtual void           SetViewport(int x0, int y0, int width, int height, float aspectRatio = 1.0f) = 0;
	virtual void           GetViewport(int& x0, int& y0, int& width, int& height, float& aspectRatio) const = 0;
	virtual void           SetViewScaleMode(EScaleModeType scaleMode) = 0;
	virtual EScaleModeType GetViewScaleMode() const = 0;
	virtual void           SetViewAlignment(EAlignType viewAlignment) = 0;
	virtual EAlignType     GetViewAlignment() const = 0;
	virtual void           SetScissorRect(int x0, int y0, int width, int height) = 0;
	virtual void           GetScissorRect(int& x0, int& y0, int& width, int& height) const = 0;
	virtual void           Advance(float deltaTime) = 0;
	virtual void           Render() = 0;
	virtual void           SetClearFlags(uint32 clearFlags, ColorF clearColor = Clr_Transparent) = 0;
	virtual void           SetCompositingDepth(float depth) = 0;
	virtual void           StereoEnforceFixedProjectionDepth(bool enforce) = 0;
	virtual void           StereoSetCustomMaxParallax(float maxParallax = -1.0f) = 0;
	virtual void           AvoidStencilClear(bool avoid) = 0;
	virtual void           EnableMaskedRendering(bool enable) = 0; // special render mode for Crysis 2 HUD markers (in stereo)
	virtual void           ExtendCanvasToViewport(bool extend) = 0;

	// Execution State
	virtual void         Restart() = 0;
	virtual bool         IsPaused() const = 0;
	virtual void         Pause(bool pause) = 0;
	virtual void         GotoFrame(unsigned int frameNumber) = 0;
	virtual bool         GotoLabeledFrame(const char* pLabel, int offset = 0) = 0;
	virtual unsigned int GetCurrentFrame() const = 0;
	virtual bool         HasLooped() const = 0;

	//! Callbacks & Events
	//! ##@{
	virtual void SetFSCommandHandler(IFSCommandHandler* pHandler, void* pUserData = 0) = 0;
	virtual void SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler, void* pUserData = 0) = 0;
	virtual void SendCursorEvent(const SFlashCursorEvent& cursorEvent) = 0;
	virtual void SendKeyEvent(const SFlashKeyEvent& keyEvent) = 0;
	virtual void SendCharEvent(const SFlashCharEvent& charEvent) = 0;
	//! ##@}

	virtual bool HitTest(float x, float y) const = 0;

	virtual void SetVisible(bool visible) = 0;
	virtual bool GetVisible() const = 0;
	virtual void SetImeFocus() = 0;

	// Action Script
	virtual bool         SetVariable(const char* pPathToVar, const SFlashVarValue& value) = 0;
	virtual bool         SetVariable(const char* pPathToVar, const IFlashVariableObject* pVarObj) = 0;
	virtual bool         GetVariable(const char* pPathToVar, SFlashVarValue& value) const = 0;
	virtual bool         GetVariable(const char* pPathToVar, IFlashVariableObject*& pVarObj) const = 0;
	virtual bool         IsAvailable(const char* pPathToVar) const = 0;
	virtual bool         SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count) = 0;
	virtual unsigned int GetVariableArraySize(const char* pPathToVar) const = 0;
	virtual bool         GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count) const = 0;
	virtual bool         Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0) = 0;

	virtual bool         CreateString(const char* pString, IFlashVariableObject*& pVarObj) = 0;
	virtual bool         CreateStringW(const wchar_t* pString, IFlashVariableObject*& pVarObj) = 0;
	virtual bool         CreateObject(const char* pClassName, const SFlashVarValue* pArgs, unsigned int numArgs, IFlashVariableObject*& pVarObj) = 0;
	virtual bool         CreateArray(IFlashVariableObject*& pVarObj) = 0;
	virtual bool         CreateFunction(IFlashVariableObject*& pFuncVarObj, IActionScriptFunction* pFunc, void* pUserData = 0) = 0;

	//! General property queries.
	//! ##@{
	virtual unsigned int GetFrameCount() const = 0;
	virtual float        GetFrameRate() const = 0;
	virtual int          GetWidth() const = 0;
	virtual int          GetHeight() const = 0;
	virtual size_t       GetMetadata(char* pBuff, unsigned int buffSize) const = 0;
	virtual bool         HasMetadata(const char* pTag) const = 0;
	virtual const char*  GetFilePath() const = 0;
	//! ##@}

	//! Coordinate Translation.
	virtual void ResetDirtyFlags() = 0;

	//! Translates the screen coordinates to the client coordinates
	virtual void ScreenToClient(int& x, int& y) const = 0;

	//! Translates the client coordinates to the screen coordinates
	virtual void ClientToScreen(int& x, int& y) const = 0;
	// </interfuscator:shuffle>

	bool Invoke0(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, 0, 0, pResult);
	}
	bool Invoke1(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, &arg, 1, pResult);
	}

#if defined(ENABLE_DYNTEXSRC_PROFILING)
	virtual void LinkDynTextureSource(const struct IDynTextureSource* pDynTexSrc) = 0;
#endif

	virtual IScaleformPlayback* GetPlayback() = 0;

protected:
	IFlashPlayer() {}
	virtual ~IFlashPlayer() {}
};

struct IFlashPlayer_RenderProxy
{
	enum EFrameType
	{
		EFT_Mono,
		EFT_StereoLeft,
		EFT_StereoRight
	};

	// <interfuscator:shuffle>
	virtual void RenderCallback(EFrameType ft) = 0;
	virtual void RenderPlaybackLocklessCallback(int cbIdx, EFrameType ft, bool finalPlayback = true) = 0;
	virtual void DummyRenderCallback(EFrameType ft) = 0;
	// </interfuscator:shuffle>

	virtual IScaleformPlayback* GetPlayback() = 0;

protected:
	virtual ~IFlashPlayer_RenderProxy() noexcept {}
};

struct IFlashVariableObject
{
	struct ObjectVisitor
	{
	public:
		virtual void Visit(const char* pName) = 0;

	protected:
		virtual ~ObjectVisitor() {}
	};
	// <interfuscator:shuffle>

	//! Lifetime.
	virtual void                  Release() = 0;
	virtual IFlashVariableObject* Clone() const = 0;

	//! Type check.
	virtual bool           IsObject() const = 0;
	virtual bool           IsArray() const = 0;
	virtual bool           IsDisplayObject() const = 0;

	virtual SFlashVarValue ToVarValue() const = 0;

	// AS Object support. These methods are only valid for Object type (which includes Array and DisplayObject types).
	virtual bool HasMember(const char* pMemberName) const = 0;
	virtual bool SetMember(const char* pMemberName, const SFlashVarValue& value) = 0;
	virtual bool SetMember(const char* pMemberName, const IFlashVariableObject* pVarObj) = 0;
	virtual bool GetMember(const char* pMemberName, SFlashVarValue& value) const = 0;
	virtual bool GetMember(const char* pMemberName, IFlashVariableObject*& pVarObj) const = 0;
	virtual void VisitMembers(ObjectVisitor* pVisitor) const = 0;
	virtual bool DeleteMember(const char* pMemberName) = 0;
	virtual bool Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0) = 0;
	virtual bool Invoke(const char* pMethodName, const IFlashVariableObject** pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0) = 0;

	//! AS Array support. These methods are only valid for Array type.
	virtual unsigned int GetArraySize() const = 0;
	virtual bool         SetArraySize(unsigned int size) = 0;
	virtual bool         SetElement(unsigned int idx, const SFlashVarValue& value) = 0;
	virtual bool         SetElement(unsigned int idx, const IFlashVariableObject* pVarObj) = 0;
	virtual bool         GetElement(unsigned int idx, SFlashVarValue& value) const = 0;
	virtual bool         GetElement(unsigned int idx, IFlashVariableObject*& pVarObj) const = 0;
	virtual bool         PushBack(const SFlashVarValue& value) = 0;
	virtual bool         PushBack(const IFlashVariableObject* pVarObj) = 0;
	virtual bool         PopBack() = 0;
	virtual bool         RemoveElements(unsigned int idx, int count = -1) = 0;

	// AS display object (MovieClips, Buttons, TextFields) support. These methods are only valid for DisplayObject type.
	virtual bool SetDisplayInfo(const SFlashDisplayInfo& info) = 0;
	virtual bool GetDisplayInfo(SFlashDisplayInfo& info) const = 0;
	virtual bool SetDisplayMatrix(const Matrix33& mat) = 0;
	virtual bool GetDisplayMatrix(Matrix33& mat) const = 0;
	virtual bool Set3DMatrix(const Matrix44& mat) = 0;
	virtual bool Get3DMatrix(Matrix44& mat) const = 0;
	virtual bool SetColorTransform(const SFlashCxform& cx) = 0;
	virtual bool GetColorTransform(SFlashCxform& cx) const = 0;
	virtual bool SetVisible(bool visible) = 0;

	// AS TextField support
	virtual bool SetText(const char* pText) = 0;
	virtual bool SetText(const wchar_t* pText) = 0;
	virtual bool SetTextHTML(const char* pHtml) = 0;
	virtual bool SetTextHTML(const wchar_t* pHtml) = 0;
	virtual bool GetText(SFlashVarValue& text) const = 0;
	virtual bool GetTextHTML(SFlashVarValue& html) const = 0;

	// AS MovieClip support. These methods are only valid for MovieClips.
	virtual bool CreateEmptyMovieClip(IFlashVariableObject*& pVarObjMC, const char* pInstanceName, int depth = -1) = 0;
	virtual bool AttachMovie(IFlashVariableObject*& pVarObjMC, const char* pSymbolName, const char* pInstanceName, int depth = -1, const IFlashVariableObject* pInitObj = 0) = 0;
	virtual bool GotoAndPlay(const char* pFrame) = 0;
	virtual bool GotoAndStop(const char* pFrame) = 0;
	virtual bool GotoAndPlay(unsigned int frame) = 0;
	virtual bool GotoAndStop(unsigned int frame) = 0;
	// </interfuscator:shuffle>
	bool         Invoke0(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, static_cast<SFlashVarValue*>(0), 0, pResult);
	}
	bool Invoke1(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, &arg, 1, pResult);
	}
	bool RemoveElement(unsigned int idx)
	{
		return RemoveElements(idx, 1);
	}
	bool ClearElements()
	{
		return RemoveElements(0);
	}

protected:
	virtual ~IFlashVariableObject() {}
};

//! \cond INTERNAL
//! Bootstrapper to efficiently instantiate Flash assets on demand with minimal file IO.
struct IFlashPlayerBootStrapper
{
	// <interfuscator:shuffle>

	//! Lifetime.
	virtual void Release() = 0;

	//! Initialization.
	virtual bool Load(const char* pFilePath) = 0;

	//! Bootstrapping.
	virtual std::shared_ptr<IFlashPlayer> CreatePlayerInstance(unsigned int options = IFlashPlayer::DEFAULT, unsigned int cat = IFlashPlayer::eCat_Default) = 0;

	//! General property queries
	//! ##@{
	virtual const char* GetFilePath() const = 0;
	virtual size_t      GetMetadata(char* pBuff, unsigned int buffSize) const = 0;
	virtual bool        HasMetadata(const char* pTag) const = 0;
	//! ##@}
	// </interfuscator:shuffle>

protected:
	virtual ~IFlashPlayerBootStrapper() {}
};
//! \endcond

//! Clients of IFlashPlayer implement this interface to receive action script events.
struct IFSCommandHandler
{
	virtual void HandleFSCommand(const char* pCommand, const char* pArgs, void* pUserData = 0) = 0;

protected:
	virtual ~IFSCommandHandler() {}
};

//! \cond INTERNAL
//! Clients of IFlashPlayer implement this interface to expose external interface calls.
struct IExternalInterfaceHandler
{
	virtual void HandleExternalInterfaceCall(const char* pMethodName, const SFlashVarValue* pArgs, int numArgs, void* pUserData = 0, SFlashVarValue* pResult = 0) = 0;

protected:
	virtual ~IExternalInterfaceHandler() {}
};

//! Clients of IFlashPlayer implement this interface to replace inject C++ code into Action Script.
struct IActionScriptFunction
{
	struct Params
	{
		IFlashPlayer*                pFromPlayer;
		void*                        pUserData;

		const IFlashVariableObject*  pThis;
		const IFlashVariableObject** pArgs;
		unsigned int                 numArgs;
	};

	struct IReturnValue
	{
		//! Clients setting the return value in their implementation of Call() should think about "createManagedValue".
		//! For PODs its value doesn't have any meaning. When passing strings however it offers an optimization opportunity.
		//! If the string passed in "value" is managed by the client, there is no need to request internal creation of a managed value (a copy)
		//! as the (pointer to the) string will still be valid after Call() returns. However, if a pointer to a string on the stack is being
		//! passed, "createManagedValue" must be set to true!
		virtual void Set(const SFlashVarValue& value, bool createManagedValue = true) = 0;
		virtual void Set(const IFlashVariableObject* value) = 0;
	protected:
		virtual ~IReturnValue() {}
	};

	virtual void Call(const Params& params, IReturnValue* pRetVal) = 0;

protected:
	virtual ~IActionScriptFunction() {}
};

//! Clients of IFlashPlayer implement this interface to handle custom loadMovie API calls.
struct IFlashLoadMovieImage
{
	enum EFmt
	{
		eFmt_None,
		eFmt_RGB_888,
		eFmt_ARGB_8888,
	};

	// <interfuscator:shuffle>
	virtual void  Release() = 0;

	virtual int   GetWidth() const = 0;
	virtual int   GetHeight() const = 0;
	virtual int   GetPitch() const = 0;
	virtual void* GetPtr() const = 0;
	virtual EFmt  GetFormat() const = 0;
	// </interfuscator:shuffle>

	bool IsValid() const
	{
		return GetPtr() && GetPitch() > 0 && GetWidth() > 0 && GetHeight() > 0;
	}

protected:
	virtual ~IFlashLoadMovieImage() {}
};

//! Clients of IFlashPlayer implement this interface to handle custom loadMovie API calls.
struct IFlashLoadMovieHandler
{
	virtual IFlashLoadMovieImage* LoadMovie(const char* pFilePath) = 0;

protected:
	virtual ~IFlashLoadMovieHandler() {}
};
//! \endcond

//! Variant type to pass values to flash variables.
struct SFlashVarValue
{
	union Data
	{
		bool           b;
		int            i;
		unsigned int   ui;
		double         d;
		float          f;
		const char*    pStr;
		const wchar_t* pWstr;
	};

	//! Enumerates types that can be sent to and received from flash.
	enum Type
	{
		eUndefined,
		eNull,

		eBool,
		eInt,
		eUInt,
		eDouble,
		eFloat,
		eConstStrPtr,
		eConstWstrPtr,

		eObject //!< Receive only!
	};

	SFlashVarValue(bool val)
		: type(eBool)
	{
		data.b = val;
	}
	SFlashVarValue(int val)
		: type(eInt)
	{
		data.i = val;
	}
	SFlashVarValue(unsigned int val)
		: type(eUInt)
	{
		data.ui = val;
	}
	SFlashVarValue(double val)
		: type(eDouble)
	{
		data.d = val;
	}
	SFlashVarValue(float val)
		: type(eFloat)
	{
		data.f = val;
	}
	SFlashVarValue(const char* val)
		: type(eConstStrPtr)
	{
		data.pStr = val;
	}
	SFlashVarValue(const wchar_t* val)
		: type(eConstWstrPtr)
	{
		data.pWstr = val;
	}
	static SFlashVarValue CreateUndefined()
	{
		return SFlashVarValue(eUndefined);
	}
	static SFlashVarValue CreateNull()
	{
		return SFlashVarValue(eNull);
	}

	bool GetBool() const
	{
		assert(type == eBool);
		return data.b;
	}
	int GetInt() const
	{
		assert(type == eInt);
		return data.i;
	}
	unsigned int GetUInt() const
	{
		assert(type == eUInt);
		return data.ui;
	}
	double GetDouble() const
	{
		assert(type == eDouble);
		return data.d;
	}
	float GetFloat() const
	{
		assert(type == eFloat);
		return data.f;
	}
	const char* GetConstStrPtr() const
	{
		assert(type == eConstStrPtr);
		return data.pStr;
	}
	const wchar_t* GetConstWstrPtr() const
	{
		assert(type == eConstWstrPtr);
		return data.pWstr;
	}

	Type GetType() const
	{
		return type;
	}
	bool IsUndefined() const
	{
		return GetType() == eUndefined;
	}
	bool IsNull() const
	{
		return GetType() == eNull;
	}
	bool IsBool() const
	{
		return GetType() == eBool;
	}
	bool IsInt() const
	{
		return GetType() == eInt;
	}
	bool IsUInt() const
	{
		return GetType() == eUInt;
	}
	bool IsDouble() const
	{
		return GetType() == eDouble;
	}
	bool IsFloat() const
	{
		return GetType() == eFloat;
	}
	bool IsConstStr() const
	{
		return GetType() == eConstStrPtr;
	}
	bool IsConstWstr() const
	{
		return GetType() == eConstWstrPtr;
	}
	bool IsObject() const
	{
		return GetType() == eObject;
	}

protected:
	Type type;
	Data data;

protected:
	//! Don't define default constructor to enforce efficient default initialization of argument lists!
	SFlashVarValue();

	SFlashVarValue(Type t)
		: type(t)
	{
		//data.d = 0;

		//static const Data init = {0};
		//data = init;

		memset(&data, 0, sizeof(data));
	}
};

//! \cond INTERNAL
//! Color transformation to control flash movie clips.
struct SFlashCxform
{
	ColorF mul; // Range: 0.0f - 1.0f
	ColorB add; // Range: 0 - 255
};

//! DisplayInfo structure for flash display objects (MovieClip, TextField, Button).
struct SFlashDisplayInfo
{
public:
	enum Flags
	{
		FDIF_X         = 0x001,
		FDIF_Y         = 0x002,
		FDIF_Z         = 0x004,

		FDIF_XScale    = 0x008,
		FDIF_YScale    = 0x010,
		FDIF_ZScale    = 0x020,

		FDIF_Rotation  = 0x040,
		FDIF_XRotation = 0x080,
		FDIF_YRotation = 0x100,

		FDIF_Alpha     = 0x200,
		FDIF_Visible   = 0x400,
	};

	SFlashDisplayInfo()
		: m_x(0), m_y(0), m_z(0)
		, m_xscale(0), m_yscale(0), m_zscale(0)
		, m_rotation(0), m_xrotation(0), m_yrotation(0)
		, m_alpha(0)
		, m_visible(false)
		, m_varsSet(0)
	{}

	SFlashDisplayInfo(float x, float y, float z,
	                  float xscale, float yscale, float zscale,
	                  float rotation, float xrotation, float yrotation,
	                  float alpha,
	                  bool visible,
	                  unsigned short varsSet)
		: m_x(x), m_y(y), m_z(z)
		, m_xscale(xscale), m_yscale(yscale), m_zscale(zscale)
		, m_rotation(rotation), m_xrotation(xrotation), m_yrotation(yrotation)
		, m_alpha(alpha)
		, m_visible(visible)
		, m_varsSet(varsSet)
	{}

	void Clear()                                            { m_varsSet = 0; }

	void SetX(float x)                                      { SetFlags(FDIF_X); m_x = x; }
	void SetY(float y)                                      { SetFlags(FDIF_Y); m_y = y; }
	void SetZ(float z)                                      { SetFlags(FDIF_Z); m_z = z; }

	void SetXScale(float xscale)                            { SetFlags(FDIF_XScale); m_xscale = xscale; } //!< 100 == 100%.
	void SetYScale(float yscale)                            { SetFlags(FDIF_YScale); m_yscale = yscale; } //!< 100 == 100%.
	void SetZScale(float zscale)                            { SetFlags(FDIF_ZScale); m_zscale = zscale; } //!< 100 == 100%.

	void SetRotation(float degrees)                         { SetFlags(FDIF_Rotation); m_rotation = degrees; }
	void SetXRotation(float degrees)                        { SetFlags(FDIF_XRotation); m_xrotation = degrees; }
	void SetYRotation(float degrees)                        { SetFlags(FDIF_YRotation); m_yrotation = degrees; }

	void SetAlpha(float alpha)                              { SetFlags(FDIF_Alpha); m_alpha = alpha; }
	void SetVisible(bool visible)                           { SetFlags(FDIF_Visible); m_visible = visible; }

	void SetPosition(float x, float y)                      { SetFlags(FDIF_X | FDIF_Y); m_x = x; m_y = y; }
	void SetPosition(float x, float y, float z)             { SetFlags(FDIF_X | FDIF_Y | FDIF_Z); m_x = x; m_y = y; m_z = z; }

	void SetScale(float xscale, float yscale)               { SetFlags(FDIF_XScale | FDIF_YScale); m_xscale = xscale; m_yscale = yscale; }
	void SetScale(float xscale, float yscale, float zscale) { SetFlags(FDIF_XScale | FDIF_YScale | FDIF_ZScale); m_xscale = xscale; m_yscale = yscale; m_zscale = zscale; }

	void Set(float x, float y, float z,
	         float xscale, float yscale, float zscale,
	         float rotation, float xrotation, float yrotation,
	         float alpha,
	         bool visible)
	{
		m_x = x;
		m_y = y;
		m_z = z;

		m_xscale = xscale;
		m_yscale = yscale;
		m_zscale = zscale;

		m_rotation = rotation;
		m_xrotation = xrotation;
		m_yrotation = yrotation;

		m_alpha = alpha;
		m_visible = visible;

		SetFlags(FDIF_X | FDIF_Y | FDIF_Z |
		         FDIF_XScale | FDIF_YScale | FDIF_ZScale |
		         FDIF_Rotation | FDIF_XRotation | FDIF_YRotation |
		         FDIF_Alpha | FDIF_Visible);
	}

	float GetX() const                       { return m_x; }
	float GetY() const                       { return m_y; }
	float GetZ() const                       { return m_z; }

	float GetXScale() const                  { return m_xscale; }
	float GetYScale() const                  { return m_yscale; }
	float GetZScale() const                  { return m_zscale; }

	float GetRotation() const                { return m_rotation; }
	float GetXRotation() const               { return m_xrotation; }
	float GetYRotation() const               { return m_yrotation; }

	float GetAlpha() const                   { return m_alpha; }
	bool  GetVisible() const                 { return m_visible; }

	bool  IsAnyFlagSet() const               { return 0 != m_varsSet; }
	bool  IsFlagSet(unsigned int flag) const { return 0 != (m_varsSet & flag); }

private:
	void SetFlags(unsigned int flags) { m_varsSet |= flags; }

private:
	float          m_x;
	float          m_y;
	float          m_z;

	float          m_xscale;
	float          m_yscale;
	float          m_zscale;

	float          m_rotation;
	float          m_xrotation;
	float          m_yrotation;

	float          m_alpha;
	bool           m_visible;

	unsigned short m_varsSet;
};
//! \endcond

//! Cursor input event sent to flash.
struct SFlashCursorEvent
{
public:
	//! Enumeration of possible cursor state.
	enum ECursorState
	{
		eCursorMoved,
		eCursorPressed,
		eCursorReleased,
		eWheel
	};

	SFlashCursorEvent(ECursorState state, int cursorX, int cursorY, int button = 0, float wheelScrollVal = 0.0f)
		: m_state(state)
		, m_cursorX(cursorX)
		, m_cursorY(cursorY)
		, m_button(button)
		, m_wheelScrollVal(wheelScrollVal)
	{
	}

	ECursorState m_state;
	int          m_cursorX;
	int          m_cursorY;
	int          m_button;
	float        m_wheelScrollVal;
};

//! Key input event sent to flash.
struct SFlashKeyEvent
{
public:
	enum EKeyState
	{
		eKeyDown,
		eKeyUp
	};

	enum EKeyCode
	{
		VoidSymbol = 0,

		//! A through Z and numbers 0 through 9.
		//! ##@{
		A = 65,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		Num0 = 48,
		Num1,
		Num2,
		Num3,
		Num4,
		Num5,
		Num6,
		Num7,
		Num8,
		Num9,
		//! ##@}

		//! Numeric keypad.
		//! ##@{
		KP_0 = 96,
		KP_1,
		KP_2,
		KP_3,
		KP_4,
		KP_5,
		KP_6,
		KP_7,
		KP_8,
		KP_9,
		KP_Multiply,
		KP_Add,
		KP_Enter,
		KP_Subtract,
		KP_Decimal,
		KP_Divide,
		//! ##@}

		//! Function keys.
		//! ##@{
		F1 = 112,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		//! ##@}

		//! Other keys.
		//! ##@{
		Backspace = 8,
		Tab,
		Clear     = 12,
		Return,
		Shift     = 16,
		Control,
		Alt,
		CapsLock = 20,        //!< Toggle.
		Escape   = 27,
		Space    = 32,
		PageUp,
		PageDown,
		End = 35,
		Home,
		Left,
		Up,
		Right,
		Down,
		Insert = 45,
		Delete,
		Help,
		NumLock      = 144,    //!< Toggle.
		ScrollLock   = 145,    //!< Toggle.

		Semicolon    = 186,
		Equal        = 187,
		Comma        = 188,    //!< Platform specific?
		Minus        = 189,
		Period       = 190,    //!< Platform specific?
		Slash        = 191,
		Bar          = 192,
		BracketLeft  = 219,
		Backslash    = 220,
		BracketRight = 221,
		Quote        = 222,
		//! ##@}

		//! Total number of keys.
		KeyCount
	};

	enum ESpecialKeyState
	{
		eShiftPressed  = 0x01,
		eCtrlPressed   = 0x02,
		eAltPressed    = 0x04,
		eCapsToggled   = 0x08,
		eNumToggled    = 0x10,
		eScrollToggled = 0x20
	};

	SFlashKeyEvent(EKeyState state, EKeyCode keyCode, unsigned char specialKeyState, unsigned char asciiCode, unsigned int wcharCode)
		: m_state(state)
		, m_keyCode(keyCode)
		, m_specialKeyState(specialKeyState)
		, m_asciiCode(asciiCode)
		, m_wcharCode(wcharCode)
	{
	}

	EKeyState     m_state;
	EKeyCode      m_keyCode;
	unsigned char m_specialKeyState;
	unsigned char m_asciiCode;
	unsigned int  m_wcharCode;
};

//! Char event sent to flash.
struct SFlashCharEvent
{
public:
	SFlashCharEvent(uint32 wCharCode, uint8 keyboardIndex = 0)
		: m_wCharCode(wCharCode)
		, m_keyboardIndex(keyboardIndex)
	{
	}

	uint32 m_wCharCode;

	//! The index of the physical keyboard controller.
	uint8 m_keyboardIndex;
};
