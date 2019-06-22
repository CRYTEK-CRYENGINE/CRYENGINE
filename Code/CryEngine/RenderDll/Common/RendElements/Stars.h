// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CStars
{
friend class CSceneForwardStage;

public:
	CStars();
	~CStars();

	void Render(bool bUseMoon);

private:
	bool LoadData();

private:
	uint32                  m_numStars;
	_smart_ptr<IRenderMesh> m_pStarMesh;
	CShader*                m_pShader;
};
