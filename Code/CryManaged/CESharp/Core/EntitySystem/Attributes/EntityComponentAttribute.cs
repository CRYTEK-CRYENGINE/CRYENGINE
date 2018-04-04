// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;


namespace CryEngine
{
	/// <summary>
	/// Used to specify specific parameters for entity components, if not used defaults will be in place
	/// Default name is the entity component class name
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
    public sealed class EntityComponentAttribute : Attribute
    {
        #region Properties
        /// <summary>
        /// Display name inside CRYENGINE's EntitySystem as well as Sandbox.
        /// </summary>
        public string Name { get; set; }
        /// <summary>
        /// Entity category inside Sandbox.
        /// </summary>
        public string Category { get; set; }
        /// <summary>
        /// User friendly description of what the component is for.
        /// </summary>
        public string Description { get; set; }
        /// <summary>
        /// Editor icon string, i.e. "icons:designer/Designer_Box.ico"
        /// </summary>
        public string Icon { get; set; }
        #endregion

        #region Constructors
        public EntityComponentAttribute(string uiName = "", string category = "", string description = "", string icon = "")
        {
            Name = uiName;
            Category = category;
            Description = description;
            Icon = icon;
        }
        #endregion
    }
}
