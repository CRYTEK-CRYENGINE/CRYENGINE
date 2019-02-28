// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
class Model;

enum ECompilerFlag
{
	eCompiler_CastShadow  = BIT(1),
	eCompiler_Physicalize = BIT(2),
	eCompiler_General     = eCompiler_CastShadow | eCompiler_Physicalize
};

//! This class plays a role of creating engine resources used for rendering and physicalizing out of the Designer::Model instance.
class ModelCompiler : public _i_reference_target_t
{
public:
	ModelCompiler(int nCompilerFlag);
	ModelCompiler(const ModelCompiler& compiler);
	virtual ~ModelCompiler();

	bool         IsValid() const;

	void         Compile(CBaseObject* pBaseObject, Model* pModel, ShelfID shelfID = eShelf_Any, bool bUpdateOnlyRenderNode = false);

	void         DeleteAllRenderNodes();
	void         DeleteRenderNode(ShelfID shelfID);
	IRenderNode* GetRenderNode() { return m_pRenderNode[0]; }
	void         UpdateHighlightPassState(bool bSelected, bool bHighlighted);
	bool         GetIStatObj(_smart_ptr<IStatObj>* pStatObj);

	bool         CreateIndexdMesh(Model* pModel, IIndexedMesh* pMesh, bool bCreateBackFaces);
	void         SaveToCgf(const char* filename);

	void         SetViewDistRatio(int nViewDistRatio) { m_viewDistRatio = nViewDistRatio; }
	int          GetViewDistRatio() const             { return m_viewDistRatio; }

	void         SetRenderFlags(int nRenderFlag)      { m_RenderFlags = nRenderFlag; }
	int          GetRenderFlags() const               { return m_RenderFlags; }

	void         SetStaticObjFlags(int nStaticObjFlag);
	int          GetStaticObjFlags();

	void         AddFlags(int nFlags)         { m_nCompilerFlag |= nFlags; }
	void         RemoveFlags(int nFlags)      { m_nCompilerFlag &= (~nFlags); }
	bool         CheckFlags(int nFlags) const { return (m_nCompilerFlag & nFlags) ? true : false; }

	void         SaveMesh(CArchive& ar, CBaseObject* pObj, Model* pModel);
	bool         LoadMesh(CArchive& ar, CBaseObject* pObj, Model* pModel);
	bool         SaveMesh(int nVersion, std::vector<char>& buffer, CBaseObject* pObj, Model* pModel);
	bool         LoadMesh(int nVersion, std::vector<char>& buffer, CBaseObject* pObj, Model* pModel);

private:

	bool       UpdateMesh(CBaseObject* pBaseObject, Model* pModel, ShelfID nShelf);
	void       UpdateRenderNode(CBaseObject* pBaseObject, ShelfID nShelf);

	void       RemoveStatObj(ShelfID nShelf);
	void       CreateStatObj(ShelfID nShelf);
	IMaterial* GetMaterialFromBaseObj(CBaseObject* pObj) const;
	void       InvalidateStatObj(IStatObj* pStatObj, bool bPhysics);

private:

	mutable IStatObj*    m_pStatObj[cShelfMax];
	mutable IRenderNode* m_pRenderNode[cShelfMax];

	int                  m_RenderFlags;
	int                  m_viewDistRatio;
	int                  m_nCompilerFlag;
};
}

