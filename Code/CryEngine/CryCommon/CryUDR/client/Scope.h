// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		//===================================================================================
		//
		// CScope* classes:
		//
		// These are the workhorses of the whole debug recording system from the game code's point of view.
		// The game code can create instances of such a class on the program stack for adding render-primitives in the according scope.
		// Scopes can get nested by creating more instances on the stack. This will create a whole tree of named nodes in the end.
		// Each node will then eventually have several render-primitives that will be drawn from inside the "UDR Recordings" editor-plugin.
		// Scopes can be re-entered on subsequent frames by simply building a stack with names used prior - this is typically done via
		// the CScope_FixedString class.
		//
		//===================================================================================

		//! Base class for all CScope_* classes
		//! Can only be instantiated via derived classes
		//! Gives access to the underlying IRenderPrimitiveCollection in the tree for adding all kinds of render-primitives to it via its Add*() methods by the game code
		//! These render-primitives will get persisted when saving the live tree do disk
		class CScopeBase
		{
		public:

			class CDebugElementsAdapter
			{
			public:

				// these forward to the INode's IRenderPrimitiveCollection
				void	                    AddSphere(const Vec3& pos, float radius, const ColorF& color) const;
				void	                    AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) const;
				void	                    AddTriangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color) const;
				void	                    AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) const PRINTF_PARAMS(5, 6);
				void	                    AddArrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) const;
				void	                    AddAxes(const Vec3& pos, const Matrix33& axes) const;
				void	                    AddAABB(const AABB& aabb, const ColorF& color) const;
				void	                    AddOBB(const OBB& obb, const Vec3& pos, const ColorF& color) const;
				void	                    AddCone(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) const;
				void	                    AddCylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) const;

				void	                    AddSphereWithDebugDrawDuration(const Vec3& pos, float radius, const ColorF& color, const float duration) const;
				void	                    AddLineWithDebugDrawDuration(const Vec3& pos1, const Vec3& pos2, const ColorF& color, const float duration) const;
				void	                    AddTriangleWithDebugDrawDuration(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color, const float duration) const;
				void	                    AddTextWithDebugDrawDuration(const Vec3& pos, float size, const ColorF& color, const float duration, const char* szFormat, ...) const PRINTF_PARAMS(6, 7);
				void	                    AddArrowWithDebugDrawDuration(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, const float duration) const;
				void	                    AddAxesWithDebugDrawDuration(const Vec3& pos, const Matrix33& axes, const float duration) const;
				void	                    AddAABBWithDebugDrawDuration(const AABB& aabb, const ColorF& color, const float duration) const;
				void	                    AddOBBWithDebugDrawDuration(const OBB& obb, const Vec3& pos, const ColorF& color, const float duration) const;
				void	                    AddConeWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) const;
				void	                    AddCylinderWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) const;

				// these forward to the INode's ILogMessageCollection
				void                        LogInformation(const char* szFormat, ...) const PRINTF_PARAMS(2, 3);
				void                        LogWarning(const char* szFormat, ...) const PRINTF_PARAMS(2, 3);

				// TODO: Implement functions with duration for log messages too.
			private:
				friend class CScopeBase;
				INode *                     m_pNode = nullptr;
			};

		public:

			const char*                     GetName() const;
			const CDebugElementsAdapter*    operator->() const;

		protected:

			CScopeBase() {}
			~CScopeBase();

			void                            PushNode(INode& node);

		private:

			CScopeBase(const CScopeBase&) = delete;
			CScopeBase(CScopeBase&&) = delete;
			CScopeBase&                     operator=(const CScopeBase&) = delete;
			CScopeBase&                     operator=(CScopeBase&&) = delete;

			static void*                    operator new(size_t) = delete;
			static void*                    operator new[](size_t) = delete;

			static void*                    operator new(size_t, void*) = delete;
			static void*                    operator new[](size_t, void*) = delete;

			static void                     operator delete(void*) = delete;
			static void                     operator delete[](void*) = delete;

			static void                     operator delete(void*, void*) = delete;
			static void                     operator delete[](void*, void*) = delete;

		private:

			CDebugElementsAdapter           m_adapter;
			CRecursiveSyncObjectAutoLock    m_lock;
		};

		inline void CScopeBase::CDebugElementsAdapter::AddSphere(const Vec3& pos, float radius, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddSphere(pos, radius, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddLine(pos1, pos2, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddTriangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddTriangle(vtx1, vtx2, vtx3, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) const
		{
			CRY_ASSERT(m_pNode);
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_pNode->GetRenderPrimitiveCollection().AddText(pos, size, color, "%s", text.c_str());
		}

		inline void CScopeBase::CDebugElementsAdapter::AddArrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddArrow(from, to, coneRadius, coneHeight, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddAxes(const Vec3& pos, const Matrix33& axes) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddAxes(pos, axes);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddAABB(const AABB& aabb, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddAABB(aabb, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddOBB(const OBB& obb, const Vec3& pos, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddOBB(obb, pos, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddCone(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddCone(pos, dir, radius, height, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddCylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddCylinder(pos, dir, radius, height, color);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddSphereWithDebugDrawDuration(const Vec3& pos, float radius, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddSphereWithDebugDrawDuration(pos, radius, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddLineWithDebugDrawDuration(const Vec3& pos1, const Vec3& pos2, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddLineWithDebugDrawDuration(pos1, pos2, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddTriangleWithDebugDrawDuration(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddTriangleWithDebugDrawDuration(vtx1, vtx2, vtx3, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddTextWithDebugDrawDuration(const Vec3& pos, float size, const ColorF& color, const float duration, const char* szFormat, ...) const
		{
			CRY_ASSERT(m_pNode);
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_pNode->GetRenderPrimitiveCollection().AddTextWithDebugDrawDuration(pos, size, color, duration, "%s", text.c_str());
		}

		inline void CScopeBase::CDebugElementsAdapter::AddArrowWithDebugDrawDuration(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddArrowWithDebugDrawDuration(from, to, coneRadius, coneHeight, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddAxesWithDebugDrawDuration(const Vec3& pos, const Matrix33& axes, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddAxesWithDebugDrawDuration(pos, axes, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddAABBWithDebugDrawDuration(const AABB& aabb, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddAABBWithDebugDrawDuration(aabb, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddOBBWithDebugDrawDuration(const OBB& obb, const Vec3& pos, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddOBBWithDebugDrawDuration(obb, pos, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddConeWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddConeWithDebugDrawDuration(pos, dir, radius, height, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::AddCylinderWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) const
		{
			CRY_ASSERT(m_pNode);
			m_pNode->GetRenderPrimitiveCollection().AddCylinderWithDebugDrawDuration(pos, dir, radius, height, color, duration);
		}

		inline void CScopeBase::CDebugElementsAdapter::LogInformation(const char* szFormat, ...) const
		{
			CRY_ASSERT(m_pNode);
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_pNode->GetLogMessageCollection().LogInformation("%s", text.c_str());
		}

		inline void CScopeBase::CDebugElementsAdapter::LogWarning(const char* szFormat, ...) const
		{
			CRY_ASSERT(m_pNode);
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_pNode->GetLogMessageCollection().LogWarning("%s", text.c_str());
		}

		inline CScopeBase::~CScopeBase()
		{
			CRY_ASSERT(m_adapter.m_pNode);
			gEnv->pUDR->GetNodeStack().PopNode();
		}

		inline void CScopeBase::PushNode(INode& node)
		{
			CRY_ASSERT(!m_adapter.m_pNode);
			m_adapter.m_pNode = &node;
			gEnv->pUDR->GetNodeStack().PushNode(node);
		}

		inline const char* CScopeBase::GetName() const
		{
			CRY_ASSERT(m_adapter.m_pNode);
			return m_adapter.m_pNode->GetName();
		}

		inline const CScopeBase::CDebugElementsAdapter* CScopeBase::operator->() const
		{
			CRY_ASSERT(m_adapter.m_pNode);
			return &m_adapter;
		}

		//===================================================================================
		//
		// CScope_FixedString
		//
		// - uses a fixed string to re-enter the particular scope with this name on subsequent calls
		// - creates a scope with this name on first call
		//
		//===================================================================================

		class CScope_FixedString : public CScopeBase
		{
		public:
			explicit CScope_FixedString(const char* szNodeName);
		};

		inline CScope_FixedString::CScope_FixedString(const char* szNodeName)
		{
			PushNode(gEnv->pUDR->GetNodeStack().GetTopNode().AddChild_FixedStringIfNotYetExisting(szNodeName));
		}

		//===================================================================================
		//
		// CScope_UniqueStringAutoIncrement
		//
		// - creates a new unique scope upon subsequent calls
		// - the scopes will be named according to given prefix plus an internal number that increments with each call, thus
		//   producing a new scope every time
		//
		//===================================================================================

		class CScope_UniqueStringAutoIncrement : public CScopeBase
		{
		public:
			explicit CScope_UniqueStringAutoIncrement(const char* szNodeNamePrefix);
		};

		inline CScope_UniqueStringAutoIncrement::CScope_UniqueStringAutoIncrement(const char* szNodeNamePrefix)
		{
			PushNode(gEnv->pUDR->GetNodeStack().GetTopNode().AddChild_UniqueStringAutoIncrement(szNodeNamePrefix));
		}

		//===================================================================================
		//
		// CScope_UseLastAutoIncrementedUniqueString
		//
		// - continues with a scope that was previously created via CScope_UniqueStringAutoIncrement
		// - the prefix to pass in should be the same for both classes
		//
		//===================================================================================

		class CScope_UseLastAutoIncrementedUniqueString : public CScopeBase
		{
		public:
			explicit CScope_UseLastAutoIncrementedUniqueString(const char* szNodeNamePrefix);
		};

		inline CScope_UseLastAutoIncrementedUniqueString::CScope_UseLastAutoIncrementedUniqueString(const char* szNodeNamePrefix)
		{
			PushNode(gEnv->pUDR->GetNodeStack().GetTopNode().AddChild_FindLastAutoIncrementedUniqueNameOrCreateIfNotYetExisting(szNodeNamePrefix));
		}

		//===================================================================================
		//
		// CScope_CurrentSystemFrame
		//
		// - creates a new scope with a name based on the current system frame
		// - if such a scope already exists, it will use that
		//
		//===================================================================================

		class CScope_CurrentSystemFrame : public CScopeBase
		{
		public:
			explicit CScope_CurrentSystemFrame();
		};

		inline CScope_CurrentSystemFrame::CScope_CurrentSystemFrame()
		{
			PushNode(gEnv->pUDR->GetNodeStack().GetTopNode().AddChild_CurrentSystemFrame());
		}

	}
}