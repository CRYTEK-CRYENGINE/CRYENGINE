namespace CryEngine.Game.Weapons
{
	public class Rifle : BaseWeapon
	{
		private static readonly BulletData _bulletData = new BulletData
		{
			GeometryUrl = "Objects/Default/primitive_sphere.cgf",
			// This material has the 'mat_bullet' surface type applied, which is set up to play sounds on collision with 'mat_default' objects in Libs/MaterialEffects
			MaterialUrl = "Materials/bullet",
			Scale = 0.05f,
			Speed = 20.0f,
			Mass = 2500.0f
		};

		public override BulletData BulletData { get { return _bulletData; } }

		public override string Name { get { return "Rifle"; } }

		public override string OutAttachementName { get { return "barrel_out"; } }

		public override void RequestFire(Vector3 firePosition, Quaternion bulletRotation)
		{
			//Spawn the bullet entity
			var bullet = Entity.SpawnWithComponent<Bullet>("Rifle Bullet", firePosition, bulletRotation, BulletData.Scale);
			//This will set the prepare the bullet and set the initial velocity.
			bullet.Initialize(BulletData);
		}
	}
}
