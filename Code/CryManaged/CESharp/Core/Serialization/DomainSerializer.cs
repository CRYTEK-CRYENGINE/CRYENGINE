using System;

namespace CryEngine.Serialization
{
	enum SerializedObjectType : byte
	{
		Null = TypeCode.Empty,
		Object = TypeCode.Object,

		Boolean = TypeCode.Boolean,
		Char = TypeCode.Char,
		SByte = TypeCode.SByte,
		Byte = TypeCode.Byte,
		Int16 = TypeCode.Int16,
		UInt16 = TypeCode.UInt16,
		Int32 = TypeCode.Int32,
		UInt32 = TypeCode.UInt32,
		Int64 = TypeCode.Int64,
		UInt64 = TypeCode.UInt64,
		Single = TypeCode.Single,
		Double = TypeCode.Double,
		Decimal = TypeCode.Decimal,
		String = TypeCode.String,

		// Start custom types below

		IntPtr,
		UIntPtr,

		Reference,
		ISerializable,
		Array,
		Type,
        Assembly,
		Enum,
		MemberInfo,

        EntityComponent
	}
}
