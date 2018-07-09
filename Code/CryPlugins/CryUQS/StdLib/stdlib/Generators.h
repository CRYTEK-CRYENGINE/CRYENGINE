// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#include <CryAISystem/INavigationSystem.h>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// CGenerator_PointsOnPureGrid
		//
		// - generates points on a grid
		// - the grid is specified by a center, size (of one edge) and a spacing between the points
		// - notice: this generator is very limited and doesn't work well if you intend to use it for things like uneven terrain
		//           -> rather use CGenerator_PointsOnNavMesh for that
		//
		//===================================================================================

		class CGenerator_PointsOnPureGrid : public Client::CGeneratorBase<CGenerator_PointsOnPureGrid, Pos3>
		{
		public:
			struct SParams
			{
				Pos3                    center;                  // center of the grid
				float                   size;                    // length of one edge of the grid
				float                   spacing;                 // space between individual points on both, the x- and y-axis

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("center", center, "CENT", "Center of the grid.");
					UQS_EXPOSE_PARAM("size", size, "SIZE", "Length of one edge of the grid.");
					UQS_EXPOSE_PARAM("spacing", spacing, "SPAC", "Space between individual points on both the x- and y-axis.");
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CGenerator_PointsOnPureGrid(const SParams& params);
			EUpdateStatus               DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate);

		private:
			const SParams               m_params;
		};

		//===================================================================================
		//
		// CGenerator_PointsOnNavMesh
		//
		// - generates Vec3s on the navigation mesh limited by an AABB
		// - the AABB is defined by a pivot and local mins and maxs
		// - the pivot has to reside within the enclosing volume of a NavMesh or else no points will be generated
		// - each of the resulting points will reside on the center of the corresponding NavMesh triangle
		//
		//===================================================================================

		class CGenerator_PointsOnNavMesh : public Client::CGeneratorBase<CGenerator_PointsOnNavMesh, Pos3>
		{
		public:
			struct SParams
			{
				Pos3                    pivot;                   // center of the AABB
				Ofs3                    localAABBMins;           // local min extents of the AABB in which the points will be generated
				Ofs3                    localAABBMaxs;           // local max extents of the AABB in which the points will be generated
				NavigationAgentTypeID   navigationAgentTypeID;   // the points will be generated in this layer of the NavMesh

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pivot", pivot, "PIVO", "Center of the AABB.");
					UQS_EXPOSE_PARAM("localAABBMins", localAABBMins, "LMIN", "Local min extents of the AABB in which the points will be generated.");
					UQS_EXPOSE_PARAM("localAABBMaxs", localAABBMaxs, "LMAX", "Local max extents of the AABB in which the points will be generated.");
					UQS_EXPOSE_PARAM("navigationAgentTypeID", navigationAgentTypeID, "AGEN", "The points will be generated in this layer of the NavMesh.");
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CGenerator_PointsOnNavMesh(const SParams& params);
			EUpdateStatus               DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate);

		private:
			const SParams               m_params;
		};

		//===================================================================================
		//
		// CGenerator_PointsOnGridProjectedOntoNavMesh
		//
		// - generates points on a grid which are then projected onto the navigation mesh
		// - internally, a flood-filling originating at the grid's center is used to radially spread out and project all grid cell positions onto the navigation mesh
		// - as opposed to CGenerator_PointsOnPureGrid, this one should work fine for hills and slopes on the terrain
		//
		//===================================================================================

		class CGenerator_PointsOnGridProjectedOntoNavMesh : public Client::CGeneratorBase<CGenerator_PointsOnGridProjectedOntoNavMesh, Pos3>
		{
		public:
			struct SParams
			{
				Pos3                    center;                     // center of the grid
				float                   size;                       // length of one edge of the grid
				float                   spacing;                    // space between individual points on both, the x- and y-axis
				NavigationAgentTypeID   navigationAgentTypeID;      // the points will be generated in this layer of the NavMesh
				float                   verticalTolerance;          // upwards + downwards tolerance (in meters) to find valid navmesh positions for each grid position

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("center", center, "CENT", "Center of the grid.");
					UQS_EXPOSE_PARAM("size", size, "SIZE", "Length of one edge of the grid.");
					UQS_EXPOSE_PARAM("spacing", spacing, "SPAC", "Space between individual points on both the x- and y-axis.");
					UQS_EXPOSE_PARAM("navigationAgentTypeID", navigationAgentTypeID, "AGEN", "The points will be generated in this layer of the NavMesh.");
					UQS_EXPOSE_PARAM("verticalTolerance", verticalTolerance, "VTOL", "Upwards and downwards tolerance (in meters) to find a valid position on the navmesh for each grid position.");
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CGenerator_PointsOnGridProjectedOntoNavMesh(const SParams& params);
			EUpdateStatus               DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate);

		private:
			bool                        Setup(const SUpdateContext& updateContext);
			EUpdateStatus               Flood(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate);
			void                        PerformOneFloodStep(const SUpdateContext& updateContext);

		private:
			const SParams               m_params;
			bool                        m_bSetupPending;
			int                         m_numCellsOnOneAxis;
			float                       m_startPosX;
			float                       m_startPosY;
			std::vector<Pos3>           m_grid;
			std::vector<bool>           m_visited;
			std::vector<int>            m_parents;
			std::deque<int>             m_openList;
			std::vector<Pos3>           m_successfullyProjectedPoints;
			int                         m_debugRunawayCounter;
		};

	}
}
