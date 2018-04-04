using System;

namespace CryEngine
{
	//Copy of the native entity_query_flags. 
	//Some entries have been removed because they were either not relevant to RayWorldIntersection or not usable in C#.

	/// <summary>
	/// Used to specify which type of entities can be hit by a raycast.
	/// </summary>
	[Flags]
	public enum EntityRaycastFlags
	{
		Static = 1,
		SleepingRigid = 2,
		Rigid = 4,
		Living = 8,
		Independent = 16,
		Areas = 32,
		Triggers = 64,
		Deleted = 128,
		Terrain = 256,
		
		/// <summary>
		/// Hits all Static, SleepingRigid, Rigid, Living, Independent entities and the Terrain.
		/// </summary>
		All = 287,
		Water = 512
	}
}
