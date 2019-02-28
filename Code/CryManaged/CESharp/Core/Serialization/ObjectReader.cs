// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	internal class ObjectReader
	{
		private const string MissingMethodText = "Unable to create a new instance of {0} because the constructor could not be found. " +
												 "This can happen if the constructor is private or has required parameters specified. " +
												 "For classes inheriting from {1} it's best to not have a constructor and instead initialize it in the OnInitialize and OnGameplayStart methods of the component.";

		private const string MethodAccessText = "Unable to create a new instance of {0} because the constructor could not be accessed. " +
												"This can happen if the constructor is private or has required parameters specified. " +
												"For classes inheriting from {1} it's best to not have a constructor and instead initialize it in the OnInitialize and OnGameplayStart methods of the component.";
		internal ObjectReader(Stream stream)
		{
			stream.Seek(0, SeekOrigin.Begin);

			Reader = new BinaryReader(stream);
			Converter = new FormatterConverter();
			PopulateGuidLookup();
		}

		private BinaryReader Reader { get; set; }
		private FormatterConverter Converter { get; set; }

		private Dictionary<Type, Dictionary<int, object>> _typeObjects = new Dictionary<Type, Dictionary<int, object>>();
		private Dictionary<string, Type> _guidLookup = new Dictionary<string, Type>();

		private readonly Type _deserializationCallbackType = typeof(IDeserializationCallback);
		private readonly MethodInfo _deserializationMethod = typeof(IDeserializationCallback).GetMethod(nameof(IDeserializationCallback.OnDeserialization), BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly);
		private readonly Type _entityComponentType = typeof(EntityComponent);
		private readonly Type _entityPropertyType = typeof(EntityPropertyAttribute);
		private readonly Type _serializedType = typeof(SerializeValueAttribute);
		private readonly Type _nonSerializedType = typeof(NonSerializedAttribute);

		private void PopulateGuidLookup()
		{
			var assemblies = AppDomain.CurrentDomain.GetAssemblies();
			foreach(var assembly in assemblies)
			{
				var types = assembly.GetTypes();
				foreach(var type in types)
				{
					if(!type.IsClass)
					{
						continue;
					}

					var guid = ObjectWriter.GetGuid(type);
					if(guid != null)
					{
						_guidLookup.Add(guid, type);
					}
				}
			}
		}

		internal object Read()
		{
			return ReadInternal();
		}

		private object ReadInternal(object existingObject = null, bool isConstructed = false, Type currentType = null)
		{
			var objectType = (SerializedObjectType)Reader.ReadByte();

			switch(objectType)
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
					return new IntPtr(Reader.ReadInt64());
				case SerializedObjectType.UIntPtr:
					return new UIntPtr(Reader.ReadUInt64());
				case SerializedObjectType.Enum:
					return ReadEnum();
				case SerializedObjectType.Reference:
					return ReadReference();
				case SerializedObjectType.Object:
					return ReadObject(existingObject, isConstructed, currentType);
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
					return ReadISerializeValue();
				case SerializedObjectType.EntityComponent:
					return ReadEntityComponent();
			}

			throw new ArgumentException("Tried to deserialize unknown object type!");
		}

		internal void ReadStatics()
		{
			var numTypes = Reader.ReadInt32();

			var fieldFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

			for(var i = 0; i < numTypes; i++)
			{
				if(!Reader.ReadBoolean())
				{
					continue;
				}

				var type = ReadType();

				ReadObjectBaseTypeMembers(null, type, fieldFlags, false);
			}
		}

		private object ReadEnum()
		{
			var type = ReadType();
			var value = Read();

			return Enum.ToObject(type, value);
		}

		private object ReadReference()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();

			Dictionary<int, object> dict;
			if(_typeObjects.TryGetValue(type, out dict))
			{
				object obj;
				if(dict.TryGetValue(referenceId, out obj))
				{
					return obj;
				}
			}
			return null;
		}

		private void AddReference(Type type, object obj, int referenceId)
		{
			Dictionary<int, object> objects;
			if(!_typeObjects.TryGetValue(type, out objects))
			{
				objects = new Dictionary<int, object>();
				_typeObjects.Add(type, objects);
			}

			objects.Add(referenceId, obj);
		}

		#region Reference types
		private object ReadEntityComponent()
		{
			var referenceId = Reader.ReadInt32();
			var componentTypeGuid = (EntityComponent.GUID)Read();

			var type = EntityComponent.GetComponentTypeByGUID(componentTypeGuid);
			if(type == null)
			{
				ReadObjectMembers(null, null, false);
				return null;
			}

			object obj = null;
			try
			{
				obj = type != null ? Activator.CreateInstance(type) : null;
			}
			catch(MissingMethodException)
			{
				throw new MissingMethodException(string.Format(MissingMethodText, type.Name, nameof(EntityComponent)));
			}
			catch(MissingMemberException)
			{
				// Same error as MissingMethodException, but required for certain platforms.
				throw new MissingMemberException(string.Format(MissingMethodText, type.Name, nameof(EntityComponent)));
			}
			catch(MethodAccessException)
			{
				throw new MethodAccessException(string.Format(MethodAccessText, type.Name, nameof(EntityComponent)));
			}
			catch(MemberAccessException)
			{
				// Same error as MethodAccessException, but required for certain platforms.
				throw new MemberAccessException(string.Format(MethodAccessText, type.Name, nameof(EntityComponent)));
			}
			catch(Exception ex)
			{
				throw (ex);
			}

			if(type != null)
			{
				AddReference(type, obj, referenceId);
			}

			ReadObjectMembers(obj, type, true);

			return obj;
		}

		private object ReadObject(object existingObject, bool isConstructed, Type currentType)
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();

			if(existingObject == null && type != null && (currentType == null || type == currentType))
			{
				existingObject = FormatterServices.GetUninitializedObject(type);
			}

			if(type != null)
			{
				AddReference(type, existingObject, referenceId);
			}

			ReadObjectMembers(existingObject, type, isConstructed);

			return existingObject;
		}

		private void ReadObjectMembers(object obj, Type type, bool isConstructed)
		{
			var numBaseTypes = Reader.ReadInt32();
			var fieldFlags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

			ReadObjectBaseTypeMembers(obj, type, fieldFlags, isConstructed);

			for(var i = 0; i < numBaseTypes; i++)
			{
				var baseType = ReadType();

				ReadObjectBaseTypeMembers(obj, baseType, fieldFlags, isConstructed);
			}

			if(type != null && _deserializationCallbackType.IsAssignableFrom(type))
			{
				_deserializationMethod.Invoke(obj, new object[] { this });
			}
		}

		private void ReadObjectBaseTypeMembers(object obj, Type objectType, BindingFlags flags, bool setSerializedFieldsOnly)
		{
			var numFields = Reader.ReadInt32();
			var typeChanged = obj == null || objectType == null ? false : !objectType.IsAssignableFrom(obj.GetType());
			for(var iField = 0; iField < numFields; iField++)
			{
				var fieldName = Reader.ReadString();

				// If the objectType is null, it means the object was removed from the assembly. 
				// Perform the ReadInternal anyway to make sure the reader doesn't go out of sync.
				if(objectType == null)
				{
					ReadInternal();
					continue;
				}

				var fieldInfo = objectType?.GetField(fieldName, flags);

				var canWrite = !typeChanged // Writing a new type is not possible.
					&& fieldInfo != null // Can't write to non-existing field.
					&& !fieldInfo.IsLiteral // Can't write to const objects
					&& (obj == null) == fieldInfo.IsStatic // If the object is null it can't be written, unless the field is static.
					&& !fieldInfo.IsNotSerialized; // Don't serialize objects that have the NonSerialized attribute.

				var existingObject = canWrite ? fieldInfo.GetValue(obj) : null;
				var fieldValue = ReadInternal(existingObject, false, fieldInfo?.FieldType);

				if ((fieldInfo == null) || (fieldValue == null && fieldInfo.FieldType.IsValueType) || (fieldValue != null && fieldValue.GetType() != fieldInfo.FieldType))
				{
					// The type of the field has changed, so setting the value would be dangerous.
					continue;
				}

				if(canWrite)
				{
					// Check if the field is a backing field for a property.
					if(!setSerializedFieldsOnly)
					{
						fieldInfo.SetValue(obj, fieldValue);
					}
					else
					{
						bool SerializeValue = false;
						// Read properties with generated get; set; values first.
						var generated = fieldInfo.GetCustomAttribute<System.Runtime.CompilerServices.CompilerGeneratedAttribute>() != null;
						if(generated)
						{
							// Extract the property's name from the backing field's name.
							// Backing fields are in the format "<NameOfProperty>k_backingfield".
							int charIndex = fieldName.IndexOf('>');
							if(charIndex < 1)
							{
								continue;
							}
							var propertyName = fieldName.Substring(1, fieldName.IndexOf('>') - 1);
							var propertyInfo = objectType.GetProperty(propertyName, flags);
							if(propertyInfo == null)
							{
								continue;
							}

							// Only set the value of properties that are either EntityProperties, or marked with SerializeValue.
							SerializeValue = propertyInfo.GetCustomAttribute<EntityPropertyAttribute>(false) != null || propertyInfo.GetCustomAttribute<SerializeValueAttribute>(false) != null;
						}
						else
						{
							// Read fields that are marked as SerializeValue.
							SerializeValue = fieldInfo.GetCustomAttribute<SerializeValueAttribute>(false) != null;
						}

						// Only set the value if the field is marked as SerializeValue.
						if(SerializeValue)
						{
							fieldInfo.SetValue(obj, fieldValue);
						}
					}
				}
			}

			var numEvents = Reader.ReadInt32();
			for(var iEvent = 0; iEvent < numEvents; iEvent++)
			{
				var eventName = Reader.ReadString();

				if(Reader.ReadBoolean())
				{
					var delegateType = ReadType();
					var functionCount = Reader.ReadInt32();

					EventInfo eventInfo = objectType != null ? objectType.GetEvent(eventName, flags) : null;

					for(int i = 0; i < functionCount; i++)
					{
						var methodInfo = Read() as MethodInfo;
						var target = Read();

						Delegate parsedDelegate;

						if(target != null)
						{
							parsedDelegate = Delegate.CreateDelegate(delegateType, target, methodInfo);
						}
						else
						{
							parsedDelegate = Delegate.CreateDelegate(delegateType, methodInfo);
						}

						if(eventInfo != null)
						{
							var addMethod = eventInfo.GetAddMethod(true);
							addMethod.Invoke(obj, new[] { parsedDelegate });
						}
					}
				}
			}

			if(objectType != null)
			{
				// Special case, always get native pointers for SWIG director types
				var swigCPtr = objectType.GetField("swigCPtr", flags);
				if(swigCPtr != null)
				{
					// Start with director handling, aka support for cross-language polymorphism
					var swigDirectorConnect = objectType.GetMethod("SwigDirectorConnect", flags);
					if(swigDirectorConnect != null)
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

		private object ReadString()
		{
			var referenceId = Reader.ReadInt32();
			var stringValue = Reader.ReadString();

			AddReference(typeof(string), stringValue, referenceId);

			return stringValue;
		}

		private object ReadArray()
		{
			var referenceId = Reader.ReadInt32();

			var arrayLength = Reader.ReadInt32();
			var elementType = ReadType();

			var array = Array.CreateInstance(elementType, arrayLength);

			AddReference(array.GetType(), array, referenceId);

			for(var i = 0; i < arrayLength; i++)
			{
				array.SetValue(Read(), i);
			}

			return array;
		}

		private object ReadMemberInfo()
		{
			var referenceId = Reader.ReadInt32();

			var memberName = Reader.ReadString();

			var declaringType = ReadType();
			var memberType = (MemberTypes)Reader.ReadByte();

			var isStatic = Reader.ReadBoolean();
			var bindingFlags = BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic;
			if(isStatic)
			{
				bindingFlags |= BindingFlags.Static;
			}
			else
			{
				bindingFlags |= BindingFlags.Instance;
			}

			MemberInfo memberInfo = null;

			switch(memberType)
			{
				case MemberTypes.Method:
				{
					var parameterCount = Reader.ReadInt32();
					var parameters = new Type[parameterCount];

					for(int i = 0; i < parameterCount; i++)
						parameters[i] = ReadType();

					memberInfo = declaringType.GetMethod(memberName, bindingFlags, null, parameters, null);
					break;
				}
				case MemberTypes.Field:
				{
					memberInfo = declaringType.GetField(memberName, bindingFlags);
					break;
				}
				case MemberTypes.Property:
				{
					memberInfo = declaringType.GetProperty(memberName, bindingFlags);
					break;
				}
			}

			AddReference(memberInfo.GetType(), memberInfo, referenceId);

			return memberInfo;
		}

		private object ReadISerializeValue()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();
			var memberCount = Reader.ReadInt32();

			var serInfo = new SerializationInfo(type, Converter);

			for(var i = 0; i < memberCount; i++)
			{
				var memberName = Reader.ReadString();
				var memberType = ReadType();
				var memberValue = Read();

				if(memberType != null)
				{
					serInfo.AddValue(memberName, memberValue, memberType);
				}
			}

			var constructor = type.GetConstructor(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance, null, new Type[] { typeof(SerializationInfo), typeof(StreamingContext) }, null);
			var obj = constructor.Invoke(new object[] { serInfo, new StreamingContext(StreamingContextStates.CrossAppDomain) });

			if(_deserializationCallbackType.IsAssignableFrom(type))
			{
				_deserializationMethod.Invoke(obj, new object[] { this });
			}

			AddReference(type, obj, referenceId);

			return obj;
		}

		private Type ReadTypeWithReference()
		{
			var referenceId = Reader.ReadInt32();
			var type = ReadType();

#pragma warning disable RECS0035 // Possible mistaken call to 'object.GetType()'
			if(type != null)
			{
				AddReference(type.GetType(), type, referenceId);
			}
#pragma warning restore RECS0035 // Possible mistaken call to 'object.GetType()'

			return type;
		}

		private Type ReadType()
		{
			Type type = null;
			if(Reader.ReadBoolean())
			{
				var elementType = ReadType();
				return elementType.MakeArrayType();
			}
			else if(Reader.ReadBoolean())
			{
				string guid = Reader.ReadString();
				_guidLookup.TryGetValue(guid, out type);
			}

			var isGeneric = Reader.ReadBoolean();

			// Only set the type if it isn't set yet.
			// The GUID lookup type is prioritized.
			if(type == null)
			{
				string typeName = Reader.ReadString();
				type = GetType(typeName);
			}
			else
			{
				GetType(Reader.ReadString());
			}

			if(isGeneric)
			{
				var numGenericArgs = Reader.ReadInt32();
				var genericArgs = new Type[numGenericArgs];
				for(int i = 0; i < numGenericArgs; i++)
				{
					Type genericType = ReadType();
					genericArgs[i] = genericType;
				}

				if(type != null)
				{
					type = type.MakeGenericType(genericArgs);
				}
			}

			return type;
		}

		private Type GetType(string typeName)
		{
			if(typeName == null)
			{
				throw new ArgumentNullException(nameof(typeName));
			}

			if(typeName.Length == 0)
			{
				throw new ArgumentException(string.Format("{0} cannot have zero length!", nameof(typeName)));
			}

			var type = Type.GetType(typeName);
			if(type != null)
				return type;

			foreach(var assembly in AppDomain.CurrentDomain.GetAssemblies())
			{
				type = assembly.GetType(typeName);
				if(type != null)
					return type;
			}

			return null;
		}

		private Assembly ReadAssembly()
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
