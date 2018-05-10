// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Model.h"
#include "Util/ElementSet.h"
#include "ToolFactory.h"

class CBaseObject;

namespace Designer
{

class Model;
class ModelCompiler;

class DesignerUndo : public IUndoObject
{
public:
	DesignerUndo() : m_Tool(eDesigner_Invalid) {}
	DesignerUndo(CBaseObject* pObj, const Model* pModel, const char* undoDescription = NULL);

	static bool           IsAKindOfDesignerTool(CEditTool* pEditor);
	static Model*         GetModel(const CryGUID& objGUID);
	static ModelCompiler* GetCompiler(const CryGUID& objGUID);
	static CBaseObject*   GetBaseObject(const CryGUID& objGUID);
	static MainContext    GetMainContext(const CryGUID& objGUID);
	static void           RestoreEditTool(Model* pModel, CryGUID objGUID, EDesignerTool tool);

protected:

	const char* GetDescription()
	{
		return m_undoDescription;
	};

	void SetDescription(const char* description)
	{
		if (description)
			m_undoDescription = description;
	}

	MainContext GetMainContext() const
	{
		return GetMainContext(m_ObjGUID);
	}

	void RestoreEditTool(Model* pModel)
	{
		RestoreEditTool(pModel, m_ObjGUID, m_Tool);
	}

	virtual void Undo(bool bUndo);
	virtual void Redo();

	void         StoreEditorTool();
	void         SwitchTool(EDesignerTool tool) { m_Tool = tool; }
	void         SetObjGUID(CryGUID guid)       { m_ObjGUID = guid; }
private:
	string           m_undoDescription;
	CryGUID           m_ObjGUID;
	EDesignerTool     m_Tool;
	_smart_ptr<Model> m_undo;
	Matrix34          m_UndoWorldTM;
	_smart_ptr<Model> m_redo;
	Matrix34          m_RedoWorldTM;
};

class UndoSelection : public DesignerUndo
{
public:

	UndoSelection(ElementSet& selectionContext, CBaseObject* pObj, bool bRetoreAfterUndo, const char* undoDescription = NULL);

	void Undo(bool bUndo);
	void Redo();

private:

	void CopyElements(ElementSet& sourceElements, ElementSet& destElements);
	void ReplacePolygonsWithExistingPolygonsInModel(ElementSet& elements);

	bool       m_bRestoreToolAfterUndo;

	ElementSet m_SelectionContextForUndo;
	ElementSet m_SelectionContextForRedo;

};

class UndoTextureMapping : public IUndoObject
{
public:
	UndoTextureMapping(){}
	UndoTextureMapping(CBaseObject* pObj, const char* undoDescription = NULL) : m_ObjGUID(pObj->GetId())
	{
		SetDescription(undoDescription);
		SaveDesignerTexInfoContext(m_UndoContext);
	}

protected:
	const char* GetDescription()
	{
		return m_undoDescription;
	};

	void SetDescription(const char* description)
	{
		if (description)
			m_undoDescription = description;
	}

	void Undo(bool bUndo);
	void Redo();
	void SetObjGUID(CryGUID guid) { m_ObjGUID = guid; }

private:

	struct ContextInfo
	{
		CryGUID m_PolygonGUID;
		TexInfo m_TexInfo;
		int     m_MatID;
	};

	void RestoreTexInfo(const std::vector<ContextInfo>& contextList);
	void SaveDesignerTexInfoContext(std::vector<ContextInfo>& contextList);

	string                  m_undoDescription;
	CryGUID                  m_ObjGUID;

	std::vector<ContextInfo> m_UndoContext;
	std::vector<ContextInfo> m_RedoContext;
};

class UndoSubdivision : public IUndoObject
{
public:
	UndoSubdivision(){}
	UndoSubdivision(const MainContext& mc);

	void        Undo(bool bUndo) override;
	void        Redo() override;
	const char* GetDescription() override { return "Designer Subdivision"; }

private:

	void UpdatePanel();

	CryGUID m_ObjGUID;
	int     m_SubdivisionLevelForUndo;
	int     m_SubdivisionLevelForRedo;
};

}

