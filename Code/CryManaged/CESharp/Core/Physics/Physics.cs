// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using CryEngine.Common;
using CryEngine.EntitySystem;

namespace CryEngine
{
	public struct RaycastHit
	{
		private Entity _entity;
		private PhysicsEntity _physicsEntity;

		/// <summary>
		/// The point in world-space where the raycast hit a surface
		/// </summary>
		public Vector3 Point { get; internal set; }

		/// <summary>
		/// The normal of the surface that was hit.
		/// </summary>
		public Vector3 Normal { get; internal set; }

		/// <summary>
		/// The ID of the entity that was hit. -1 if no entity was hit.
		/// </summary>
		public EntityId EntityId { get; internal set; }

		/// <summary>
		/// The uv coordinates of the point on the surface that was hit.
		/// </summary>
		public Vector2 UvPoint { get; internal set; }

		/// <summary>
		/// The distance from the raycast's origin to the point of impact.
		/// </summary>
		public float Distance { get; internal set; }

		/// <summary>
		/// The index of the triangle in the collision mesh that was hit by the raycast.
		/// </summary>
		public int TriangleIndex { get; internal set; }

		/// <summary>
		/// The unique identifier of the surface-type set in the material that's assigned to the object that was hit.
		/// </summary>
		public int SurfaceId { get; internal set; }

		/// <summary>
		/// Unique identifier for a part of the physical representation of the entity.
		/// </summary>
		public int PartId { get; internal set; }

		/// <summary>
		/// The index of the part of the physical representation of the entity.
		/// </summary>
		public int PartIndex { get; internal set; }

		/// <summary>
		/// The collider that was hit by the raycast.
		/// </summary>
		public PhysicsEntity Collider
		{
			get
			{
				if(_physicsEntity == null)
				{
					_physicsEntity = Entity.Physics;
				}
				return _physicsEntity;
			}
		}

		/// <summary>
		/// Returns the entity that was hit. Returns null if no entity was hit.
		/// If you only need to compare this to another Entity it will be faster to compare the EntityId's instead.
		/// </summary>
		public Entity Entity
		{
			get
			{
				if(_entity == null)
				{
					_entity = Entity.Get(EntityId);
				}
				return _entity;
			}
		}

		internal static void FromNativeHit(ref RaycastHit hit, ray_hit nativeHit)
		{
			hit.Distance = nativeHit.dist;
			hit.Point = nativeHit.pt;
			hit.Normal = nativeHit.n;
			hit.TriangleIndex = nativeHit.iPrim;
			hit.SurfaceId = nativeHit.surface_idx;
			hit.PartId = nativeHit.partid;
			hit.PartIndex = nativeHit.ipart;
		}
	}

	public static class Physics
	{
		public struct Ray
		{
			private Vector3 _direction;

			/// <summary>
			/// The origin position of this Ray.
			/// </summary>
			public Vector3 Origin { get; set; }

			/// <summary>
			/// The lenght this ray will point in a direction.
			/// </summary>
			public float Length { get; set; }

			/// <summary>
			/// The normalized direction this Ray will point in. All values will be normalized when set.
			/// </summary>
			public Vector3 Direction
			{
				get
				{
					return _direction;
				}
				set
				{
					//TODO Change to MathHelpers.Epsilon.
					if(value.IsNearlyZero())
					{
						Log.Error<Ray>("Direction cannot be set to {0, 0, 0}!");
						return;
					}
					_direction = value.Normalized;
				}
			}
		}

		//TODO Create a usable raycast-flags enum
		internal const uint DefaultRaycastFlags = unchecked((uint)((int)geom_flags.geom_colltype_ray <<
			(int)rwi_flags.rwi_colltype_bit) |
			(int)rwi_flags.rwi_colltype_any |
			(10 & (int)rwi_flags.rwi_pierceability_mask) |
			((int)geom_flags.geom_colltype14 << (int)rwi_flags.rwi_colltype_bit));

		/// <summary>
		/// Casts a ray hitting all entities. Returns true if an entity is hit, otherwise false.
		/// </summary>
		/// <param name="ray"></param>
		/// <returns></returns>
		public static bool Raycast(Ray ray)
		{
			return Raycast(ray.Origin, ray.Direction, ray.Length, EntityRaycastFlags.All, null);
		}

		/// <summary>
		/// Casts a ray starting from origin going along direction up until the maxDistance is reached, meanwhile hitting all entities. 
		/// Returns true if an entity is hit, otherwise false.
		/// </summary>
		/// <param name="origin"></param>
		/// <param name="direction"></param>
		/// <param name="maxDistance"></param>
		/// <returns></returns>
		public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance)
		{
			return Raycast(origin, direction, maxDistance, EntityRaycastFlags.All, null);
		}

		/// <summary>
		/// Casts a ray hitting all entities specified with the entityFlags. Returns true if an entity is hit, otherwise false.
		/// </summary>
		/// <param name="ray"></param>
		/// <param name="entityFlags"></param>
		/// <param name="skipList"></param>
		/// <returns></returns>
		public static bool Raycast(Ray ray, EntityRaycastFlags entityFlags, params IPhysicalEntity[] skipList)
		{
			return Raycast(ray.Origin, ray.Direction, ray.Length, entityFlags, skipList);
		}

		/// <summary>
		/// Casts a ray starting from origin going along direction up until the maxDistance is reached, meanwhile hitting all entities specified by entityFlags. 
		/// Returns true if an entity is hit, otherwise false.
		/// </summary>
		/// <param name="origin"></param>
		/// <param name="direction"></param>
		/// <param name="maxDistance"></param>
		/// <param name="entityFlags"></param>
		/// <param name="skipList"></param>
		/// <returns></returns>
		public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, EntityRaycastFlags entityFlags, params IPhysicalEntity[] skipList)
		{
			var magnitude = direction.Dot(direction);
			if(magnitude > 1.0f || magnitude < 1.0f)
			{
				direction = direction.Normalized;
			}

			var rayParams = new IPhysicalWorld.SRWIParams();
			rayParams.org = origin;
			rayParams.dir = direction * maxDistance;
			rayParams.objtypes = (int)entityFlags;
			rayParams.flags = DefaultRaycastFlags;
			rayParams.nMaxHits = 1;
			rayParams.hits = new ray_hit();

			if(skipList != null)
			{
				rayParams.AllocateSkipEnts(skipList.Length);

				for(int i = 0; i < skipList.Length; ++i)
				{
					rayParams.SetSkipEnt(i, skipList[i]);
				}
			}
			else
			{
				rayParams.AllocateSkipEnts(0);
			}

			var hits = Global.gEnv.pPhysicalWorld.RayWorldIntersection(rayParams);
			rayParams.DeleteSkipEnts();
			return hits > 0;
		}


		/// <summary>
		/// Casts a ray hitting all entities. Returns true if an entity is hit, otherwise false.
		/// If a hit is registered details about the hit are returned in raycastHit.
		/// </summary>
		/// <param name="ray"></param>
		/// <param name="raycastHit"></param>
		/// <returns></returns>
		public static bool Raycast(Ray ray, out RaycastHit raycastHit)
		{
			return Raycast(ray.Origin, ray.Direction, ray.Length, EntityRaycastFlags.All, out raycastHit);
		}

		/// <summary>
		/// Casts a ray starting from origin going along direction up until the maxDistance is reached, meanwhile hitting all entities. 
		/// Returns true if an entity is hit, otherwise false. If a hit is registered details about the hit are returned in raycastHit.
		/// </summary>
		/// <param name="origin"></param>
		/// <param name="direction"></param>
		/// <param name="maxDistance"></param>
		/// <param name="raycastHit"></param>
		/// <returns></returns>
		public static bool Raycast(Vec3 origin, Vec3 direction, float maxDistance, out RaycastHit raycastHit)
		{
			return Raycast(origin, direction, maxDistance, EntityRaycastFlags.All, out raycastHit);
		}

		/// <summary>
		/// Casts a ray hitting all entities specified with the entityFlags. Returns true if an entity is hit, otherwise false.
		/// If a hit is registered details about the hit are returned in raycastHit.
		/// </summary>
		/// <param name="ray"></param>
		/// <param name="entityFlags"></param>
		/// <param name="raycastHit"></param>
		/// <param name="skipList"></param>
		/// <returns></returns>
		public static bool Raycast(Ray ray, EntityRaycastFlags entityFlags, out RaycastHit raycastHit, params PhysicsEntity[] skipList)
		{
			return Raycast(ray.Origin, ray.Direction, ray.Length, entityFlags, out raycastHit, skipList);
		}

		/// <summary>
		/// Casts a ray starting from origin going along direction up until the maxDistance is reached, meanwhile hitting all entities specified by entityFlags. 
		/// Returns true if an entity is hit, otherwise false. If a hit is registered details about the hit are returned in raycastHit.
		/// </summary>
		/// <param name="origin"></param>
		/// <param name="direction"></param>
		/// <param name="maxDistance"></param>
		/// <param name="entityFlags"></param>
		/// <param name="raycastHit"></param>
		/// <param name="skipList"></param>
		/// <returns></returns>
		public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, EntityRaycastFlags entityFlags, out RaycastHit raycastHit, params PhysicsEntity[] skipList)
		{
			raycastHit = default(RaycastHit);

			var magnitude = direction.Dot(direction);
			if(magnitude > 1.0f || magnitude < 1.0f)
			{
				direction = direction.Normalized;
			}

			var rayParams = new IPhysicalWorld.SRWIParams();
			rayParams.org = origin;
			rayParams.dir = direction * maxDistance;
			rayParams.objtypes = (int)entityFlags;
			rayParams.flags = DefaultRaycastFlags;
			rayParams.nMaxHits = 1;
			rayParams.hits = new ray_hit();

			if(skipList != null && skipList.Length > 0)
			{
				rayParams.AllocateSkipEnts(skipList.Length);

				for(int i = 0; i < skipList.Length; ++i)
				{
					rayParams.SetSkipEnt(i, skipList[i].NativeHandle);
				}
			}
			else
			{
				rayParams.AllocateSkipEnts(0);
			}

			var hits = Global.gEnv.pPhysicalWorld.RayWorldIntersection(rayParams);
			if(hits > 0)
			{
				var hit = rayParams.hits;
				float u = 0;
				float v = 0;

				var entityId = Global.gEnv.pRenderer.GetDetailedRayHitInfo(hit.pCollider, origin, direction, maxDistance, ref u, ref v);

				raycastHit = new RaycastHit
				{
					EntityId = entityId,
					UvPoint = new Vec2(u, v)
				};
				RaycastHit.FromNativeHit(ref raycastHit, hit);
			}
			rayParams.DeleteSkipEnts();
			return hits > 0;
		}

		/// <summary>
		/// Generate a skiplist for the specified entity including all of it's child-entities.
		/// </summary>
		/// <param name="entity"></param>
		/// <returns></returns>
		public static PhysicsEntity[] GetSkipListForEntity(Entity entity)
		{
			if(entity == null)
			{
				return null;
			}

			var list = new List<PhysicsEntity>();

			foreach(var child in entity.Children)
			{
				var childList = GetSkipListForEntity(child);
				if(childList != null)
				{
					list.AddRange(childList);
				}
			}

			var pEntity = entity.Physics;
			if(pEntity != null)
			{
				list.Add(pEntity);
			}

			return list.ToArray();
		}
	}
}