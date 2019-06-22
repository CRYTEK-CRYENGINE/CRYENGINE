// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{
		class CNode;

		class CRenderPrimitiveCollection final : public IRenderPrimitiveCollection
		{
		public:

			explicit CRenderPrimitiveCollection(CNode* pNode = nullptr);

			// IRenderPrimitiveCollection
			virtual void                                          AddSphere(const Vec3& pos, float radius, const ColorF& color) override;
			virtual void                                          AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) override;
			virtual void                                          AddTriangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color) override;
			virtual void                                          AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) override PRINTF_PARAMS(5, 6);
			virtual void                                          AddArrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) override;
			virtual void                                          AddAxes(const Vec3& pos, const Matrix33& axes) override;
			virtual void                                          AddAABB(const AABB& aabb, const ColorF& color) override;
			virtual void                                          AddOBB(const OBB& obb, const Vec3& pos, const ColorF& color) override;
			virtual void                                          AddCone(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) override;
			virtual void                                          AddCylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) override;
			
			virtual void                                          AddSphereWithDebugDrawDuration(const Vec3& pos, float radius, const ColorF& color, const float duration) override;
			virtual void                                          AddLineWithDebugDrawDuration(const Vec3& pos1, const Vec3& pos2, const ColorF& color, const float duration) override;
			virtual void                                          AddTriangleWithDebugDrawDuration(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color, const float duration) override;
			virtual void                                          AddTextWithDebugDrawDuration(const Vec3& pos, float size, const ColorF& color, const float duration, const char* szFormat, ...) override PRINTF_PARAMS(6, 7);
			virtual void                                          AddArrowWithDebugDrawDuration(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, const float duration) override;
			virtual void                                          AddAxesWithDebugDrawDuration(const Vec3& pos, const Matrix33& axes, const float duration) override;
			virtual void                                          AddAABBWithDebugDrawDuration(const AABB& aabb, const ColorF& color, const float duration) override;
			virtual void                                          AddOBBWithDebugDrawDuration(const OBB& obb, const Vec3& pos, const ColorF& color, const float duration) override;
			virtual void                                          AddConeWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) override;
			virtual void                                          AddCylinderWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) override;
			// ~IRenderPrimitiveCollection

			CTimeMetadata                                         GetTimeMetadataMin() const;
			CTimeMetadata                                         GetTimeMetadataMax() const;

			void                                                  SetParentNode(CNode* pNode);

			void                                                  Draw() const;
			void                                                  Draw(const CTimeValue start, const CTimeValue end) const;
			void                                                  GatherStatistics(size_t& outNumRenderPrimitivesInTotalSoFar, size_t& outRoughMemoryUsageOfRenderPrimitivesInTotalSoFar) const;
			void                                                  Serialize(Serialization::IArchive& ar);

		private:

			void                                                  UpdateTimeMetadata(const CTimeMetadata& timeMetadata);

		private:

			std::vector<std::shared_ptr<CRenderPrimitiveBase>>    m_prims;
			CNode*                                                m_pNode;

			// Required to Update Node Time Metadata when a node is deleted. Prevents us from iterating all render primitives.
			CTimeMetadata                                         m_timeMetadataMin;
			CTimeMetadata                                         m_timeMetadataMax;
		};

	}
}