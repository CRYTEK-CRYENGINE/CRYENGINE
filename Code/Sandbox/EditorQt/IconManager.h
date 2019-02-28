// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IStatObj;
struct IMaterial;

#include "IIconManager.h"

/*!
 *	CIconManager contains map of icon names to icon textures,
 *	Ensuring that only one instance of texture for specified icon will be allocated.
 *	Also release textures when editor exit.
 *	Note: This seems like it could be improved, why does the icon manager have to store objects?
 */
class CIconManager : public IIconManager, public IAutoEditorNotifyListener
{
public:
	// Construction
	CIconManager();
	~CIconManager();

	void Init();
	void Done();

	// Unload all loaded resources.
	void Reset();

	// Operations
	virtual int        GetIconTexture(EIcon icon);

	virtual IStatObj*  GetObject(EStatObject object);
	virtual int        GetIconTexture(const char* szIconName);
	virtual int        GetIconTexture(const char* szIconName, CryIcon& icon);

	virtual IMaterial* GetHelperMaterial();

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	virtual void OnNewDocument()   { Reset(); };
	virtual void OnLoadDocument()  { Reset(); };
	virtual void OnCloseDocument() { Reset(); };
	virtual void OnMissionChange() { Reset(); };
	//////////////////////////////////////////////////////////////////////////

private:
	std::unordered_map<string, int, stl::hash_strcmp<string>>  m_textures;

	_smart_ptr<IMaterial> m_pHelperMtl;

	IStatObj*             m_objects[eStatObject_COUNT];
	int                   m_icons[eIcon_COUNT];

	//////////////////////////////////////////////////////////////////////////
	// Icons bitmaps.
	//////////////////////////////////////////////////////////////////////////
	typedef std::map<string, CBitmap*> IconsMap;
	IconsMap m_iconBitmapsMap;
};

