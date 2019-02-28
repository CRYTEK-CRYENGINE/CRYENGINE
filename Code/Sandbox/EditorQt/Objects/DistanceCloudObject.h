// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _DISTANCECLOUDOBJECT_H_
#define _DISTANCECLOUDOBJECT_H_

#pragma once

#include "Objects/BaseObject.h"

struct IDistanceCloudRenderNode;

class CDistanceCloudObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CDistanceCloudObject)

	// CBaseObject overrides
	virtual void         Display(CObjectRenderHelper& objRenderHelper);

	virtual bool         HitTest(HitContext& hc);

	virtual void         GetLocalBounds(AABB& box);
	virtual void         SetHidden(bool bHidden);
	virtual void         UpdateVisibility(bool visible);
	virtual void         RegisterOnEngine() override;
	virtual void         UnRegisterFromEngine() override;
	virtual void         SetMaterial(IEditorMaterial* mtl);
	virtual void         Serialize(CObjectArchive& ar);
	virtual XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);
	virtual IRenderNode* GetEngineNode() const { return m_pRenderNode; };
	virtual int          MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);

	void                 UpdateEngineNode();

protected:
	// CBaseObject overrides
	virtual bool Init(CBaseObject* prev, const string& file);
	virtual bool CreateGameObject();
	virtual void Done();
	virtual void DeleteThis() { delete this; };
	virtual void InvalidateTM(int nWhyFlags);

private:
	CDistanceCloudObject();
	void       OnProjectionTypeChange(IVariable* pVar);
	CMaterial* GetDefaultMaterial() const;

private:
	//CVariable< int > m_projectionType;
	IDistanceCloudRenderNode* m_pRenderNode;
	IDistanceCloudRenderNode* m_pPrevRenderNode;
	int                       m_renderFlags;
};

class CDistanceCloudObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()     { return OBJTYPE_DISTANCECLOUD; };
	const char*         ClassName()         { return "DistanceCloud"; };
	const char*         Category()          { return "Misc"; };
	CRuntimeClass*      GetRuntimeClass()   { return RUNTIME_CLASS(CDistanceCloudObject); };
	const char*         GetFileSpec()       { return ""; };
	virtual const char* GetTextureIcon()    { return "%EDITOR%/ObjectIcons/Clouds.bmp"; };
};

#endif // _DISTANCECLOUDOBJECT_H_

