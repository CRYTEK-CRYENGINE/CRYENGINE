// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsReference.h"

class COpticsProxy : public IOpticsElementBase
{
public:

	COpticsProxy(const char* name);
	~COpticsProxy(){}

	EFlareType          GetType() override                     { return eFT_Proxy; }
	bool                IsGroup() const override               { return false; }

	const char*         GetName() const override               { return m_name.c_str();  }
	void                SetName(const char* ch_name) override  { m_name = ch_name; }
	void                Load(IXmlNode* pNode) override;

	IOpticsElementBase* GetParent() const override                            { return NULL;  }

	bool                IsEnabled() const override                            { return m_bEnable; }
	void                SetEnabled(bool b) override                           { m_bEnable = b;    }

	void                AddElement(IOpticsElementBase* pElement) override               {}
	void                InsertElement(int nPos, IOpticsElementBase* pElement) override  {}
	void                Remove(int i) override                                          {}
	void                RemoveAll() override                                            {}
	int                 GetElementCount() const override                                { return 0; }
	IOpticsElementBase* GetElementAt(int i) const override                              { return NULL;  }

	void                GetMemoryUsage(ICrySizer* pSizer) const override;
	void                Invalidate() override;

	void                RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos) override;

	void                SetOpticsReference(IOpticsElementBase* pReference) override
	{
		if (pReference->GetType() == eFT_Reference)
			m_pOpticsReference = (COpticsReference*)pReference;
	}
	IOpticsElementBase* GetOpticsReference() const override
	{
		return m_pOpticsReference;
	}
#if defined(FLARES_SUPPORT_EDITING)
	DynArray<FuncVariableGroup> GetEditorParamGroups();
#endif

	void                DeleteThis() override;

public:

	bool                         m_bEnable;
	string                       m_name;
	_smart_ptr<COpticsReference> m_pOpticsReference;

};
