// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CharAttachHelper_h__
#define __CharAttachHelper_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

//////////////////////////////////////////////////////////////////////////
class SANDBOX_API CCharacterAttachHelperObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CCharacterAttachHelperObject)

	CCharacterAttachHelperObject();

	//////////////////////////////////////////////////////////////////////////
	// CEntity
	//////////////////////////////////////////////////////////////////////////
	virtual void  InitVariables() {};
	virtual void  Display(CObjectRenderHelper& objRenderHelper);

	virtual void  SetHelperScale(float scale);
	virtual float GetHelperScale() { return m_charAttachHelperScale; };
	//////////////////////////////////////////////////////////////////////////
private:
	static float m_charAttachHelperScale;
};

/*!
 * Class Description of Entity
 */
class CCharacterAttachHelperObjectClassDesc : public CObjectClassDesc
{
public:
	virtual ObjectType     GetObjectType()     { return OBJTYPE_ENTITY; };
	virtual const char*    ClassName()         { return "CharAttachHelper"; };
	virtual const char*    Category()          { return "Misc"; };
	virtual CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CCharacterAttachHelperObject); };
	virtual const char*    GetTextureIcon()    { return "%EDITOR%/ObjectIcons/Magnet.bmp"; };
	virtual bool           IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("CharacterAttachHelper") != nullptr; }
};

#endif // __CharAttachHelper_h__

