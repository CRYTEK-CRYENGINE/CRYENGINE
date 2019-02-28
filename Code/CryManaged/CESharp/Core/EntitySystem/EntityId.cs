// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Entity ID's store references to game entities as unsigned integers.
	/// </summary>
	public struct EntityId
	{
		internal uint _value;

		/// <summary>
		/// Constructor for the <see cref="EntityId"/>. Entity ID's store references to game entities as unsigned integers.
		/// </summary>
		/// <param name="id"></param>
		public EntityId(uint id)
		{
			_value = id;
		}

		#region Overrides
		/// <summary>
		/// Compare this EntityID to another object. Can only be compared to objects of the type <see cref="EntityId"/>, <see cref="int"/> and <see cref="uint"/>.
		/// </summary>
		/// <param name="obj"></param>
		/// <returns></returns>
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

		/// <summary>
		/// Returns the hash code for this instance.
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
#pragma warning disable RECS0025 // Non-readonly field referenced in 'GetHashCode()'
			return _value.GetHashCode();
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'
		}

		/// <summary>
		/// Returns the ID of this <see cref="EntityId"/> as a string.
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return _value.ToString();
		}
		#endregion

		#region Operators
		/// <summary>
		/// Returns true if the ID of the <see cref="EntityId"/>s are equal. False otherwise.
		/// </summary>
		/// <param name="entId1"></param>
		/// <param name="entId2"></param>
		/// <returns></returns>
		public static bool operator ==(EntityId entId1, EntityId entId2)
		{
			return entId1._value == entId2._value;
		}

		/// <summary>
		/// Returns true if the ID of the <see cref="EntityId"/>s are not equal. False otherwise.
		/// </summary>
		/// <param name="entId1"></param>
		/// <param name="entId2"></param>
		/// <returns></returns>
		public static bool operator !=(EntityId entId1, EntityId entId2)
		{
			return entId1._value != entId2._value;
		}

		/// <summary>
		/// Implicitly casts the uint to an <see cref="EntityId"/>.
		/// </summary>
		/// <param name="value"></param>
		public static implicit operator EntityId(uint value)
		{
			return new EntityId(value);
		}

		/// <summary>
		/// Implicitly casts the int to an <see cref="EntityId"/>.
		/// </summary>
		/// <param name="value"></param>
		public static implicit operator EntityId(int value)
		{
			return new EntityId((uint)value);
		}

		/// <summary>
		/// Implicitly casts the <see cref="EntityId"/> to an int.
		/// </summary>
		/// <param name="value"></param>
		public static implicit operator int(EntityId value)
		{
			return (int)value._value;
		}

		/// <summary>
		/// Implicitly casts the <see cref="EntityId"/> to an uint.
		/// </summary>
		/// <param name="value"></param>
		public static implicit operator uint(EntityId value)
		{
			return value._value;
		}
		#endregion
	}
}
