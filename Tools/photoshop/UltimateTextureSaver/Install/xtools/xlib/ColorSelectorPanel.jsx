//
// ColorSelectorPanel
//    pnl.fontColor = pnl.add('group', [xx,yy,xx+300,yy+45]);
//    ColorSelectorPanel.createPanel(pnl.fontColor, opts.fontColor, '');
//    ...
//    var color = pnl.fontColor.getColor();
//
// $Id: ColorSelectorPanel.jsx,v 1.5 2012/06/15 00:50:49 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

ColorSelectorPanel = function() {
};

ColorSelectorPanel.temp = Folder.temp;
ColorSelectorPanel.cachesFiles = true;

ColorSelectorPanel.RGB = "RGB...";

ColorSelectorPanel.rgbFromString = function(str) {
  var rex = /([\d\.]+),([\d\.]+),([\d\.]+)/;
  var m = str.match(rex);
  if (m) {
    return ColorSelectorPanel.createRGBColor(Number(m[1]),
                                 Number(m[2]),
                                 Number(m[3]));
  }
  return undefined;
};
ColorSelectorPanel.rgbToString = function(c) {
  return c.rgb.red + "," + c.rgb.green + "," + c.rgb.blue;
};

ColorSelectorPanel.createRGBColor = function(r, g, b) {
  var c = new RGBColor();
  if (r instanceof Array) {
    b = r[2]; g = r[1]; r = r[0];
  }
  c.red = parseInt(r); c.green = parseInt(g); c.blue = parseInt(b);
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

ColorSelectorPanel.COLOR_BLACK = ColorSelectorPanel.createRGBColor(0, 0, 0);
ColorSelectorPanel.COLOR_WHITE = ColorSelectorPanel.createRGBColor(255, 255, 255);

ColorSelectorPanel.hexColorFromString = function(str) {
  var clr = undefined;
  var hexColorRex = /^#?[A-Fa-f0-9]{6}$/;

  if (str.constructor == String) {
    if (str.match(hexColorRex)) {
      clr = new SolidColor();
      if (str[0] == '#') {
        str = str.slice(1);
      }

      clr.rgb.hexValue = str;
    }
  } else if (str instanceof SolidColor) {
    clr = str;
  }

  return clr;
};

ColorSelectorPanel.colorFromString = function(str) {
  var c = undefined;

  if (str && str.constructor == String) {
    str = str.toLowerCase();

    if (str.match(/,/)) {
      c = ColorSelectorPanel.rgbFromString(str);
    } else if (str[0] == '#') {
      c = ColorSelectorPanel.hexColorFromString(color);
    } else if (str == "black") {
      c = ColorSelectorPanel.COLOR_BLACK;
    } else if (str == "white") {
      c = ColorSelectorPanel.COLOR_WHITE;
    } else if (str == "foreground") {
      c = app.foregroundColor;
    } else if (str == "background") {
      c = app.backgroundColor;
    }
  }

  return c;
};

ColorSelectorPanel.stringFromColor = function(clr) {
  var str = '';

  if (clr && clr.typename == 'SolidColor') {
    if (clr.rgb.hexValue == '000000') {
      str = 'Black';
    } else if (clr.rgb.hexValue == 'FFFFFF') {
      str = 'White';
    } else if (clr.isEqual(app.foregroundColor)) {
      str = "Foreground";
    } else if (clr.isEqual(app.backgroundColor)) {
      str = "Background";
    } else {
      str = ColorSelectorPanel.rgbToString(clr);
    }
  }

  return str;
};
ColorSelectorPanel.menuItemFromColor = function(clr) {
  var str = ColorSelectorPanel.stringFromColor(clr);

  if (str.match(/,/)) {
    str = ColorSelectorPanel.RGB;
  }
  return str;
};

//
// ColorSelectorPanel.configColorButton
//   This function adds a callback to iconBtn to launch a ColorChooser
//   to allow a user to select a new color. The color of the button changes
//   to reflect the chosen color.
//   The clrStr is the default color. It can be a color object, array, or
//   string. See ColorSelectorPanel.getColorIcon for format details for the
//   parameter.
//   When a color has been chosen, the iconBtn._color property will be
//   set to the corresponding color object. The button will also call
//   a new onColorChange callback when the color is changed.
//
ColorSelectorPanel.configColorButton = function(iconBtn) {

  iconBtn.onColorChange = function() {
    var pnl = this.parent;
    var clr = pnl.colorIconButton._color;

    var menuItem = ColorSelectorPanel.menuItemFromColor(clr);

    var it = pnl.colorList.find(menuItem);
    it.selected = true;
    pnl.colorList.selection = it;
  };

  iconBtn.setColor = function(clr) {
    var self = this;

    if (!clr) {
      return;
    }
    var icon = ColorSelectorPanel.getColorIcon(clr);
    if (icon) {
      self.icon = icon.file;
      self._color = icon.color;

      if (self.onColorChange) {
        self.onColorChange();
      }
    }
    return icon.color;
  };

  iconBtn.onClick = function() {
    var self = this;
    var pnl = this.parent;
    var color = ColorChooser.run(self._color);

    pnl._settingColor = true;
    if (color) {
      self.setColor(color);
    }
    pnl._settingColor = false;

    return color;
  };

  iconBtn.setColor("Black");
};

//
// ColorSelectorPanel.getColorIcon (color)
//   Returns an plain object with two fields  or undefined if it couldn't
//               figure out what 'color' was.
//       file:   is a 40x20 png of the specified color
//       color:  is the actual underlying SolidColor. If you passed a
//               SolidColor object, that's what you get back, if not, this
//               is what happened when your 'color' got converted
//
//   'color' can be a SolidColor, RGBColor, Array, or a String.
//   If it's an Array, it must be 3 RGB numbers.
//   If it's a String, it must be in one of these formats:
//     "255,255,255"
//     "#FFFFFF"
//     "FEFEFE"
//
//   The png file that is returned has a name format like this: 'FFFF00.png'
//   and it created in the Folder.temp directory. This folder can be overridden
//   by setting the property ColorSelectorPanel.getColorIcon.temp to
//  another folder.
//
//   The property ColorSelectorPanel.getColorIcon.cacheFiles controls whether or
//   not a new png icon file is generated with each request or the cache is
//   used.
//   Files are cached by default.
//
ColorSelectorPanel.getColorIcon = function(color) {
  var clr = undefined;

  if (!color) {
    return undefined;
  }

  // Try to make sense of the 'color' we've been given
  if (color.constructor == String) {
    clr = ColorSelectorPanel.colorFromString(color);

  } else if (color.constructor == Array && color.length == 3) {
    clr = Stdlib.createRGBColor(color);

  } else if (color instanceof RGBColor) {
    clr = new SolidColor();
    clr.rgb = color;

  } else if (color instanceof SolidColor) {
    clr = color;
  }

  if (!clr) {
    return undefined;
  }

  // Now lets make sure that we have a good 'temp' Folder
  if (ColorSelectorPanel.temp.constructor == String) {
    var f = new Folder(ColorSelectorPanel.temp);

    if (!f.exists) {
      if (!f.create()) {
        f = Folder.temp;
      }
    }
    ColorSelectorPanel.temp = f;

  } else if (!(ColorSelectorPanel.temp instanceof Folder)) {
    ColorSelectorPanel.temp = Folder.temp;
  }

  var cname = clr.rgb.hexValue;
  var file = new File(ColorSelectorPanel.temp + '/' + cname + '.png');

  // this checks to see if we've already built the preview before
  if (ColorSelectorPanel.cachesFiles) {
    if (file.exists) {
      return { file: file, color: clr };
    }
  }

  var ru = app.preferences.rulerUnits;
  app.preferences.rulerUnits = Units.PIXELS;

  try {
    var doc = Stdlib.newDocument(cname, "RGBM", 40, 20, 72, 8);

  } finally {
    app.preferences.rulerUnits = ru;
  }

  doc.selection.selectAll();
  doc.selection.fill(clr, ColorBlendMode.NORMAL, 100);
  var saveOpts = new PNGSaveOptions();
  doc.saveAs(file, saveOpts, true);
  doc.close(SaveOptions.DONOTSAVECHANGES);

  return { file: file, color: clr };
};

ColorSelectorPanel.createPanel = function(pnl, defaultColor, label, lwidth) {
  var xx = 0;
  var yy = 0;
  var tOfs = ((CSVersion() > 2) ? 3 : 0);

  if (pnl.type == 'panel') {
    xx += 5;
    yy += 5;
  }

  if (!defaultColor) {
    defaultColor = "Black";
  }

  if (label == undefined) {
    label = "Color:";
    lwidth = 40;
  }

  if (label != '') {
    pnl.colorListLabel = pnl.add("statictext",
                                 [xx,yy+tOfs,xx+lwidth,yy+22+tOfs],
                                 label);
    xx += lwidth;
  }

  pnl.colorList = pnl.add('dropdownlist',
                          [xx,yy,xx+130,yy+22],
                          ['Black',
                           'White',
                           'Foreground',
                           'Background',
                           ColorSelectorPanel.RGB]);

  pnl.colorList.isRGB = function() {
    return this.selection && this.selection.text == ColorSelectorPanel.RGB;
  }

  xx += 150;
  pnl.colorIconButton = pnl.add('iconbutton', [xx,yy,xx+44,yy+24],
                              undefined, {style: 'button'});

  ColorSelectorPanel.configColorButton(pnl.colorIconButton);

  pnl.colorList.onChange = function() {
    var pnl = this.parent
    if (pnl._settingColor) {
      return;
    }

    var txt = pnl.colorList.selection.text;
    var clr = ColorSelectorPanel.colorFromString(txt);

    if (clr) {
      pnl.colorIconButton.setColor(clr);

    } else {
      clr = pnl.colorIconButton.onClick();

      if (!clr && pnl._lastMenuItem && pnl._lastMenuItem !=  txt) {
        pnl._settingColor = true;

        txt = pnl._lastMenuItem;
        var it = pnl.colorList.find(txt);

        if (it) {
          pnl.colorList.selection = it;
        } else {
          pnl.colorList.selection = pnl.colorList.items[0];
        }

        pnl._settingColor = false;
      }
    }

    pnl._lastMenuItem = txt;
  }

  pnl._settingColor = false;
  pnl.setColor = function(color) {
    var pnl = this;

    if (pnl._settingColor) {
      return;
    }
    pnl._settingColor = true;

    try {
      if (!color) {
        return;
      }

      if (color.constructor == String) {
        color = ColorSelectorPanel.colorFromString(color);
      }

      if (color && color.typename == "SolidColor") {
        var txt = ColorSelectorPanel.menuItemFromColor(color);

        var it = pnl.colorList.find(txt);
        // notification is turned off...

        if (it) {
          pnl.colorList.selection = it;
        } else {
          pnl.colorList.selection = pnl.colorList.items[0];
        }
        pnl._lastMenuItem = pnl.colorList.selection.text;

        pnl.colorIconButton.setColor(color);
      }

    } finally {
      pnl._settingColor = false;
    }
  }

  pnl.getColor = function() {
    var pnl = this;
    var ctxt = pnl.colorList.selection.text;
    var cstr;

    if (ctxt == ColorSelectorPanel.RGB) {
      cstr = Stdlib.rgbToString(pnl.colorIconButton._color);

    } else {
      cstr = ctxt;
    }

    return pnl.colorIconButton._color;
  }

  //
  // Set the initial color
  //
  pnl.setColor(defaultColor);

  return pnl;
};

"ColorSelectorPanel.jsx";
// EOF
