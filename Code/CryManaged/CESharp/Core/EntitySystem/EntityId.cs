namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Entity ID's store references to game entities as unsigned integers.
	/// </summary>
	public class EntityId
	{
		internal uint _value;

		public EntityId(uint id)
		{
			_value = id;
		}

		#region Overrides
		public override bool Equals(object obj)
		{
			if (obj is EntityId)
				return (EntityId)obj == this;
			if (obj is int)
				return (int)obj == _value;
			if (obj is uint)
				return (uint)obj == _value;

			return false;
		}

		public override int GetHashCode()
		{
#pragma warning disable RECS0025 // Non-readonly field referenced in 'GetHashCode()'
			return _value.GetHashCode();
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'
		}

		public override string ToString()
		{
			return _value.ToString();
		}
		#endregion

		#region Operators
		public static bool operator ==(EntityId entId1, EntityId entId2)
		{
			return entId1._value == entId2._value;
		}

		public static bool operator !=(EntityId entId1, EntityId entId2)
		{
			return entId1._value != entId2._value;
		}

		public static implicit operator EntityId(uint value)
		{
			return new EntityId(value);
		}

		public static implicit operator EntityId(int value)
		{
			return new EntityId((uint)value);
		}

		public static implicit operator int(EntityId value)
		{
			return System.Convert.ToInt32(value._value);
		}

		public static implicit operator uint(EntityId value)
		{
			return System.Convert.ToUInt32(value._value);
		}
		#endregion
	}
}
