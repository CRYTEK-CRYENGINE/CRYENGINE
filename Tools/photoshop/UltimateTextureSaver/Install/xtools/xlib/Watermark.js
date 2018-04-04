//
// Watermark
//
// History:
//  2006-08-06 1.7 Complete rewrite
//  2004-09-20 v0.6 Added sample code (comment inline) for extreme panoramas
//  2004-09-19 v0.5 Cleaned up code. Rationalized names. Added docs.
//  2004-09-17 v0.1 Creation date
//
// $Id: Watermark.js,v 1.20 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2008
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

var gWatermarkVersion = 0.8;

// =======================================================

WatermarkOptions = function(obj) {
  var self = this;

  self.style = "Watermark Style";
  self.shape = "Watermark Shape";
  self.layer = "Watermark Layer";

  self.size  = 10.0; // percent of largest document dimension
  self.ofsX  = 5.0;  // offset from the horizontal side
  self.ofsY  = 5.0;  // offset from the verical side

  self.font = "ArialMT";
  self.fontSize = 42;
  self.fontColor = "0,0,0";

  self.copyrightText = '';
  self.copyrightNotice = '';
  self.copyrightUrl = '';
  self.copyrighted = "unmarked";

  self.getCopyrightedType = function() {
    var s = this.copyrighted.toString().toLowerCase();
    if (s == "true" || s == "copyrighted") {
      s = CopyrightedType.COPYRIGHTEDWORK;
    } else if (s == "false" || s == "" || s == "unmarked") {
      s = CopyrightedType.UNMARKED;
    } else if (s == "publicdomain") {
      s = CopyrightedType.PUBLICDOMAIN;
    }
    return s;
  };

  self.setCopyrightedType = function(c) {
    switch (c) {
      case CopyrightedType.COPYRIGHTEDWORK: {
        this.copyrighted = "copyrighted";
        break;
      }
      case CopyrightedType.UNMARKED:  {
        this.copyrighted = "unmarked";
        break;
      }
      case CopyrightedType.PUBLICDOMAIN: {
        this.copyrighted = "publicdomain";
        break;
      }
      default: {
      }
    }
  }

  self.getFontColor = function() {
    var c = this.fontColor;
    if (!(c instanceof SolidColor)) {
      if (c.constructor == String) {
        c = c.split(',');
      }
      if (c instanceof Array) {
        var rgbc = new SolidColor();
        rgbc.rgb.red = c[0];
        rgbc.rgb.green = c[1];
        rgbc.rgb.blue = c[2];
        c = rgbc;
      } else {
        c = undefined;
      }
    }

    return c;
  }
  self.setFontColor = function(c) {
    this.fontColor = c.rgb.red + "," + c.rgb.green + "," + c.rgb.blue;
  }

  if (obj) {
    for (var idx in obj) {
      var v = obj[idx];
      if (typeof v != 'function') {
        self[idx] = v;
      }
    }
  }
};

WatermarkOptions.prototype.typename = "WatermarkOptions";

// ================================= Watermark ===============================
Watermark = function Watermark() {};

Watermark.exec = function(doc, opts) {
  opts = new WatermarkOptions(opts);

  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;

    // compute the bounding box for the watermark shape
    var bnds = Watermark.computeBounds(doc, Number(opts.ofsX),
                                       Number(opts.ofsY),
                                       Number(opts.size));

    var marked = true;
    if (opts.shape) {
      // place the shape
      Watermark.placeShape(doc, opts.shape, bnds);

      Watermark.deselectActivePath(doc);

    } else if (opts.copyrightNotice || opts.copyrightText) {
      var text = (opts.copyrightText ?
                  opts.copyrightText : opts.copyrightNotice);
      var x = opts.ofsX * doc.width.value / 100.00;
      var y = opts.ofsY * doc.height.value / 100.00;
      Watermark.addCopyrightText(doc, text,
                                 x, y, opts.font, opts.fontSize,
                                 opts.getFontColor());
    } else {
      marked = false;
    }

    if (marked) {
      // set the style
      if (opts.style) {
        try {
          doc.activeLayer.applyStyle(opts.style);
        } catch (e) {
          alert(e.toSource());
        }
      }

      // set the layer name
      if (opts.layer) {
        doc.activeLayer.name = opts.layer;
      }
    }

    // set any copyright info requested
    if (opts.copyrightNotice != opts.NULL_STRING) {
      doc.info.copyrightNotice = opts.copyrightNotice;
    }
    if (opts.copyrighted != CopyrightedType.UNMARKED) {
      doc.info.copyrighted = opts.getCopyrightedType();
    }
    if (opts.copyrightUrl != opts.NULL_STRING) {
      doc.info.ownerUrl = opts.copyrightUrl;
    }

  } catch (e) {
    alert(e.toSource());

  } finally {
    app.preferences.rulerUnits = ru;
  }

  return;
};


//
// returns the bounds of the watermark to be placed...
// top, left, bottom, right
//
Watermark.computeBounds = function(doc, ofsX, ofsY, size) {
  var height = doc.height.value;
  var width = doc.width.value;

  // calc the size in pixels based on the largest dimension
  if (height > width) {
    size = height * size / 100.0;
  } else {
    size = width * size / 100.0;
  }

  // compute the horizontal points
  var x;
  var rx;
  if (ofsX > 0) {
    x = ofsX * width / 100.0;
    rx = x + size;
  } else {
    rx = width - Math.abs(ofsX) * width / 100.0;
    x = rx - size;
  }

  // compute the vertical points
  var y;
  var ry;

  if (ofsY > 0) {
    y = ofsY * height / 100.0;
    ry = y + size;
  } else {
    ry = height - Math.abs(ofsY) * height / 100.0;
    y = ry - size;
  }

  // The set up is all done.
  var b = new Object();
  b.top = y;
  b.left = x;
  b.bottom = ry;
  b.right = rx;

  return b;
};

cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

Watermark.placeShape = function(doc, shape, bounds) {
  if (app.activeDocument != doc) {
    app.activeDocument = doc;
  }
  function _ftn() {
    // This is the lovely ScriptListener output for dropping a shape
    // into a document with position, size, and style information
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putClass(sTID('contentLayer'));
    desc.putReference(cTID('null'), ref);
    var tdesc = new ActionDescriptor();
    tdesc.putClass(cTID('Type'), sTID('solidColorLayer'));
    var sdesc = new ActionDescriptor();
    sdesc.putString(cTID('Nm  '), shape);
    sdesc.putUnitDouble(cTID('Top '), cTID('#Pxl'), bounds.top);
    sdesc.putUnitDouble(cTID('Left'), cTID('#Pxl'), bounds.left);
    sdesc.putUnitDouble(cTID('Btom'), cTID('#Pxl'), bounds.bottom);
    sdesc.putUnitDouble(cTID('Rght'), cTID('#Pxl'), bounds.right);
    tdesc.putObject(cTID('Shp '), sTID('customShape'), sdesc);
    desc.putObject(cTID('Usng'), sTID('contentLayer'), tdesc);
    executeAction(cTID('Mk  '), desc, DialogModes.NO);
  }

  _ftn();
};

Watermark.addCopyrightText = function(doc, contents, x, y, font, size, color) {
  var layer = doc.artLayers.add();
  layer.name = "WM Layer";

  layer.kind = LayerKind.TEXT;
  layer.blendMode = BlendMode.NORMAL;
  layer.opacity = 100.0;

  var text = layer.textItem;
  var ru = app.preferences.rulerUnits;
  var tu = app.preferences.typeUnits;

  try {
    app.preferences.typeUnits = TypeUnits.POINTS;
    app.preferences.rulerUnits = Units.PIXELS;

    text.contents = contents;

    text.size = size;
    text.font = font;
    text.color = color;

    text.kind = (contents.match(/\r|\n/) ?
                 TextType.PARAGRAPHTEXT : TextType.POINTTEXT);

    var bnds = layer.bounds;
    var width = (bnds[2].value - bnds[0].value);
    var height = (bnds[3].value - bnds[1].value);

    if (x < 0) {
      x = doc.width.value + x - width;
    }
    if (y < 0) {
      y = doc.height.value + y - height;
    }

    text.position = new Array(x, y);

  } finally {
    app.preferences.rulerUnits = ru;
    app.preferences.typeUnits = tu;
  }

  return layer;
};

Watermark.deselectActivePath = function(doc) {
  if (app.activeDocument != doc) {
    app.activeDocument = doc;
  }
  function _ftn() {
    var ref = new ActionReference();
    ref.putClass(cTID("Path"));

    var desc = new ActionDescriptor();
    desc.putReference(cTID("null"), ref);
    executeAction( cTID( "Dslc" ), desc, DialogModes.NO );
  };
  _ftn();
};

"Watermark.js";
// EOF

