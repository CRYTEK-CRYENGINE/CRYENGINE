// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CWaterRippleManager final
{
public:
	static int32 OnEventPhysCollision(const EventPhys* pEvent);

public:
	CWaterRippleManager();
	~CWaterRippleManager();

	void GetMemoryUsage(ICrySizer* pSizer) const;

	void Initialize();

	void Finalize();

	void OnFrameStart();

	void Render(const SRenderingPassInfo& passInfo);

	void AddWaterRipple(const Vec3 position, float scale, float strength);

private:
	std::vector<SWaterRippleInfo> m_waterRippleInfos;
};
