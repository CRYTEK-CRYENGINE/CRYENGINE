using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	public class ObjectReader
	{
		public ObjectReader(Stream stream)
		{
			stream.Seek(0, SeekOrigin.Begin);

			Reader = new BinaryReader(stream);
			Converter = new FormatterConverter();
		}

		BinaryReader Reader { get; set; }
		FormatterConverter Converter { get; set; }

		Dictionary<Type, Dictionary<int, object>> _typeObjects = new Dictionary<Type, Dictionary<int, object>>();

		Type _deserializationCallbackType = typeof(IDeserializationCallback);
		MethodInfo _deserializationMethod = typeof(IDeserializationCallback).GetMethod("OnDeserialization", BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly);

		public object Read()
		{
			var objectType = (SerializedObjectType)Reader.ReadByte();

			switch (objectType)
			{
				case SerializedObjectType.Null:
					return null;
				case SerializedObjectType.Boolean:
					return Reader.ReadBoolean();
				case SerializedObjectType.Char:
					return Reader.ReadChar();
				case SerializedObjectType.SByte:
					return Reader.ReadSByte();
				case SerializedObjectType.Byte:
					return Reader.ReadByte();
				case SerializedObjectType.Int16:
					return Reader.ReadInt16();
				case SerializedObjectType.UInt16:
					return Reader.ReadUInt16();
				case SerializedObjectType.Int32:
					return Reader.ReadInt32();
				case SerializedObjectType.UInt32:
					return Reader.ReadUInt32();
				case SerializedObjectType.Int64:
					return Reader.ReadInt64();
				case SerializedObjectType.UInt64:
					return Reader.ReadUInt64();
				case SerializedObjectType.Single:
					return Reader.ReadSingle();
				case SerializedObjectType.Double:
					return Reader.ReadDouble();
				case SerializedObjectType.Decimal:
					return Reader.ReadDecimal();
				case SerializedObjectType.IntPtr:
					return new IntPtr(Reader.ReadInt64()); ;
				case SerializedObjectType.UIntPtr:
					return new UIntPtr(Reader.ReadUInt64());
				case SerializedObjectType.Enum:
					return ReadEnum();
				case SerializedObjectType.Reference:
					return ReadReference();
				case SerializedObjectType.Object:
					return ReadObject();
				case SerializedObjectType.Array:
					return ReadArray();
                case SerializedObjectType.Assembly:
                    return ReadAssembly();
				case SerializedObjectType.Type:
					return ReadTypeWithReference();
				case SerializedObjectType.String:
					return ReadString();
				case SerializedObjectType.MemberInfo:
					return ReadMemberInfo();
				case SerializedObjectType.ISerializable:
					return ReadISerializable();
			}

			throw new ArgumentException("Tried to deserialize unknown object type!");
		}

		public void ReadStatics()
		{
			var numTypes = Reader.ReadInt32();

			var fieldFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

			for (var i = 0; i < numTypes; i++)
			{
				if (!Reader.ReadBoolean())
				{
					continue;
				}

				var type = ReadType();

				ReadObjectMembers(null, type, fieldFlags);
			}
		}

		object ReadEnum()
		{
			var type = ReadType();
			var value = Read();

			return Enum.ToObject(type, value);
		}

		object ReadReference()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();

			return _typeObjects[type][referenceId];
		}

		void AddReference(Type type, object obj, int referenceId)
		{
			Dictionary<int, object> objects;
			if (!_typeObjects.TryGetValue(type, out objects))
			{
				objects = new Dictionary<int, object>();
				_typeObjects.Add(type, objects);
			}

			objects.Add(referenceId, obj);
		}

		#region Reference types
		object ReadObject()
		{
			var referenceId = Reader.ReadInt32();
			var numTypes = Reader.ReadInt32();

			object obj = null;

			var fieldFlags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

			bool hasDeserializeCallback = false;

			for (var i = 0; i < numTypes; i++)
			{
				var type = ReadType();

				if (obj == null)
				{
					// Create the object, first type is the actual implementation
					obj = FormatterServices.GetUninitializedObject(type);

					AddReference(type, obj, referenceId);

					hasDeserializeCallback = _deserializationCallbackType.IsAssignableFrom(type);
				}

				ReadObjectMembers(obj, type, fieldFlags);
			}

			if (hasDeserializeCallback)
			{
				_deserializationMethod.Invoke(obj, new object[] { this });
			}

			return obj;
		}

		void ReadObjectMembers(object obj, Type objectType, BindingFlags flags)
		{
			var numFields = Reader.ReadInt32();
			for (var iField = 0; iField < numFields; iField++)
			{
				var fieldName = Reader.ReadString();
				var fieldValue = Read();

                if (objectType != null)
                {
                    var fieldInfo = objectType.GetField(fieldName, flags);
                    if (fieldInfo != null && !fieldInfo.IsLiteral)
                    {
                        fieldInfo.SetValue(obj, fieldValue);
                    }
                }
			}

			var numEvents = Reader.ReadInt32();
			for (var iEvent = 0; iEvent < numEvents; iEvent++)
			{
				var eventName = Reader.ReadString();
				
				if (Reader.ReadBoolean())
				{
					var delegateType = ReadType();
					var functionCount = Reader.ReadInt32();

                    EventInfo eventInfo = objectType != null ? objectType.GetEvent(eventName, flags) : null;

                    for (int i = 0; i < functionCount; i++)
					{
						var methodInfo = Read() as MethodInfo;
						var target = Read();

						Delegate parsedDelegate;

						if (target != null)
						{
							parsedDelegate = Delegate.CreateDelegate(delegateType, target, methodInfo);
						}
						else
						{
							parsedDelegate = Delegate.CreateDelegate(delegateType, methodInfo);
						}

                        if (eventInfo != null)
                        {
                            var addMethod = eventInfo.GetAddMethod(true);
                            addMethod.Invoke(obj, new[] { parsedDelegate });
                        }
					}
				}
			}

            if (objectType != null)
            {
                // Special case, always get native pointers for SWIG director types
                var swigCPtr = objectType.GetField("swigCPtr", flags);
                if (swigCPtr != null)
                {
                    // Start with director handling, aka support for cross-language polymorphism
                    var swigDirectorConnect = objectType.GetMethod("SwigDirectorConnect", flags);
                    if (swigDirectorConnect != null)
                    {
                        var swigCMemOwn = objectType.GetField("swigCMemOwn", flags);

                        // Should not be null, if it is we need to check if something is wrong
                        var constructor = objectType.GetConstructor(Type.EmptyTypes);

                        var newObj = constructor.Invoke(null);

                        swigCMemOwn.SetValue(newObj, false);
                        swigCMemOwn.SetValue(obj, true);

                        var handleReference = new HandleRef(obj, ((HandleRef)swigCPtr.GetValue(newObj)).Handle);

                        swigCPtr.SetValue(obj, handleReference);
                        swigDirectorConnect.Invoke(obj, null);
                    }
                    else // Support for other types, such as Vec3 (where we should simply take ownership of the native pointer
                    {
                        var handleReference = new HandleRef(obj, ((HandleRef)swigCPtr.GetValue(obj)).Handle);
                        swigCPtr.SetValue(obj, handleReference);
                    }
                }
            }
		}

		object ReadString()
		{
			var referenceId = Reader.ReadInt32();
			var stringValue = Reader.ReadString();

			AddReference(typeof(string), stringValue, referenceId);

			return stringValue;
		}

		object ReadArray()
		{
			var referenceId = Reader.ReadInt32();

			var arrayLength = Reader.ReadInt32();
			var elementType = ReadType();

			var array = Array.CreateInstance(elementType, arrayLength);

			AddReference(array.GetType(), array, referenceId);

			for (var i = 0; i < arrayLength; i++)
			{
				array.SetValue(Read(), i);
			}

			return array;
		}

		object ReadMemberInfo()
		{
			var referenceId = Reader.ReadInt32();

			var memberName = Reader.ReadString();

			var declaringType = ReadType();
			var memberType = (MemberTypes)Reader.ReadByte();

			var isStatic = Reader.ReadBoolean();
			var bindingFlags = BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic;
			if (isStatic)
			{
				bindingFlags |= BindingFlags.Static;
			}
			else
			{
				bindingFlags |= BindingFlags.Instance;
			}

			MemberInfo memberInfo = null;

			switch (memberType)
			{
				case MemberTypes.Method:
					{
						var parameterCount = Reader.ReadInt32();
						var parameters = new Type[parameterCount];

						for (int i = 0; i < parameterCount; i++)
							parameters[i] = ReadType();

						memberInfo = declaringType.GetMethod(memberName, bindingFlags, null, parameters, null);
					}
					break;
				case MemberTypes.Field:
					memberInfo = declaringType.GetField(memberName, bindingFlags);
					break;
				case MemberTypes.Property:
					memberInfo = declaringType.GetProperty(memberName, bindingFlags);
					break;
			}

			AddReference(memberInfo.GetType(), memberInfo, referenceId);

			return memberInfo;
		}

		object ReadISerializable()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();
			var memberCount = Reader.ReadInt32();

			var serInfo = new SerializationInfo(type, Converter);

			for (var i = 0; i < memberCount; i++)
			{
				var memberName = Reader.ReadString();
				var memberType = ReadType();
				var memberValue = Read();
                
                if (memberType != null)
                {
                    serInfo.AddValue(memberName, memberValue, memberType);
                }
			}

			var constructor = type.GetConstructor(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance, null, new Type[] { typeof(SerializationInfo), typeof(StreamingContext) }, null);
			var obj = constructor.Invoke(new object[] { serInfo, new StreamingContext(StreamingContextStates.CrossAppDomain) });

			if (_deserializationCallbackType.IsAssignableFrom(type))
			{
				_deserializationMethod.Invoke(obj, new object[] { this });
			}

			AddReference(type, obj, referenceId);

			return obj;
		}

		Type ReadTypeWithReference()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();

#pragma warning disable RECS0035 // Possible mistaken call to 'object.GetType()'
            if (type != null)
            {
                AddReference(type.GetType(), type, referenceId);
            }
#pragma warning restore RECS0035 // Possible mistaken call to 'object.GetType()'

			return type;
		}

		Type ReadType()
		{
			if(Reader.ReadBoolean())
			{
				var elementType = ReadType();
				return elementType.MakeArrayType();
			}

			var isGeneric = Reader.ReadBoolean();

			var type = GetType(Reader.ReadString());

			if(isGeneric)
			{
				var numGenericArgs = Reader.ReadInt32();
				var genericArgs = new Type[numGenericArgs];
				for(int i = 0; i < numGenericArgs; i++)
					genericArgs[i] = ReadType();

                if (type != null)
                {
                    type = type.MakeGenericType(genericArgs);
                }
			}

			return type;
		}

		Type GetType(string typeName)
		{
			if (typeName == null)
			{
				throw new ArgumentNullException(nameof(typeName));
			}

			if (typeName.Length == 0)
			{
				throw new ArgumentException(string.Format("{0} cannot have zero length!", nameof(typeName)));
			}

			if (typeName.Contains('+'))
			{
				var splitString = typeName.Split('+');
				var ownerType = GetType(splitString.FirstOrDefault());

				return ownerType.Assembly.GetType(typeName);
			}

			var type = Type.GetType(typeName);
			if (type != null)
				return type;

			foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
			{
				type = assembly.GetType(typeName);
				if (type != null)
					return type;
			}

            return null;
		}

        Assembly ReadAssembly()
        {
            var referenceId = Reader.ReadInt32();
            var assembly = Assembly.LoadFrom(Reader.ReadString());
            if(assembly != null)
            {
                AddReference(assembly.GetType(), assembly, referenceId);
            }

            return assembly;
        }
        #endregion
    }
}
