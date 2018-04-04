//
// ResizePanel
//  This is a simplified Image Resize panel; it only works in pixels.
//  See ResizePanel.test() for usage.
//
// $Id: ResizePanel.jsx,v 1.4 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

//
// disable includes if not testing...
//
//includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//include "xlib/stdlib.js"
//include "xlib/GenericUI.jsx"
//

ResizeOptions = function(obj, prefix) {
  var self = this;

  self.enabled = false;
  self.width = 1024;
  self.height = 1024;
  self.constrain = true;
  self.scale = true;  // styles
  self.resample = true;
  self.resampleMethod = ResampleMethod.BICUBIC;

  self._fitImage = false;

  if (obj) {
    if (prefix == undefined) {
      prefix = '';
    }
    var props = ResizeOptions.properties;
    for (var i = 0; i < props.length; i++) {
      var name = props[i];
      var oname = prefix + name;
      if (oname in obj) {
        self[name] = obj[oname];
      } else if (name == 'width' || name == 'height') {
        self[name] = '';
      }
    }
  }
};

ResizeOptions.properties = ["enabled", "width", "height", "constrain",
                            "scale", "resample", "resampleMethod"];

ResizeOptions.resizeImage = function(doc, opts, p) {
  p = p || '';

  if (!toBoolean(opts[p + 'enabled'])) {
    return;
  }
  function _ftn() {
    var desc = new ActionDescriptor();
    var w = opts[p + 'width'];
    if (w) {
      desc.putUnitDouble(cTID('Wdth'), cTID('#Pxl'),
                         toNumber(w));
    }
    var h = opts[p + 'height'];
    if (h) {
      desc.putUnitDouble(cTID('Hght'), cTID('#Pxl'),
                         toNumber(h));
    }

    if (!h && !w) {
      Error.runtimeError(9001, "Height and width not specifed for resizeImage");
    }

    if (!h || !w) {
      if (!opts[p + 'constrain']) {
        Error.runtimeError(9001, "Height or width not specifed for resizeImage");
      }
      desc.putBoolean( cTID('CnsP'), true );
    }

    desc.putBoolean(sTID('scaleStyles'),
                    toBoolean(opts[p + 'scaleStyles']));

    if (opts[p + 'resample']) {
      var id = ResizePanel.typeIdFromResampleMethod(opts[p + 'resampleMethod']);
      desc.putEnumerated(cTID('Intr'), cTID('Intp'), id);
    }
    executeAction(cTID('ImgS'), desc, DialogModes.NO);
  }

  Stdlib.wrapLC(doc, _ftn);
};


//
// ResizePanel
//
ResizePanel = function() {};

ResizePanel.DEFAULT_WIDTH = 200;
ResizePanel.DEFAULT_HEIGHT = 320;

ResizePanel.methodLabels = ["Bicubic", "BicubicSharper","BicubicSmoother",
                            "None", "Bilinear", "NearestNeighbor"];

ResizePanel.createPanel = function(pnl, ini, prefix) {
  var self = this;

  var xofs = 0;
  var yofs = 0;
  var tOfs = ((isCS3() || isCS4() || isCS5()) ? 3 : 0);
  var deltaY = 23;

  if (pnl.type == 'panel') {
    xofs += 5;
    yofs += 5;
  }

  pnl.prefix = prefix || '';

  var xx = xofs;
  var yy = yofs;

  var opts = new ResizeOptions(ini, pnl.prefix);

//   alert(listProps(ini));
//   alert(listProps(opts));

  //
  // Enabled
  //
  pnl.resizeEnabled = pnl.add('checkbox', [xx,yy,xx+100,yy+25], 'Resize');
  pnl.resizeEnabled.value = toBoolean(opts.enabled);

  pnl.resizeEnabled.onClick = function() {
    var pnl = this.parent;
    var st = pnl.resizeEnabled.value;
    var props = ["resizeWidthLabel", "resizeWidth", "resizeWidthTag",
      "resizeHeightLabel", "resizeHeight", "resizeHeightTag",
      "resizeConstrain", "resizeScale", "resample","resizeReset"];
    for (var i = 0; i < props.length; i++) {
      var w = props[i];
      pnl[w].enabled = st;
    }

    pnl.resampleMethod.enabled = st && pnl.resample.value;
  }

  xx += 100;

  // Reset
  pnl.resizeReset = pnl.add('button', [xx,yy,xx+80,yy+20], 'Reset...');

  pnl.resizeReset.onClick = function() {
    var pnl = this.parent;

    var f = this._file;

    if (f) {
      if (this._file == f) {
        pnl.resizeWidth.text = pnl._width;
        pnl.resizeHeight.text = pnl._height;
        return;
      }
      this._file = f;

      var xmd = XBridgeTalk.getMetadata(f);
      if (xmd) {
        var md = new Metadata(xmd);
        var x = toNumber(md.get("${EXIF:PixelXDimension}"));
        if (!isNaN(x)) {
          this._x = pnl.resizeWidth.text = x;
        }
        var y = toNumber(md.get("${EXIF:PixelYDimension}"));
        if (!isNaN(y)) {
          this._y = pnl.resizeHeight.text = y;
        }
      }

    } else {
      if (pnl._width) {
        pnl.resizeWidth.text = pnl._width;
      }
      if (pnl._height) {
        pnl.resizeHeight.text = pnl._height;
      }
    }
  }

  yy += deltaY;
  xx = xofs;

  //
  // Width
  //
  pnl.resizeWidthLabel = pnl.add('statictext',
                                 [xx,yy+tOfs,xx+60,yy+tOfs+23], 'Width:');
  xx += 60;
  pnl.resizeWidth = pnl.add('edittext', [xx,yy,xx+50,yy+23], 1024);
  var w = toNumber(opts.width) || '';
  pnl._width = w;
  pnl.resizeWidth.text = w;
  xx += 55;
  pnl.resizeWidthTag = pnl.add('statictext',
                               [xx,yy+tOfs,xx+20,yy+tOfs+25], 'px');
  yy += deltaY;
  xx = xofs;

  // Height
  pnl.resizeHeightLabel = pnl.add('statictext',
                                 [xx,yy+tOfs,xx+60,yy+tOfs+23], 'Height:');
  xx += 60;
  pnl.resizeHeight = pnl.add('edittext', [xx,yy,xx+50,yy+23], 1024);
  var h = toNumber(opts.height) || '';
  pnl._height = h;
  pnl.resizeHeight.text = h;
  xx += 55;
  pnl.resizeHeightTag = pnl.add('statictext',
                               [xx,yy+tOfs,xx+20,yy+tOfs+25], 'px');

  yy += deltaY;
  xx = xofs;

  pnl.resizeWidth.onChanging = pnl.resizeHeight.onChanging = function() {
    var pnl = this.parent;
    GenericUI.numericKeystrokeFilter.call(this);

    if (pnl.resizeConstrain.value && pnl.resizeConstrain._ratio && this.text) {
      if (this == pnl.resizeWidth) {
        var v = toNumber(this.text)/pnl.resizeConstrain._ratio;
        pnl.resizeHeight.text = Math.round(v);
      } else {
        var v = toNumber(this.text)*pnl.resizeConstrain._ratio;
        pnl.resizeWidth.text = Math.round(v);
      }
    }
  }

  // Constrain
  pnl.resizeConstrain = pnl.add('checkbox', [xx,yy,xx+180,yy+23],
                                'Constrain Proportions');
  pnl.resizeConstrain.value = toBoolean(opts.constrain);

  pnl.resizeConstrain.onClick = function() {
    var pnl = this.parent;
    if (this.value) {
      pnl.resizeConstrain._ratio = (toNumber(pnl.resizeWidth.text)/
                                    toNumber(pnl.resizeHeight.text));
    } else {
      pnl.resizeConstrain._ratio = undefined;
    }
  };

  yy += deltaY;
  xx = xofs;

  // Scale
  pnl.resizeScale = pnl.add('checkbox', [xx,yy,xx+180,yy+23],
                            'Scale Styles');
  pnl.resizeScale.value = toBoolean(opts.scale);

  yy += deltaY;
  xx = xofs;

  // Resample
  pnl.resample = pnl.add('checkbox', [xx,yy,xx+180,yy+23],
                         'Resample Image:');
  pnl.resample.value = toBoolean(opts.resample);

  pnl.resample.onClick = function() {
    var pnl = this.parent;
    var st = this.value;
    pnl.resampleMethod.enabled = st;
  };

  yy += deltaY;
  xx += 20;

  var methods = ResizePanel.methodLabels;
  var rsm = opts.resampleMethod;

  pnl.resampleMethod = pnl.add('dropdownlist', [xx,yy,xx+150,yy+22], methods);

  var it = pnl.resampleMethod[0];
  var items = pnl.resampleMethod.items;
  for (var i = 0; i < items.length; i++) {
    if (rsm == items[i].text.toLowerCase()) {
      it = items[i];
      break;
    }
  }
  pnl.resampleMethod.selection = it;

  yy += deltaY;
  xx = xofs;

  pnl.resizeConstrain.onClick();
  pnl.resizeEnabled.onClick();

  return pnl;
};

GenericUI.prototype.validateResizePanel = function(pnl, opts) {
  var self = this;

  var p = pnl.prefix;

  var st = pnl.resizeEnabled.value;
  opts[p + 'enabled'] = st;

 if (!st) {
    return opts;
  }

  var w = '';
  if (pnl.resizeWidth.text.trim() != '') {
    var n = toNumber(pnl.resizeWidth.text);
    if (isNaN(n) || n < 10) {
      return self.errorPrompt("Bad value for resize width");
    }
    w = n;
  }
  opts[p + 'width'] = w;

  var h = '';
  if (pnl.resizeHeight.text.trim() != '') {
    n = toNumber(pnl.resizeHeight.text);
    if (isNaN(n) || n < 10) {
      return self.errorPrompt("Bad value for resize height");
    }
    h = n;
  }
  opts[p + 'height']  = h;

  if (!h && !w) {
    return self.errorPrompt("Width and/or Height must be specified");
  }

  var cnst = pnl.resizeConstrain.value;
  opts[p + 'constrain'] = cnst;

  if (!w && !h) {
    if (cnst) {
      return self.errorPrompt("Width or Height must be specified");
    } else {
      return self.errorPrompt("Width and Height must be specified");
    }
  } else if ((!w || !h) && !cnst) {
    return self.errorPrompt("Width and Height must be specified");
  }

  opts[p + 'scale'] = pnl.resizeScale.value;

  opts[p + 'resample'] = pnl.resample.value;

  if (pnl.resample.value) {
    var str = pnl.resampleMethod.selection.text;
    opts[p + 'resampleMethod'] = str.toLowerCase();
  }

  return opts;
};

ResizePanel.refreshPanel = function(pnl, opts) {
  var self = this;

  if (opts.width) {
    pnl._width = toNumber(opts.width);
  }

  if (opts.height) {
    pnl.height = toNumber(opts.height);
  }

  pnl.resizeWidth.onChanging();
  pnl.resizeHeight.onChanging();
};

ResizePanel.strToResampleMethod = function(s) {
  s = s.toString();
  var str = s.replace(/(ResampleMethod\.)|(\s+)/, '').toLowerCase();
  var methods = ["bicubic", "bicubicsharper","bicubicsmoother",
    "none", "bilinear", "nearestneighbor"];

  if (methods.contains(str)) {
    return ResampleMethod[str.toUpperCase()];
  }

  return undefined;
};

ResizePanel.strFromResampleMethod = function(rsm) {
  var str = rsm.toString();

  return str.replace(/ResampleMethod\./, '').toLowerCase();
};
ResizePanel.typeIdFromResampleMethod = function(rsm) {
  var str = (rsm.constructor == String ?
             rsm.toLowerCase() : ResizePanel.strFromResampleMethod(rsm));

  if (str == 'bicubic') {
    return sTID(str);
  }
  if (str == 'bicubicsmoother') {
    return sTID('bicubicSmoother');
  }
  if (str == 'bicubicsharper') {
    return sTID('bicubicSharper');
  }
  if (str == 'none') {
    return sTID('none');
  }
  if (str == 'bilinear') {
    return sTID('bilinear');
  }
  if (str == 'nearestneighbor') {
    return sTID('nearestNeighbor');
  }

  return 0;
};

ResizePanel.test = function() {
  function TestDialog() {
    var self = this;

    self.title = "Resize Test Dialog";
    self.notesSize = 0;
    self.winRect = {
      x: 100,
      y: 100,
      w: 220,
      h: 320
    };
    self.documentation = undefined;
    self.iniFile = undefined;
    self.saveIni = false;
    self.hasBorder = false;
    self.center = false;
  };

  TestDialog.prototype = new GenericUI();

  TestDialog.prototype.createPanel = function(pnl, ini) {
    var self = this;

    self.resizePanel = pnl.add('group', pnl.bounds);

    ResizePanel.createPanel(self.resizePanel, ini, "resize_");
    return pnl;
  };

  TestDialog.prototype.validatePanel = function(pnl, opts) {
    var self = this;

    return self.validateResizePanel(self.resizePanel, opts);
  };

  try {
    var ui = new TestDialog();

    var ini = {};
    ini.resize_enabled = true;
    ini.resize_width = 2048;
    ini.resize_height = 2048;
    ini.resize_constrain = true;
    ini.resize_scale = false;  // styles
    ini.resize_resample = true;
    ini.resize_resampleMethod = ResampleMethod.BICUBICSHARPER;

    ui.process = function(opts, doc) {
      alert(listProps(opts));

      if (doc) {
        ResizeOptions.resizeImage(doc, opts, "resize_");
      }
    };

    var doc;
    if (app.documents.length > 0) {
      doc = app.activeDocument;
      ini.resize_width = doc.width.as("px");
      ini.resize_height = doc.height.as("px");
    }

    ui.exec(ini, doc);

  } catch (e) {
    alert(Stdlib.exceptionMessage(e));
  }
};

// ResizePanel.test();

"ResizePanel.jsx";
// EOF