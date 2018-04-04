namespace CryEngine.Game.Weapons
{
	public struct BulletData
	{
		public string GeometryUrl { get; set; }
		public string MaterialUrl { get; set; }
		public float Scale { get; set; }
		public float Speed { get; set; }
		public float Mass { get; set; }
	}

	public class Bullet : EntityComponent
	{
		/// <summary>
		/// The lifetime of a bullet. After this amount of time has passed the bullet will be removed.
		/// </summary>
		private const float MaxLifetime = 10f;

		/// <summary>
		/// The time this bullet has been alive. If it reaches MaxLifetime this bullet will be removed.
		/// </summary>
		private float _lifetime = 0;

		public void Initialize(BulletData bulletData)
		{
			// Set the model
			Entity.LoadGeometry(0, bulletData.GeometryUrl);

			// Load the custom bullet material.
			if(!string.IsNullOrWhiteSpace(bulletData.MaterialUrl))
			{
				Entity.LoadMaterial(bulletData.MaterialUrl);
			}

			// Now create the physical representation of the entity
			var physics = Entity.Physics;

			var physicParams = new RigidPhysicalizeParams();
			physicParams.Mass = bulletData.Mass;
			physics.Physicalize(physicParams);

			// Make sure that bullets are always rendered regardless of distance
			Entity.SetViewDistanceRatio(1.0f);

			physics.AddImpulse(Entity.WorldRotation.Forward * bulletData.Speed);
		}

		protected override void OnUpdate(float frameTime)
		{
			base.OnUpdate(frameTime);

			_lifetime += frameTime;
			if(_lifetime > MaxLifetime)
			{
				Entity.Remove();
			}
		}


		protected override void OnCollision(EntitySystem.CollisionEvent collisionEvent)
		{
			base.OnCollision(collisionEvent);

			//Remove the entity when it hits something.
			Entity.Remove();
		}
	}
}
