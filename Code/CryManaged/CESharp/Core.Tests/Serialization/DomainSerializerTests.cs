using CryEngine.Serialization;
using NUnit.Framework;
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;

namespace Core.Tests.Serialization
{
    class DomainSerializerTests
    {
        [Test]
        public void Primitive_With_MemoryStream()
        {
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
            {
                {
                    var list = new List<int>();

                    for (var i = 0; i < 5; i++)
                        list.Add(1 << i);

                    var writer = new ObjectWriter(stream);
                    writer.Write(list);
                }

                var reader = new ObjectReader(stream);

                var readObject = reader.Read();
                Assert.AreSame(readObject.GetType(), typeof(List<int>));

                var parsedObject = (List<int>)readObject;

                Assert.AreEqual(parsedObject.Count, 5);

                for (var i = 0; i < parsedObject.Count; i++)
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            public event CryEngine.EventHandler Event;

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
            using (var stream = new MemoryStream())
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
        
        public static event CryEngine.EventHandler<CryEngine.EventArgs<StringTestClass>> GenericEvent;

        private void DomainSerializerTests_GenericEvent(CryEngine.EventArgs<StringTestClass> e)
        {
            throw new NotImplementedException();
        }

        [Test]
        public void Generic_Static_Delegate_With_MemoryStream()
        {
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
            {
                {
                    var handler = new CryEngine.EventHandler(EmptyFunction);
                    handler += () => MyFunction();

                    var writer = new ObjectWriter(stream);
                    writer.Write(handler);
                }

                var reader = new ObjectReader(stream);
                var readHolder = reader.Read() as CryEngine.EventHandler;
            }
        }

        [Test]
        public void Multistep_Serialization_With_MemoryStream()
        {
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
            using (var stream = new MemoryStream())
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
    }
}