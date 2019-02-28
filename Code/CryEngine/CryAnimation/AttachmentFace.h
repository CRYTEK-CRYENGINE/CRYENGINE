// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AttachmentBase.h"
#include "SocketSimulation.h"
#include "ModelMesh.h"

class CAttachmentFACE : public SAttachmentBase
{
public:

	CAttachmentFACE()
	{
		m_AttRelativeDefault.SetIdentity();
		m_AttAbsoluteDefault.SetIdentity();
		m_AttModelRelative.SetIdentity();
		m_addTransformation.SetIdentity();
	};

	~CAttachmentFACE() {};

	virtual void AddRef()
	{
		++m_nRefCounter;
	}

	virtual void Release()
	{
		if (--m_nRefCounter == 0)
			delete this;
	}

	virtual uint32             GetType() const                             { return CA_FACE; }
	virtual uint32             SetJointName(const char* szJointName)       { return 0; }

	virtual const char*        GetName() const                             { return m_strSocketName; };
	virtual uint32             GetNameCRC() const                          { return m_nSocketCRC32; }
	virtual uint32             ReName(const char* szJointName, uint32 crc) { m_strSocketName.clear();  m_strSocketName = szJointName; m_nSocketCRC32 = crc;  return 1; };

	virtual uint32             GetFlags() const                            { return m_AttFlags; }
	virtual void               SetFlags(uint32 flags)                      { m_AttFlags = flags; }

	virtual uint32             Immediate_AddBinding(IAttachmentObject* pIAttachmentObject, ISkin* pISkin = 0, uint32 nLoadingFlags = 0);
	virtual void               Immediate_ClearBinding(uint32 nLoadingFlags = 0);
	virtual uint32             Immediate_SwapBinding(IAttachment* pNewAttachment);

	virtual IAttachmentObject* GetIAttachmentObject() const { return m_pIAttachmentObject;  }
	virtual IAttachmentSkin*   GetIAttachmentSkin()         { return 0; }

	virtual void               HideAttachment(uint32 x);
	virtual uint32             IsAttachmentHidden() const             { return m_AttFlags & FLAGS_ATTACH_HIDE_MAIN_PASS; }
	virtual void               HideInRecursion(uint32 x);
	virtual uint32             IsAttachmentHiddenInRecursion() const  { return m_AttFlags & FLAGS_ATTACH_HIDE_RECURSION; }
	virtual void               HideInShadow(uint32 x);
	virtual uint32             IsAttachmentHiddenInShadow() const     { return m_AttFlags & FLAGS_ATTACH_HIDE_SHADOW_PASS; }

	virtual void               SetAttAbsoluteDefault(const QuatT& qt) { m_AttAbsoluteDefault = qt;  m_AttFlags &= (~FLAGS_ATTACH_PROJECTED);  assert(m_AttAbsoluteDefault.q.IsValid()); };
	virtual const QuatT&      GetAttAbsoluteDefault() const          { return m_AttAbsoluteDefault;  };
	virtual void              SetAttRelativeDefault(const QuatT& qt) { m_AttRelativeDefault = qt; m_AttFlags |= FLAGS_ATTACH_PROJECTED;  assert(m_AttRelativeDefault.q.IsValid()); };
	virtual const QuatT&      GetAttRelativeDefault() const          { return m_AttRelativeDefault; };
	virtual const QuatT&      GetAttModelRelative() const            { return m_AttModelRelative;  };//this is relative to the animated bone
	virtual const QuatTS      GetAttWorldAbsolute() const;
	virtual const QuatT&      GetAdditionalTransformation() const    { return m_addTransformation; }
	virtual void              UpdateAttModelRelative();

	virtual uint32            GetJointID() const     { return -1; };

	virtual void              AlignJointAttachment() {};

	virtual SimulationParams& GetSimulationParams()  { return m_Simulation;  };
	virtual void              PostUpdateSimulationParams(bool bAttachmentSortingRequired, const char* pJointName = 0);

	uint32                    ProjectAttachment(const Skeleton::CPoseData* pPoseData);
	void                      ComputeTriMat();
	void                      Update_Empty(Skeleton::CPoseData& pPoseData);
	void                      Update_Static(Skeleton::CPoseData& pPoseData);
	void                      Update_Execute(Skeleton::CPoseData& pPoseData);

	virtual void              Serialize(TSerialize ser);
	virtual size_t            SizeOfThis() const;
	virtual void              GetMemoryUsage(ICrySizer* pSizer) const;

	QuatT       m_AttRelativeDefault; //attachment location relative to the facematrix/bonematrix in default pose;
	QuatT       m_AttAbsoluteDefault; //attachment location relative to the model when character is in default-pose
	QuatT       m_AttModelRelative;   //the position and orientation of the attachment in animated relative model-space (=relative to Vec3(0,0,0))
	QuatT       m_addTransformation;  //additional rotation applied during rendering.
	ClosestTri  m_Triangle;
	CSimulation m_Simulation;
protected:
	void ClearBinding_Internal(bool release);
};
