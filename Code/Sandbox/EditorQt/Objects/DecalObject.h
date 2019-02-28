// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _DECALOBJECT_H_
#define _DECALOBJECT_H_

#pragma once

#include "Objects/BaseObject.h"

class CEdMesh;
struct IDecalRenderNode;

class CDecalObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CDecalObject)

	// CBaseObject overrides
	virtual void         Display(CObjectRenderHelper& objRenderHelper);

	virtual bool         HitTest(HitContext& hc);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	virtual int          MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void         GetLocalBounds(AABB& box);
	virtual void         SetHidden(bool bHidden);
	virtual void         UpdateVisibility(bool visible);
	virtual void         SetMaterial(IEditorMaterial* mtl);
	virtual void         Serialize(CObjectArchive& ar);
	virtual XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);
	virtual void         SetMinSpec(uint32 nSpec, bool bSetChildren = true);
	virtual void         SetMaterialLayersMask(uint32 nLayersMask);
	virtual IRenderNode* GetEngineNode() const { return m_pRenderNode; };

	// special input handler (to be reused by this class as well as the decal object tool)
	void MouseCallbackImpl(CViewport* view, EMouseEvent event, CPoint& point, int flags, bool callerIsMouseCreateCallback = false);

	void UpdateProjection();
	int  GetProjectionType() const;
	void UpdateEngineNode();

protected:
	// CBaseObject overrides
	virtual bool Init(CBaseObject* prev, const string& file);
	virtual bool CreateGameObject();
	virtual void Done();
	virtual void DeleteThis() { delete this; };
	virtual void InvalidateTM(int nWhyFlags);

private:
	CDecalObject();
	bool       RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt);
	void       OnFileChange(string filename);
	void       OnParamChanged(IVariable* pVar);
	void       OnViewDistRatioChanged(IVariable* pVar);
	CMaterial* GetDefaultMaterial() const;

private:
	int                m_renderFlags;
	CVariable<int>     m_viewDistRatio;
	CVariableEnum<int> m_projectionType;
	CVariable<bool>    m_deferredDecal;
	CVariable<int>     m_sortPrio;
	CVariable<float>   m_depth;
	IDecalRenderNode*  m_pRenderNode;
	IDecalRenderNode*  m_pPrevRenderNode;
};

class CDecalObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()     { return OBJTYPE_DECAL; };
	const char*         ClassName()         { return "Decal"; };
	const char*         Category()          { return "Misc"; };
	CRuntimeClass*      GetRuntimeClass()   { return RUNTIME_CLASS(CDecalObject); };
	const char*         GetFileSpec()       { return ""; };
	virtual const char* GetTextureIcon()    { return "%EDITOR%/ObjectIcons/Decal.bmp"; };
};

#endif // _DECALOBJECT_H_

