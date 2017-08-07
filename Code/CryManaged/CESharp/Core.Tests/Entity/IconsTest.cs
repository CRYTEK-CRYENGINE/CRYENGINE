// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Attributes;

using System.IO;
using System;
using System.Collections.Generic;

namespace Entity.Tests
{
	[TestFixture]
	public class IconTests
	{
		static string enginePath = null;
		const string CRY_ENGINE_ROOT = "CryEngineRoot"; 
		
		[OneTimeSetUp]
		public void CheckEnvironment()
		{
			enginePath = Environment.GetEnvironmentVariable(CRY_ENGINE_ROOT);
			if(enginePath == null)
			{
				Assert.Ignore("CryEngineRoot environment variable not defined");
			}
		}
		
		[Test]
		public void ValidateIconsFileLocation()
		{
			int totalNumberOfIcons = 77;
			string objectIconsPath = Path.Combine(enginePath,"Editor\\ObjectIcons");
			
			// to verify that path exists bitmap files exist
			bool folderExists = Directory.Exists(objectIconsPath);
			Assert.IsTrue(folderExists, "Icons directory "+ objectIconsPath + " does not exists!");

			int actualFileCount = new System.Collections.Generic.List<String>(Directory.EnumerateFiles(objectIconsPath)).Count;
			Assert.IsTrue(actualFileCount == totalNumberOfIcons, "Actual number of files different from test case input , Expected :" + totalNumberOfIcons + ", Actual :" + actualFileCount);

			// engine path
			var enumsList = Enum.GetValues(typeof(IconType));
			int noOfFiles = 0;
			foreach(IconType iconType in enumsList)
			{
				var enumType = iconType.GetType();
				var enumMemberInfo = enumType.GetMember(iconType.ToString());
				IconPathAttribute iconPathAttribute = (IconPathAttribute)enumMemberInfo[0].GetCustomAttributes(typeof(IconPathAttribute), true)[0];

				if(iconPathAttribute.Path != null) // "None" has no path (null)
				{
					++noOfFiles;
					//check if file exists
					string filePath = Path.Combine(objectIconsPath, iconPathAttribute.Path);
					bool filePathExists = File.Exists(filePath);
					Assert.IsTrue(filePathExists, "File path " + filePath + " does not exists");
				}
			}
			
			Assert.IsTrue(totalNumberOfIcons == actualFileCount, "Number of physical icons and enum values do not match ! Expected :"+totalNumberOfIcons+ ", Actual :"+ actualFileCount);
			
		}
	}
}