// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;

namespace Statoscope
{
	public class UserMarkerRDI : RecordDisplayInfo<UserMarkerRDI>
	{
		public readonly UserMarkerLocation m_uml;
		public readonly LogView m_logView;

		public UserMarker DisplayUserMarker
		{
			get { return new UserMarker(BasePath, DisplayName); }
		}

		public UserMarkerRDI()
		{
		}

		public UserMarkerRDI(UserMarkerLocation uml, LogView logView)
			: base(uml.m_path, uml.GetNameForLogView(logView), 1.0f)
		{
			DisplayName = uml.m_name;
			m_uml = uml;
			m_logView = logView;
			Unhighlight();
		}

		static RGB s_highlightedColour = new RGB(0.0f, 1.0f, 0.0f);
		static RGB s_unhighlightedColour = new RGB(0.0f, 0.0f, 0.0f);

		public void Highlight()
		{
			m_colour = s_highlightedColour;
		}

		public void Unhighlight()
		{
			m_colour = s_unhighlightedColour;
		}

		public UserMarkerRDI GetIfDisplayed(UserMarkerLocation uml, LogView logView)
		{
			UserMarkerRDI umrdi = this;
			if (!umrdi.Displayed)
				return null;

			string[] locations = uml.m_path.PathLocations();
			foreach (string location in locations)
			{
				umrdi = (UserMarkerRDI)umrdi.m_children[location];
				if (!umrdi.Displayed)
					return null;
			}

			umrdi = (UserMarkerRDI)umrdi.m_children[uml.GetNameForLogView(logView)];
			return umrdi.Displayed ? umrdi : null;
		}
	}
}
