// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/ModelCompiler.h"
#include "Core/Model.h"

namespace Designer
{
enum EDesignerTool;

template<class T>
class DesignerBaseObject : public T
{
public:

	DesignerBaseObject()
	{
		m_pModel = new Model;
	}

	void                   Done() override;

	void                   SetCompiler(ModelCompiler* pCompiler);
	virtual ModelCompiler* GetCompiler() const;

	MainContext            GetMainContext() const { return MainContext((CBaseObject*)this, GetCompiler(), GetModel()); }

	void                   SetModel(Model* pModel);
	Model*                 GetModel() const { return m_pModel; }

	void                   UpdateEngineNode();
	IStatObj*              GetIStatObj() override;

	virtual void           UpdateGameResource() {};

	bool                   QueryNearestPos(const BrushVec3& worldPos, BrushVec3& outPos) const;
	void                   DrawDimensions(DisplayContext& dc, AABB* pMergedBoundBox) override {}

	void                               Validate() override;
	virtual void                       UpdateHighlightPassState(bool bSelected, bool bHighlighted) override;

	bool                               IsEmpty() const { return m_pModel->IsEmpty(eShelf_Base); }

	virtual void                       UpdateVisibility(bool visible) override;
	virtual bool                       IsHiddenByOption() { return false; }
	virtual std::vector<EDesignerTool> GetIncompatibleSubtools() = 0;

protected:
	void UpdateHiddenIStatObjState();

	_smart_ptr<Model>                 m_pModel;
	mutable _smart_ptr<ModelCompiler> m_pCompiler;

};
}

#include "DesignerBaseObject_Impl.h"

