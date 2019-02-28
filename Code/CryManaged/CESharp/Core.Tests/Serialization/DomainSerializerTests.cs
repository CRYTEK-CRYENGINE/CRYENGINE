// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using CryEngine.Serialization;
using NUnit.Framework;

namespace Core.Tests.Serialization
{
	class DomainSerializerTests
	{
		#region Source code template
		private const string SourceTemplate = @"
using System;
using System.Collections.Generic;
using System.IO;
using CryEngine;
using CryEngine.Serialization;

[EntityComponent(Guid=""2A9E8901-CCC8-4721-B524-33E19F3431FD"")]
public class {0}
{
	public string _string = ""{1}"";
	public int _int = {2};
	public float _float = {3}f;

	private string _privateString = ""{1}"";
	private int _privateInt = {2};
	private float _privateFloat = {3}f;

	public bool IsTheSame({0} instance)
	{
		return _string == instance._string &&
		       _int == instance._int &&
		       MathHelpers.Approximately(_float, instance._float, 0.001f) &&
		       _privateString == instance._privateString &&
		       _privateInt == instance._privateInt &&
		       MathHelpers.Approximately(_privateFloat, instance._privateFloat, 0.001f);
	}
}

[Serializable]
public class Serializer : MarshalByRefObject, ISerializedObject
{
	public List<byte> data;

	public Serializer()
	{
		data = new List<byte>();

		using(var stream = new MemoryStream())
		{
			var classInstance = new {0}();

			var writer = new ObjectWriter(stream);
			writer.Write(classInstance);

			data = new List<byte>(stream.GetBuffer());
		}

	}

	public byte[] GetBuffer()
	{
		return data.ToArray();
	}

	public bool ValidateBuffer(byte[] buffer)
	{
		using(var stream = new MemoryStream(buffer))
		{
			var reader = new ObjectReader(stream);
			var serializedInstance = reader.Read() as {0};

			if(serializedInstance == null)
			{
				return false;
			}

			var classInstance = new {0}();
			return classInstance.IsTheSame(serializedInstance);
		}
	}
}";
		#endregion

		private delegate bool DataProcessingHandler(ISerializedObject serializer, ref byte[] data);

		[Test]
		public void Primitive_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					int myInt = 1337;

					var writer = new ObjectWriter(stream);
					writer.Write(myInt);
				}

				var reader = new ObjectReader(stream);

				var readInteger = (int)reader.Read();
				Assert.AreSame(readInteger.GetType(), typeof(int));
				Assert.AreEqual(readInteger, 1337);
			}
		}

		public class PrimitiveTestClass
		{
			public int Integer { get; set; }
			public float Float { get; set; }
		}

		[Test]
		public void Primitives_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var testObject = new PrimitiveTestClass();

					testObject.Integer = 75;
					testObject.Float = 13.37f;

					var writer = new ObjectWriter(stream);
					writer.Write(testObject);
				}

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(PrimitiveTestClass));

				var parsedObject = (PrimitiveTestClass)readObject;

				Assert.AreEqual(parsedObject.Integer, 75);
				Assert.AreEqual(parsedObject.Float, 13.37f);
			}
		}

		public class StringTestClass
		{
			public string String { get; set; }
		}

		[Test]
		public void String_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					StringTestClass testObject = new StringTestClass();

					testObject.String = "This string was brought to you by Crytek";

					var writer = new ObjectWriter(stream);
					writer.Write(testObject);
				}

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(StringTestClass));

				var parsedObject = (StringTestClass)readObject;

				Assert.AreEqual(parsedObject.String, "This string was brought to you by Crytek");
			}
		}

		[Test]
		public void Generic_List_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var list = new List<int>();

					for(var i = 0; i < 5; i++)
						list.Add(1 << i);

					var writer = new ObjectWriter(stream);
					writer.Write(list);
				}

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(List<int>));

				var parsedObject = (List<int>)readObject;

				Assert.AreEqual(parsedObject.Count, 5);

				for(var i = 0; i < parsedObject.Count; i++)
				{
					Assert.IsTrue((parsedObject[i] & (parsedObject[i] - 1)) == 0);
				}
			}
		}

		enum TestEnumeration
		{
			First = 15910,
			Second = 5102131,
			Third = 120213
		}

		[Test]
		public void Enum_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var myEnum = TestEnumeration.Second;

					var writer = new ObjectWriter(stream);
					writer.Write(myEnum);
				}

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(TestEnumeration));

				var enumObject = (TestEnumeration)readObject;

				Assert.AreEqual(enumObject, TestEnumeration.Second);
			}
		}

		interface Animal { }
		class Dog : Animal { }
		class Cat : Animal { }

		[Test]
		public void Polymorphism_Array_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var animals = new Animal[2];
					animals[0] = new Dog();
					animals[1] = new Cat();

					var writer = new ObjectWriter(stream);
					writer.Write(animals);
				}

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(Animal[]));

				var animalArray = readObject as Animal[];
				Assert.AreEqual(animalArray.Length, 2);

				Assert.AreEqual(animalArray[0].GetType(), typeof(Dog));
				Assert.AreEqual(animalArray[1].GetType(), typeof(Cat));
			}
		}

		[Test]
		public void Generic_Dictionary_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var dictionary = new Dictionary<string, float>();
					dictionary["first"] = 10.7f;
					dictionary["second"] = 17.5f;

					var writer = new ObjectWriter(stream);
					writer.Write(dictionary);
				}

				stream.Position = 0;
				var streamReader = new StreamReader(stream);
				var readString = streamReader.ReadToEnd();

				var reader = new ObjectReader(stream);

				var readObject = reader.Read();
				Assert.AreSame(readObject.GetType(), typeof(Dictionary<string, float>));

				var parsedObject = readObject as Dictionary<string, float>;
				Assert.AreEqual(parsedObject.Count, 2);

				Assert.IsTrue(parsedObject.ContainsKey("first"));
				Assert.IsTrue(parsedObject.ContainsKey("second"));

				Assert.AreEqual(parsedObject["first"], 10.7f);
				Assert.AreEqual(parsedObject["second"], 17.5f);
			}
		}

		[Test]
		public void Generic_Dictionary_With_Key_References_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var dictionary = new Dictionary<PrimitiveTestClass, int>();
					var firstPrimitive = new PrimitiveTestClass();
					firstPrimitive.Float = 70.5f;

					dictionary[firstPrimitive] = 1;

					var secondPrimitive = new PrimitiveTestClass();
					secondPrimitive.Float = 85.7f;

					dictionary[secondPrimitive] = 0;

					var writer = new ObjectWriter(stream);
					writer.Write(dictionary);

					writer.Write(firstPrimitive);
					writer.Write(secondPrimitive);
				}

				var reader = new ObjectReader(stream);

				var readDictionary = reader.Read() as Dictionary<PrimitiveTestClass, int>;

				var firstReadPrimitive = reader.Read() as PrimitiveTestClass;
				var secondReadPrimitive = reader.Read() as PrimitiveTestClass;

				var keyEnumerator = readDictionary.Keys.GetEnumerator();
				keyEnumerator.MoveNext();

				Assert.AreSame(keyEnumerator.Current, firstReadPrimitive);

				keyEnumerator.MoveNext();
				Assert.AreSame(keyEnumerator.Current, secondReadPrimitive);

				Assert.True(readDictionary.ContainsKey(firstReadPrimitive));
				Assert.True(readDictionary.ContainsKey(secondReadPrimitive));

				Assert.AreEqual(readDictionary[firstReadPrimitive], 1);
				Assert.AreEqual(readDictionary[secondReadPrimitive], 0);
			}
		}

		class StaticTest
		{
			public float Float { get; set; }

			public MemberInfo Member { get; set; }

			public static StaticTest instance = new StaticTest();

			public static MemberInfo StaticMember { get; set; }
		}

		// Test generic static member serialization
		public class StaticInstance<Q> where Q : new()
		{
			public static Q Instance = new Q();
		}

		[Test]
		public void Full_Assembly_Static_Members_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					StaticTest.instance.Float = 13.37f;
					StaticTest.instance.Member = StaticTest.instance.GetType().GetProperty("Float");
					StaticTest.StaticMember = typeof(StaticTest).GetProperty("Member");

					var assembly = Assembly.GetExecutingAssembly();

					var writer = new ObjectWriter(stream);
					writer.WriteStatics(assembly);

					// Reset value to see if we get it in statics
					StaticTest.instance.Float = 1012;
				}

				var reader = new ObjectReader(stream);
				reader.ReadStatics();

				Assert.AreEqual(StaticTest.instance.Float, 13.37f);
			}
		}

		public class GenericInstance<Q> where Q : new()
		{
			public Q Instance = new Q();
		}

		[Test]
		public void Generic_Member_Instance_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var genericInstance = new GenericInstance<PrimitiveTestClass>();

					var writer = new ObjectWriter(stream);
					writer.Write(genericInstance);
				}

				var reader = new ObjectReader(stream);
				var readValue = reader.Read() as GenericInstance<PrimitiveTestClass>;

				Assert.AreSame(readValue.GetType(), typeof(GenericInstance<PrimitiveTestClass>));
			}
		}

		class MyReferenceClass
		{
			public PrimitiveTestClass testClass = new PrimitiveTestClass();
			public PrimitiveTestClass testClassCopy;
		}

		[Test]
		public void ObjectReference_Test_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var value = new MyReferenceClass();
					value.testClassCopy = value.testClass;

					Assert.AreSame(value.testClass, value.testClassCopy);

					var writer = new ObjectWriter(stream);
					writer.Write(value);
				}

				var reader = new ObjectReader(stream);
				var readValue = reader.Read() as MyReferenceClass;

				Assert.AreSame(readValue.testClass, readValue.testClassCopy);
			}
		}

		[Test]
		public void IntPtr_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var ptr = new IntPtr(0xDEDEDE);

					var writer = new ObjectWriter(stream);
					writer.Write(ptr);
				}

				var reader = new ObjectReader(stream);
				var readValue = (IntPtr)reader.Read();

				Assert.AreEqual(readValue.ToInt64(), 0xDEDEDE);
			}
		}

		public class DelegateOwner
		{
			public event Action Event;

			public void NotifyEvent()
			{
				Event();
			}

			public Delegate GetDelegate()
			{
				return Event;
			}
		}

		class DelegateUser
		{
			public virtual void EventHappened()
			{
				throw new NotImplementedException();
			}
		}

		class DelegateContainer
		{
			public DelegateOwner Owner { get; set; }
			public DelegateUser User { get; set; }
		}

		[Test]
		public void Delegate_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var container = new DelegateContainer();

					container.Owner = new DelegateOwner();
					container.User = new DelegateUser();

					container.Owner.Event += container.User.EventHappened;

					var writer = new ObjectWriter(stream);
					writer.Write(container);
				}

				var reader = new ObjectReader(stream);
				var readContainer = reader.Read() as DelegateContainer;

				Assert.AreEqual(readContainer.Owner.GetDelegate().Target, readContainer.User);
				Assert.AreEqual(readContainer.Owner.GetDelegate().Target.GetType(), typeof(DelegateUser));
				Assert.AreEqual(readContainer.Owner.GetDelegate().Method.Name, "EventHappened");
			}
		}

		public static event Action<CryEngine.EventArgs<StringTestClass>> GenericEvent;

		private void DomainSerializerTests_GenericEvent(CryEngine.EventArgs<StringTestClass> e)
		{
			throw new NotImplementedException();
		}

		[Test]
		public void Generic_Static_Delegate_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var stringClass1 = new StringTestClass();
					stringClass1.String = "Testing1";

					var stringClass2 = new StringTestClass();
					stringClass2.String = "Testing2";

					GenericEvent += DomainSerializerTests_GenericEvent;

					var assembly = Assembly.GetExecutingAssembly();

					var writer = new ObjectWriter(stream);
					writer.WriteStatics(assembly);
				}

				var reader = new ObjectReader(stream);
				reader.ReadStatics();
			}
		}

		public class LambdaActionHolder
		{
			public Action method;
		}

		public class LambdaActionOwner
		{
			public void MyFunction() { throw new NotImplementedException(); }
		}

		[Test]
		public void Lambda_Action_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var actionHolder = new LambdaActionHolder();
					var actionOwner = new LambdaActionOwner();

					actionHolder.method = () => actionOwner.MyFunction();

					var writer = new ObjectWriter(stream);
					writer.Write(actionHolder);
				}

				var reader = new ObjectReader(stream);
				var readHolder = reader.Read() as LambdaActionHolder;

				//Assert.AreEqual(readHolder.method.Target.GetType(), typeof(LambaActionOwner));
			}
		}

		public static void MyFunction() { throw new NotImplementedException(); }
		public static void EmptyFunction() { throw new NotImplementedException(); }

		[Test]
		public void Delegate_Null_Target_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var handler = new Action(EmptyFunction);
					handler += () => MyFunction();

					var writer = new ObjectWriter(stream);
					writer.Write(handler);
				}

				var reader = new ObjectReader(stream);
				var readHolder = reader.Read() as Action;
			}
		}

		[Test]
		public void Multistep_Serialization_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var writer = new ObjectWriter(stream);

					var primitiveClass1 = new PrimitiveTestClass();
					primitiveClass1.Float = 20.5f;
					primitiveClass1.Integer = 77;
					writer.Write(primitiveClass1);

					var primitiveClass2 = new PrimitiveTestClass();
					primitiveClass2.Float = 86.7f;
					primitiveClass2.Integer = 12;
					writer.Write(primitiveClass2);
				}

				var reader = new ObjectReader(stream);
				var readPrimitiveClass1 = reader.Read() as PrimitiveTestClass;

				Assert.AreEqual(readPrimitiveClass1.Float, 20.5f);
				Assert.AreEqual(readPrimitiveClass1.Integer, 77);

				var readPrimitiveClass2 = reader.Read() as PrimitiveTestClass;

				Assert.AreEqual(readPrimitiveClass2.Float, 86.7f);
				Assert.AreEqual(readPrimitiveClass2.Integer, 12);
			}
		}

		[Test]
		public void Multistep_Serialization_References_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var writer = new ObjectWriter(stream);

					var primitiveClass1 = new PrimitiveTestClass();
					primitiveClass1.Float = 20.5f;
					primitiveClass1.Integer = 77;
					writer.Write(primitiveClass1);

					writer.Write(primitiveClass1);
				}

				var reader = new ObjectReader(stream);
				var readPrimitiveClass1 = reader.Read() as PrimitiveTestClass;

				Assert.AreEqual(readPrimitiveClass1.Float, 20.5f);
				Assert.AreEqual(readPrimitiveClass1.Integer, 77);

				var readPrimitiveClass2 = reader.Read() as PrimitiveTestClass;

				Assert.AreSame(readPrimitiveClass1, readPrimitiveClass2);
			}
		}

		public static class StaticPrimitiveContainer
		{
			public static PrimitiveTestClass TestClass { get; set; }
		}

		[Test]
		public void Multistep_Serialization_References_Static_NonStatic_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var myClass = new PrimitiveTestClass();

					StaticPrimitiveContainer.TestClass = myClass;
					StaticPrimitiveContainer.TestClass.Float = 5.7f;
					StaticPrimitiveContainer.TestClass.Integer = 32;

					var assembly = Assembly.GetExecutingAssembly();

					var writer = new ObjectWriter(stream);
					writer.WriteStatics(assembly);

					writer.Write(myClass);
				}

				var reader = new ObjectReader(stream);
				reader.ReadStatics();

				Assert.AreEqual(StaticPrimitiveContainer.TestClass.Float, 5.7f);

				var readClass = reader.Read() as PrimitiveTestClass;
				Assert.AreSame(readClass, StaticPrimitiveContainer.TestClass);
			}
		}

		class ReadOnlyOwnerClass
		{
			public readonly PrimitiveTestClass test;

			public ReadOnlyOwnerClass()
			{
				test = new PrimitiveTestClass();
				test.Float = 5;
			}
		}

		[Test]
		public void ReadOnlySerialization_With_MemoryStream()
		{
			using(var stream = new MemoryStream())
			{
				{
					var myClass = new ReadOnlyOwnerClass();

					var writer = new ObjectWriter(stream);
					writer.Write(myClass);
				}

				var reader = new ObjectReader(stream);

				var readClass = reader.Read() as ReadOnlyOwnerClass;
				Assert.AreEqual(readClass.test.Float, 5);
			}
		}

		class PropertyHolder
		{
			public float Float { get; set; } = 800.0f;
		}

		[Test]
		public void PropertyDefaultValue()
		{
			using(var stream = new MemoryStream())
			{
				{
					var myClass = new PropertyHolder();

					var writer = new ObjectWriter(stream);
					writer.Write(myClass);
				}

				var reader = new ObjectReader(stream);

				var readClass = reader.Read() as PropertyHolder;
				Assert.AreEqual(readClass.Float, 800.0);
			}
		}

		[Test]
		public void AssemblyLoad()
		{
			using(var stream = new MemoryStream())
			{
				{
					var myAssembly = AppDomain.CurrentDomain.GetAssemblies()[1];

					var writer = new ObjectWriter(stream);
					writer.Write(myAssembly);
				}

				var reader = new ObjectReader(stream);
				var readAssembly = reader.Read() as Assembly;
			}
		}

		private void GetAssemblyReferences(AssemblyName name, ref List<string> assemblies)
		{
			Assembly assembly = Assembly.ReflectionOnlyLoad(name.FullName);
			if(!assemblies.Contains(assembly.Location))
			{
				assemblies.Add(assembly.Location);

				foreach(var assemblyName in assembly.GetReferencedAssemblies())
				{
					GetAssemblyReferences(assemblyName, ref assemblies);
				}
			}
		}

		private List<string> GetReferencedAssemblies(Type type)
		{
			List<string> assemblies = new List<string>();

			GetAssemblyReferences(type.Assembly.GetName(), ref assemblies);
			return assemblies;
		}

		[Test]
		public void RenameClassAndCompile()
		{
			string stringValue = "Serialization test string";
			int intValue = CryEngine.Random.Range(int.MinValue, int.MaxValue);
			float floatValue = 12345.67f;

			string originalFilePath = null;
			string renamedFilePath = null;
			byte[] data = null;
			Exception exception = null;
			bool valid = false;

			InvokeOnSerializer(GetDataAction, "Original", stringValue, intValue, floatValue, ref data, out originalFilePath, out exception);
			if(exception == null)
			{
				valid = InvokeOnSerializer(ValidateDataAction, "Renamed", stringValue, intValue, floatValue, ref data, out renamedFilePath, out exception);
			}

			CleanUpRenameTest(originalFilePath, renamedFilePath);

			if(exception != null)
			{
				throw exception;
			}

			Assert.True(valid);
		}

		private bool GetDataAction(ISerializedObject serializer, ref byte[] data)
		{
			var buffer = serializer.GetBuffer();
			if(buffer != null)
			{
				data = CopyBuffer(buffer);
				return true;
			}
			throw new Exception("Unable to retrieve data from serialized object!");
		}

		private bool ValidateDataAction(ISerializedObject serializer, ref byte[] data)
		{
			var valid = serializer.ValidateBuffer(data);
			return valid;
		}

		private bool InvokeOnSerializer(DataProcessingHandler processDataAction, string uniqueName, string stringValue, int intValue, float floatValue, ref byte[] data, out string filePath, out Exception exception)
		{
			exception = null;
			filePath = null;

			string originalClassName = string.Format("{0}Class", uniqueName);
			string coreAssembly = typeof(ISerializedObject).Assembly.Location;
			var domain = AppDomain.CreateDomain(string.Format("{0}Domain", uniqueName));
			bool success = true;

			try
			{
				// Create an instance of the color struct, so the core library is loaded.
				domain.CreateInstanceFrom(coreAssembly, "CryEngine.Color");

				string generateResult;
				bool compiled = GenerateAssembly(coreAssembly, originalClassName, stringValue, intValue, floatValue, out generateResult);

				if(!compiled)
				{
					throw new CryEngine.Compilation.CompilationFailedException(string.Format("Failed to compile the source code!\n{0}", generateResult));
				}
				
				filePath = generateResult;
				var handle = domain.CreateInstanceFrom(filePath, "Serializer");
				var serializer = handle.Unwrap() as ISerializedObject;

				if(serializer == null)
				{
					throw new NullReferenceException("Getting the Serializer returned null!");
				}

				success = processDataAction(serializer, ref data);
			}
			catch(Exception ex)
			{
				exception = ex;
				success = false;
			}

			AppDomain.Unload(domain);
			return success;
		}

		private void CleanUpRenameTest(string originalFilePath, string renamedFilePath)
		{
			GC.Collect();
			GC.WaitForPendingFinalizers();
			GC.Collect();

			DeleteFile(originalFilePath);
			DeleteFile(renamedFilePath);
		}

		private byte[] CopyBuffer(byte[] buffer)
		{
			var newBuffer = new byte[buffer.Length];
			for(int i = 0; i < buffer.Length; ++i)
			{
				newBuffer[i] = buffer[i];
			}
			return newBuffer;
		}

		private void DeleteFile(string filePath)
		{
			if(string.IsNullOrWhiteSpace(filePath))
			{
				return;
			}

			if(File.Exists(filePath))
			{
				try
				{
					File.SetAttributes(filePath, FileAttributes.Normal);
					File.Delete(filePath);
				}
				catch
				{
					Console.WriteLine(string.Format("Unable to delete temporary file \"{0}\"", filePath));
				}
			}
		}

		#region Code Generating methods
		public bool GenerateAssembly(string referencedAssembly, string className, string stringValue, int intValue, float floatValue, out string result)
		{
			object[] testValues = new object[]
			{
				className,
				stringValue,
				intValue,
				floatValue
			};

			string sourceCode = SourceTemplate.Replace("{", "{{");
			sourceCode = sourceCode.Replace("}", "}}");
			for(int i = 0; i < testValues.Length; ++i)
			{
				string key = "{{" + i + "}}";
				string replace = "{" + i + "}";
				sourceCode = sourceCode.Replace(key, replace);
			}
			sourceCode = string.Format(sourceCode, testValues);

			var tempFile = Path.GetTempFileName();

			DeleteFile(tempFile);

			var outputAssemblyPath = Path.ChangeExtension(tempFile, "dll");
			var codeDomProvider = CodeDomProvider.CreateProvider("CSharp");
			var compilerParameters = new CompilerParameters();
			compilerParameters.GenerateExecutable = false;
			compilerParameters.IncludeDebugInformation = false;
			compilerParameters.GenerateInMemory = false;
			compilerParameters.OutputAssembly = outputAssemblyPath;

			if(referencedAssembly != null)
			{
				compilerParameters.ReferencedAssemblies.Add(referencedAssembly);
			}

			var compileResult = codeDomProvider.CompileAssemblyFromSource(compilerParameters, sourceCode);
			if(compileResult.Errors.HasErrors)
			{
				result = ErrorsToReadableString(compileResult.Errors);
				return false;
			}

			result = compileResult.PathToAssembly;

			if(!File.Exists(result))
			{
				result = string.Format("Missing assembly file at {0}!", result);
				return false;
			}
			return true;
		}

		private string ErrorsToReadableString(CompilerErrorCollection errors)
		{
			string message = "";
			foreach(var error in errors)
			{
				message += error.ToString() + Environment.NewLine;
			}
			return message;
		}
		#endregion
	}
}