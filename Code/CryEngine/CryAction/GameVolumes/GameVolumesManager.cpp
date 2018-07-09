// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameVolumesManager.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

CGameVolumesManager::CGameVolumesManager()
{

}

CGameVolumesManager::~CGameVolumesManager()
{

}

IGameVolumesEdit* CGameVolumesManager::GetEditorInterface()
{
	return gEnv->IsEditor() ? this : NULL;
}

bool CGameVolumesManager::GetVolumeInfoForEntity(EntityId entityId, IGameVolumes::VolumeInfo* pOutInfo) const
{

	TEntityToIndexMap::const_iterator indexIt = m_entityToIndexMap.find(entityId);
	if (indexIt != m_entityToIndexMap.end())
	{
		const EntityVolume& entityVolume = m_volumesData[indexIt->second];

		if (!entityVolume.vertices.empty())
		{
			pOutInfo->volumeHeight = entityVolume.height;
			pOutInfo->closed = entityVolume.closed;
			pOutInfo->verticesCount = entityVolume.vertices.size();
			pOutInfo->pVertices = &entityVolume.vertices[0];

			return true;
		}
	}

	return false;
}

void CGameVolumesManager::Load(const char* fileName)
{
	//////////////////////////////////////////////////////////////////////////
	/// Free any left data (it should be empty though...)
	Reset();

	//////////////////////////////////////////////////////////////////////////
	/// No need to load in editor
	/// The saved entities will restore the data inside the manager
	if (gEnv->IsEditor())
		return;

	CCryFile file;
	if (false != file.Open(fileName, "rb"))
	{
		uint32 nFileVersion = GAME_VOLUMES_FILE_VERSION;
		file.ReadType(&nFileVersion);

		// Verify version...
		if ((nFileVersion >= 1) && (nFileVersion <= GAME_VOLUMES_FILE_VERSION))
		{
			const uint32 maxVertices = 512;
			Vec3 readVertexBuffer[maxVertices];

			// Read volumes
			uint32 nVolumeCount = 0;
			file.ReadType(&nVolumeCount);

			m_volumesData.resize(nVolumeCount);

			for (uint32 i = 0; i < nVolumeCount; ++i)
			{
				EntityVolume& volumeInfo = m_volumesData[i];

				uint32 nVertexCount = 0;
				uint32 nEntityId = 0;
				f32 fHeight = 0;
				bool bClosed = false;

				file.ReadType(&nEntityId);
				file.ReadType(&fHeight);
				if (nFileVersion > 1)
				{
					file.ReadType(&bClosed);
				}
				file.ReadType(&nVertexCount);

				volumeInfo.entityId = (EntityId)nEntityId;
				volumeInfo.height = fHeight;
				volumeInfo.closed = bClosed;
				volumeInfo.vertices.resize(nVertexCount);
				if (nVertexCount > 0)
				{
					file.ReadType(&readVertexBuffer[0], nVertexCount);

					for (uint32 v = 0; v < nVertexCount; ++v)
					{
						volumeInfo.vertices[v] = readVertexBuffer[v];
					}
				}
			}
		}
		else
		{
			GameWarning("GameVolumesManger:Load - Failed to load file '%s'. Version mis-match, try to re-export your level", fileName);
		}

		file.Close();
	}
	RebuildIndex();
}

void CGameVolumesManager::Reset()
{
	stl::free_container(m_entityToIndexMap);
	stl::free_container(m_volumesData);
}

void CGameVolumesManager::SetVolume(EntityId entityId, const IGameVolumes::VolumeInfo& volumeInfo)
{
	TEntityToIndexMap::iterator indexIt = m_entityToIndexMap.find(entityId);
	if (indexIt == m_entityToIndexMap.end())
	{
		m_entityToIndexMap[entityId] = static_cast<uint32>(m_volumesData.size());
		m_volumesData.push_back(EntityVolume());
		indexIt = --m_entityToIndexMap.end();
	}

	EntityVolume& entityVolume = m_volumesData[indexIt->second];
	entityVolume.entityId = entityId;
	entityVolume.height = volumeInfo.volumeHeight;
	entityVolume.closed = volumeInfo.closed;
	entityVolume.vertices.resize(volumeInfo.verticesCount);
	for (uint32 i = 0; i < volumeInfo.verticesCount; ++i)
	{
		entityVolume.vertices[i] = volumeInfo.pVertices[i];
	}
}

void CGameVolumesManager::DestroyVolume(EntityId entityId)
{
	stl::find_and_erase(m_volumesData, entityId);
	RebuildIndex(); // That's a bit costly, but it only happens in editor when a designer actually deletes a volume
}

void CGameVolumesManager::RegisterEntityClass(const char* className)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	if (pClass)
	{
		stl::push_back_unique(m_classes, pClass);
	}
}

size_t CGameVolumesManager::GetVolumeClassesCount() const
{
	return m_classes.size();
}

const char* CGameVolumesManager::GetVolumeClass(size_t index) const
{
	if (index < m_classes.size())
	{
		return m_classes[index]->GetName();
	}

	return NULL;
}

void CGameVolumesManager::Export(const char* fileName) const
{
	CCryFile file;
	if (false != file.Open(fileName, "wb"))
	{
		const uint32 maxVertices = 512;
		Vec3 writeVertexBuffer[maxVertices];

		// File version
		uint32 nFileVersion = GAME_VOLUMES_FILE_VERSION;
		file.Write(&nFileVersion, sizeof(nFileVersion));

		// Save volume info
		uint32 nVolumeCount = (uint32)m_volumesData.size();
		file.Write(&nVolumeCount, sizeof(nVolumeCount));

		for (uint32 i = 0; i < nVolumeCount; ++i)
		{
			const EntityVolume& volumeInfo = m_volumesData[i];

			CRY_ASSERT(volumeInfo.vertices.size() < maxVertices);

			uint32 nVertexCount = min((uint32)volumeInfo.vertices.size(), maxVertices);
			uint32 nEntityId = volumeInfo.entityId;
			f32 fHeight = volumeInfo.height;
			bool bClosed = volumeInfo.closed;

			file.Write(&nEntityId, sizeof(nEntityId));
			file.Write(&fHeight, sizeof(fHeight));
			if (nFileVersion > 1)
			{
				file.Write(&bClosed, sizeof(bClosed));
			}
			file.Write(&nVertexCount, sizeof(nVertexCount));

			if (nVertexCount > 0)
			{
				for (uint32 v = 0; v < nVertexCount; ++v)
				{
					writeVertexBuffer[v] = volumeInfo.vertices[v];
				}
				file.Write(&writeVertexBuffer[0], sizeof(writeVertexBuffer[0]) * nVertexCount);
			}
		}

		file.Close();
	}
}

void CGameVolumesManager::RebuildIndex()
{
	m_entityToIndexMap.clear();
	m_entityToIndexMap.reserve(m_volumesData.size());
	const uint32 count = static_cast<uint32>(m_volumesData.size());
	for (uint32 index = 0; index < count; ++index)
	{
		const EntityId entityId = m_volumesData[index].entityId;
		m_entityToIndexMap[entityId] = index;
	}
}
