// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugRenderWorld.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDebugRenderWorldImmediate
		//
		//===================================================================================

		void CDebugRenderWorldImmediate::DrawSphere(const Vec3& pos, float radius, const ColorF& color) const
		{
			CDebugRenderPrimitive_Sphere::Draw(pos, radius, color, false);
		}

		void CDebugRenderWorldImmediate::DrawDirection(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) const
		{
			CDebugRenderPrimitive_Direction::Draw(from, to, coneRadius, coneHeight, color, false);
		}

		void CDebugRenderWorldImmediate::DrawLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) const
		{
			CDebugRenderPrimitive_Line::Draw(pos1, pos2, color, false);
		}

		void CDebugRenderWorldImmediate::DrawCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color) const
		{
			CDebugRenderPrimitive_Cone::Draw(pos, dir, baseRadius, height, color, false);
		}

		void CDebugRenderWorldImmediate::DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color) const
		{
			CDebugRenderPrimitive_Cylinder::Draw(pos, dir, radius, height, color, false);
		}

		void CDebugRenderWorldImmediate::DrawText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) const
		{
			va_list ap;
			char text[1024];

			va_start(ap, szFormat);
			cry_vsprintf(text, szFormat, ap);
			va_end(ap);

			CDebugRenderPrimitive_Text::Draw(pos, size, text, color, false);
		}

		void CDebugRenderWorldImmediate::DrawQuat(const Vec3& pos, const Quat& q, float r, const ColorF& color) const
		{
			CDebugRenderPrimitive_Quat::Draw(pos, q, r, color, false);
		}

		void CDebugRenderWorldImmediate::DrawAABB(const AABB& aabb, const ColorF& color) const
		{
			CDebugRenderPrimitive_AABB::Draw(aabb, color, false);
		}

		void CDebugRenderWorldImmediate::DrawOBB(const OBB& obb, const ColorF& color) const
		{
			CDebugRenderPrimitive_OBB::Draw(obb, color, false);
		}

		//===================================================================================
		//
		// CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation
		//
		//===================================================================================

		CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::SDebugRenderPrimitiveWithItemAssociation()
			: pPrimitive()
			, associatedItemIndex(kIndexWithoutAssociation)
			, bIsPartOfTheItem(false)
			, associatedInstantEvaluatorIndexInBlueprint(kIndexWithoutAssociation)
			, associatedDeferredEvaluatorIndexInBlueprint(kIndexWithoutAssociation)
		{}

		CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::SDebugRenderPrimitiveWithItemAssociation(const SDebugRenderPrimitiveWithItemAssociation& other)
			: pPrimitive()   // bypass the unique_ptr
			, associatedItemIndex(other.associatedItemIndex)
			, bIsPartOfTheItem(other.bIsPartOfTheItem)
			, associatedInstantEvaluatorIndexInBlueprint(other.associatedInstantEvaluatorIndexInBlueprint)
			, associatedDeferredEvaluatorIndexInBlueprint(other.associatedDeferredEvaluatorIndexInBlueprint)
		{}

		CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::SDebugRenderPrimitiveWithItemAssociation(std::unique_ptr<CDebugRenderPrimitiveBase>& _primitive, size_t _associatedItemIndex, bool _bIsPartOfTheItem, size_t _associatedInstantEvaluatorIndexInBlueprint, size_t _associatedDeferredEvaluatorIndexInBlueprint)
			: pPrimitive(std::move(_primitive))
			, associatedItemIndex(_associatedItemIndex)
			, bIsPartOfTheItem(_bIsPartOfTheItem)
			, associatedInstantEvaluatorIndexInBlueprint(_associatedInstantEvaluatorIndexInBlueprint)
			, associatedDeferredEvaluatorIndexInBlueprint(_associatedDeferredEvaluatorIndexInBlueprint)
		{}

		CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::SDebugRenderPrimitiveWithItemAssociation(SDebugRenderPrimitiveWithItemAssociation&& other)
			: pPrimitive(std::move(other.pPrimitive))
			, associatedItemIndex(other.associatedItemIndex)
			, bIsPartOfTheItem(other.bIsPartOfTheItem)
			, associatedInstantEvaluatorIndexInBlueprint(other.associatedInstantEvaluatorIndexInBlueprint)
			, associatedDeferredEvaluatorIndexInBlueprint(other.associatedDeferredEvaluatorIndexInBlueprint)
		{}

		CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation& CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::operator=(const SDebugRenderPrimitiveWithItemAssociation& other)
		{
			if (this != &other)
			{
				// bypass .pPrimitive due to its unique_ptr
				associatedItemIndex = other.associatedItemIndex;
				bIsPartOfTheItem = other.bIsPartOfTheItem;
				associatedInstantEvaluatorIndexInBlueprint = other.associatedInstantEvaluatorIndexInBlueprint;
				associatedDeferredEvaluatorIndexInBlueprint = other.associatedDeferredEvaluatorIndexInBlueprint;
			}
			return *this;
		}

		void CDebugRenderWorldPersistent::SDebugRenderPrimitiveWithItemAssociation::Serialize(Serialization::IArchive& ar)
		{
			ar(pPrimitive, "pPrimitive");
			ar(associatedItemIndex, "associatedItemIndex");
			ar(bIsPartOfTheItem, "bIsPartOfTheItem");
			ar(associatedInstantEvaluatorIndexInBlueprint, "associatedInstantEvaluatorIndexInBlueprint");
			ar(associatedDeferredEvaluatorIndexInBlueprint, "associatedDeferredEvaluatorIndexInBlueprint");
		}

		//===================================================================================
		//
		// CDebugRenderWorldPersistent
		//
		//===================================================================================

		CDebugRenderWorldPersistent::CDebugRenderWorldPersistent()
			: m_indexOfCurrentlyEvaluatedItem(kIndexWithoutAssociation)
			, m_bIsItemConstructionInProgress(false)
			, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint(kIndexWithoutAssociation)
			, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint(kIndexWithoutAssociation)
		{
			// nothing
		}

		void CDebugRenderWorldPersistent::AddSphere(const Vec3& pos, float radius, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Sphere(pos, radius, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddDirection(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Direction(from, to, coneRadius, coneHeight, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Line(pos1, pos2, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Cone(pos, dir, baseRadius, height, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Cylinder(pos, dir, radius, height, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...)
		{
			va_list ap;
			char text[1024];

			va_start(ap, szFormat);
			cry_vsprintf(text, szFormat, ap);
			va_end(ap);

			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Text(pos, size, text, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddQuat(const Vec3& pos, const Quat& q, float r, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_Quat(pos, q, r, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddAABB(const AABB& aabb, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_AABB(aabb, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::AddOBB(const OBB& obb, const ColorF& color)
		{
			std::unique_ptr<CDebugRenderPrimitiveBase> p(new CDebugRenderPrimitive_OBB(obb, color));
			m_primitives.emplace_back(p, m_indexOfCurrentlyEvaluatedItem, m_bIsItemConstructionInProgress, m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint, m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint);
		}

		void CDebugRenderWorldPersistent::ItemConstructionBegin()
		{
			m_bIsItemConstructionInProgress = true;
		}

		void CDebugRenderWorldPersistent::AssociateAllUpcomingAddedPrimitivesWithItem(size_t indexOfCurrentlyEvaluatedItem)
		{
			m_indexOfCurrentlyEvaluatedItem = indexOfCurrentlyEvaluatedItem;
		}

		void CDebugRenderWorldPersistent::ItemConstructionEnd()
		{
			m_bIsItemConstructionInProgress = false;
		}

		void CDebugRenderWorldPersistent::AssociateAllUpcomingAddedPrimitivesAlsoWithInstantEvaluator(size_t indexOfInstantEvaluatorInBlueprint)
		{
			m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint = indexOfInstantEvaluatorInBlueprint;
		}

		void CDebugRenderWorldPersistent::AssociateAllUpcomingAddedPrimitivesAlsoWithDeferredEvaluator(size_t indexOfDeferredEvaluatorInBlueprint)
		{
			m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint = indexOfDeferredEvaluatorInBlueprint;
		}

		void CDebugRenderWorldPersistent::DrawAllAddedPrimitivesAssociatedWithItem(size_t indexOfItemToDrawPrimitivesOf, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks, const ColorF& colorToOverridePrimitivesWithIfPartOfTheItem, bool bHighlight) const
		{
			// FIXME: linear search of all relevant primitives is stupid
			for (const SDebugRenderPrimitiveWithItemAssociation& p : m_primitives)
			{
				// skip if not associated with given item
				if (p.associatedItemIndex != indexOfItemToDrawPrimitivesOf)
					continue;

				// skip if also associated with an instant-evaluator but which is masked out from drawing
				if (p.associatedInstantEvaluatorIndexInBlueprint != kIndexWithoutAssociation)
				{
					const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)(1 << p.associatedInstantEvaluatorIndexInBlueprint);
					if (!(evaluatorDrawMasks.maskInstantEvaluators & bit))
						continue;
				}

				// skip if also associated with a deferred-evaluator but which is masked out from drawing
				if (p.associatedDeferredEvaluatorIndexInBlueprint != kIndexWithoutAssociation)
				{
					const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)(1 << p.associatedDeferredEvaluatorIndexInBlueprint);
					if (!(evaluatorDrawMasks.maskDeferredEvaluators & bit))
						continue;
				}

				// - primitives that the item is made of will get their color overridden (because the overriding color represents the score of that item)
				// - all other primitives come from evaluators (but are still in the context of the item) and therefore will retain their color
				const ColorF* pColor = p.bIsPartOfTheItem ? &colorToOverridePrimitivesWithIfPartOfTheItem : nullptr;
				p.pPrimitive->Draw(bHighlight, pColor);
			}
		}

		void CDebugRenderWorldPersistent::DrawAllAddedPrimitivesWithNoItemAssociation() const
		{
			for (const SDebugRenderPrimitiveWithItemAssociation& p : m_primitives)
			{
				if (p.associatedItemIndex == kIndexWithoutAssociation)
				{
					p.pPrimitive->Draw(false, nullptr);
				}
			}
		}

		size_t CDebugRenderWorldPersistent::GetRoughMemoryUsage() const
		{
			size_t memoryUsed = 0;

			for (const SDebugRenderPrimitiveWithItemAssociation& p : m_primitives)
			{
				memoryUsed += p.pPrimitive->GetRoughMemoryUsage();
			}

			return memoryUsed;
		}

		void CDebugRenderWorldPersistent::Serialize(Serialization::IArchive& ar)
		{
			ar(m_primitives, "m_primitives");
			ar(m_indexOfCurrentlyEvaluatedItem, "m_indexOfCurrentlyEvaluatedItem");
			ar(m_bIsItemConstructionInProgress, "m_bIsItemConstructionInProgress");
		}

	}
}
