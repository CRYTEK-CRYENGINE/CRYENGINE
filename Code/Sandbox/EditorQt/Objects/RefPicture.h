// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __REFPICTURE_H__
#define __REFPICTURE_H__

#pragma once

#include "Objects/BaseObject.h"

class CMaterial;
class CEdMesh;

//////////////////////////////////////////////////////////////////////////
class CRefPicture : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CRefPicture)

	CRefPicture();

	void Display(CObjectRenderHelper& objRenderHelper);
	bool HitTest(HitContext& hc);
	void          GetLocalBounds(AABB& box);

	virtual float GetCreationOffsetFromTerrain() const override { return 0.f; }

	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);

	virtual void SerializeGeneralProperties(Serialization::IArchive& ar, bool bMultiEdit) override;
	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:
	bool CreateGameObject();
	void Done();
	void DeleteThis() { delete this; };
	void InvalidateTM(int nWhyFlags);
	void OnVariableChanged(IVariable* piVariable);
	void UpdateImage(const string& picturePath);
	void ApplyScale(bool bHeight = true);

	CVariable<string> mv_picFilepath;
	CVariable<bool>    mv_scale;

private:
	_smart_ptr<IMaterial> m_pMaterial;
	IRenderNode*          m_pRenderNode;
	_smart_ptr<CEdMesh>   m_pGeometry;

	float                 m_aspectRatio;
	AABB                  m_bbox;
	Matrix34              m_invertTM;
};

//////////////////////////////////////////////////////////////////////////
class CRefPictureClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_REFPICTURE; };

	const char*    ClassName()       { return "ReferencePicture"; }

	const char*    Category()        { return "Misc"; }

	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CRefPicture); }

};

#endif//__REFPICTURE_H__

