// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>

#include <vector>

enum class ESceneElementType : int;

class CScene;

class CSceneElementCommon
{
public:
	CSceneElementCommon(CScene* pScene, int id);
	virtual ~CSceneElementCommon() {}

	int                    GetId() const;
	CSceneElementCommon*   GetParent() const;
	CSceneElementCommon*   GetChild(int index) const;
	int                    GetNumChildren() const;
	bool                   IsLeaf() const;
	string                GetName() const;
	int                    GetSiblingIndex() const;
	CScene*     GetScene();

	void                   SetName(const string& name);
	void                   SetName(const char* szName);
	void                   SetSiblingIndex(int index);

	void                   AddChild(CSceneElementCommon* pChild);
	CSceneElementCommon*   RemoveChild(int index);

	virtual ESceneElementType GetType() const = 0;

	static void MakeRoot(CSceneElementCommon* pSceneElement);  // Remove element from its parent.
private:
	string                           m_name;

	std::vector<CSceneElementCommon*> m_children;
	CSceneElementCommon*              m_pParent;
	int                               m_siblingIndex;

	CScene*                m_pScene;
	int                               m_id;
};

