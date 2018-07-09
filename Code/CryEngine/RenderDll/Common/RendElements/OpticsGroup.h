// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"

class COpticsGroup : public COpticsElement
{

protected:
	std::vector<IOpticsElementBasePtr> children;
	void _init();

public:

	COpticsGroup(const char* name = "[Unnamed_Group]") : COpticsElement(name) { _init(); };
	COpticsGroup(const char* name, COpticsElement* elem, ...);
	virtual ~COpticsGroup(){}

	COpticsGroup&       Add(IOpticsElementBase* pElement);
	void                Remove(int i) override;
	void                RemoveAll() override;
	virtual int         GetElementCount() const override;
	IOpticsElementBase* GetElementAt(int i) const override;

	void                AddElement(IOpticsElementBase* pElement) override { Add(pElement);    }
	void                InsertElement(int nPos, IOpticsElementBase* pElement) override;
	void                SetElementAt(int i, IOpticsElementBase* elem);
	void                Invalidate() override;

	bool                IsGroup() const override { return true; }
	void                validateChildrenGlobalVars(const SAuxParams& aux);

	virtual void        GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual EFlareType  GetType() override { return eFT_Group; }
	virtual void        validateGlobalVars(const SAuxParams& aux) override;

	bool                PreparePrimitives(const COpticsElement::SPreparePrimitivesContext& context) override;


#if defined(FLARES_SUPPORT_EDITING)
	virtual void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
#endif

public:
	static COpticsGroup predef_simpleCamGhost;
	static COpticsGroup predef_cheapCamGhost;
	static COpticsGroup predef_multiGlassGhost;
};
