// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INCLUDE_SANDBOX_CLIPVOLUMEOBJECT_H
#define __INCLUDE_SANDBOX_CLIPVOLUMEOBJECT_H

#pragma once

#include "EntityObject.h"
#include "Brush/BrushUtil.h"
#include "Brush/DesignerBaseObject.h"

class CClipVolumeObjectPanel;

class CClipVolumeObject : public CDesignerBaseObject<CEntityObject>
{
public:
	DECLARE_DYNCREATE(CClipVolumeObject)
	CClipVolumeObject();
	virtual ~CClipVolumeObject();

	virtual void Display(DisplayContext& dc);

	virtual bool Init(IEditor* ie, CBaseObject* prev, const CString& file) override;
	virtual void InitVariables() override {}
	virtual void Serialize(CObjectArchive& ar) override;

	virtual bool CreateGameObject() override;
	virtual void Done() override;
	virtual void EntityLinked(const CString& name, GUID targetEntityId);

	virtual void GetLocalBounds(AABB& box) override;
	virtual bool HitTest(HitContext& hc) override;

	void         BeginEditParams(IEditor* ie, int flags);
	void         EndEditParams(IEditor* ie);

	virtual void UpdateGameResource() override;
	CString      GenerateGameFilename();

private:
	void              UpdateCollisionData();
	IVariable*        GetLinkedLightClipVolumeVar(GUID targetEntityId) const;

	virtual IStatObj* GetIStatObj() override;
	virtual void      DeleteThis() override { delete this; }

	CVariable<bool>                mv_filled;
	std::vector<Lineseg>           m_MeshEdges;

	int                            m_nUniqFileIdForExport;
	static CClipVolumeObjectPanel* m_pEditClipVolumePanel;
	static int                     s_nEditClipVolumeRollUpID;
	static int                     s_nGlobalFileClipVolumeID;

};

/*!
 * Class Description of EnvironmentProbeObject.
 */
class CClipVolumeObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {C91100AA-1DBF-4410-B25A-0325853480C4}
		static const GUID guid = { 0xc91100aa, 0x1dbf, 0x4410, { 0xb2, 0x5a, 0x3, 0x25, 0x85, 0x34, 0x80, 0xc4 } };
		return guid;
	}
	ObjectType     GetObjectType()            { return OBJTYPE_SOLID; };
	const char*    ClassName()                { return "ClipVolume"; };
	const char*    Category()                 { return "Area"; };
	CRuntimeClass* GetRuntimeClass()          { return RUNTIME_CLASS(CClipVolumeObject); };
	int            GameCreationOrder()        { return 52; };
	const char*    GetTextureIcon()           { return "Editor/ObjectIcons/bird.bmp"; };
	virtual bool   RenderTextureOnTop() const { return true; }
};

#endif //__INCLUDE_SANDBOX_CLIPVOLUMEOBJECT_H

