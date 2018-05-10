// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Skeleton.h"

class CAttachmentManager;

struct SAttachmentBase : public IAttachment
{
	SAttachmentBase()
		: m_AttFlags(0)
		, m_nSocketCRC32(0)
		, m_nRefCounter(0)
		, m_pIAttachmentObject(nullptr)
		, m_pAttachmentManager(nullptr)
	{
	}

	// these functions will queue commands in the attachment manager
	// and they are exposed in the interface, since they are safe
	// to call from the main thread at any time
	virtual void AddBinding(IAttachmentObject* pModel, ISkin* pISkin = 0, uint32 nLoadingFlags = 0) override;
	virtual void ClearBinding(uint32 nLoadingFlags = 0) override;
	virtual void SwapBinding(IAttachment* pNewAttachment) override;

	// these are the actual implementations and will get overloaded by the actual classes
	virtual uint32 Immediate_AddBinding(IAttachmentObject* pModel, ISkin* pISkin = 0, uint32 nLoadingFlags = 0) = 0;
	virtual void   Immediate_ClearBinding(uint32 nLoadingFlags = 0) = 0;
	virtual uint32 Immediate_SwapBinding(IAttachment* pNewAttachment) = 0;

	//shared members
	uint32              m_AttFlags;
	string              m_strSocketName;
	uint32              m_nSocketCRC32;
	int32               m_nRefCounter;
	IAttachmentObject*  m_pIAttachmentObject;
	CAttachmentManager* m_pAttachmentManager;
};
