// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MaterialFXGraphMan_H__
#define __MaterialFXGraphMan_H__

#include "LevelIndependentFileMan.h"
#include "HyperGraph/HyperGraph.h"

class CMaterialFXGraphMan : public ILevelIndependentFileModule
{
public:
	CMaterialFXGraphMan();
	~CMaterialFXGraphMan();

	void Init();
	void ReloadFXGraphs();

	void ClearEditorGraphs();
	void SaveChangedGraphs();
	bool HasModifications();

	bool NewMaterialFx(CString& filename, CHyperGraph** pHyperGraph = NULL);

	//ILevelIndependentFileModule
	virtual bool PromptChanges();
	//~ILevelIndependentFileModule

private:
	typedef std::list<IFlowGraphPtr> TGraphList;
	TGraphList m_matFxGraphs;
};

#endif

