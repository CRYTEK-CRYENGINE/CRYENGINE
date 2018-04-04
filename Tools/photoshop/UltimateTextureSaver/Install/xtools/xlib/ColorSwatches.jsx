//
// ColorSwatches
//   This class reads ColorSwatches files from disk amongst other things...
//   Details to come...
// $Id: ColorSwatches.jsx,v 1.15 2014/09/26 20:38:25 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@show include
//-include "xlib/stdlib.js"
//-include "xlib/Stream.js"
//

//============================= ColorSwatches =================================

ColorSwatches = function() {
  var self = this;

  self.file     = '';
  self.version1 = 0;
  self.version2 = 0;
  self.numberOfColors1 = 0;
  self.numberOfColors2 = 0;
  self.count1 = 0;
  self.count2 = 0;
  self.colors = [];
  self.unnamedColors = [];
  self.colorsByName = {};

  return self;
};


ColorSwatches.RcsId = "$Id: ColorSwatches.jsx,v 1.15 2014/09/26 20:38:25 anonymous Exp $";
ColorSwatches.prototype.typename = "ColorSwatches";
ColorSwatches.RGB_TYPE  = 0;
ColorSwatches.HSB_TYPE = 1;
ColorSwatches.CMYK_TYPE = 2;
ColorSwatches.LAB_TYPE  = 7;
ColorSwatches.GRAYSCALE_TYPE = 8;

ColorSwatches.getColor = function(name) {
  if (!ColorSwatches._runtime) {
    ColorSwatches._runtime = new ColorSwatches();
    ColorSwatches._runtime.readFromRuntime();
  }
  return ColorSwatches._runtime.getColorByName(name);
};

ColorSwatches.prototype.getColorByName = function(name) {
  return this.colorsByName[name];
};
ColorSwatches.prototype.getColorByIndex = function(idx) {
  return this.colors[idx];
};
ColorSwatches.prototype.addColor = function(c, name) {
  var self = this;
  var csc;
  if (c instanceof ColorSwatchesColor) {
    csc = c;
  } else if (c instanceof SolidColor) {
    csc = new ColorSwatchesColor();
    csc.parent = self;
    csc.name = name;
    csc.color = c;
  } else {
    Error.runtimeError(9001, "Attempted to add invalid color.");
  }

  if (csc.name) {
    self.colorsByName[csc.name] = csc.color;
    self.colors.push(csc);
    self.version2 = 2;
    self.count2++;
  } else {
    self.unnamedColors.push(csc.color);
    self.version1 = 1;
    self.count1++;
  }
};
ColorSwatches.prototype.read = function(str) {
  var self = this;

  var version = str.readInt16();
  if (version == 1) {
    self.version1 = 1;
    self.numberOfColors1 = str.readInt16();
    self.count1 = 0;
    self.readColors(str, self.numberOfColors1, false);

    if (str.eof()) {
      return;
    }
    version = str.readInt16();
  }

  if (version != 2) {
    Error.runtimeError(9001, 
                       "Bad version number " + version + " in swatches file.");
  }

  self.version2 = 2;

  self.numberOfColors2 = str.readInt16();
  self.count2 = 0;
  self.readColors(str, self.numberOfColors2, true);
};
ColorSwatches.prototype.readColors = function(str, count, names) {
  var self = this;

  for (var i = 0; i < count; i++) {
    var cname;
    var cbytes = [];
    var ctype = str.readInt16();

    cbytes.push(str.readInt16());
    cbytes.push(str.readInt16());
    cbytes.push(str.readInt16());
    cbytes.push(str.readInt16());

    if (names) {
      cname = str.readUnicode();
    }

    var csc = new ColorSwatchesColor();
    csc.parent = self;
    csc.name   = cname;
    csc.setColor(cbytes, ctype);

    self.addColor(csc);
  }
  return self.colors;
};

ColorSwatches.prototype.colorsToString = function(colors) {
  var str = '';
  for (var i = 0; i < colors.length; i++) {
    str += colors[i].toString() + '\n';
  }
  return str;
};

ColorSwatches.prototype.toString = function() {

  return "[Color Swatches]";

  var self = this;
  var typeMap = [];

  typeMap[ColorSwatches.RGB_TYPE]  = "RGB";
  typeMap[ColorSwatches.HSB_TYPE]  = "HSB";
  typeMap[ColorSwatches.CMYK_TYPE] = "CMYK";
  typeMap[ColorSwatches.LAB_TYPE]  = "Lab";
  typeMap[ColorSwatches.GRAYSCALE_TYPE]  = "Grayscale";

//   var str = "{ name: " + self.vendorName + ", title: \"" + self.title +
//       "\", description: \"" + self.description + "\", numberOfColors: " +
//       self.numberOfColors + ", colorType: " + typeMap[self.colorType] + "}";

//   return str;

  return '';
};

ColorSwatches.createRGBColor = function (r, g, b) {
  var c = new RGBColor();
  c.red = r; c.green = g; c.blue = b;
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

ColorSwatches.prototype.readFromACOFile = function(fptr) {
  var self = this;

  if (fptr.constructor == String && fptr.charAt(0) != '/') {
    var presetsFolder = app.path + '/Presets';
    var f = new File(presetsFolder + '/Color Swatches/' + fptr);
    if (!f.exists) {
      f = new File(presetsFolder + '/Color Swatches/Adobe Photoshop Only/'
                   + fptr);
      if (f.exists) {
        fptr = f;
      }
    } else {
      fptr = f;
    }
  }
  self.file = Stream.convertFptr(fptr);
  var str = Stream.readStream(self.file);
  self.read(str);
};
ColorSwatches.prototype.readFromRuntime = function() {
  var self = this;
  var file = new File(Folder.temp + '/' + File().name + ".aco");

  if (file.exists) {
    file.remove();
  }

  SwatchPalette.saveSwatches(file);
  self.readFromACOFile(file);
  file.remove();
  self.file = '';
};
ColorSwatches.prototype.readFromX11File = function(fptr) {
  var self = this;
  var infile = Stream.convertFptr(fptr);

  infile.open("r") || throwFileError("Unable to open input file");
  var str = infile.read();
  infile.close();

  self.file = infile;

  var lines = str.split(/\n/);

  // skip the first line
  if (!lines[0].match(/rgb\.txt/)) {
    throw "The file does not appear to be an X11 rgb.txt file.";
  }
  for (var i = 1; i < lines.length; i++) {
    var line = lines[i].split(/\s+/);
    var r = line.shift();
    if (r == '') {
      r = line.shift();
    }
    var g = line.shift();
    var b = line.shift();
    var nm = line.join(' ');

    var c = ColorSwatches.createRGBColor(r, g, b);
    self.addColor(c, nm);
  }
};

ColorSwatches.fromCSVString = function(str, ar, hasHeaders) {
  hasHeaders = (hasHeaders != false);
  if (!ar) {
    ar = [];
  }
  var lines = str.split(/\r|\n/);
  if (lines.length == 0) {
    return ar;
  }

  var rexp = new RegExp('([^",]+)|"((?:[^"]|"")*)"');
  function parseCSVLine(line) {
    var parts = [];
    line = line.trim();
    var res;
    while ((res = line.match(rexp)) != null) {
      if (res[1]) {
        parts.push(res[1]);
      } else {
        parts.push(res[2].replace(/""/g, '"'));
      }
      line = line.substr(res[0].length + res.index);
    }
    return parts;
  }

  var headers = [];
  if (hasHeaders) {
    headers = parseCSVLine(lines[0].trim());
    lines.shift();
  }
  ar.headers = headers;

  if (lines.length == 0) {
    return ar;
  }

  for (var i = 1; i < lines.length; i++) {
    var row = parseCSVLine(lines[i]);
    if (hasHeaders) {
      var obj = new Object();
      for (var j = 0; j < row.length; j++) {
        if (headers[j]) {
          obj[headers[j]] = row[j];
        } else {
          obj[j] = row[j];
        }
      }
      ar.push(obj);
    } else {
      ar.push(row);
    }
  }
  return ar;
};

ColorSwatches.prototype.readFromCSVFile = function(fptr) {
  var self = this;

  sellf.init();
  var infile = Stream.convertFptr(fptr);

  infile.open("r") || throwFileError("Unable to open input file");
  var str = infile.read();
  infile.close();

  self.file = infile;

  var lines = str.split(/\n|\r/);

  if (!lines[0].match(/\d/)) {            // if there are numbers in the first
    lines.unshift("red,green,blue,name"); // line, then it's a plain RGB file
  }

  lines[0] = lines[0].toLowerCase();
  var header = lines[0];
  var cobjs = ColorSwatches.fromCSVString(lines.join('\n'));

  if (!cobjs) {
    return;
  }

  var ctype;
  if (header.contains("red")) {
    ctype = ColorSwatches.RGB_TYPE;
  }
  // XXX add other color spaces here

  for (var i = 1; i < cobjs.length; i++) {
    var cobj = cobjs[i];
    var csc = new ColorSwatchesColor();

    csc.parent = self;
    csc.name = cobj.name;
    var c;
    switch (ctype) {
      case ColorSwatches.RGB_TYPE:
        c = ColorSwatches.createRGBColor(cobj.red, cobj.green, cobj.blue);
        break;
        // XXX add other color spaces here
    default:
      break;
    }
    csc.color = c;
    self.addColor(csc);
  }
};
ColorSwatches.prototype.readFromINIFile = function(fptr) {
  alert("This feature is not yet implemented.");
  return;

  var self = this;

  var infile = Stream.convertFptr(fptr);

  infile.open("r") || throwFileError("Unable to open input file");
  var str = infile.read();
  infile.close();

  self.file = infile;
};

// this output is primarily for diagnostic purposes
ColorSwatches.prototype.writeToTextFile = function(fptr) {
  var self = this;
  var str = '';

  str += self.colorsToString(self.colors);

  var file = Stream.convertFptr(fptr);
  file.open("w") || Stream.throwError("Unable to open output file \"" +
                                      file + "\".\r" + file.error);
  file.write(str);
  file.close();
};

ColorSwatches.prototype.writeColor = function(str, csc) {
  var ctype = csc.getColorType();
  str.writeInt16(ctype);
  var bytes = csc.toACOBytes();
  for (var i = 0; i < bytes.length; i++) {
    str.writeInt16(bytes[i]);
  }
  if (csc.name) {
    str.writeUnicode(csc.name);
  }
};
ColorSwatches.prototype.writeToACOFile = function(fptr) {
  var self = this;
  var str = new Stream();

  self.version1 = 1;
  if (self.version1) {
    str.writeInt16(self.version1);
    str.writeInt16(self.colors.length);

    for (var i = 0; i < self.colors.length; i++) {
      var c = self.colors[i];
      var n = c.name;
      c.name = undefined;
      self.writeColor(str, c);
      c.name = n;
    }
  } else {
    str.writeInt16(1);
    str.writeInt16(0);
  }
  if (self.version2) {
    str.writeInt16(self.version2);
    str.writeInt16(self.colors.length);

    for (var i = 0; i < self.colors.length; i++) {
      var c = self.colors[i];
      if (c.name) {
        self.writeColor(str, c);
      }
    }
  } else {
    str.writeInt16(2);
    str.writeInt16(0);
 }

  Stream.writeToFile(fptr, str.toStream());
};
ColorSwatches.prototype.writeToCSVFile = function(fptr) {
  alert("This feature is not yet implemented.");
};
ColorSwatches.prototype.writeToINIFile = function(fptr) {
  alert("This feature is not yet implemented.");
};
ColorSwatches.prototype.writeToRuntime = function() {
  var self = this;
  for (var i = 0; i < self.colors.length; i++) {
    var csc = self.colors[i];
    SwatchPalette.addRuntime(csc.color, csc.name);
  }
};

SwatchPalette = function() {};
SwatchPalette.getColor = function(name) {
  return ColorSwatches.getColor(name);
};

SwatchPalette.saveSwatches = function(fptr) {
  var file = Stream.convertFptr(fptr);
  var desc = new ActionDescriptor();
  desc.putPath( cTID('null'), file);
  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), cTID('Clrs'));
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));
  desc.putReference(cTID('T   '), ref);

  executeAction(cTID('setd'), desc, DialogModes.NO);
};

SwatchPalette.addRuntime = function(color, name) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putClass(cTID('Clrs'));
  desc.putReference(cTID('null'), ref);
    var desc1 = new ActionDescriptor();
    desc1.putString( cTID('Nm  '), name);
      var desc2 = new ActionDescriptor();
      desc2.putDouble(cTID('Rd  '), color.rgb.red);
      desc2.putDouble(cTID('Grn '), color.rgb.green);
      desc2.putDouble(cTID('Bl  '), color.rgb.blue);
    desc1.putObject( cTID('Clr '), cTID('RGBC'), desc2);
  desc.putObject( cTID('Usng'), cTID('Clrs'), desc1);

  executeAction( cTID('Mk  '), desc, DialogModes.NO);
};

SwatchPalette.appendFile = function(fptr) {
  SwatchPalette.loadFile(fptr, true);
};
SwatchPalette.loadFile = function(fptr, append) {
  var file = Stream.convertFptr(fptr);

  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), cTID('Clrs'));
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);
  desc.putPath(cTID('T   '), file);

  if (append) {
    desc.putBoolean(cTID('Appe'), true);
  }

  executeAction(cTID('setd'), desc, DialogModes.NO);
};

SwatchPalette.deleteSwatch = function(index) {
  var ref = new ActionReference();
  ref.putIndex(cTID('Clrs'), index);

  var list = new ActionList();
  list.putReference(ref);

  var desc = new ActionDescriptor();
  desc.putList(cTID('null'), list);

  executeAction(cTID('Dlt '), desc, DialogModes.NO);
};
SwatchPalette.getIndexOfName = function(name) {
  var names = SwatchPalette.getPresets();
  for (var i = 0; i < names.length; i++) {
    if (names[i] == name) {
      return i+1;
    }
  }
  return -1;
};
SwatchPalette.deleteSwatchByName = function(name) {
  var idx = SwatchPalette.getIndexOfName(name);
  if (idx != -1) {
    SwatchPalette.deleteSwatch(idx);
  }
};
SwatchPalette.renameSwatch = function(index, name) {
  var ref = new ActionReference();
  ref.putIndex(cTID('Clrs'), index);

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);
  desc.putString(cTID('T   '), name);

  executeAction(cTID('Rnm '), desc, DialogModes.NO );
};
SwatchPalette.renameSwatchByName = function(oldName, newName) {
  var idx = SwatchPalette.getIndexOfName(oldName);
  if (idx != -1) {
    SwatchPalette.renameSwatch(idx, newName);
  }
};
SwatchPalette.reset = function() {
  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), cTID('Clrs'));
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);

  executeAction(cTID('Rset'), desc, DialogModes.NO);
};

SwatchPalette.getPresetManager = function() {
  var ref = new ActionReference();
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));

  var appDesc = app.executeActionGet(ref);
  return appDesc.getList(sTID('presetManager'));
};

SwatchPalette.getPresets = function() {
  var key = cTID('Clr ');
  var names = [];

  var mgr = SwatchPalette.getPresetManager();
  var max = mgr.count;

  for (var i = 0; i < max; i++) {
    var objType = mgr.getObjectType(i);
    if (objType == key) {
      break;
    }
  }

  if (i != max) {
    var preset = mgr.getObjectValue(i);
    var list = preset.getList(cTID('Nm  '));
    var max = list.count;
    for (var i = 0; i < max; i++) {
      var str = list.getString(i);
      names.push(str);
    }
  }

  return names;
};

//============================ ColorSwatchesColor =============================

SolidColor.prototype._toString = function() {
  var c = this;
  var m = c.model.toString().match(/\.(\w+)$/);
  var model = m[1];
  return "{ " + model + ": [" + c._toBytes() + "]}";
};

SolidColor.prototype._toBytes = function() {
  var c = this;
  var bytes;

  if (c.model == ColorModel.RGB) {
    bytes = [c.rgb.red, c.rgb.green, c.rgb.blue];
  } else if (c.model == ColorModel.HSB) {
    bytes = [c.hsb.hue, c.hsb.saturation, c.hsb.brightness];
  } else if (c.model == ColorModel.CMYK) {
    bytes = [c.cmyk.cyan, c.cmyk.magenta, c.cmyk.yellow, c.cmyk.black];
  } else if (c.model == ColorModel.LAB) {
    bytes = [c.lab.l, c.lab.a, c.lab.b];
  } else if (c.model == ColorModel.GRAYSCALE) {
    bytes = [c.gray.gray];
  } else {
    throw "Unknown/unsupported color model.";
  }

  return bytes;
};

ColorSwatchesColor = function() {
  var self = this;

  self.name   = '';
  self.color  = null;
  self.parent = null;
};
ColorSwatchesColor.prototype.typename = "ColorSwatchesColor";

ColorSwatchesColor.prototype.toString = function() {
  var self = this;

  function colorToString(c) {
    var str;
    var m = c.model.toString().match(/\.(\w+)$/);
    var model = m[1];
    if (model == "NONE") {
      str = "{ NONE: []}";
    } else {
      str = "{ " + model + ": [" + c._toBytes() + "]}";
    }
    return str;
  }
  var cstr = (self.color ? colorToString(self.color) : "[]");
  if (self.name) {
    return "{ \"" + self.name + "\", " + cstr + "}";
  } else {
    return cstr;
  }
};

ColorSwatchesColor.prototype.toBytes = function() {
  return this.color._toBytes();
};

ColorSwatchesColor.prototype.setColor = function(cwords, ctype) {
  var self = this;
  var color = new SolidColor();

  // Need to add all conversion routines

  function cnvt256(v) {
    var rc = v/256.0;
    return (rc > 255.0) ? 255 : rc; // XXX this seems a bit odd (bug)
  }
  function cnvt100(v) {
    return 100.0 * v / 0xFFFF;
  }

  switch (ctype) {
    case ColorSwatches.RGB_TYPE:
      // 0..65535 mapped to 0..255
      color.rgb.red   = cnvt256(cwords[0]);
      color.rgb.green = cnvt256(cwords[1]);
      color.rgb.blue  = cnvt256(cwords[2]);
      break;

    case ColorSwatches.HSB_TYPE:
      // 0..65535 mapped to 0..360
      color.hsb.hue = 360 * cwords[0]/0xFFFF;
      // 0..65535 mapped to 0..100
      color.hsb.saturation = cnvt100(cwords[1]);
      // 0..65535 mapped to 0..100
      color.hsb.brightness  = cnvt100(cwords[2]);
      break;

    case ColorSwatches.CMYK_TYPE:
      // 0..65535 mapped to 0..100
      color.cmyk.cyan    = 100 - cnvt100(cwords[0]);
      color.cmyk.magenta = 100 - cnvt100(cwords[1]);
      color.cmyk.yellow  = 100 - cnvt100(cwords[2]);
      color.cmyk.black   = 100 - cnvt100(cwords[3]);
      break;

    case ColorSwatches.LAB_TYPE:
      // 0..10000 mapped to 0..100
      cwords[0]  = cwords[0] / 100;

      // -12800..12700 mapped to -128.0..127.0
      if (cwords[1] & 0x8000) {
        cwords[1] = (cwords[1]-0xFFFF) / 100;
      } else {
        cwords[1] = cwords[1] / 100;
      }

      // -12800..12700 mapped to -128.0..127.0
      if (cwords[2] & 0x8000) {
        cwords[2] = (cwords[2]-0xFFFF) / 100;
      } else {
        cwords[2] = cwords[2] / 100;
      }

      color.lab.l = cwords[0];
      color.lab.a = cwords[1];
      color.lab.b = cwords[2];
      break;

    case ColorSwatches.GRAYSCALE_TYPE:
      // 0..10000 mapped to 0..100
      color.gray.gray = cwords[0] / 100;
      break;

    default:
      throw "Bad color type specified for color swatch";
  }

  self.color = color;
};

ColorSwatchesColor.prototype.toACOBytes = function() {
  var self = this;
  var color = self.color;
  var ctype = self.getColorType();
  var cwords = [0,0,0,0];

  function cnvtCMYK(v) {
    return (100 - v) * 0xFFFF / 100;
  }

  switch (ctype) {
    case ColorSwatches.RGB_TYPE:
      // 0..65535 mapped from 0..256
      cwords[0] = color.rgb.red * 256;
      cwords[1] = color.rgb.green * 256;
      cwords[2] = color.rgb.blue * 256;
      break;

    case ColorSwatches.HSB_TYPE:
      // 0..65535 mapped from 0..60
      cwords[0] = color.hsb.hue * 0xFFFF / 360;
      // 0..65535 mapped from 0..100
      cwords[1] = color.hsb.saturation * 0xFFFF / 100;
      // 0..65535 mapped from 0..100
      cwords[2] = color.hsb.brightness * 0xFFFF / 100;
      break;

    case ColorSwatches.CMYK_TYPE:
      // 0..65535 mapped from 0..100
      cwords[0] = cnvtCMYK(color.cmyk.cyan);
      cwords[1] = cnvtCMYK(color.cmyk.magenta);
      cwords[2] = cnvtCMYK(color.cmyk.yellow);
      cwords[3] = cnvtCMYK(color.cmyk.black);
      break;

    case ColorSwatches.LAB_TYPE:
      // 0..10000 mapped from 0..100
      cwords[0]  = color.lab.l * 100;

      // -12800..12700 mapped from -128.0..127.0
      if (color.lab.a < 0) {
        cwords[1] = color.lab.a * 100 + 0xFFFF;
      } else {
        cwords[1] = color.lab.a * 100;
      }

      // -12800..12700 mapped from -128.0..127.0
      if (color.lab.b < 0) {
        cwords[2] = color.lab.b * 100 + 0xFFFF;
      } else {
        cwords[2] = color.lab.b * 100;
      }

      break;

    case ColorSwatches.GRAYSCALE_TYPE:
      // 0..10000 mapped from 0..100
      cwords[0] = color.gray.gray * 100;
      break;

    default:
      throw "Bad color type specified for color swatch";
  }

  return cwords;
};
ColorSwatchesColor.prototype.getColorType = function() {
  var ctype;
  var c = this.color;
  if (c.model == ColorModel.RGB) {
    ctype = ColorSwatches.RGB_TYPE;
  } else if (c.model == ColorModel.HSB) {
    ctype = ColorSwatches.HSB_TYPE;
  } else if (c.model == ColorModel.CMYK) {
    ctype = ColorSwatches.CMYK_TYPE;
  } else if (c.model == ColorModel.LAB) {
    ctype = ColorSwatches.LAB_TYPE;
  } else if (c.model == ColorModel.GRAYSCALE) {
    ctype = ColorSwatches.GRAYSCALE_TYPE;
  } else {
    throw "Unknown/unsupported color model.";
  }
  return ctype;
};
// probably unneeded
ColorSwatchesColor.prototype.getSolidColor = function() {
  var self = this;
  var c = self.color;
  if (c instanceof SolidColor) {
    return c;
  }
  var sc = new SolidColor();

  if (c.model == ColorModel.RGB) {
    sc.rgb = c;
  } else if (c.model == ColorModel.HSB) {
    sc.hsb = c;
  } else if (c.model == ColorModel.CMYK) {
    sc.cmyk = c;
  } else if (c.model == ColorModel.LAB) {
    sc.lab = c;
  } else if (c.model == ColorModel.GRAYSCALE) {
    sc.lab = c;
  } else {
    throw "Unsupported color space";
  }
  return sc;
};

ColorSwatchesColor.testConversions = function() {
  var rgb = { red: 0, green:111, blue: 222};
  var cmyk = { cyan: 2, magenta:122, yellow:222, black:255};
  var hsb = { hue: 50, saturation: 75, brightness: 80};
  var lab = { l: 90, a: -20, b: 20 };
  var gray = { gray: 80 };

  function copy(from, to) {
    for (var idx in from) {
      to[idx] = from[idx];
    }
  }

  function testconv(bytes, ctype) {
    var c = new SolidColor();
    switch (ctype) {
      case ColorSwatches.RGB_TYPE: copy(bytes, c.rgb); break;
      case ColorSwatches.HSB_TYPE: copy(bytes, c.hsb); break;
      case ColorSwatches.CMYK_TYPE: copy(bytes, c.cmyk); break;
      case ColorSwatches.LAB_TYPE: copy(bytes, c.lab); break;
      case ColorSwatches.GRAYSCALE_TYPE: copy(bytes, c.gray); break;
    default:
      throw "Bad color type specified for color swatch";
    }

    var csc = new ColorSwatchesColor();
    csc.color = c;

    var acobytes = csc.toACOBytes();

    var dupe = new ColorSwatchesColor();
    dupe.setColor(acobytes, ctype);
    if (dupe.color._toString() != c._toString()) {
      alert("conversion failed for " + bytes.toSource());
    }
  }

  testconv(rgb, ColorSwatches.RGB_TYPE);
  testconv(hsb, ColorSwatches.HSB_TYPE);
  testconv(cmyk, ColorSwatches.CMYK_TYPE);
  testconv(lab, ColorSwatches.LAB_TYPE);
  testconv(gray, ColorSwatches.GRAYSCALE_TYPE);
};

ColorSwatches.test = function() {
  var start = new Date().getTime();

  var sw = new ColorSwatches();

  var tmp = Folder.temp.toString() + '/';

  //sw.readFromACOFile("Default.aco");

  // Here are some different testcases
  // To activate one, change 'false' to 'true'

  if (false) { // test reading/writing ACO files, LAB colorspace
    sw.readFromACOFile("ANPA Colors.aco");
    sw.writeToTextFile(tmp + "apna.txt");
    sw.writeToACOFile(tmp + "apna.aco");
  }

  if (true) {  // test reading the runtime palette
    //SwatchPalette.resetRuntime();
    sw.readFromRuntime();
    sw.writeToTextFile(tmp + "runtime.txt");
    sw.writeToACOFile(tmp + "runtime.aco");
  }

  if (false) {
    sw.readFromACOFile("Web Safe Colors.aco");
    sw.writeToTextFile(tmp + "websafe.txt");
    sw.writeToACOFile(tmp + "websafe.aco");
  }

  if (false) {
    sw.readFromCSVFile(tmp + "larrys.csv");
    sw.writeToRuntime();
  }

  if (false) {
    sw.readFromX11File(tmp + "rgb.txt");
    sw.writeToACOFile(tmp + "X11.aco");
    return;
    sw.readFromACOFile(tmp + "rgb.aco");
    sw.writeToTextFile(tmp + "rgb-aco.txt");
    sw.writeToACOFile(tmp + "rgb-out.aco");
  }

  var stop = new Date().getTime();
  var elapsed = (stop - start)/1000;
  alert(tmp + '\r\n' + "Done (" + Number(elapsed).toFixed(3) + " secs).");
};

//ColorSwatchesColor.testConversions();

//ColorSwatches.test();

"ColorSwatches.jsx";
// EOF
