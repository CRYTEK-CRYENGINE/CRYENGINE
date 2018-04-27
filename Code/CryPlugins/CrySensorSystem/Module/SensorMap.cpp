// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SensorMap.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include "../Interface/ISensorTagLibrary.h"
#include "OctreePlotter.inl"

namespace Cry
{
	namespace SensorSystem
	{
		CSensorMap::SVolume::SVolume()
			: id(SensorVolumeId::Invalid)
			, prevId(SensorVolumeId::Invalid)
			, nextId(SensorVolumeId::Invalid)
			, pCell(nullptr)
		{}

		CSensorMap::SCell::SCell()
			: firstVolumeId(SensorVolumeId::Invalid)
			, lastVolumeId(SensorVolumeId::Invalid)
		{}

		CSensorMap::SUpdateStats::SUpdateStats()
			: queryCount(0)
			, time(0.0f)
		{}

		CSensorMap::CSensorMap(ISensorTagLibrary& tagLibrary, const SSensorMapParams& params)
			: m_tagLibrary(tagLibrary)
			, m_octreePlotter(SOctreeParams(params.octreeDepth, params.octreeBounds))
			, m_cells(m_octreePlotter.GetCellCount())
		{
			const uint32 minVolumeCount = 100;
			AllocateVolumes(max(params.volumeCount, minVolumeCount));
		}

		SensorVolumeId CSensorMap::CreateVolume(const SSensorVolumeParams& params)
		{
			if (m_freeVolumes.empty())
			{
				if (!AllocateVolumes(m_volumes.size() + 1))
				{
					return SensorVolumeId::Invalid;
				}
			}

			const uint32 volumeIdx = m_freeVolumes.back();
			m_freeVolumes.pop_back();

			SVolume& volume = m_volumes[volumeIdx];

			volume.params = params;

			if (!params.listenerTags.IsEmpty())
			{
				volume.cache.reserve(20);
			}

			if (!params.attributeTags.IsEmpty())
			{
				MarkCellsDirty(params.bounds.ToAABB(), params.attributeTags);
			}

			MapVolumeToCell(volume);

			m_activeVolumes.insert(volume.id);

			return volume.id;
		}

		void CSensorMap::DestroyVolume(SensorVolumeId volumeId)
		{
			uint32 volumeIdx;
			uint32 salt;
			if (ExpandVolumeId(volumeId, volumeIdx, salt))
			{
				m_activeVolumes.erase(volumeId);
				m_strayVolumes.erase(volumeId);
				m_pendingQueries.erase(volumeId);

				SVolume& volume = m_volumes[volumeIdx];

				RemoveVolumeFromCurrentCell(volume);

				const uint32 maxSalt = 0x7fff;
				if (++salt > maxSalt)
				{
					salt = 0;
				}

				volume.id = CreateVolumeId(volumeIdx, salt);
				volume.params = SSensorVolumeParams();

				volume.cache.clear(); // #TODO : Might be a good idea to release memory at this point but currently VectorSet has no shrink_to_fit() or swap() function.

				m_freeVolumes.push_back(volumeIdx);
			}
		}

		void CSensorMap::UpdateVolumeBounds(SensorVolumeId volumeId, const CSensorBounds& bounds)
		{
			SVolume* pVolume = GetVolume(volumeId);
			if (pVolume)
			{
				const SensorTags attributeTags = pVolume->params.attributeTags;

				if (!attributeTags.IsEmpty())
				{
					MarkCellsDirty(pVolume->params.bounds.ToAABB(), attributeTags);
				}

				pVolume->params.bounds = bounds;
				MapVolumeToCell(*pVolume);

				if (!attributeTags.IsEmpty())
				{
					MarkCellsDirty(bounds.ToAABB(), attributeTags);
				}

				if (!pVolume->params.listenerTags.IsEmpty())
				{
					m_pendingQueries.insert(volumeId);
				}
			}
		}

		void CSensorMap::SetVolumeAttributeTags(SensorVolumeId volumeId, const SensorTags& attributeTags, bool bValue)
		{
			SVolume* pVolume = GetVolume(volumeId);
			if (pVolume)
			{
				if (bValue)
				{
					pVolume->params.attributeTags.Add(attributeTags);
				}
				else
				{
					pVolume->params.attributeTags.Remove(attributeTags);
				}

				MarkCellsDirty(pVolume->params.bounds.ToAABB(), attributeTags);
			}
		}

		void CSensorMap::SetVolumeListenerTags(SensorVolumeId volumeId, const SensorTags& listenerTags, bool bValue)
		{
			SVolume* pVolume = GetVolume(volumeId);
			if (pVolume)
			{
				if (bValue)
				{
					pVolume->params.listenerTags.Add(listenerTags);
				}
				else
				{
					pVolume->params.listenerTags.Remove(listenerTags);
				}

				m_pendingQueries.insert(volumeId);
			}
		}

		SSensorVolumeParams CSensorMap::GetVolumeParams(SensorVolumeId volumeId) const
		{
			const SVolume* pVolume = GetVolume(volumeId);
			return pVolume ? pVolume->params : SSensorVolumeParams();
		}

		void CSensorMap::Query(SensorVolumeIdSet& results, const CSensorBounds& bounds, const SensorTags& tags, SensorVolumeId exclusionId) const
		{
			auto visitOctreeCell = [this, &results, &bounds, tags, exclusionId](const SOctreeCell& octreeCell)
			{
				const SCell& cell = m_cells[octreeCell.idx];
				for (const SVolume* pVolume = GetVolume(cell.firstVolumeId); pVolume; pVolume = GetVolume(pVolume->nextId))
				{
					if (tags.IsEmpty() || tags.CheckAny(pVolume->params.attributeTags))
					{
						if (pVolume->id != exclusionId)
						{
							if (pVolume->params.bounds.Overlap(bounds))
							{
								results.insert(pVolume->id);
							}
						}
					}
				}
			};
			m_octreePlotter.VisitOverlappingCells(visitOctreeCell, bounds.ToAABB());
		}

		void CSensorMap::Update()
		{
			const CTimeValue startTime = gEnv->pTimer->GetAsyncTime();

			for (DirtyCells::value_type dirtyCell : m_dirtyCells)
			{
				const SCell& cell = m_cells[dirtyCell.first];
				for (const SVolume* pVolume = GetVolume(cell.firstVolumeId); pVolume; pVolume = GetVolume(pVolume->nextId))
				{
					if (pVolume->params.listenerTags.CheckAny(dirtyCell.second))
					{
						m_pendingQueries.insert(pVolume->id);
					}
				}
			}
			m_dirtyCells.clear();

			m_updateStats.queryCount = m_pendingQueries.size();

			Events events;
			events.reserve(100);

			for (SensorVolumeId volumeId : m_pendingQueries)
			{
				Query(volumeId, events);
			}
			m_pendingQueries.clear();

			for (const SSensorEvent& event : events)
			{
				SVolume* pVolume = GetVolume(event.listenerVolumeId);
				if (pVolume && pVolume->params.eventListener)
				{
					pVolume->params.eventListener(event);
				}
			}

			const CTimeValue deltaTime = gEnv->pTimer->GetAsyncTime() - startTime;
			m_updateStats.time = deltaTime.GetMilliSeconds();
		}

		void CSensorMap::Debug(float drawRange, const SensorMapDebugFlags& flags)
		{
			if (flags.Check(ESensorMapDebugFlags::DrawStats))
			{
				DebugDrawStats();
			}
			if (flags.CheckAny({ ESensorMapDebugFlags::DrawVolumes, ESensorMapDebugFlags::DrawVolumeEntities, ESensorMapDebugFlags::DrawVolumeTags, ESensorMapDebugFlags::DrawOctree, ESensorMapDebugFlags::DrawStrayVolumes }))
			{
				DebugDrawVolumesAndOctree(drawRange, flags);
			}
			if (flags.Check(ESensorMapDebugFlags::Interactive))
			{
				DebugInteractive(drawRange);
			}
		}

		void CSensorMap::SetOctreeDepth(uint32 depth)
		{
			SOctreeParams octreeParams = m_octreePlotter.GetParams();
			octreeParams.depth = depth;
			m_octreePlotter.SetParams(octreeParams);

			Remap();
		}

		void CSensorMap::SetOctreeBounds(const AABB& bounds)
		{
			SOctreeParams octreeParams = m_octreePlotter.GetParams();
			octreeParams.bounds = bounds;
			m_octreePlotter.SetParams(octreeParams);

			Remap();
		}

		bool CSensorMap::AllocateVolumes(uint32 volumeCount)
		{
			const uint32 maxVolumeCount = 0xffff;
			if (volumeCount > maxVolumeCount)
			{
				return false;
			}

			const uint32 prevVolumeCount = m_volumes.size();
			volumeCount = max(volumeCount, prevVolumeCount * 2);

			m_volumes.resize(volumeCount);
			m_freeVolumes.reserve(volumeCount);
			m_activeVolumes.reserve(volumeCount);
			m_pendingQueries.reserve(volumeCount);

			for (uint32 volumeIdx = prevVolumeCount; volumeIdx < volumeCount; ++volumeIdx)
			{
				m_volumes[volumeIdx].id = CreateVolumeId(volumeIdx, 0);
				m_freeVolumes.push_back(volumeIdx);
			}

			return true;
		}

		SensorVolumeId CSensorMap::CreateVolumeId(uint32 volumeIdx, uint32 salt) const
		{
			return static_cast<SensorVolumeId>((volumeIdx << 16) | salt);
		}

		CSensorMap::SVolume* CSensorMap::GetVolume(SensorVolumeId volumeId)
		{
			uint32 volumeIdx;
			uint32 salt;
			if (ExpandVolumeId(volumeId, volumeIdx, salt))
			{
				return &m_volumes[volumeIdx];
			}
			return nullptr;
		}

		const CSensorMap::SVolume* CSensorMap::GetVolume(SensorVolumeId volumeId) const
		{
			uint32 volumeIdx;
			uint32 salt;
			if (ExpandVolumeId(volumeId, volumeIdx, salt))
			{
				return &m_volumes[volumeIdx];
			}
			return nullptr;
		}

		bool CSensorMap::ExpandVolumeId(SensorVolumeId volumeId, uint32& volumeIdx, uint32& salt) const
		{
			const uint32 value = static_cast<uint32>(volumeId);
			volumeIdx = value >> 16;
			salt = value & 0xffff;
			return (volumeIdx < m_volumes.size()) && (m_volumes[volumeIdx].id == volumeId);
		}

		uint32 CSensorMap::GetCellIdx(const SCell* pCell) const
		{
			return pCell ? static_cast<uint32>(pCell - &m_cells[0]) : SOctreeCell::s_invalidIdx;
		}

		void CSensorMap::MarkCellsDirty(const AABB& bounds, const SensorTags& tags)
		{
			auto visitOctreeCell = [this, tags](const SOctreeCell& octreeCell)
			{
				m_dirtyCells[octreeCell.idx].Add(tags);
			};
			m_octreePlotter.VisitOverlappingCells(visitOctreeCell, bounds);
		}

		void CSensorMap::MapVolumeToCell(SVolume& volume)
		{
			const AABB bounds = volume.params.bounds.ToAABB();

			SOctreeCell octreeCell;
			m_octreePlotter.GetContainingCell(octreeCell, bounds);

			SCell* pCell = octreeCell.idx != SOctreeCell::s_invalidIdx ? &m_cells[octreeCell.idx] : nullptr;
			if (pCell != volume.pCell)
			{
				RemoveVolumeFromCurrentCell(volume);

				if (pCell)
				{
					if (!volume.pCell)
					{
						m_strayVolumes.erase(volume.id);
					}

					volume.pCell = pCell;
					volume.prevId = pCell->lastVolumeId;

					if (pCell->firstVolumeId != SensorVolumeId::Invalid)
					{
						GetVolume(pCell->lastVolumeId)->nextId = volume.id;
					}
					else
					{
						pCell->firstVolumeId = volume.id;
					}

					pCell->lastVolumeId = volume.id;
				}
			}

			if (!volume.pCell)
			{
				m_strayVolumes.insert(volume.id);
			}
		}

		void CSensorMap::RemoveVolumeFromCurrentCell(SVolume& volume)
		{
			if (volume.pCell)
			{
				if (volume.prevId != SensorVolumeId::Invalid)
				{
					GetVolume(volume.prevId)->nextId = volume.nextId;
				}
				else
				{
					volume.pCell->firstVolumeId = volume.nextId;
				}

				if (volume.nextId != SensorVolumeId::Invalid)
				{
					GetVolume(volume.nextId)->prevId = volume.prevId;
				}
				else
				{
					volume.pCell->lastVolumeId = volume.prevId;
				}

				volume.prevId = SensorVolumeId::Invalid;
				volume.nextId = SensorVolumeId::Invalid;
				volume.pCell = nullptr;
			}
		}

		void CSensorMap::Query(SensorVolumeId volumeId, Events& events)
		{
			SVolume* pVolume = GetVolume(volumeId);
			if (pVolume)
			{
				m_queryResults.reserve(100);

				Query(m_queryResults, pVolume->params.bounds, pVolume->params.listenerTags, volumeId);

				uint32 eventCount = 0;

				for (SensorVolumeId otherVolumeId : pVolume->cache)
				{
					if (m_queryResults.find(otherVolumeId) == m_queryResults.end())
					{
						events.emplace_back(ESensorEventType::Leaving, volumeId, otherVolumeId);

						++eventCount;
					}
				}

				for (SensorVolumeId otherVolumeId : m_queryResults)
				{
					if (pVolume->cache.find(otherVolumeId) == pVolume->cache.end())
					{
						events.emplace_back(ESensorEventType::Entering, volumeId, otherVolumeId);

						++eventCount;
					}
				}

				if (eventCount)
				{
					pVolume->cache = m_queryResults;
				}

				m_queryResults.clear();
			}
		}

		void CSensorMap::Remap()
		{
			m_cells.clear();
			m_cells.resize(m_octreePlotter.GetCellCount());

			for (SensorVolumeId volumeId : m_activeVolumes)
			{
				SVolume* pVolume = GetVolume(volumeId);
				if (pVolume)
				{
					pVolume->prevId = SensorVolumeId::Invalid;
					pVolume->nextId = SensorVolumeId::Invalid;
					pVolume->pCell = nullptr;

					MapVolumeToCell(*pVolume);

					m_pendingQueries.insert(pVolume->id);
				}
			}

			m_dirtyCells.clear();
		}

		void CSensorMap::DebugDrawStats() const
		{
			stack_string text = "Sensor Map Stats\n";
			{
				stack_string line;
				line.Format("[Update] queries = %d, time = %f(ms)\n", m_updateStats.queryCount, m_updateStats.time);
				text.append(line);
			}
			{
				stack_string line;
				line.Format("[Volumes] active = %d, stray = %d\n", m_activeVolumes.size(), m_strayVolumes.size());
				text.append(line);
			}

			const float textColor[] = { 0.0f, 1.0f, 0.0f, 1.0f };
			gEnv->pRenderer->GetIRenderAuxGeom()->Draw2dLabel(20.0f, 10.0f, 1.5f, textColor, false, text.c_str());
		}

		void CSensorMap::DebugDrawVolumesAndOctree(float drawRange, const SensorMapDebugFlags& flags) const
		{
			IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();

			const SOctreeParams octreeParams = m_octreePlotter.GetParams();

			SensorVolumeIdSet volumeIds;
			volumeIds.reserve(100);

			const Sphere cameraSphere(gEnv->pSystem->GetViewCamera().GetPosition(), drawRange);

			Query(volumeIds, CSensorBounds(cameraSphere));

			for (SensorVolumeId volumeId : volumeIds)
			{
				const SVolume* pVolume = GetVolume(volumeId);
				if (pVolume)
				{
					if (flags.Check(ESensorMapDebugFlags::DrawVolumes))
					{
						ColorB color(255, 100, 10);
						if (!pVolume->params.listenerTags.IsEmpty())
						{
							color = !pVolume->cache.empty() ? ColorB(255, 0, 0) : ColorB(0, 255, 0);
						}
						pVolume->params.bounds.DebugDraw(color);
					}

					stack_string text;

					if (flags.Check(ESensorMapDebugFlags::DrawVolumeEntities))
					{
						const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pVolume->params.entityId);
						if (pEntity)
						{
							text.append("Entity = ");
							text.append(pEntity->GetName());
							text.append("\n");
						}
					}

					if (flags.Check(ESensorMapDebugFlags::DrawVolumeTags))
					{
						DynArray<const char*> tagNames;
						tagNames.reserve(32);

						auto formatTagNames = [this, &text, &tagNames](const SensorTags& tags, const char* szHeader)
						{
							tagNames.clear();
							m_tagLibrary.GetTagNames(tagNames, tags);

							const uint32 tagCount = tagNames.size();
							if (tagCount)
							{
								text.append(szHeader);
								text.append(" = ");
								for (uint32 tagIdx = 0; tagIdx < tagCount; ++tagIdx)
								{
									if (tagIdx > 0)
									{
										text.append(" | ");
									}
									text.append(tagNames[tagIdx]);
								}
								text.append("\n");
							}
						};

						formatTagNames(pVolume->params.attributeTags, "Attribute Tags");
						formatTagNames(pVolume->params.listenerTags, "Listener Tags");
					}

					if (!text.empty())
					{
						SDrawTextInfo drawTextInfo;
						drawTextInfo.flags = eDrawText_Center | eDrawText_CenterV | eDrawText_FixedSize | eDrawText_800x600;
						drawTextInfo.scale = Vec2(1.2f);
						renderAuxGeom.RenderTextQueued(pVolume->params.bounds.GetCenter(), drawTextInfo, text.c_str());
					}

					if (flags.Check(ESensorMapDebugFlags::DrawOctree))
					{
						SOctreeCell octreeCell;
						if (m_octreePlotter.GetCell(octreeCell, GetCellIdx(pVolume->pCell)))
						{
							const ColorB color(255, 255, 0);
							const AABB volumeBounds = pVolume->params.bounds.ToAABB();
							renderAuxGeom.DrawAABB(volumeBounds, Matrix34(IDENTITY), false, color, eBBD_Faceted);
							renderAuxGeom.DrawAABB(octreeCell.bounds, Matrix34(IDENTITY), false, color, eBBD_Faceted);
							renderAuxGeom.DrawLine(volumeBounds.max, color, octreeCell.bounds.max, color);
						}
					}
				}
			}

			if (flags.Check(ESensorMapDebugFlags::DrawStrayVolumes))
			{
				for (SensorVolumeId volumeId : m_strayVolumes)
				{
					const SVolume* pVolume = GetVolume(volumeId);
					if (pVolume)
					{
						pVolume->params.bounds.DebugDraw(ColorB(255, 0, 255));
					}
				}
			}

			if (flags.Check(ESensorMapDebugFlags::DrawOctree))
			{
				renderAuxGeom.DrawAABB(octreeParams.bounds, Matrix34(IDENTITY), false, ColorB(0, 0, 255), eBBD_Faceted);
			}
		}

		void CSensorMap::DebugInteractive(float drawRange) const
		{
			const CCamera viewCamera = gEnv->pSystem->GetViewCamera();
			const Vec3 viewPos = viewCamera.GetPosition();
			const Vec3 viewDir = viewCamera.GetViewdir().GetNormalized();

			ray_hit rayHit;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(viewPos, viewDir * drawRange, ent_all, rwi_stop_at_pierceable | rwi_colltype_any | rwi_ignore_back_faces, &rayHit, 1))
			{
				SensorVolumeIdSet volumeIds;
				volumeIds.reserve(100);

				const Sphere sphere(rayHit.pt + Vec3(0.0f, 0.0f, 0.5f), 0.8f);
				const CSensorBounds bounds(sphere);

				Query(volumeIds, bounds);

				const ColorB color = !volumeIds.empty() ? ColorB(255, 0, 0) : ColorB(0, 255, 0);

				bounds.DebugDraw(color);

				IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();

				for (SensorVolumeId volumeId : volumeIds)
				{
					const SVolume* pVolume = GetVolume(volumeId);
					if (pVolume)
					{
						renderAuxGeom.DrawLine(sphere.center, color, pVolume->params.bounds.GetCenter(), color);
					}
				}
			}
		}
	}
}