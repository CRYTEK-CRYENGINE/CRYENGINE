//
// WatermarkUI
//
// $Id: WatermarkUI.jsx,v 1.33 2014/11/27 05:50:31 anonymous Exp $
// Copyright: ©2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
app;
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//include "xlib/stdlib.js"
//include "xlib/GenericUI.jsx"
//include "xlib/ColorChooser.jsx"
//include "xlib/ColorSelectorPanel.jsx"
//include "xlib/PresetsManager.jsx"
//include "xlib/ShapesFile.js"
//include "xlib/PreviewWindow.jsx"
//include "xlib/Styles.js"

WatermarkUIOptions = function(obj) {
  var self = this;

  // Shape settings
  self.shapeName = "Watermark Shape";
  self.shapeSize = "10 %";    // percent of largest document dimension

  // Image settings
  self.imagePath = "~/Desktop/Watermark.jpg";
  self.imageSize = "10 %";    // percent of largest document dimension

  // Text settings
  self.watermarkText = 'xbytor';

  self.font = "Arial";
  self.fontSize = 42;

  self.color = "Black";

  self.layerStyle = null;
  self.layerName  = "Watermark Layer";

  self.valign  = 'Middle';   // Top,Middle,Bottom
  self.halign  = 'Center';   // Left,Center,Right

  self.vspace  = "50 px";    // offset from the verical side
  self.hspace  = "50 px";    // offset from the horizontal side

  self.watermarkType = "text";   // text or shape

  // Copyright settings
  self.copyrightNoticeEnabled = false;
  self.copyrightNotice = '';
  self.copyrightUrlEnabled = false;
  self.copyrightUrl = '';
  self.copyrightedEnabled = false;
  self.copyrighted = "unmarked";

  Stdlib.copyFromTo(obj, self);
};
WatermarkUIOptions.prototype.typename = "WatermarkUIOptions";

WatermarkUIOptions.INI_FILE = Stdlib.PREFERENCES_FOLDER + "/watermark.ini";
WatermarkUIOptions.LOG_FILE = Stdlib.PREFERENCES_FOLDER + "/watermark.log";
WatermarkUIOptions.PREVIEW_IMG = Stdlib.PREFERENCES_FOLDER + "/wm-preview.png";
WatermarkUIOptions.DEFAULT_PRESETS_FOLDER = Stdlib.PREFERENCES_FOLDER;
WatermarkUIOptions.DESCRIPTOR_KEY = sTID('WatermarkOptions');


WatermarkUIOptions.prototype.rationalize = function() {
  var self = this;

  if (!self.watermarkType && !self.watermarkText) {
    self.watermarkType = "shape";
  } else {
    self.watermarkType = (self.watermarkType.toLowerCase());
  }

  if (self.watermarkText) {
//     self.watermarkText = self.watermarkText.replace(/\\n/g, "\r");
  }

  if (self.copyrightNotice) {
    self.copyrightNotice = self.copyrightNotice.replace(/\\n/g, "\r");
  }

  var oprops = ["shapeSize","vspace","hspace","imageSize", "fontSize"];

  for (var i = 0; i < oprops.length; i++) {
    var idx = oprops[i];
    var un = UnitValue(self[idx]);

    if (un.type == '?') {
      var type = 'px';
      var defVal = 100;

      if (idx == 'fontSize') {
        type = 'pt';
        defVal = 42;
      }
      un = new UnitValue(0, type);

      var num = toNumber(self[idx]);
      un.value = isNaN(num) ? defVal : num;
    }

    self[idx] = un;
  }

  if (self.imagePath) {
    self.imagePath = Stdlib.convertFptr(self.imagePath);
  }

  if (self.font && self.font.constructor == String) {
    self.font = toFont(self.font);
  }

  if (self.color && self.color.constructor == String) {
    var c = Stdlib.colorFromString(self.color);

    if (c) {
      self.color = c;
    }
  }
//   self.fontSize = toNumber(self.fontSize);

  self.copyrightNoticeEnabled = toBoolean(self.copyrightNoticeEnabled);
  self.copyrightUrlEnabled = toBoolean(self.copyrightUrlEnabled);
  self.copyrightedEnabled = toBoolean(self.copyrightedEnabled);

  if (self.copyrighted && self.copyrighted.constructor == String) {
    var str = self.copyrighted.toLowerCase().replace(/\s/g, '');
    var s = '';

    if (str == "true" || str == "copyrighted") {
      s = CopyrightedType.COPYRIGHTEDWORK;
    } else if (str == "false" || str == "" ||
               str == "unmarked" || str == "unknown") {
      s = CopyrightedType.UNMARKED;
    } else if (str == "publicdomain") {
      s = CopyrightedType.PUBLICDOMAIN;
    }

    self.copyrighted = s;
  }
};

WatermarkUIOptions.prototype.toIni = function() {
  var self = this;
  var obj = {};

  Stdlib.copyFromTo(self, obj);

  var oprops = ["shapeSize","fontSize","vspace","hspace","imageSize"];
  for (var i = 0; i < oprops.length; i++) {
    var idx = oprops[i];
    obj[idx] = obj[idx].toString();
  }

  var color = obj.color;
  if (color instanceof SolidColor) {
    obj.color = Stdlib.rgbToString(color);

  } else if (color.constructor == String) {
    var str = color;
    var c = undefined;
    if (str == "Black") {
      c = Stdlib.COLOR_BLACK;
    } else if (str == "White") {
      c = Stdlib.COLOR_WHITE;
    } else if (str == "Foreground") {
      c = app.foregroundColor;
    } else if (str == "Background") {
      c = app.backgroundColor;
    }
    if (c) {
      obj.color = Stdlib.rgbToString(c);
    }
  }

  if (obj.imagePath && (obj.imagePath instanceof File)) {
    obj.imagePath = obj.imagePath.toUIString();
  }

  return obj;
};

WatermarkUIOptions.toDescriptor = function(opts) {
  if (!(opts instanceof WatermarkUIOptions)) {
    opts = new WatermarkUIOptions(opts);
    opts.rationalize();
  }
  if (opts.color.constructor != String) {
    opts.color = Stdlib.rgbToString(opts.color);
  }

  var desc = new ActionDescriptor();
  var str = Stdlib.toIniString(opts);
  desc.putString(WatermarkUIOptions.DESCRIPTOR_KEY, str);

  return desc;
};

WatermarkUIOptions.prototype.toDescriptor = function() {
  return WatermarkUIOptions.toDescriptor(this);
};

WatermarkUIOptions.fromDescriptor = function(desc, opts) {
  if (desc.hasKey(WatermarkUIOptions.DESCRIPTOR_KEY)) {
    var str = desc.getString(WatermarkUIOptions.DESCRIPTOR_KEY);
    opts = Stdlib.fromIniString(str, opts);
  }
  return opts;
};
WatermarkUIOptions.prototype.fromDescriptor = function(desc) {
  return WatermarkUIOptions.fromDescriptor(desc, this);
};

WatermarkUI = function() {
  var self = this;

  self.title = "XWatermark"; // our window title
  self.notesSize = 0;          // no notes
  self.winRect = {             // the size of our window
    x: 100,
    y: 100,
    w: 525,
    h: 720
  };
  self.documentation = undefined; // no notes/docs

  self.iniFile = WatermarkUIOptions.INI_FILE;
  self.saveIni = true;
  self.optionsClass = WatermarkUIOptions;

  self.processTxt = "OK";
  self.setDefault = false;

  self.previewFile = undefined;
  self.presetsFolder = undefined;
  self.styles = {};   // name->ActionDescriptor for dynamically loaded styles
  self.shapes = {};   // name->Shape Info for dynamically loaded shapes
};
WatermarkUI.prototype = new GenericUI();
WatermarkUI.TEMP_STYLE_NAME = "[Temp Style]";

WatermarkUI.prototype.createPanel = function(pnl, ini) {
  var self = this;
  var xOfs = 10;
  var yy = 10;
  var xx = xOfs;
  var gutter = 130;
  var tOfs = ((isCS3() || isCS4() || isCS5()) ? 3 : 0);
  var txtWidth = 300;

  pnl.mgr = self;

  var opts = new WatermarkUIOptions(ini);   // default values

  opts.rationalize();

  if (ini.uiX == undefined) {
    ini.uiX = ini.uiY = 100;
  }

  if (isWindows() && $.screens[0].bottom <= 768) {
    ini.uiX = 0;
    ini.uiY = 100;
  }

  //
  // Watermark Type Selectors
  //

  // restore the window's location
  self.moveWindow(toNumber(opts.uiX), toNumber(opts.uiY));

  pnl.shapeCheck = pnl.add("radiobutton",
                           [xx,yy+tOfs,xx+gutter-30,yy+22+tOfs],
                           "Shape:");

  pnl.imageCheck = pnl.add("radiobutton",
                          [xx,yy+tOfs+35*2,xx+gutter-30,yy+22+tOfs+35*2],
                          "Image:");

  pnl.textCheck = pnl.add("radiobutton",
                          [xx,yy+tOfs+35*4,xx+gutter-30,yy+22+tOfs+35*4],
                          "Text:");

  xOfs = 30;
  gutter -= 20;

  xx = xOfs + gutter;

  //
  // Shape Name
  //
  pnl.shapeName = pnl.add("dropdownlist", [xx,yy,xx+160,yy+22]);
  var pm = new PresetsManager();
  pm.populateDropdownList(PresetType.CUSTOM_SHAPES, pnl.shapeName, true);
  var it = pnl.shapeName.find(opts.shapeName);
  if (it) {
    it.selected = true;
  } else {
    pnl.shapeName.items[0].selected = true;
  }

  pnl.shapeName.add('separator');
  pnl.shapeName.add('item', 'Load Shape...');

  pnl.shapeName.onChange = function() {
    var pnl = this.parent;

    if (pnl.shapeName.selection.text != 'Load Shape...') {
      return;
    }

    try {
      var mgr = pnl.mgr;
      var shapeInfo = mgr.loadShape(pnl);

      if (shapeInfo) {
        var it = pnl.shapeName.add('item', shapeInfo.name);
        pnl.shapeName.selection = it;

        mgr.shapes[shapeInfo.name] = shapeInfo;
      }

    } catch (e) {
      var msg = (e + '@' + (e.line || '??'));
      alert(msg);
    }
  }

  //
  // Preview button
  //
  var pfile = new File(WatermarkUIOptions.PREVIEW_IMG);
  if (pfile.exists) {
    self.previewFile = pfile;
  }
  if (app.documents.length || pfile.exists) {
    xx += 260;
    pnl.preview = pnl.add("button", [xx,yy,xx+80,yy+22], 'Preview...');
    pnl.preview.onClick = function() {
      var win = this.window;
      var mgr = win.mgr;
      mgr.preview();
    }
  }

  pnl.shapeCheck.onClick = pnl.textCheck.onClick = pnl.imageCheck.onClick =
  function() {
    var pnl = this.parent;
    var isShape = pnl.shapeCheck.value;
    var isImage = pnl.imageCheck.value;
    var isText = pnl.textCheck.value;

    var flds = ["shapeName", "sizeLabel", "shapeSize"];
    for (var i = 0; i < flds.length; i++) {
      pnl[flds[i]].enabled = isShape;
    }

    var flds = ["imagePath", "imageBrowse", "imageSizeLabel", "imageSize"];
    for (var i = 0; i < flds.length; i++) {
      pnl[flds[i]].enabled = isImage;
    }

    var flds = ["watermarkText", "fontLabel", "font"];

    for (var i = 0; i < flds.length; i++) {
      pnl[flds[i]].enabled = isText;
    }
  }

  yy += 38;

  xx = xOfs;

  //
  // Shape Size
  //
  pnl.sizeLabel = pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
                          'Size:');
  xx += gutter;
  pnl.shapeSize = pnl.add('edittext', [xx,yy,xx+75,yy+20],
                          (opts.shapeSize || "5 %"));
  pnl.shapeSize.onChanging = GenericUI.unitValueKeystrokeFilter;

  yy += 35;
  xx = xOfs + gutter;

  //
  // Image Path
  //
  var textType = (isMac() ? 'statictext' : 'edittext');
  pnl.imagePath = pnl.add(textType, [xx,yy,xx+txtWidth,yy+23],
                          '', { readonly : true});

  if (opts.imagePath) {
    pnl.imagePath.text = Stdlib.convertFptr(opts.imagePath).toUIString();
  }

  xx += txtWidth + 5;
  var bnds = [xx,yy+1,xx,yy+20+1];

  pnl.imageBrowse = pnl.add('button', [xx,yy+1,xx+10,yy+20], '...');
  pnl.imageBrowse.bounds.width = 30;

  pnl.imageBrowse.onClick = function() {
    try {
      var pnl = this.parent;
      pnl.imageBrowse.bounds.width = 30;

      var def = (pnl.imagePath.text ?
                 new File(pnl.imagePath.text) : Folder.current);
      var f = Stdlib.selectFileOpen('Browse for Watermark Image',
                                    undefined, def);
      if (f) {
        pnl.imagePath.text = decodeURI(f.fsName);
      }
    } catch (e) {
      var msg = (e + '@' + (e.line || '??'));
      alert(msg);
    }
  }
  yy += 35;
  xx = xOfs;

  //
  // Image Size
  //
  pnl.imageSizeLabel = pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
                               'Size:');
  xx += gutter;
  pnl.imageSize = pnl.add('edittext', [xx,yy,xx+75,yy+20],
                              (opts.imageSize || "5 %"));
  pnl.imageSize.onChanging = GenericUI.unitValueKeystrokeFilter;

  yy += 35;
  xx = xOfs + gutter;

  //
  // Watermark Text
  //
  var text = opts.watermarkText || '';
  //   text = text.replace(/\\n/g, "\r");

  pnl.watermarkText = pnl.add('edittext', [xx,yy,xx+txtWidth,yy+20], text);

  yy += 35;
  xx = xOfs;

  //
  // Font
  //
  pnl.fontLabel = pnl.add('statictext',
                          [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
                          'Font:');

  xx += gutter;
  pnl.font = pnl.add('group', [xx,yy,xx+440,yy+30]);
  self.createFontPanel(pnl.font, opts, '');

  pnl.font.sizeLabel.visible = false;
  pnl.font.fontSize.bounds.width += 20;
  pnl.font.fontSize.onChanging = GenericUI.unitValueKeystrokeFilter;

  pnl.font.setFont(opts.font, opts.fontSize);

  pnl.font.getFont = function() {
    var pnl = this;
    var font = pnl.style.selection.font;
    return { font: font.postScriptName, size: pnl.fontSize.text };
  }

  xx = xOfs;

  yy += 45;

  var xOfs = 10;
  var xx = xOfs;
  var gutter = 130;

  //
  // LayerName
  //
  pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs], 'Layer Name:');
  xx += gutter;
  pnl.layerName = pnl.add('edittext', [xx,yy,xx+140,yy+20],
                          opts.layerName || '');

  yy += 40;
  xx = xOfs;

  //
  // Color
  //
  pnl.colorLabel = pnl.add('statictext',
                               [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
                               'Watermark Color:');
  xx += gutter;
  pnl.color = pnl.add('group', [xx,yy,xx+300,yy+40]);
  ColorSelectorPanel.createPanel(pnl.color, opts.color, '');

  yy += 40;
  xx = xOfs;

  //
  // Layer Style
  //
  pnl.layerStyleLabel = pnl.add("statictext",
                                [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
                                "Layer Style:");
  xx += gutter;

  pnl.layerStyle = pnl.add("dropdownlist", [xx,yy,xx+200,yy+22]);
  var pm = new PresetsManager();
  pm.populateDropdownList(PresetType.STYLES, pnl.layerStyle, true);

  var it = pnl.layerStyle.find(opts.layerStyle);
  if (it) {
    it.selected = true;
  } else {
    pnl.layerStyle.items[0].selected = true;
  }

  pnl.layerStyle.add('separator');
  pnl.layerStyle.add('item', 'Load Style...');
  pnl.layerStyle.add('item', 'Define Style...');

  pnl.layerStyle.onChange = function() {
    var pnl = this.parent;

    if (pnl.layerStyle.selection.text == 'Load Style...') {
      try {
        var mgr = pnl.mgr;
        var sdesc = mgr.loadStyle(pnl);

        if (sdesc) {
          var fileInfo = sdesc.getObjectValue(cTID('FlIn'));
          var name = fileInfo.getString(cTID('Nm  '));
          var it = pnl.layerStyle.add('item', name);
          pnl.layerStyle.selection = it;
          mgr.styles[name] = sdesc;
        }

      } catch (e) {
        var msg = (e + '@' + (e.line || '??'));
        alert(msg);
      }
    }

    if (pnl.layerStyle.selection.text == 'Define Style...') {
      try {
        var mgr = pnl.mgr;
        if (!isCS4()) { // PSBUG
          self.window.visible = false;
        }
        var sdesc = mgr.defineStyle(pnl);

        if (sdesc) {
          var lefx = sdesc.getObjectValue(cTID('T   '));
          var name = WatermarkUI.TEMP_STYLE_NAME;
          var it = pnl.layerStyle.find(name);
          if (!it) {
            it = pnl.layerStyle.add('item', name);
          }

          pnl.layerStyle.selection = it;
          mgr.styles[name] = lefx;
        }

      } catch (e) {
        var msg = (e + '@' + (e.line || '??'));
        alert(msg);

      } finally {
        if (!isCS4()) { // PSBUG
          self.window.visible = true;
        }
      }
    }
  }

  yy += 35;
  xx = xOfs;

  //
  // Vertical Alignment
  //
  pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs], 'Alignment:');
  xx += gutter;
  pnl.valign = pnl.add('dropdownlist', [xx,yy,xx+140,yy+20],
                       ['Top','Middle', 'Bottom']);

  var idx = 1;
  if (opts.valign == 'Top')    idx = 0;
  if (opts.valign == 'Middle') idx = 1;
  if (opts.valign == 'Bottom') idx = 2;
  pnl.valign.items[idx].selected = true;

  pnl.valign.onChange = function() {
    var pnl = this.parent;
    pnl.vspace.enabled = (pnl.valign.selection.text != 'Middle');
  }

  xx += 145;

  //
  // Horizontal Alignment
  //
  pnl.halign = pnl.add('dropdownlist', [xx,yy,xx+138,yy+20],
                       ['Left','Center', 'Right']);

  var idx = 1;
  if (opts.halign == 'Left')   idx = 0;
  if (opts.halign == 'Center') idx = 1;
  if (opts.halign == 'Right')  idx = 2;
  pnl.halign.items[idx].selected = true;

  pnl.halign.onChange = function() {
    var pnl = this.parent;
    pnl.hspace.enabled = (pnl.halign.selection.text != 'Center');
  }

  yy += 35;
  xx = xOfs;

  //
  // Vertical Space
  //
  pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs], 'Vertical Offset:');
  xx += gutter;
  pnl.vspace = pnl.add('edittext', [xx,yy,xx+75,yy+20],
                       opts.vspace || "50 px");
  pnl.vspace.onChanging = GenericUI.unitValueKeystrokeFilter;


  yy += 35;
  xx = xOfs;

  //
  // Horizontal Space
  //
  pnl.add('statictext', [xx,yy+tOfs,xx+gutter,yy+22+tOfs],
          'Horizontal Offset:');
  xx += gutter;
  pnl.hspace = pnl.add('edittext', [xx,yy,xx+75,yy+20],
                       opts.hspace || "50 px");
  pnl.hspace.onChanging = GenericUI.unitValueKeystrokeFilter;

  yy += 35;
  xx = xOfs;

  pnl.valign.onChange();
  pnl.halign.onChange();

  //
  // Copyrighted
  //
  var en = 17;
  pnl.copyrightedEnabled = pnl.add('checkbox',
                                   [xx,yy,xx+en,yy+22]);
  pnl.copyrightedEnabled.value = opts.copyrightedEnabled;
  pnl.copyrightedEnabled.onClick = function() {
    var pnl = this.parent;
    var st = pnl.copyrightedEnabled.value;;
    pnl.copyrighted.enabled = st;
  };

  xx += en;

  pnl.copyrightedLabel = pnl.add('statictext',
                                 [xx,yy+tOfs,xx+gutter-en,yy+22+tOfs],
                                 'Copyright Status:');

  xx += gutter - en;

  var lst = ["Unknown", "Copyrighted", "Public Domain"];
  pnl.copyrighted = pnl.add("dropdownlist", [xx,yy,xx+160,yy+22], lst);
  var it = pnl.copyrighted.find(opts.copyrighted);
  if (it) {
    it.selected = true;
  } else {
    pnl.copyrighted.items[0].selected = true;
  }

  pnl.copyrightedEnabled.onClick();

  yy += 35;
  xx = xOfs;

  //
  // Copyright Notice
  //
  pnl.copyrightNoticeEnabled = pnl.add('checkbox',
                                   [xx,yy,xx+en,yy+22]);
  pnl.copyrightNoticeEnabled.value = opts.copyrightNoticeEnabled;
  pnl.copyrightNoticeEnabled.onClick = function() {
    var pnl = this.parent;
    var st = pnl.copyrightNoticeEnabled.value;;
    pnl.copyrightNotice.enabled = st;
  };

  xx += en;

  var notice = opts.copyrightNotice || '';
  notice = notice.replace(/\\n/g, "\r");
  pnl.copyrightNoticeLabel = pnl.add('statictext',
                                     [xx,yy+tOfs,xx+gutter-en,yy+22+tOfs],
                                     'Copyright Notice:');
  xx += gutter-en;
  pnl.copyrightNotice = pnl.add('edittext', [xx,yy,xx+txtWidth,yy+90],
                                notice, {multiline:true});
  yy += 90;
  pnl.formatNote = pnl.add('statictext',
                           [xx,yy+tOfs,xx+txtWidth,yy+22+tOfs],
                           '[Ctrl-Enter(XP)/Ctrl-m(OSX) to add a line]');

  pnl.copyrightNoticeEnabled.onClick();

  yy += 30;
  xx = xOfs;

  //
  // Copyright Url
  //
  pnl.copyrightUrlEnabled = pnl.add('checkbox',
                                   [xx,yy,xx+en,yy+22]);
  pnl.copyrightUrlEnabled.value = opts.copyrightUrlEnabled;
  pnl.copyrightUrlEnabled.onClick = function() {
    var pnl = this.parent;
    var st = pnl.copyrightUrlEnabled.value;
    pnl.copyrightUrl.enabled = st;
  };

  xx += en;

  pnl.copyrightUrlLabel = pnl.add('statictext',
                                     [xx,yy+tOfs,xx+gutter-en,yy+22+tOfs],
                                     'Copyright URL:');
  xx += gutter - en;
  pnl.copyrightUrl = pnl.add('edittext', [xx,yy,xx+txtWidth,yy+20],
                             opts.copyrightUrl || '');

  pnl.copyrightUrlEnabled.onClick();

  yy += 45;
  xx = xOfs;

  if (opts.watermarkType == "shape") {
    pnl.shapeCheck.value = true;
    pnl.shapeCheck.onClick();

  } else if (opts.watermarkType == "image") {
    pnl.imageCheck.value = true;
    pnl.imageCheck.onClick();

  } else {
    pnl.textCheck.value = true;
    pnl.textCheck.onClick();
  }
};

WatermarkUI.prototype.validatePanel = function(pnl, ini) {
  var self = this;
  var opts = new WatermarkUIOptions(ini);

  function _getUN(str, defType) {
    var un = UnitValue(str);
    if (un.type == '?') {
      un = UnitValue(str + " " + (defType || 'px'));
      if (un.type == '?') {
        un = undefined;
      }
    }
    return un;
  }

  if (pnl.shapeCheck.value) {
    opts.watermarkType = "shape";
    opts.shapeName = pnl.shapeName.selection.text;

    var un = _getUN(pnl.shapeSize.text);
    if (!un) {
      return self.errorPrompt("Bad value for Shape Size");
    }

    opts.shapeSize = un;
  }

  if (pnl.imageCheck.value) {
    opts.watermarkType = "image";
    var f;
    if (pnl.imagePath.text) {
      f = new File(pnl.imagePath.text);
    }
    if (!f || !f.exists) {
      return self.errorPrompt("Watermark Image not found.");
    }
    opts.imagePath = f.toUIString();

    var un = _getUN(pnl.imageSize.text);
    if (!un) {
      return self.errorPrompt("Bad value for Image Size");
    }

    opts.imageSize = un;
  }

  if (pnl.textCheck.value) {
    opts.watermarkType = "text";
    opts.watermarkText = pnl.watermarkText.text;
//     opts.watermarkText = pnl.watermarkText.text.replace(/[\r\n]+/g, '\\n');

    var f = pnl.font.getFont();
    if (!f.font) {
      Error.runtimeError(9001, "Very bad font: " + listProps(f));
    }
    opts.font     = f.font;
    opts.fontSize = _getUN(f.size, 'pt');
  }

  opts.color = Stdlib.rgbToString(pnl.color.getColor());

  opts.layerName = pnl.layerName.text;
  opts.layerStyle = (pnl.layerStyle.selection ?
                     pnl.layerStyle.selection.text : "");

  opts.valign = pnl.valign.selection.text;
  opts.halign = pnl.halign.selection.text;

  var un = _getUN(pnl.vspace.text);
  if (!un) {
    return self.errorPrompt("Bad value for Vertical Offset");
  }
  opts.vspace = un;

  var un = _getUN(pnl.hspace.text);
  if (!un) {
    return self.errorPrompt("Bad value for Horizontal Offset");
  }
  opts.hspace = un;

  opts.copyrightedEnabled = pnl.copyrightedEnabled.value;
  if (opts.copyrightedEnabled) {
    opts.copyrighted = pnl.copyrighted.selection.text;
  }

  opts.copyrightNoticeEnabled = pnl.copyrightNoticeEnabled.value;
  if (opts.copyrightNoticeEnabled) {
    opts.copyrightNotice = pnl.copyrightNotice.text.replace(/[\r\n]+/g, '\\n');
  }

  opts.copyrightUrlEnabled = pnl.copyrightUrlEnabled.value;
  if (opts.copyrightUrlEnabled) {
    opts.copyrightUrl = pnl.copyrightUrl.text;
  }

  return opts;
};

WatermarkUI.prototype.process = function(ini, doc) {
  var self = this;

  var opts = new WatermarkUIOptions(ini);
  opts.rationalize();

  return opts;
};

WatermarkUI.prototype.applyWatermark = function(opts, doc) {
  var self = this;
  var ru = app.preferences.rulerUnits;

  try {
    app.preferences.rulerUnits = Units.PIXELS;

    var modified = true;

    if (opts.watermarkType == 'shape') {
      if (opts.shapeName && opts.shapeName != "None") {
        // place the shape
        self.placeShape(opts, doc);

        var layer = doc.activeLayer;
        Stdlib.setFillLayerColor(doc, layer, opts.color);

        Stdlib.deselectActivePath(doc);

      } else {
        modified = false;
      }

      if (modified) {
        if (opts.layerName) {
          layer.name = opts.layerName;
        } else {
          layer.name = 'Watermark Layer';
        }

        if (opts.layerStyle && opts.layerStyle != 'None') {
          self.applyStyle(opts, doc, layer, opts.layerStyle);
        }
      }

    } else if (opts.watermarkType == 'image') {
      var bnds = self.computeImageBounds(opts, doc);
      var file = Stdlib.convertFptr(opts.imagePath);

      var layer = doc.artLayers.add();

      Stdlib.selectBounds(doc, bnds);
      Stdlib.insertImageIntoSelection(doc, layer, file, true);
      layer = doc.activeLayer;

      // re-align the image _before_ applying the layer style
      var dx = 0;
      var dy = 0;
      var lbnds = Stdlib.getLayerBounds(doc, layer);
      if (opts.halign == 'Left' && bnds[0] != lbnds[0]) {
        dx = bnds[0] - lbnds[0];
      }
      if (opts.halign == 'Right' && bnds[2] != lbnds[2]) {
        dx = bnds[2] - lbnds[2];
      }

      if (opts.valign == 'Top' && bnds[1] != lbnds[1]) {
        dy = bnds[0] - lbnds[0];
      }
      if (opts.valign == 'Bottom' && bnds[3] != lbnds[3]) {
        dy = bnds[3] - lbnds[3];
      }

      if (dx || dy) {
        layer.translate(dx, dy);
        // var lbnds = Stdlib.getLayerBounds(doc, layer);
        // alert("bnds = " + bnds + "\rlbnds = " + lbnds);
      }

      if (opts.layerName) {
        layer.name = opts.layerName;
      } else {
        layer.name = 'Watermark Shape Layer';
      }

      if (opts.layerStyle && opts.layerStyle != 'None') {
        self.applyStyle(opts, doc, layer, opts.layerStyle);
      }

    } else if (opts.watermarkType == 'text') {

      if (opts.copyrightNotice || opts.watermarkText) {

        var text = opts.copyrightNotice;
        if (opts.watermarkText) {
          text = opts.watermarkText.replace(/\\n/g, '\r');
          var md = new Metadata(doc);
          if (text.contains('%')) {
            text = md.strf(text);
          }
        }

        self.addWatermarkText(opts, doc, text);

      } else {
        modified = false;
      }

      if (modified) {
        var layer = doc.activeLayer;
        // set the style
        if (opts.layerStyle && opts.layerStyle != 'None') {
          self.applyStyle(opts, doc, layer, opts.layerStyle);
        }

        // set the layer name
        if (opts.layerName) {
          layer.name = opts.layerName;
        } else {
          layer.name = 'Watermark Text Layer';
        }
      }

      // set any copyright info requested
      if (opts.copyrightedEnabled && opts.copyrighted) {
        doc.info.copyrighted = opts.copyrighted;
      }

      if (opts.copyrightNoticeEnabled && opts.copyrightNotice) {
        doc.info.copyrightNotice = opts.copyrightNotice;
      }

      if (opts.copyrightUrlEnabled && opts.copyrightUrl) {
        doc.info.ownerUrl = opts.copyrightUrl;
      }
    }

  } catch (e) {
    Stdlib.logException(e, true);

  } finally {
    app.preferences.rulerUnits = ru;
  }

  return;
};

WatermarkUI.prototype.applyStyle = function(opts, doc, layer, styleName) {
  var self = this;
  try {
    var pm = new PresetsManager();
    var sdesc = self.styles[styleName];

    if (!sdesc) {
      layer.applyStyle(styleName);
    } else {
      Styles.setLayerStyleDescriptor(doc, layer, sdesc);
    }

  } catch (e) {
    var msg = ("Failed to apply style '" + styleName + "': " +
               (e + '@' + (e.line || '??')));
    Stdlib.log(msg);
    alert(msg);
  }
};

WatermarkUI.prototype.computeOffsets = function(opts, doc) {
  var self = this;
  var height = doc.height.as("px");
  var width = doc.width.as("px");
  var rez = doc.resolution;

  var h = Stdlib.getPixelValue(rez, opts.hspace.toString(), width, "px");
  var v = Stdlib.getPixelValue(rez, opts.vspace.toString(), height, "px");

  if (opts.hspace.type == '%' && (opts.hspace == opts.vspace)) {
    v = h = Math.min(v, h);
  }

  return [h, v];
};


//
// returns the bounds of the watermark to be placed...
// top, left, bottom, right
//
WatermarkUI.prototype.computeShapeBounds = function(opts, doc, shapeName) {
  var self = this;

  var shapeInfo = CustomShapeInfo.getShapeInfo(shapeName);

  var dheight = doc.height.as("px");
  var dwidth = doc.width.as("px");

  var ratio = shapeInfo.h/shapeInfo.w;
  var dratio = dheight/dwidth;

  var rez = doc.resolution;

  var width;
  var height;

  // calc the size in pixels based on the largest dimension
  var size;
  if (dheight > dwidth) {
    height = Stdlib.getPixelValue(rez, opts.shapeSize.toString(),
                                  dheight, "px");
    width = height/ratio;

  } else {
    width = Stdlib.getPixelValue(rez, opts.shapeSize.toString(), dwidth, "px");
    height = width/ratio;
  }

  var pos = self.computeOffsets(opts, doc);
  var hspace = pos[0];
  var vspace = pos[1];

  var x;
  if (opts.halign == 'Left') {
    x = hspace;
    rx = x + width;
  } else if (opts.halign == 'Right') {
    var rx = dwidth - hspace;
    x = rx - width;
  } else {
    x = (dwidth/2 - width/2);
  }

  var y;
  if (opts.valign == 'Top') {
    y = vspace;
  } else if (opts.valign == 'Bottom') {
    var ry = dheight - vspace;
    y = ry - height;
  } else {
    y = (dheight/2 - height/2);
  }

  // The set up is all done.
  var b = new Object();
  b.top = Math.floor(y);
  b.left = Math.floor(x);
  b.bottom = b.top + Math.floor(height); // Math.floor(ry);
  b.right = b.left + Math.floor(width); // Math.floor(rx);

  return b;
};

WatermarkUI.prototype.placeShape = function(opts, doc) {
  var self = this;
  var bounds;
  var shape;

  function _placeShape() {
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

  var shapeInfo = self.shapes[opts.shapeName];

  if (shapeInfo) {
    // if we are loading a shape off the disk, append it to the current
    // palette set, rename it (in case of dupes), place it, then delete it

    var pm = new PresetsManager();
    var names = pm.getNames(PresetType.CUSTOM_SHAPES);
    var length = names.length;
    pm.appendPresets(PresetType.CUSTOM_SHAPES, shapeInfo.file);

    shape = "_WM_" + (new Date().getTime());
    pm.renameByIndex(PresetType.CUSTOM_SHAPES, length+1, shape);

    // compute the bounding box for the watermark shape
    bounds = self.computeShapeBounds(opts, doc, shape);
    Stdlib.wrapLC(doc, _placeShape);

    pm.deleteElementAt(PresetType.CUSTOM_SHAPES, length+1);

  } else {
    shape = opts.shapeName;
    bounds = self.computeShapeBounds(opts, doc, shape);
    Stdlib.wrapLC(doc, _placeShape);
  }
};

//
// returns the bounds of the watermark to be placed...
// top, left, bottom, right
//
WatermarkUI.prototype.computeImageBounds = function(opts, doc) {
  var self = this;

  var dheight = doc.height.as("px");
  var dwidth = doc.width.as("px");

  var rez = doc.resolution;

  var dim = Math.max(doc.height.as("px"), dwidth);
  var size = Stdlib.getPixelValue(rez, opts.imageSize.toString(), dim, "px");

  var width = height = size;

  var pos = self.computeOffsets(opts, doc);
  var hspace = pos[0];
  var vspace = pos[1];

  var x;
  if (opts.halign == 'Left') {
    x = hspace;
    rx = x + width;
  } else if (opts.halign == 'Right') {
    var rx = dwidth - hspace;
    x = rx - width;
  } else {
    x = (dwidth/2 - width/2);
  }

  var y;
  if (opts.valign == 'Top') {
    y = vspace;
  } else if (opts.valign == 'Bottom') {
    var ry = dheight - vspace;
    y = ry - height;
  } else {
    y = (dheight/2 - height/2);
  }

  // The set up is all done.
  var b = new Object();
  b.top = Math.floor(y);
  b.left = Math.floor(x);
  b.bottom = b.top + Math.floor(height); // Math.floor(ry);
  b.right = b.left + Math.floor(width); // Math.floor(rx);

  return [b.left, b.top, b.right, b.bottom];
};


WatermarkUI.prototype.addWatermarkText = function(opts, doc, str) {
  var self = this;

  var pos = self.computeOffsets(opts, doc);
  var x = pos[0];
  var y = pos[1];

  var height = doc.height.as("px");
  var width = doc.width.as("px");
  var rez = doc.resolution;

  var layer = doc.artLayers.add();

  layer.kind = LayerKind.TEXT;
  layer.blendMode = BlendMode.NORMAL;
  layer.opacity = 100.0;

  var text = layer.textItem;
  var ru = app.preferences.rulerUnits;
  var tu = app.preferences.typeUnits;

  try {
    app.preferences.typeUnits = TypeUnits.POINTS;
    app.preferences.rulerUnits = Units.PIXELS;

    var fontSize;
    if (opts.fontSize.type == 'pt' || opts.fontSize.type == '?') {
      fontSize = opts.fontSize;

    } else {
      // convert the fontSize to pixels first
      fontSize = Stdlib.getPixelValue(rez, opts.fontSize.toString(),
                                      height, "pt");
      // then to points
      fontSize *= (72/rez);
    }

    text.contents = str;

    text.size = fontSize;
    if (text.size != fontSize) {
      PSCCFontSizeFix.setFontSizePoints(layer, fontSize);
    }
    text.font = opts.font || "ArialMT";
    text.color = opts.color;

    text.kind = (str.match(/\r|\n/) ?
                 TextType.PARAGRAPHTEXT : TextType.POINTTEXT);

    var bnds = layer.bounds;
    var twidth = (bnds[2].as("px") - bnds[0].as("px"));
    var theight = (bnds[3].as("px") - bnds[1].as("px"));

    if (opts.halign == 'Right') {
      x = width - x - twidth;

    } else if (opts.halign == 'Center') {
      x = Math.floor(width/2 - twidth/2);
    }

    if (opts.valign == 'Top') {
      y += theight;

    } else if (opts.valign == 'Bottom') {
      y = height - y;

    } else {
      y = Math.floor(height/2 - theight/2) + theight;
    }

    text.position = new Array(x, y);

  } finally {
    app.preferences.rulerUnits = ru;
    app.preferences.typeUnits = tu;
  }

  return layer;
};

WatermarkUI.prototype.preview = function() {
  try {
    var self = this;
    var opts = self.validatePanel(self.win.appPanel, {});

    try {
      var doc;
      if (app.documents.length > 0) {
        doc = app.activeDocument.duplicate();

      } else if (self.previewFile) {
        doc = app.open(self.previewFile);

      } else {
        if (confirm("No preview image was specified.\r\r" +
                    "Select a preview image?")) {

          var def = WatermarkUIOptions.PREVIEW_IMG;
          var f = Stdlib.selectFileOpen("Select a Preview image",
                                        undefined, def);
          if (f) {
            self.previewFile = f;
            doc = app.open(self.previewFile);
          }
        }
      }

    } catch (e) {
      Stdlib.logException(e, "Unable to open/find preview image.", true);
    }

    if (typeof(res) == 'boolean') {
      return;
    }

    $.level = 1; debugger;
    var snapname = "PreviewSnapshot";
    opts.rationalize();
    try { Stdlib.deleteSnapshot(doc, snapname); } catch (e) {};
    Stdlib.takeSnapshot(doc, snapname);
    self.applyWatermark(opts, app.activeDocument);

    if (doc.width.as("px") > PreviewWindow.MAX_WIDTH ||
        doc.height.as("px") > PreviewWindow.MAX_HEIGHT) {
      Stdlib.fitImage(doc, PreviewWindow.MAX_WIDTH, PreviewWindow.MAX_HEIGHT);
    }
    var file = new File(Folder.temp + "/wm-preview.png");
    file.remove();
    doc.saveAs(file, new PNGSaveOptions(), true);

    var w = doc.width.as("px");
    var h = doc.height.as("px");

    Stdlib.revertToSnapshot(doc, snapname);
    Stdlib.deleteSnapshot(doc, snapname);

    doc.close(SaveOptions.DONOTSAVECHANGES);

    if (!isCS4()) { // PSBUG
      self.win.visible = false;
    }

    try {
      PreviewWindow.openFile(file, w, h, undefined, undefined, self.win);

    } finally {
      if (!isCS4()) { // PSBUG
        self.win.visible = true;
      }
    }

    file.remove();

    Stdlib.waitForRedraw();
    Stdlib.waitForRedraw();
    Stdlib.waitForRedraw();

  } catch (e) {
    var msg = (e + '@' + (e.line || '??'));
    alert(msg);
    debugger;
  }
};

WatermarkUI.prototype.loadStyle = function(pnl) {
  var self = this;
  var folder = (self.stylesFolder || WatermarkUIOptions.DEFAULT_PRESETS_FOLDER);
  var stylesFolder = Stdlib.convertFptr(folder);
  var sdesc = undefined;

  var fsel = Stdlib.createFileSelect("Styles Files: *.asl");
  var files = folder.getFiles("*.asl");
  var def = files[0] || stylesFolder;

  while (true) {
    var stylesFile = Stdlib.selectFileOpen("Select a Style",
                                           fsel, def);
    if (!stylesFile) {
      break;
    }

    try {
      sdesc = Styles.loadFileDescriptor(stylesFile);

    } catch (e) {
      var msg = (e + '@' + (e.line || '??'));
      alert(msg);
      continue;
    }

    if (sdesc) {
      break;
    }
  }

  return sdesc;
};

WatermarkUI.prototype.defineStyle = function(pnl) {
  var self = this;
  var color = pnl.color.getColor();
  var text = pnl.watermarkText.text || "Watermark Text";
  var f = pnl.font.getFont();

  var desc = Styles.defineStyle(text, f.font, color);

  return desc;
};

WatermarkUI.prototype.loadShape = function(pnl) {
  var self = this;
  var folder = (self.presetsFolder || WatermarkUIOptions.DEFAULT_PRESETS_FOLDER);
  var shapesFolder = Stdlib.convertFptr(folder);
  var shapeInfo = undefined;

  var fsel = Stdlib.createFileSelect("Custom Shape Files: *.csh");
  var files = folder.getFiles("*.csh");
  var def = files[0] || shapesFolder;

  while (true) {
    var file = Stdlib.selectFileOpen("Select a Custom Shape",
                                     fsel, def);
    if (!file) {
      break;
    }

    try {
      var shapesFile = new ShapesFile();
      var shapes = shapesFile.read(file);

      if (!shapes || shapes.length == 0) {
        Error.runtimeError(9001, "Failed to read shape from: " +
                           file.toUIString());
      }

      if (shapes.length > 1) {
        Error.runtimeError(9001, "Custom Shapes can only be loaded from .csh " +
                           "files with a single shape");
      }

      shapeInfo = shapes[0];
      shapeInfo.file = file;

    } catch (e) {
      var msg = (e + '@' + (e.line || '??'));
      alert(msg);
      continue;
    }

    if (shapeInfo) {
      break;
    }
  }

  return shapeInfo;
};

//
// scratch code. may use later
//
function _strokeStyle(doc, layer, size, opacity, color) {
  function _ftn() {
    var descSet = new ActionDescriptor();

    // descSet = Styles.getLayerStyleDescriptor(doc, layer);
    // var styleDesc = descSet.getObjectValue(cTID('T   ');
    // if (styleDesc.hasKey(cTID('FrFX')) { styleDesc.remove(cTID('FrFX'); }

    var refProp = new ActionReference();
    refProp.putProperty( cTID('Prpr'), cTID('Lefx') );
    refProp.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
    descSet.putReference( cTID('null'), refProp );

    var descStyle = new ActionDescriptor();
    descStyle.putUnitDouble( cTID('Scl '), cTID('#Prc'), 100.000000 );

    // Start Stroke style
    var descStrk = new ActionDescriptor();
    // Enabled
    descStrk.putBoolean( cTID('enab'), true );
    // Position
    descStrk.putEnumerated( cTID('Styl'), cTID('FStl'), cTID('OutF') );
    // Fill Type
    descStrk.putEnumerated( cTID('PntT'), cTID('FrFl'), cTID('SClr') );
    // Blend Mode
    descStrk.putEnumerated( cTID('Md  '), cTID('BlnM'), cTID('Nrml') );
    // Opacity
    descStrk.putUnitDouble( cTID('Opct'), cTID('#Prc'), opacity );
    // Size
    descStrk.putUnitDouble( cTID('Sz  '), cTID('#Pxl'), size );

    // Color
    var descClr = new ActionDescriptor();
    descClr.putDouble( cTID('Rd  '), color.rgb.red );
    descClr.putDouble( cTID('Grn '), color.rgb.green );
    descClr.putDouble( cTID('Bl  '), color.rgb.blue );

    descStrk.putObject( cTID('Clr '), cTID('RGBC'), descClr );
    // End Stroke style

    // Insert stroke style
    descStyle.putObject( cTID('FrFX'), cTID('FrFX'), descStrk );

    // Insert style descriptor
    descSet.putObject( cTID('T   '), cTID('Lefx'), descStyle );

    executeAction( cTID('setd'), descSet, DialogModes.NO );
  }

  _ftn();
};


WatermarkUI.main = function() {
  if (!app.documents.length) {
    alert("Please open a document before running this script.");
    return;
  }
  var doc = app.activeDocument;
  var ui  = new WatermarkUI();

  var ini = {
//     watermarkText: "Lisa"
  };

  var opts = ui.exec(ini, doc);

  if (!opts) {
    return;
  }

//   alert(listProps(opts)); return;

  ui.applyWatermark(opts, doc);

  // Text location tests
  if (false) {
    var ini = new WatermarkUIOptions();
    ini.valign = 'Top';
    ini.halign = 'Left';
    ini.fontSize = 12;
    ini.shapeName = '';
    ini.watermarkText = "Lisa";

    var opts = ui.process(ini, doc);
    if (!opts) {
      return;
    }
    ui.applyWatermark(opts, doc);

    opts.valign = 'Top';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Top';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Left';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Left';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);
  }

  // Shape location tests
  if (false) {
    var ini = new WatermarkUIOptions();
    ini.valign = 'Top';
    ini.halign = 'Left';

    var opts = ui.process(ini, doc);

    if (!opts) {
      return;
    }
    ui.applyWatermark(opts, doc);

    opts.valign = 'Top';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Top';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Left';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Middle';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Left';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Center';
    ui.applyWatermark(opts, doc);

    opts.valign = 'Bottom';
    opts.halign = 'Right';
    ui.applyWatermark(opts, doc);
  }
  if (ui.saveIni) {
    ui.updateIniFile({ uiX: ui.winX, uiY: ui.winY });
  }
};

//WatermarkUI.main();  // for testing

"WatermarkUI.jsx";
// EOF
