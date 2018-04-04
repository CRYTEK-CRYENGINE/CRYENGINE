namespace CryEngine.Game.Weapons
{
	public abstract class BaseWeapon
	{
		public abstract string Name { get; }

		public abstract BulletData BulletData { get; }

		public abstract string OutAttachementName { get; }

		public abstract void RequestFire(Vector3 firePosition, Quaternion bulletRotation);
	}
}
