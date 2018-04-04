//
// ColorIconSelector
//
// $Id: ColorIconSelector.jsx,v 1.4 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

// requires stdlib.js and ColorChooser.jsx
// See CSXCore.jsx for a sample usage

ColorIconSelector = function() {
};

//
// ColorIconSelector.configColorButton
//   This function adds a callback to iconBtn to launch a ColorChooser
//   to allow a user to select a new color. The color of the button changes
//   to reflect the chosen color.
//   The clrStr is the default color. It can be a color object, array, or
//   string. See ColorIconSelector.getColorIcon for format details for the
//   parameter.
//   When a color has been chosen, the iconBtn._color property will be
//   set to the corresponding color object. The button will also call
//   a new onColorChange callback when the color is changed.
//
ColorIconSelector.configColorButton = function(iconBtn, clrStr) {
  iconBtn.setColor = function(clr) {
    if (!clr) {
      return undefined;
    }
    var self = this;
    var icon = ColorIconSelector.getColorIcon(clr);
    if (icon) {
      self.icon = icon.file;
      self._color = icon.color;
      if (self.onColorChange) {
        self.onColorChange();
      }
    }
    return icon.color;
  };

  iconBtn.setColor(clrStr);

  iconBtn.onClick = function() {
    var pnl = this.parent;
    var self = this;
    var color = ColorChooser.run(self._color);

    if (color) {
      self.setColor(color);
    }
    return;
  };
};

//
// ColorIconSelector.getColorIcon (color)
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
//   by setting the property ColorIconSelector.getColorIcon.temp to
//  another folder.
//
//   The property ColorIconSelector.getColorIcon.cacheFiles controls whether or
//   not a new png icon file is generated with each request or the cache is
//   used.
//   Files are cached by default.
//
ColorIconSelector.getColorIcon = function(color) {
  var clr = undefined;

  if (!color) {
    return undefined;
  }

  function hexColorFromString(str) {
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

  // Try to make sense of the 'color' we've been given
  if (color.constructor == String) {
    clr = hexColorFromString(color);
    if (!clr) {
      clr = Stdlib.rgbFromString(color);
    }

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
  if (ColorIconSelector.getColorIcon.temp.constructor == String) {
    var f = new Folder(ColorIconSelector.getColorIcon.temp);

    if (!f.exists) {
      if (!f.create()) {
        f = Folder.temp;
      }
    }
    ColorIconSelector.getColorIcon.temp = f;

  } else if (!(ColorIconSelector.getColorIcon.temp instanceof Folder)) {
    ColorIconSelector.getColorIcon.temp = Folder.temp;
  }

  var cname = clr.rgb.hexValue;
  var file = new File(Folder.temp + '/' + cname + '.png');

  // this checks to see if we've already built the preview before
  if (ColorIconSelector.getColorIcon.cachesFiles) {
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

ColorIconSelector.getColorIcon.temp = Folder.temp;
ColorIconSelector.getColorIcon.cachesFiles = true;

"ColorIconSelector.jsx";
// EOF
