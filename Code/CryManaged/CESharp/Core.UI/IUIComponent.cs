using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine.UI.Components
{
	public interface IUIComponent
	{
		bool HitTest(int x, int y);

		UIElement Owner { get; set; }
	}
}
