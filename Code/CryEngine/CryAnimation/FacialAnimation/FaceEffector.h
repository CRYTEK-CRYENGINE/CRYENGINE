// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IFacialAnimation.h>
#include <CryMath/ISplines.h>
#include <CryMath/TCBSpline.h>
#include <CryMath/Cry_Quat.h>

class CFacialEffCtrl;
class CFacialEffectorsLibrary;

class CFaceIdentifierStorage
{
private:
#ifdef FACE_STORE_ASSET_VALUES
	string m_str;
#endif
	uint32 m_crc32;

public:
	CFaceIdentifierStorage(const CFaceIdentifierHandle& handle) :
#ifdef FACE_STORE_ASSET_VALUES
		m_str(handle.GetString()),
#endif
		m_crc32(handle.GetCRC32()) {}
	CFaceIdentifierStorage(string str)
	{
#ifdef FACE_STORE_ASSET_VALUES
		m_str = str;
#endif
		m_crc32 = CCrc32::ComputeLowercase(str.c_str());
	}
	CFaceIdentifierStorage() { m_crc32 = 0; };
	string GetString() const
	{
#ifdef FACE_STORE_ASSET_VALUES
		return m_str;
#endif
		return string("");
	}
	uint32 GetCRC32() const
	{
		return m_crc32;
	}
	CFaceIdentifierHandle GetHandle()
	{
		const char* str = NULL;
#ifdef FACE_STORE_ASSET_VALUES
		str = m_str.c_str();
#endif
		CFaceIdentifierHandle handle(str, m_crc32);
		return handle;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
#ifdef FACE_STORE_ASSET_VALUES
		pSizer->AddObject(m_str);
#endif
		pSizer->AddObject(m_crc32);
	}
};

#define EFFECTOR_MIN_WEIGHT_EPSILON 0.0001f

//////////////////////////////////////////////////////////////////////////
class CFacialEffector : public IFacialEffector, public _i_reference_target_t
{
public:
	CFacialEffector()
	{
		m_nIndexInState = -1;
		m_nFlags = 0;
		m_pLibrary = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual void                  SetIdentifier(CFaceIdentifierHandle ident);
	virtual CFaceIdentifierHandle GetIdentifier() { return m_name.GetHandle(); }

#ifdef FACE_STORE_ASSET_VALUES
	virtual void        SetName(const char* str) { m_name = string(str); }
	virtual const char* GetName() const          { return m_name.GetString(); }
#endif

	virtual EFacialEffectorType GetType()                                                   { return EFE_TYPE_GROUP; };

	virtual void                SetFlags(uint32 nFlags)                                     { m_nFlags = nFlags; };
	virtual uint32              GetFlags()                                                  { return m_nFlags; };

	virtual int                 GetIndexInState()                                           { return m_nIndexInState; };

	virtual void                SetParamString(EFacialEffectorParam param, const char* str) {};
	virtual const char*         GetParamString(EFacialEffectorParam param)                  { return ""; };
	virtual void                SetParamVec3(EFacialEffectorParam param, Vec3 vValue)       {};
	virtual Vec3                GetParamVec3(EFacialEffectorParam param)                    { return Vec3(0, 0, 0); };
	virtual void                SetParamInt(EFacialEffectorParam param, int nValue)         {};
	virtual int                 GetParamInt(EFacialEffectorParam param)                     { return 0; };

	virtual QuatT               GetQuatT()                                                  { return QuatT(IDENTITY); };
	virtual uint32              GetAttachmentCRC()                                          { return 0; };

	virtual int                 GetSubEffectorCount()                                       { return (int)m_subEffectors.size(); };
	virtual IFacialEffector*    GetSubEffector(int nIndex);
	virtual IFacialEffCtrl*     GetSubEffCtrl(int nIndex);
	virtual IFacialEffCtrl*     GetSubEffCtrlByName(const char* effectorName);
	virtual IFacialEffCtrl*     AddSubEffector(IFacialEffector* pEffector);
	virtual void                RemoveSubEffector(IFacialEffector* pEffector);
	virtual void                RemoveAllSubEffectors();
	//////////////////////////////////////////////////////////////////////////

	// Serialize extension of facial effector.
	virtual void Serialize(XmlNodeRef& node, bool bLoading);

	void         SetIndexInState(int nIndex)               { m_nIndexInState = nIndex; };

	void         SetLibrary(CFacialEffectorsLibrary* pLib) { m_pLibrary = pLib; }

	// Fast access to sub controllers.
	CFacialEffCtrl* GetSubCtrl(int nIndex) { return m_subEffectors[nIndex]; }

	// calling SetIdentfier from a EffectorLibrary would lead into an infinite loop, thats why
	// we need this function - should be removed in the future
	void         SetIdentifierByLibrary(CFaceIdentifierHandle handle) { m_name = handle; }

	virtual void GetMemoryUsage(ICrySizer* pSizer) const;
protected:
	friend class CFaceState; // For faster access.
	typedef std::vector<_smart_ptr<CFacialEffCtrl>> SubEffVector;

	CFaceIdentifierStorage   m_name;
	uint32                   m_nFlags;
	int                      m_nIndexInState;
	SubEffVector             m_subEffectors;
	CFacialEffectorsLibrary* m_pLibrary;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorExpression : public CFacialEffector
{
public:
	virtual EFacialEffectorType GetType() { return EFE_TYPE_EXPRESSION; };
};

//////////////////////////////////////////////////////////////////////////
class CFacialMorphTarget : public CFacialEffector
{
public:
	CFacialMorphTarget(uint32 nMorphTargetId) { m_nMorphTargetId = nMorphTargetId; }
	virtual EFacialEffectorType GetType()                        { return EFE_TYPE_MORPH_TARGET; };

	void                        SetMorphTargetId(uint32 nTarget) { m_nMorphTargetId = nTarget; }
	uint32                      GetMorphTargetId() const         { return m_nMorphTargetId; }

private:
	uint32 m_nMorphTargetId;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorAttachment : public CFacialEffector
{
public:
	CFacialEffectorAttachment();

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual EFacialEffectorType GetType() { return EFE_TYPE_ATTACHMENT; };
	virtual void                SetParamString(EFacialEffectorParam param, const char* str);
	virtual const char*         GetParamString(EFacialEffectorParam param);
	virtual void                SetParamVec3(EFacialEffectorParam param, Vec3 vValue);
	virtual Vec3                GetParamVec3(EFacialEffectorParam param);
	virtual void                SetQuatT(QuatT qt)           { m_vQuatT = qt; };
	virtual void                SetAttachmentCRC(uint32 crc) { m_attachmentCRC = crc; };
	virtual QuatT               GetQuatT();
	virtual uint32              GetAttachmentCRC()           { return m_attachmentCRC; };
	//////////////////////////////////////////////////////////////////////////

	virtual void Serialize(XmlNodeRef& node, bool bLoading);
	virtual void Set(const CFacialEffectorAttachment* const rhs);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
#ifdef FACE_STORE_ASSET_VALUES
		pSizer->AddObject(m_sAttachment);
		pSizer->AddObject(m_vOffset);
		pSizer->AddObject(m_vAngles);
#endif
		pSizer->AddObject(m_attachmentCRC);
		pSizer->AddObject(m_vQuatT);

		CFacialEffector::GetMemoryUsage(pSizer);
	}
protected:

#ifdef FACE_STORE_ASSET_VALUES // we dont need these values anymore during runtime
	string m_sAttachment;        // Name of the attachment or bone in character.
	Vec3 m_vOffset;
	Vec3 m_vAngles;
#endif

	uint32 m_attachmentCRC;
	// this is calculated when setting or serializing, and can be retrieved fastly at runtime
	QuatT m_vQuatT;
};

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorBone : public CFacialEffectorAttachment
{
public:
	//////////////////////////////////////////////////////////////////////////
	// IFacialEffector.
	//////////////////////////////////////////////////////////////////////////
	virtual EFacialEffectorType GetType() { return EFE_TYPE_BONE; };
};

//////////////////////////////////////////////////////////////////////////
// Controller of the face effector.
//////////////////////////////////////////////////////////////////////////

// Since the devirtualization parser doesn't support template parents classes this typedef is needed
typedef spline::CSplineKeyInterpolator<spline::CatmullRomKey<float>> FacialBaseSplineInterpolator;
class CFacialEffCtrl;

class CFacialEffCtrlSplineInterpolator : public FacialBaseSplineInterpolator
{
public:
	CFacialEffCtrlSplineInterpolator(CFacialEffCtrl* pOwner) : m_pOwner(pOwner) {};

	virtual void Interpolate(float time, ValueType& val);

private:
	CFacialEffCtrl* m_pOwner;
};

class CFacialEffCtrl : public IFacialEffCtrl, public _i_reference_target_t
{
public:
	CFacialEffCtrl();
	~CFacialEffCtrl()
	{
		if (m_pSplineInterpolator)
			delete m_pSplineInterpolator;
	}

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffCtrl interface
	//////////////////////////////////////////////////////////////////////////
	virtual IFacialEffCtrl::ControlType GetType()            { return m_type; };
	virtual void                        SetType(ControlType t);
	virtual IFacialEffector*            GetEffector()        { return m_pEffector; };
	virtual void                        SetConstantWeight(float fWeight);
	virtual float                       GetConstantWeight()  { return m_fWeight; }
	virtual float                       GetConstantBalance() { return m_fBalance; }
	virtual void                        SetConstantBalance(float fBalance);

	virtual int                         GetFlags()           { return m_nFlags; }
	virtual void                        SetFlags(int nFlags) { m_nFlags = nFlags; };
	virtual ISplineInterpolator*        GetSpline()          { return m_pSplineInterpolator; };
	virtual float                       Evaluate(float fInput);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void GetMinMax(float& min, float& max)
	{
		switch (m_type)
		{
		case CTRL_LINEAR:
			min = std::max(-1.0f, m_fWeight);
			max = std::min(1.0f, m_fWeight);
			return;
		case CTRL_SPLINE:
			// get min/max of spline points.
			break;
		}
	}

	// Evaluate input.
	CFacialEffector* GetCEffector() const                { return m_pEffector; };
	void             SetCEffector(CFacialEffector* pEff) { m_pEffector = pEff; };

	void             Serialize(CFacialEffectorsLibrary* pLibrary, XmlNodeRef& node, bool bLoading);

	bool             CheckFlag(int nFlag) const { return (m_nFlags & nFlag) == nFlag; }

	//////////////////////////////////////////////////////////////////////////
	// ISplineInterpolator.
	//////////////////////////////////////////////////////////////////////////
	// Dimension of the spline from 0 to 3, number of parameters used in ValueType.
	virtual void SerializeSpline(XmlNodeRef& node, bool bLoading)
	{
		if (m_pSplineInterpolator)
			m_pSplineInterpolator->SerializeSpline(node, bLoading);
	}

	void GetMemoryUsage(ICrySizer* pSizer, bool self) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(&m_type, sizeof(m_type));
		pSizer->AddObject(m_nFlags);
		pSizer->AddObject(m_fWeight);
		pSizer->AddObject(m_fBalance);
		pSizer->AddObject(m_pEffector);
		pSizer->AddObject(m_pSplineInterpolator);
	}
	//////////////////////////////////////////////////////////////////////////

public:
	ControlType                       m_type;
	int                               m_nFlags;
	float                             m_fWeight; // Constant weight.
	float                             m_fBalance;

	CFacialEffCtrlSplineInterpolator* m_pSplineInterpolator;
	_smart_ptr<CFacialEffector>       m_pEffector;
};
