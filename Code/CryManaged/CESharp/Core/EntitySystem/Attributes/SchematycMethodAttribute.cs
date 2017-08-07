// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Attributes;

namespace CryEngine
{
    /// <summary>
    /// Exposes the specified method to Schematyc
    /// </summary>
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class SchematycMethodAttribute : Attribute
    {
        public SchematycMethodAttribute() { }
    }
}