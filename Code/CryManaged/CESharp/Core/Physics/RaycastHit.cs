using CryEngine.Common;

namespace CryEngine.OldPhysics
{
	/// <summary>
	/// Wrapper class for a ray cast output.
	/// </summary>
	public class RaycastHit
	{
		public enum HitType
		{
			Unknown,
			Terrain,
			Entity,
			Static,
		}

		private uint _hitID;

		[SerializeValue]
		public ray_hit NativeHandle { get; private set; }
		[SerializeValue]
		public int Hits { get; private set; }
		public bool Intersected { get { return Hits != 0; } }
		public Vector3 Point { get { return NativeHandle.pt; } }
		public Vector3 Normal { get { return NativeHandle.n; } }
		public float Distance { get { return NativeHandle.dist; } }

		/// <summary>
		/// The native CRYENGINE entity that was hit. Returns null if nothing was hit, or the hit wasn't an Entity.
		/// </summary>
		public IEntity HitNativeEntity { get { return _hitID != 0 ? Global.gEnv.pEntitySystem.GetEntity(_hitID) : null; } }

		/// <summary>
		/// The Entity that was hit. Returns null if nothing was hit, or the hit wasn't an Entity.
		/// </summary>
		public Entity HitBaseEntity { get { return Entity.Get(_hitID); } }

		/// <summary>
		/// The type of surface that was hit, if any. Returns 'Unknown' if nothing was hit.
		/// </summary>
		[SerializeValue]
		public HitType Type { get; private set; }

		/// <summary>
		/// The native entity's physical entity, if something was hit.
		/// </summary>
		public IPhysicalEntity Collider { get { return NativeHandle.pCollider; } }

		public RaycastHit(ray_hit ray, int hits)
		{
			NativeHandle = ray;
			Hits = hits;

			if (ray.bTerrain == 1)
			{
				Type = HitType.Terrain;
			}
			else
			{
				var pEntity = Global.gEnv.pEntitySystem.GetEntityFromPhysics(ray.pCollider);

				if (pEntity != null)
					pEntity = pEntity.GetAttachedChild(ray.partid);

				if (pEntity != null)
				{
					// We hit some sort of entity.
					_hitID = pEntity.GetId();

					Type = HitType.Entity;
				}
				else
				{
					// We hit something that isn't registered with the entity system. Probably a designer object or brush.
					Type = HitType.Static;
				}
			}
		}

		/// <summary>
		/// Returns True if the entity passed in was hit.
		/// </summary>
		/// <returns><c>true</c>, if entity was hit, <c>false</c> otherwise.</returns>
		/// <param name="entity">Entity.</param>
		public bool HasHitEntity(Entity entity)
		{
			//Disable this warning because it's possible that the Entities == operator gets overloaded in which case ?? will not work anymore.
#pragma warning disable RECS0059 // Conditional expression can be simplified
			return HitBaseEntity != null ? HitBaseEntity.Id == entity.Id : false;
#pragma warning restore RECS0059 // Conditional expression can be simplified
		}
	}
}
