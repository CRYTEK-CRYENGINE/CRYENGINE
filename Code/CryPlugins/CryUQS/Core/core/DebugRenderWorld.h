// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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

		class CDebugRenderWorldImmediate : public IDebugRenderWorldImmediate
		{
		public:
			               CDebugRenderWorldImmediate() {}  // user-defined default ctor required for const instances of this class

			// IDebugRenderWorldImmediate
			virtual void   DrawSphere(const Vec3& pos, float radius, const ColorF& color) const override;
			virtual void   DrawDirection(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) const override;
			virtual void   DrawLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) const override;
			virtual void   DrawCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color) const override;
			virtual void   DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color) const override;
			virtual void   DrawText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) const override PRINTF_PARAMS(5, 6);
			virtual void   DrawQuat(const Vec3& pos, const Quat& q, float r, const ColorF& color) const override;
			virtual void   DrawAABB(const AABB& aabb, const ColorF& color) const override;
			virtual void   DrawOBB(const OBB& obb, const ColorF& color) const override;
			// ~IDebugRenderWorldImmediate
		};

		//===================================================================================
		//
		// CDebugRenderWorldPersistent
		//
		//===================================================================================

		class CDebugRenderWorldPersistent : public IDebugRenderWorldPersistent
		{
		public:
			static const size_t                                     kIndexWithoutAssociation = (size_t)-1;

		private:

			//===================================================================================
			//
			// SDebugRenderPrimitiveWithItemAssociation
			//
			// - holds the render-primitive plus some extra information related to a potential item it may be associated with
			// - basically, we have 3 situations:
			//
			//   (1) it's not associated with an item at all (.associatedItemIndex == kIndexWithoutAssociation, .bIsPartOfTheItem == false); this would be the case for when a generator adds some primitves
			//   (2) it's associated with an item, but not part of the item itself (.associatedItemIndex != kIndexWithoutAssociation, .bIsPartOfTheItem == false); this would be the case for when an evaluator adds its custom primitives while evaluating the item
			//   (3) it's associated with an item, and it's part of the item itself (.associatedItemIndex != kIndexWithoutAssociation, .bIsPartOfTheItem == true); this would be the case for when all items have just been generated and being exposed to the render-world all at once
			//
			// - if the render-primitive is associated with an item, it might additionally be associated with the evaluator that added this primitive to the CDebugRenderWorldPersistent
			//
			//===================================================================================

			struct SDebugRenderPrimitiveWithItemAssociation
			{
				explicit                                            SDebugRenderPrimitiveWithItemAssociation();  // default ctor required for yasli serialization
				                                                    SDebugRenderPrimitiveWithItemAssociation(const SDebugRenderPrimitiveWithItemAssociation& other);  // copy-ctor (that is not explicit) required for yasli serialization of STL containers
				explicit                                            SDebugRenderPrimitiveWithItemAssociation(std::unique_ptr<CDebugRenderPrimitiveBase>& _primitive, size_t _associatedItemIndex, bool _bIsPartOfTheItem, size_t _associatedInstantEvaluatorIndexInBlueprint, size_t _associatedDeferredEvaluatorIndexInBlueprint);
				explicit                                            SDebugRenderPrimitiveWithItemAssociation(SDebugRenderPrimitiveWithItemAssociation&& other);
				SDebugRenderPrimitiveWithItemAssociation&           operator=(const SDebugRenderPrimitiveWithItemAssociation& other);   // required for yasli serialization

				void                                                Serialize(Serialization::IArchive& ar);

				std::unique_ptr<CDebugRenderPrimitiveBase>          pPrimitive;
				size_t                                              associatedItemIndex;                           // is kIndexWithoutAssociation if no item is associated with the render primitive (which is the case when adding primitives outside the evaluation phases)
				bool                                                bIsPartOfTheItem;                              // true if the render primitive is one of many primitives to make the item look as it is; false if the primitive is just additional information that was added by e. g. an evaluator
				size_t                                              associatedInstantEvaluatorIndexInBlueprint;    // if != kIndexWithoutAssociation, then this specifies the index of the instant-evaluator in the blueprint that added this primitive while evaluating an item
				size_t                                              associatedDeferredEvaluatorIndexInBlueprint;   // if != kIndexWithoutAssociation, then this specifies the index of the deferred-evaluator in the blueprint that added this primitive while evaluating an item
			};

		public:
			explicit                                                CDebugRenderWorldPersistent();

			// IDebugRenderWorldPersistent
			virtual void                                            AddSphere(const Vec3& pos, float radius, const ColorF& color) override;
			virtual void                                            AddDirection(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) override;
			virtual void                                            AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) override;
			virtual void                                            AddCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color) override;
			virtual void                                            AddCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color) override;
			virtual void                                            AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) override PRINTF_PARAMS(5, 6);
			virtual void                                            AddQuat(const Vec3& pos, const Quat& q, float r, const ColorF& color) override;
			virtual void                                            AddAABB(const AABB& aabb, const ColorF& color) override;
			virtual void                                            AddOBB(const OBB& obb, const ColorF& color) override;
			// ~IDebugRenderWorldPersistent

			// These 3 methods deal with whether primitives will be associated with an item and whether they will be even part of what an item will look like.
			// They basically change the inner "state" of the CDebugRenderWorldPersistent in terms of how newly added primitives will be treated.
			void                                                    ItemConstructionBegin();
			void                                                    AssociateAllUpcomingAddedPrimitivesWithItem(size_t indexOfCurrentlyEvaluatedItem);
			void                                                    ItemConstructionEnd();

			// These 2 methods deal with whether primitives will not only be associated with a specific item, but also with the evaluator that is currently evaluating the item.
			// As such, we can simply skip rendering any primitives of evaluators which are currently masked out in the Query History Inspector tool.
			void                                                    AssociateAllUpcomingAddedPrimitivesAlsoWithInstantEvaluator(size_t indexOfInstantEvaluatorInBlueprint);
			void                                                    AssociateAllUpcomingAddedPrimitivesAlsoWithDeferredEvaluator(size_t indexOfDeferredEvaluatorInBlueprint);

			void                                                    DrawAllAddedPrimitivesAssociatedWithItem(size_t indexOfItemToDrawPrimitivesOf, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks, const ColorF& colorToOverridePrimitivesWithIfPartOfTheItem, bool bHighlight) const;
			void                                                    DrawAllAddedPrimitivesWithNoItemAssociation() const;

			size_t                                                  GetRoughMemoryUsage() const;

			void                                                    Serialize(Serialization::IArchive& ar);

		private:
			                                                        UQS_NON_COPYABLE(CDebugRenderWorldPersistent);

		private:
			std::vector<SDebugRenderPrimitiveWithItemAssociation>   m_primitives;
			size_t                                                  m_indexOfCurrentlyEvaluatedItem;                        // used for associating render-primitives when being added via the Add*() methods with the current item that is being evaluated by the ongoing query
			bool                                                    m_bIsItemConstructionInProgress;                        // while true, it means that all render-primitives that get added are part of what the item makes it look as it is
			size_t                                                  m_indexOfCurrentlyRunningInstantEvaluatorInBlueprint;   // used for associating render-primitives not only with an item, but also with the instant-evaluator that added the primitive while evaluating the item
			size_t                                                  m_indexOfCurrentlyRunningDeferredEvaluatorInBlueprint;  // ditto for deferred-evaluators
		};

	}
}
