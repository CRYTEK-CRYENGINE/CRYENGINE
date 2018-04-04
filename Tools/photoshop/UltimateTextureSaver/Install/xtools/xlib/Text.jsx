//
// Text.jsx
//
//============================================================================
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-@include "xlib/PSConstants.js"
//-@include "xlib/stdlib.js"
//
//-@include "xlib/Action.js"
//-@include "xlib/xml/atn2xml.jsx"
//

//============================================================================
//                             Text management code
//============================================================================

Text = function() {};
Text.findFont = function(f) {
  var flist = app.fonts;
  for (var i = 0; i < flist.length; i++) {
    if (f == flist[i].name) {
      return flist[i];
    }
  }
  return undefined;
};
Text.toRGBColor = function(r, g, b) {
  var c = new RGBColor();
  if (r instanceof Array) {
    b = r[2]; g = r[1]; r = r[0];
  }
  c.red = Math.round(r); c.green = Math.round(g); c.blue = Math.round(b);
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

//
// addNewTextLayer
//   This isn't necessarily any easier than using the normal scripting
//   interface. This is intended to correspond with modifyTextLayer.
//   This will later be implemented as a call to exectuteAction _after_
//   I figure out what to do about paragraph styles and a few other bits
//   and pieces.
//
Text.addNewTextLayer = function(doc, opts, layers) {
  var layer;
  var ad;
  if (doc != app.activeDocument) {
    ad = app.activeDocument;
    app.activeDocument = doc;
  }
  var ru = app.preferences.rulerUnits;
  var tu = app.preferences.typeUnits;
  try {
    app.preferences.typeUnits = TypeUnits.POINTS;
    app.preferences.rulerUnits = Units.PIXELS;
    if (!layers) {
      layers = doc.artLayers;
    }
    layer = doc.artLayers.add();
    layer.kind = LayerKind.TEXT;
    var item = layer.textItem;

    if (opts.layerName)             { layer.name = opts.layerName; }
    if (opts.kind)                  { item.kind = opts.kind; }
    if (opts.kind == TextType.PARAGRAPHTEXT) {
      if (opts.height)                { item.height = opts.height; }
      if (opts.width)                 { item.width = opts.width; }
    }
    if (opts.position)              { item.position = opts.position; }
    if (opts.color)                 { item.color = opts.color; }
    if (opts.size)                  { item.size = opts.size; }
    if (opts.font) {
      var font = opts.font;
      if (font.constructor == String) {
        var textFont = Text.findFont(font);
        if (!textFont) {
          textFont = app.fonts[font];
        }
        font = textFont;
      }
      if (font instanceof TextFont) {
        item.font = font.postScriptName;
      }
    }
    if (opts.contents) { item.contents = opts.contents; }

  } catch (e) {
    alert(e);

  } finally {
    if (ad) app.activeDocument = ad;
    app.preferences.rulerUnits = ru;
    app.preferences.typeUnits = tu;
  }
  return layer;
};

Text.modifyTextLayer = function(doc, opts, layer) {
  doc.activeLayer = layer;

  if (opts.ranges) {
    opts.ranges.apply(doc, layer, opts.contents, true);
  }
  return layer;
};

Text.getLayerDescriptor = function(doc, layer, dontWrap) {
  function _ftn() {
    var ref = new ActionReference();
    ref.putEnumerated(cTID("Lyr "), cTID("Ordn"), cTID("Trgt"));
    return executeActionGet(ref)
  };
  if (doc != app.activeDocument) {
    app.activeDocument = doc;
  }
  if (layer != doc.activeLayer) {
    doc.activeLayer = layer;
  }
  return _ftn();
};


//============================================================================
// TextOptions
// Any style options (e.g. color, font) set here are defaults for the textitem
// Style information in the TextStyleRanges will override these defaults
//
//============================================================================
TextOptions = function(arg) {
  var self = this;

  var argIsTextItem = false;

  try { argIsTextItem = (arg instanceof TextItem); } catch (e) {}

  if (argIsTextItem) {
    // need to figure out what to do about defaults management
    self.item = arg;
  }

  if (arg.constructor == String) {
    self.contents = arg;
  }
};
TextOptions.prototype.typename = "TextOptions";
TextOptions.prototype.layerName = "Text Layer";
TextOptions.prototype.contents  = '';
TextOptions.prototype.color     = Text.toRGBColor(40, 40, 40);
TextOptions.prototype.font      = Text.findFont("Times New Roman");
TextOptions.prototype.height    = 100; // pixels
TextOptions.prototype.width     = 200; // pixels
TextOptions.prototype.size      = 20;  // points
TextOptions.prototype.kind      = TextType.PARAGRAPHTEXT;
TextOptions.prototype.position  = [50, 50];
TextOptions.prototype.baseline  = cTID('Nrml');  // PSString.superScript

TextOptions.prototype.justification   = undefined;
TextOptions.prototype.horizontalScale = undefined;
TextOptions.prototype.verticalScale   = undefined;
TextOptions.prototype.tsRanges  = undefined;


TextOptions.prototype.asActionDescriptor = function() {
  var desc = new ActionDescriptor();
  // fill up desc
  return desc;
};


TextShape = function() {
  var self = this;
  self.orientation;
  self.transform;
  self.spacing;
  self.frameBaselineAlignment;
};

TextStyleRanges = function() {
  var self = this;

  self.length = 0;
  self.tsrs = [];
};
TextStyleRanges.prototype.typename = "TextStyleRanges";
TextStyleRanges.prototype.add = function(tsr) {
  var self = this;
  self.length++;
  self.tsrs.push(tsr);
  return tsr;
};
TextStyleRanges.prototype.remove = function(index) {
  var self = this;
  self.tsrs.splice(index, 1);
  self.length--;
};
TextStyleRanges.prototype.get = function(index) {
  return self.tsrs[index];
};

//
// all arguments are options.
//   doc      defaults to app.activeDocument
//   layer    defaults to doc.activeLayer
//   contents defaults to layer.textItem.contents
//   replace  defaults to true
//
TextStyleRanges.prototype.apply = function(doc, layer, contents, replace) {
  var self = this;
  var ranges = self.tsrs;

  if (ranges == null) {
    return;
  }
  if (doc == undefined) {
    doc = app.activeDocument;
  } else {
    if (doc != app.activeDocument) {
      app.activeDocument = doc;
    }
  }
  if (layer == undefined) {
    if (layer != doc.activeLayer) {
      layer = doc.activeLayer;
    }
  }
  if (layer.kind != LayerKind.TEXT) {
    throw "The layer \"" + layer.name + "\" is not a text layer.";
  }
  replace = (replace != false);  // default is true...

  var desc = Text.getLayerDescriptor(doc, layer);
  var textDesc = desc.getObjectValue(PSKey.Text);
  if (!contents) {
    contents = textDesc.getString(PSKey.Text);
  } else {
    textDesc.putString(PSKey.Text, contents);
  }

  var tsrList = textDesc.getList(PSKey.TextStyleRange);
  var tsr = tsrList.getObjectValue(0);
  var ts = tsr.getObjectValue(PSKey.TextStyle);
  TextStyle.setDefaults(ts);

  if (replace) {
    tsrList = self.asActionList(contents.length);
    textDesc.putList(PSKey.TextStyleRange, tsrList);
  } else {
    throw "There is currently no support for modifying an existing range set.";
  }

  if (textDesc.hasKey(PSString.paragraphStyleRange)) {
    var psrList = textDesc.getList(PSString.paragraphStyleRange);
    var psrIndex = psrList.count - 1;
    var psr = psrList.getObjectValue(psrIndex)
    psr.putInteger(PSKey.From, contents.length);
  }

  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putEnumerated(PSClass.TextLayer, PSType.Ordinal, PSEnum.Target);
  desc.putReference(cTID('null'), ref);
  desc.putObject(PSKey.To, PSClass.TextLayer, textDesc);

  executeAction(PSEvent.Set, desc, DialogModes.NO);
};

TextStyleRanges.prototype.asActionList = function(len) {
  var self = this;

  if (!len) {
    return;
  }

  var ranges = self.tsrs;
  var rmap = [];
  rmap.length = len;

  for (var i = 0; i < ranges.length; i++) {
    var range = ranges[i];
    if (range.to == undefined || range.to > len) {
      range.to = len;
    }
    if (range.from > range.to) {
      throw "Bad text range values";
    }
    for (var j = range.from; j < range.to; j++) {
      rmap[j] = i;
    }
  }

  //  $.level = 1; debugger;
  var exranges = [];
  var start = 0;
  var rindex = rmap[0];

  for (var i = 1; i < rmap.length; i++) {
    if (rmap[i] != rindex) {
      exranges.push(new TextStyleRange(start, i, ranges[rindex].textStyle));
      start = i;
      rindex = rmap[start];
    }
  }

  exranges.push(new TextStyleRange(start, i, ranges[rindex].textStyle));

  var str = '';
  for (var i = 0; i < exranges.length; i++) {
    str += exranges[i] + '\n';
  }

  // $.level = 1; debugger;

  var list = new ActionList();
  for (var i = 0; i < exranges.length; i++) {
    list.putObject(PSClass.TextStyleRange, exranges[i].asActionDescriptor());
  }
  return list;
};

TextStyleRange = function(from, to, textStyle) {
  // This lets us call this as a function and return an object
  if (this.constructor != arguments.callee) {
    return new TextStyleRange(from, to, textStyle);
  }
  var self = this;

  self.from = from;
  self.to = to;
  self.textStyle = textStyle;
};
TextStyleRange.prototype.typename = "TextStyleRange";

TextStyleRange.prototype.toString = function() {
  return "{from: " + this.from + ", to: " + this.to +
         ", style: " + this.textStyle + "}";
};

TextStyleRange.prototype.asActionDescriptor = function() {
  var self = this;
  var desc = new ActionDescriptor();

  desc.putInteger(PSKey.From, self.from);
  desc.putInteger(PSKey.To, self.to);
  desc.putObject(PSKey.TextStyle, PSClass.TextStyle,
                 self.textStyle.asActionDescriptor());

  return desc;
};

TextStyleRange.prototype.dupe = function() {
  return new TextStyleRange(this.to, this.from, this.textStyle);
};



TextStyle = function(font, size, color) {
  // This lets us call this as a function and return an object
  if (this.constructor != arguments.callee) {
    return new TextStyle(font, size, color);
  }
  var self = this;

  if (font instanceof TextStyle) {
    var arg = font;
    self.font  = arg.font;
    self.size  = arg.size;
    self.color = arg.color;
    self.baseline = arg.baseline;

  } else {
    if (font && font.constructor == String) {
      font = app.fonts.getByName(font);
    }
    self.font  = font;
    self.size  = size;
    self.color = color;
    self.baseline = PSEnum.Normal;
  }
};
TextStyle.prototype.typename = "TextStyle";

TextStyle.setDefaults = function(desc) {
  // Font
  var fontName = desc.getString(PSString.fontPostScriptName);
  TextStyle.defaultFont = app.fonts.getByName(fontName);

  // Font Size
  TextStyle.defaultSize = desc.getUnitDoubleValue(PSKey.SizeKey,
                                                  PSUnit.Points);

  // Color
  var colorDesc = desc.getObjectValue(PSClass.Color);
  var color = new SolidColor()
  color.rgb.red   = colorDesc.getDouble(PSKey.Red);
  color.rgb.green = colorDesc.getDouble(PSKey.Green);
  color.rgb.blue  = colorDesc.getDouble(PSKey.Blue);
  TextStyle.defaultColor = color;

  // Baseline
  TextStyle.defaultBaseline = desc.getEnumerationValue(PSString.baseline);
};

TextStyle.prototype.toString = function() {
  var self = this;
  var str = '{';
  var font  = (self.font  != undefined) ? self.font : TextStyle.defaultFont;
  var size  = (self.size  != undefined) ? self.size : TextStyle.defaultSize;
  var color = (self.color != undefined) ? self.color : TextStyle.defaultColor;
  str += "font: " + font + ", ";
  str += "size: " + size + ", ";
  str += "color: " + RGBtoString(color);

  if (self.baseline) {
    str += ", baseline: " + id2char(self.baseline, "Enum");
  }
  str += "}";
  return str;
};

TextStyle.defaultFont = app.fonts.getByName("ArialMT");
TextStyle.defaultSize = 20.0;
TextStyle.defaultColor = Text.toRGBColor(0, 255, 255);
TextStyle.defaultBaseline = cTID('Nrml');

TextStyle.prototype.asActionDescriptor = function() {
  var self  = this;
  var font  = self.font ? self.font : TextStyle.defaultFont;
  var size  = self.size ? self.size : TextStyle.defaultSize;
  var color = self.color ? self.color : TextStyle.defaultColor;
  var tsDesc = new ActionDescriptor();
  if (!font) {
    font = TextStyle.defaultFont;
  }
  tsDesc.putString(PSString.fontPostScriptName,  font.postScriptName);
  tsDesc.putString(PSKey.FontName, font.family);
  tsDesc.putString(PSKey.FontStyleName, font.style);
  tsDesc.putInteger(PSKey.FontScript, 0);
  tsDesc.putInteger(PSKey.FontTechnology, 1);
  if (!size) {
    size = TextStyle.defaultSize;
  }
  tsDesc.putUnitDouble(PSKey.SizeKey, PSUnit.Points, size);
  tsDesc.putDouble(PSKey.HorizontalScale, 100.000000);
  tsDesc.putDouble(PSKey.VerticalScale, 100.000000);
  tsDesc.putBoolean(PSString.syntheticBold, false);
  tsDesc.putBoolean(PSString.syntheticItalic, false);
  tsDesc.putBoolean(PSString.autoLeading, true);
  tsDesc.putInteger(PSKey.Tracking, 0);
  tsDesc.putUnitDouble(PSKey.BaselineShift, PSUnit.Points, 0.000000);
  tsDesc.putDouble(PSString.characterRotation, 0.000000);
  tsDesc.putEnumerated(PSKey.AutoKern,
                       PSKey.AutoKern,
                       PSString.metricsKern);
  tsDesc.putEnumerated(PSString.fontCaps,
                       PSString.fontCaps,
                       PSEnum.Normal);
  tsDesc.putEnumerated(PSString.baseline,
                       PSString.baseline,
                       (self.baseline ? self.baseline : PSEnum.Normal));
  tsDesc.putEnumerated(PSString.otbaseline,
                       PSString.otbaseline,
                       PSEnum.Normal);
  tsDesc.putEnumerated(PSString.strikethrough,
                       PSString.strikethrough,
                       PSString.strikethroughOff);
  tsDesc.putEnumerated(PSKey.Underline,
                       PSKey.Underline,
                       PSString.underlineOff);
  tsDesc.putUnitDouble(PSString.underlineOffset, PSUnit.Points, 0.000000);
  tsDesc.putBoolean(PSString.ligature, true);
  tsDesc.putBoolean(PSString.altligature, false);
  tsDesc.putBoolean(PSString.contextualLigatures, false);
  tsDesc.putBoolean(PSString.alternateLigatures, false);
  tsDesc.putBoolean(PSString.oldStyle, false);
  tsDesc.putBoolean(PSString.fractions, false);
  tsDesc.putBoolean(PSString.ordinals, false);
  tsDesc.putBoolean(PSString.swash, false);
  tsDesc.putBoolean(PSString.titling, false);
  tsDesc.putBoolean(PSString.connectionForms, false);
  tsDesc.putBoolean(PSString.stylisticAlternates, false);
  tsDesc.putBoolean(PSString.ornaments, false);
  tsDesc.putEnumerated(PSString.figureStyle,
                       PSString.figureStyle,
                       PSEnum.Normal);
  tsDesc.putBoolean(PSString.proportionalMetrics, false);
  tsDesc.putBoolean(PSString.kana, false);
  tsDesc.putBoolean(PSString.italics, false);
  tsDesc.putBoolean(PSString.ruby, false);
  tsDesc.putEnumerated(PSString.baselineDirection,
                       PSString.baselineDirection,
                       PSString.rotated);
  tsDesc.putEnumerated(PSString.textLanguage,
                       PSString.textLanguage,
                       PSString.englishLanguage);
  tsDesc.putEnumerated(PSString.japaneseAlternate,
                       PSString.japaneseAlternate,
                       PSString.defaultForm);
  tsDesc.putDouble(PSString.mojiZume, 0.000000);
  tsDesc.putEnumerated(PSString.gridAlignment,
                       PSString.gridAlignment,
                       PSString.roman);
  tsDesc.putBoolean(PSString.enableWariChu, false);
  tsDesc.putInteger(PSString.wariChuCount, 2);
  tsDesc.putInteger(PSString.wariChuLineGap, 0);
  tsDesc.putDouble(PSString.wariChuScale, 0.500000);
  tsDesc.putInteger(PSString.wariChuWidow, 2);
  tsDesc.putInteger(PSString.wariChuOrphan, 2);
  tsDesc.putEnumerated(PSString.wariChuJustification,
                       PSString.wariChuJustification,
                       PSString.wariChuAutoJustify);
  tsDesc.putInteger(PSString.tcyUpDown, 0);
  tsDesc.putInteger(PSString.tcyLeftRight, 0);
  tsDesc.putDouble(PSString.leftAki, -1.000000);
  tsDesc.putDouble(PSString.rightAki, -1.000000);
  tsDesc.putInteger(PSString.jiDori, 0);
  tsDesc.putBoolean(PSString.noBreak, false);

  var colorDesc = new ActionDescriptor();
  if (color instanceof SolidColor) {
    color = color.rgb;
  }
  colorDesc.putDouble(PSKey.Red, color.red);
  colorDesc.putDouble(PSKey.Green, color.green);
  colorDesc.putDouble(PSKey.Blue, color.blue);
  tsDesc.putObject(PSKey.Color, PSClass.RGBColor, colorDesc);

  var strokeDesc = new ActionDescriptor();
  strokeDesc.putDouble(PSKey.Red, 0.000000);
  strokeDesc.putDouble(PSKey.Green, 0.000000);
  strokeDesc.putDouble(PSKey.Blue, 0.000000);
  tsDesc.putObject(PSString.strokeColor, PSClass.RGBColor, strokeDesc);

  tsDesc.putBoolean(PSKey.Fill, true);
  tsDesc.putBoolean(PSKey.Stroke, false);
  tsDesc.putBoolean(PSString.fillFirst, true);
  tsDesc.putBoolean(PSString.fillOverPrint, false);
  tsDesc.putBoolean(PSString.strokeOverPrint, false);
  tsDesc.putEnumerated(PSString.lineCap,
                       PSString.lineCap,
                       PSString.buttCap);
  tsDesc.putEnumerated(PSString.lineJoin,
                       PSString.lineJoin,
                       PSString.miterJoin);
  tsDesc.putUnitDouble(PSString.lineWidth, PSUnit.Points, 0.009999);
  tsDesc.putUnitDouble(PSString.miterLimit, PSUnit.Points, 0.009999);
  tsDesc.putDouble(PSString.lineDashOffset, 0.000000);

  return tsDesc;
};


//====================================================================
// Misc
//====================================================================

RGBtoString = function(c) {
  if (c instanceof SolidColor) {
    c = c.rgb;
  }
  return "{r: " + c.red + ", g: " + c.green + ", b: " + c.blue + "}";
};

//====================================================================
// PSConstants
//====================================================================
cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

PSClass = function() {};
PSEnum = function() {};
PSEvent = function() {};
PSForm = function() {};
PSKey = function() {};
PSType = function() {};
PSUnit = function() {};
PSString = function() {};

PSClass.AntiAliasedPICTAcquire = cTID('AntA');
PSClass.Color = cTID('Clr ');
PSClass.Document = cTID('Dcmn');
PSClass.Point = cTID('Pnt ');
PSClass.Property = cTID('Prpr');
PSClass.Layer = cTID('Lyr ');
PSClass.RGBColor = cTID('RGBC');
PSClass.TextLayer = cTID('TxLr');
PSClass.TextStyle = cTID('TxtS');
PSClass.TextStyleRange = cTID('Txtt');

PSEnum.AntiAliasCrisp = cTID('AnCr');
PSEnum.Horizontal = cTID('Hrzn');
PSEnum.JustifyAll = cTID('JstA');
PSEnum.Left = cTID('Left');
PSEnum.None = cTID('None');
PSEnum.Normal = cTID('Nrml');
PSEnum.Target = cTID('Trgt');
PSEnum.Vertical = cTID('Vrtc');

PSEvent.Set = cTID('setd');

PSKey.AutoKern = cTID('AtKr');
PSKey.BaselineShift = cTID('Bsln');
PSKey.Blue = cTID('Bl  ');
PSKey.Color =cTID('Clr ');
PSKey.Fill = cTID('Fl  ');
PSKey.FontName = cTID('FntN');
PSKey.FontScript = cTID('Scrp');
PSKey.FontStyleName = cTID('FntS');
PSKey.FontTechnology = cTID('FntT');
PSKey.From = cTID('From');
PSKey.Green = cTID('Grn ');
PSKey.HorizontalScale = cTID('HrzS');
PSKey.Orientation = cTID('Ornt');
PSKey.Red = cTID('Rd  ');
PSKey.SizeKey = cTID('Sz  ');
PSKey.Spacing = cTID('Spcn');
PSKey.Stroke = cTID('Strk');
PSKey.Text = cTID('Txt ');
PSKey.TextStyle = cTID('TxtS');
PSKey.TextStyleRange = cTID('Txtt');
PSKey.To = cTID('T   ');
PSKey.Tracking = cTID('Trck');
PSKey.Underline = cTID('Undl');
PSKey.VerticalScale = cTID('VrtS');

PSString.alignByAscent = sTID('alignByAscent');
PSString.alternateLigatures = sTID('alternateLigatures');
PSString.altligature = sTID('altligature');
PSString.autoLeading = sTID('autoLeading');
PSString.autoLeadingPercentage = sTID('autoLeadingPercentage');
PSString.autoTCY = sTID('autoTCY');
PSString.base = sTID('base');
PSString.baseline = sTID('baseline');
PSString.baselineDirection = sTID('baselineDirection');
PSString.burasagari = sTID('burasagari');
PSString.burasagariNone = sTID('burasagariNone');
PSString.buttCap = sTID('buttCap');
PSString.characterRotation = sTID('characterRotation');
PSString.columnCount = sTID('columnCount');
PSString.columnGutter = sTID('columnGutter');
PSString.connectionForms = sTID('connectionForms');
PSString.contextualLigatures = sTID('contextualLigatures');
PSString.defaultForm = sTID('defaultForm');
PSString.defaultStyle = sTID('defaultStyle');
PSString.defaultTabWidth = sTID('defaultTabWidth');
PSString.dropCapMultiplier = sTID('dropCapMultiplier');
PSString.enableWariChu = sTID('enableWariChu');
PSString.endIndent = sTID('endIndent');
PSString.englishLanguage = sTID('englishLanguage');
PSString.figureStyle = sTID('figureStyle');
PSString.fillFirst = sTID('fillFirst');
PSString.fillOverPrint = sTID('fillOverPrint');
PSString.firstBaselineMinimum = sTID('firstBaselineMinimum');
PSString.firstLineIndent = sTID('firstLineIndent');
PSString.fontCaps = sTID('fontCaps');
PSString.fontPostScriptName = sTID('fontPostScriptName');
PSString.fractions = sTID('fractions');
PSString.frameBaselineAlignment = sTID('frameBaselineAlignment');
PSString.gridAlignment = sTID('gridAlignment');
PSString.hangingRoman = sTID('hangingRoman');
PSString.hyphenate = sTID('hyphenate');
PSString.hyphenateCapitalized = sTID('hyphenateCapitalized');
PSString.hyphenateLimit = sTID('hyphenateLimit');
PSString.hyphenatePostLength = sTID('hyphenatePostLength');
PSString.hyphenatePreLength = sTID('hyphenatePreLength');
PSString.hyphenateWordSize = sTID('hyphenateWordSize');
PSString.hyphenationPreference = sTID('hyphenationPreference');
PSString.hyphenationZone = sTID('hyphenationZone');
PSString.italics = sTID('italics');
PSString.japaneseAlternate = sTID('japaneseAlternate');
PSString.jiDori = sTID('jiDori');
PSString.justificationGlyphDesired = sTID('justificationGlyphDesired');
PSString.justificationGlyphMaximum = sTID('justificationGlyphMaximum');
PSString.justificationGlyphMinimum = sTID('justificationGlyphMinimum');
PSString.justificationLetterDesired = sTID('justificationLetterDesired');
PSString.justificationLetterMaximum = sTID('justificationLetterMaximum');
PSString.justificationLetterMinimum = sTID('justificationLetterMinimum');
PSString.justificationWordDesired = sTID('justificationWordDesired');
PSString.justificationWordMaximum = sTID('justificationWordMaximum');
PSString.justificationWordMinimum = sTID('justificationWordMinimum');
PSString.kana = sTID('kana');
PSString.keepTogether = sTID('keepTogether');
PSString.kerningRange = sTID('kerningRange');
PSString.kurikaeshiMojiShori = sTID('kurikaeshiMojiShori');
PSString.leadingBelow = sTID('leadingBelow');
PSString.leadingType = sTID('leadingType');
PSString.leftAki = sTID('leftAki');
PSString.ligature = sTID('ligature');
PSString.lineCap = sTID('lineCap');
PSString.lineDashOffset = sTID('lineDashoffset');
PSString.lineJoin = sTID('lineJoin');
PSString.lineWidth = sTID('lineWidth');
PSString.metricsKern = sTID('metricsKern');
PSString.miterJoin = sTID('miterJoin');
PSString.miterLimit = sTID('miterLimit');
PSString.mojiKumiName = sTID('mojiKumiName');
PSString.mojiZume = sTID('mojiZume');
PSString.noBreak = sTID('noBreak');
PSString.oldStyle = sTID('oldStyle');
PSString.ordinals = sTID('ordinals');
PSString.ornaments = sTID('ornaments');
PSString.otbaseline = sTID('otbaseline');
PSString.paragraphStyle = sTID('paragraphStyle');
PSString.paragraphStyleRange = sTID('paragraphStyleRange');
PSString.preferredKinsokuOrder = sTID('preferredKinsokuOrder');
PSString.proportionalMetrics = sTID('proportionalMetrics');
PSString.pushIn = sTID('pushIn');
PSString.rightAki = sTID('rightAki');
PSString.roman = sTID('roman');
PSString.rotated = sTID('rotated');
PSString.rowCount = sTID('rowCount');
PSString.rowGutter = sTID('rowGutter');
PSString.rowMajorOrder = sTID('rowMajorOrder');
PSString.ruby = sTID('ruby');
PSString.singleWordJustification = sTID('singleWordJustification');
PSString.spaceAfter = sTID('spaceAfter');
PSString.spaceBefore = sTID('spaceBefore');
PSString.startIndent = sTID('startIndent');
PSString.strikethrough = sTID('strikethrough');
PSString.strikethroughOff = sTID('strikethroughOff');
PSString.strokeColor = sTID('strokeColor');
PSString.strokeOverPrint = sTID('strokeOverPrint');
PSString.stylisticAlternates = sTID('stylisticAlternates');
PSString.superScript = sTID('superScript');
PSString.swash = sTID('swash');
PSString.syntheticBold = sTID('syntheticBold');
PSString.syntheticItalic = sTID('syntheticItalic');
PSString.tcyLeftRight = sTID('tcyLeftRight');
PSString.tcyUpDown = sTID('tcyUpDown');
PSString.textEveryLineComposer = sTID('textEveryLineComposer');
PSString.textGridding = sTID('textGridding');
PSString.textLanguage = sTID('textLanguage');
PSString.textShape = sTID('textShape');
PSString.textType = sTID('TEXT');
PSString.titling = sTID('titling');
PSString.tx = sTID('tx');
PSString.ty = sTID('ty');
PSString.underlineOff = sTID('underlineOff');
PSString.underlineOffset = sTID('underlineOffset');
PSString.wariChuAutoJustify = sTID('wariChuAutoJustify');
PSString.wariChuCount = sTID('wariChuCount');
PSString.wariChuJustification = sTID('wariChuJustification');
PSString.wariChuLineGap = sTID('wariChuLineGap');
PSString.wariChuOrphan = sTID('wariChuOrphan');
PSString.wariChuScale = sTID('wariChuScale');
PSString.wariChuWidow = sTID('wariChuWidow');
PSString.warp = sTID('warp');
PSString.warpNone = sTID('warpNone');
PSString.warpPerspective = sTID('warpPerspective');
PSString.warpPerspectiveOther = sTID('warpPerspectiveOther');
PSString.warpRotate = sTID('warpRotate');
PSString.warpStyle = sTID('warpStyle');
PSString.warpValue = sTID('warpValue');
PSString.xx = sTID('xx');
PSString.xy = sTID('xy');
PSString.yx = sTID('yx');
PSString.yy = sTID('yy');

PSType.Alignment = cTID('Alg ');
PSType.AntiAlias = cTID('Annt');
PSType.Ordinal = cTID('Ordn');

PSUnit.Points = cTID('#Pnt');


"Text.jsx";
// EOF
