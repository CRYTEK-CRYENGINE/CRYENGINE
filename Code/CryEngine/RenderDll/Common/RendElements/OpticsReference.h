// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IFlares.h>

class COpticsReference : public IOpticsElementBase
{
public:

#if defined(FLARES_SUPPORT_EDITING)
	DynArray<FuncVariableGroup> GetEditorParamGroups();
#endif

	COpticsReference(const char* name);
	~COpticsReference(){}

	EFlareType          GetType() override                    { return eFT_Reference; }
	bool                IsGroup() const override              { return false; }

	const char*         GetName() const override              { return m_name.c_str();  }
	void                SetName(const char* ch_name) override { m_name = ch_name; }
	void                Load(IXmlNode* pNode) override;

	IOpticsElementBase* GetParent() const override            { return NULL;  }

	bool                IsEnabled() const override            { return true;  }
	void                SetEnabled(bool b) override           {}
	void                AddElement(IOpticsElementBase* pElement) override;
	void                InsertElement(int nPos, IOpticsElementBase* pElement) override;
	void                Remove(int i) override;
	void                RemoveAll() override;
	int                 GetElementCount() const override;
	IOpticsElementBase* GetElementAt(int i) const override;

	void                GetMemoryUsage(ICrySizer* pSizer) const override;
	void                Invalidate() override;

	void                RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos) override;
	void                DeleteThis() override;

public:
	string                             m_name;
	std::vector<IOpticsElementBasePtr> m_OpticsList;
};
