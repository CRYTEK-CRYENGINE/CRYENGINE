//
// ColorChooser
//
// This script has undergone a lof of change. Larry's basic idea is still
// in there, but much of it has been reworked.
//
// This script is an extension of Larry Ligon's color picker script.
// Found at: http://www.ps-scripts.com/bb/viewtopic.php?t=659
//
// I have left as much of his code intact as is possible. I did fix
// one typo, added code to make the text entry fields functional,
// and set it up so that app.foreground is reset after the chooser is closed
//
// Description:
//   This class has one public function ('run') that will open an RGB
//   color selection dialog and return the selected color or 'undefined'
//   if the user canceled the operation.
//
// Usage:
//    var color = ColorChooser.run();
//
// $Id: ColorChooser.jsx,v 1.11 2012/06/15 00:50:49 anonymous Exp $
//
// Program Name:  Set the foreground color
//
// Author:        Larry B. Ligon
//
// Purpose:       This JavaScript will allow the user to change the foreground color
//

ColorChooser = function() {
};

ColorChooser.selectedColor = undefined;

ColorChooser.numericKeystrokeFilter = function() {
  if (this.text.match(/[^\d]/)) {
    this.text = this.text.replace(/[^\d]/g, '');
  }
};

ColorChooser.createDialog = function(defColor, winX, winY) {
  if (!winX) {
    winX = 100;
  }
  if (!winY) {
    winY= 100;
  }

  var winW = 400;
  var winH = 400;

  var bounds = { x : winX, y : winY, width : winW, height : winH };

  var win = new Window('dialog', 'Color Chooser' );

  if (!defColor) {
    defColor = app.foregroundColor;
  }

  win.orientation = 'column';

  win.updateColor = function() {
    var win = this;
    var clr = new SolidColor();
    clr.rgb.red   = Math.round(win.redPnl.cslider.value);
    clr.rgb.green = Math.round(win.greenPnl.cslider.value);
    clr.rgb.blue  = Math.round(win.bluePnl.cslider.value);
    win._color = clr;
    app.foregroundColor = clr;
  };

  function createChannelPanel(win, name) {
    var pnl = win.add("panel", undefined, name);
    pnl.updateColor = function() {
      var win = this.parent;
      win.updateColor();
    }
    pnl.alignChildren = "right";
    pnl.orientation = 'row';
    pnl.cslider = pnl.add('scrollbar', undefined,
                                255, 0, 255);
    pnl.cslider.csize = [100,20];
    pnl.cvalue = pnl.add('edittext');
    pnl.cvalue.preferRedSize = [40,25];

    pnl.cvalue.onChanging = ColorChooser.numericKeystrokeFilter;

    pnl.cvalue.onChange = function () {
      var pnl = this.parent;
      var cn = Number(pnl.cvalue.text);
      if (isNaN(cn)) {
        alert(pnl.name + " value is not a valid number");
      }
      if (cn > 255) {
        cn = 255;
        pnl.cvalue.text = 255;
      }
      if (cn < 0) {
        cn = 0;
        pnl.cvalue.text = 0;
      }
      pnl.cslider.value = cn;
      pnl.cslider.onChanging();
    };

    pnl.cvalue.text = Math.round(pnl.cslider.value);
    pnl.cslider.onChanging = function () {
      var pnl = this.parent;
      pnl.cvalue.text = Math.round(pnl.cslider.value);
      pnl.updateColor();
    };

    return pnl;
  }

  win.redPnl = createChannelPanel(win, "Red");
  win.greenPnl = createChannelPanel(win, "Green");
  win.bluePnl = createChannelPanel(win, "Blue");

  win.ok = win.add("button", undefined, "OK" );
  win.ok.onClick = function() {
    this.parent.close(1);
  };

  win.layout.layout(true);

  win.redPnl.cslider.value = defColor.rgb.red;
  win.redPnl.cslider.onChanging();
  win.greenPnl.cslider.value = defColor.rgb.green;
  win.greenPnl.cslider.onChanging();
  win.bluePnl.cslider.value = defColor.rgb.blue;
  win.bluePnl.cslider.onChanging();

  return win;
};

ColorChooser.runColorPicker = function(defColor) {
  if (isBridge()) {
    if (!defColor) {
      defColor = "0x000000";
    } else {
      defColor = "0x" + defColor.replace(/#/g, '');
    }

    var c = parseInt(defColor);
    var bytes = $.colorPicker(c);

    if (bytes != -1) {
      var str = Stdlib.longToHex(bytes);
      return str.substring(2);
    }
    return undefined;
  }


  var color = undefined;
  if (!defColor) {
    defColor = app.foregroundColor;
  }

  try {
    var bytes;
    var rgb = defColor.rgb;
    bytes = (rgb.red << 16) + (rgb.green << 8) + rgb.blue;
    bytes = $.colorPicker(bytes);
    
    if (bytes != -1) {
      var c = new SolidColor();
      c.rgb.red = (bytes >> 16);
      c.rgb.green = (bytes >> 8) & 0xFF;
      c.rgb.blue = bytes & 0xFF;
      color = c;
    }
  } catch (e) {
    alert(e);
  }

  return color;
};

ColorChooser.run = function(def) {
  var rev = toNumber(app.version.match(/^\d+/)[0]);
  if (rev > 10 || isBridge()) {
    return ColorChooser.runColorPicker(def);
  }

  var fgOrig = app.foregroundColor;
  var fgColor = app.foregroundColor;
  if (def) {
    app.foregroundColor = def;
    fgColor = def;
  }

  try {
    var win = ColorChooser.createDialog(fgColor);
    var rc = win.show();

    if (rc == 1) {
      ColorChooser.selectedColor = win._color;
    }

  } finally {
    app.foregroundColor = fgOrig;
  }

  return ColorChooser.selectedColor;
};

//
// Sample usage
//
ColorChooser.main = function() {
  var c = ColorChooser.run();
  if (c) {
    c = c.rgb;
    alert("RGB=[" + c.red + ", " + c.green + ", " + c.blue + "]");
  } else {
    alert("No color chosen");
  }
};
//ColorChooser.main();

"ColorChooser.jsx";
// EOF

