// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GAME_VOLUMES_MANAGER_H_
#define _GAME_VOLUMES_MANAGER_H_

#pragma once

#include <CryGame/IGameVolumes.h>

class CGameVolumesManager : public IGameVolumes, IGameVolumesEdit
{

private:

	typedef std::vector<Vec3> Vertices;
	struct EntityVolume
	{
		EntityVolume()
			: entityId(0)
			, height(0.0f)
			, closed(false)
		{
		}

		bool operator==(const EntityId& id) const
		{
			return entityId == id;
		}

		EntityId entityId;
		f32      height;
		bool     closed;
		Vertices vertices;
	};

	typedef std::vector<EntityVolume>   TEntityVolumes;
	typedef std::vector<IEntityClass*>  TVolumeClasses;
	typedef VectorMap<EntityId, uint32> TEntityToIndexMap;

public:
	CGameVolumesManager();
	virtual ~CGameVolumesManager();

	// IGameVolumes
	virtual IGameVolumesEdit* GetEditorInterface();
	virtual bool              GetVolumeInfoForEntity(EntityId entityId, VolumeInfo* pOutInfo) const;
	virtual void              Load(const char* fileName);
	virtual void              Reset();
	// ~IGameVolumes

	// IGameVolumesEdit
	virtual void        SetVolume(EntityId entityId, const IGameVolumes::VolumeInfo& volumeInfo);
	virtual void        DestroyVolume(EntityId entityId);

	virtual void        RegisterEntityClass(const char* className);
	virtual size_t      GetVolumeClassesCount() const;
	virtual const char* GetVolumeClass(size_t index) const;

	virtual void        Export(const char* fileName) const;
	// ~IGameVolumesEdit

private:
	void RebuildIndex();

private:
	TEntityToIndexMap m_entityToIndexMap; // Level memory
	TEntityVolumes    m_volumesData; // Level memory
	TVolumeClasses    m_classes;     // Global memory, initialized at start-up

	const static uint32 GAME_VOLUMES_FILE_VERSION = 2;
};

#endif
