// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AttachmentSkin.h"

class CAttachmentMerger;

class CAttachmentMerged : public _reference_target_t
{
	friend class CAttachmentMerger;

public:
	struct MergeInfo
	{
		_smart_ptr<IAttachment> pAttachment;
		IAttachmentObject*      pAttachmentObject;

		struct
		{
			uint32 nFirstIndex;
			uint32 nIndexCount;
		} IndexInfo[MAX_STATOBJ_LODS_NUM];
	};

	CAttachmentMerged(string strName, CAttachmentManager* pAttachmentManager);
	virtual ~CAttachmentMerged();

	void Invalidate();
	bool AreAttachmentBindingsValid();

	void HideMergedAttachment(const IAttachment* pAttachment, bool bHide);
	bool HasAttachment(const IAttachment* pAttachment) const;
	void AddAttachments(const DynArray<MergeInfo>& attachmentInfos);

	void DrawAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo, const Matrix34& rWorldMat34, f32 fZoomFactor = 1);

private:
	DynArray<MergeInfo>::iterator       FindAttachment(IAttachment* pAttachment);
	DynArray<MergeInfo>::const_iterator FindAttachment(const IAttachment* pAttachment) const;

	DynArray<MergeInfo>         m_MergedAttachments;
	DynArray<vtx_idx>           m_MergedAttachmentIndices[MAX_STATOBJ_LODS_NUM];
	bool                        m_bRequiresIndexUpdate;

	_smart_ptr<CAttachmentSKIN> m_pMergedSkinAttachment;
};
