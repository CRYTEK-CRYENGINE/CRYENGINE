// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __tagcomment_h__
#define __tagcomment_h__
#pragma once

#include "EntityObject.h"

/*!
 *	CTagComment is an object that represent text commentary added to named 3d position in world.
 *
 */
class CTagComment : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CTagComment)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CEntityObject.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitVariables() {}
	bool         IsScalable() const override { return false; }
	virtual void Serialize(CObjectArchive& ar);

protected:
	//! Ctor must be protected.
	CTagComment();
};

/*!
 * Class Description of CTagComment.
 */
class CTagCommentClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_TAGPOINT; };
	const char*    ClassName()       { return "Comment"; };
	const char*    Category()        { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTagComment); };
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("Comment") != nullptr; }
};

#endif // __tagcomment_h__

