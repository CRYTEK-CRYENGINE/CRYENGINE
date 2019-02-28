// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine.Serialization
{
	/// <summary>
	/// Interface that helps with serialization tests.
	/// </summary>
	public interface ISerializedObject
	{
		/// <summary>
		/// Returns the data buffer of the serialized object.
		/// </summary>
		/// <returns></returns>
		byte[] GetBuffer();

		/// <summary>
		/// Validates the specified data buffer to the data of the serialized object.
		/// </summary>
		/// <param name="buffer"></param>
		/// <returns></returns>
		bool ValidateBuffer(byte[] buffer);
	}
}
