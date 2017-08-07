// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Attributes;

namespace CryEngine
{
    /// <summary>
    /// Exposes the specified signal to Schematyc
    /// </summary>
    [AttributeUsage(AttributeTargets.Delegate)]
    public sealed class SchematycSignalAttribute : Attribute
    {
        public SchematycSignalAttribute() { }
    }
}