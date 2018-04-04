//
// ColorBook
//   This class reads colorbook files from disk. There are a couple of minor
//   bugs under CS2 that I will track down later.
//
// $Id: ColorBook.js,v 1.10 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//

//============================== ColorBook ====================================

ColorBook = function() {
  var self = this;

  self.file      = null;
  self.signature = 0;
  self.version   = 0;

  self.vendorID = 0;
  self.title = '';
  self.prefix = '';
  self.description = '';
  self.numberOfColors = 0;
  self.colorsPerPage  = 0;
  self.keyColorPage   = 0;
  self.colorType      = -1;
  self.colors = [];
};
ColorBook.RcsId = "$Id: ColorBook.js,v 1.10 2010/03/29 02:23:23 anonymous Exp $";
ColorBook.prototype.typename = "ColorBook";
ColorBook.RGB_TYPE  = 0;
ColorBook.CMYK_TYPE = 2;
ColorBook.LAB_TYPE  = 7;

ColorBook.prototype.read = function(str) {
  var self = this;

  self.signature      = str.readWord();
  self.version        = str.readInt16();

  self.vendorID       = str.readInt16();
  self.vendorName     = ColorBookIDMap.reverseLookup(self.vendorID);
  self.title          = self.readLocalizedUnicode(str);
  self.prefix         = self.readLocalizedUnicode(str);
  self.postfix        = self.readLocalizedUnicode(str);
  self.description    = self.readLocalizedUnicode(str);
  self.numberOfColors = str.readInt16();
  self.colorsPerPage  = str.readInt16();
  self.keyColorPage   = str.readInt16();
  self.colorType      = str.readInt16();

  for (var i = 0; i < self.numberOfColors; i++) {
    var c = new ColorBookColor();
    c.parent = self;
    c.name   = self.readLocalizedUnicode(str);
    c.displayName = self.prefix + c.name + self.postfix;
    c.key    = str.readString(6);
    c.setColor(self.readColorBytes(str), self.colorType);

    // Some colors can be nameless. Deal with it here, if you like
    // if (c.name.length != 0) { self.colors.push(c); }
    self.colors.push(c);
  }
};

ColorBook.prototype.toString = function() {
  var self = this;
  var typeMap = [];

  typeMap[ColorBook.RGB_TYPE]  = "RGB";
  typeMap[ColorBook.CMYK_TYPE] = "CMYK";
  typeMap[ColorBook.LAB_TYPE]  = "LAB";

  var str = "{ name: " + self.vendorName + ", title: \"" + self.title +
      "\", description: \"" + self.description + "\", numberOfColors: " +
      self.numberOfColors + ", colorType: " + typeMap[self.colorType] + "}";

  return str;
};

ColorBook.prototype.readLocalizedUnicode = function(str) {
  var s = str.readUnicode(false);
  return s.replace(/\$\$\$.+=/g, '');
};

ColorBook.prototype.readColorBytes = function(str) {
  var self = this;
  var wlen = (self.colorType == ColorBook.CMYK_TYPE) ? 4 : 3;
  var cbytes = [];

  for (var i = 0; i < wlen; i++) {
    cbytes.push(str.readByte());
  }

  if (self.colorType == ColorBook.LAB_TYPE) {
    cbytes[0]  = Math.round(100 * cbytes[0] / 256);
    cbytes[1] -= 128;
    cbytes[2] -= 128;
  }

  // probably need to do a similar conversion for CMYK

  return cbytes;
};

ColorBook.prototype.readFromFile = function(fptr) {
  var self = this;
  self.file = Stream.convertFptr(fptr);
  var str = Stream.readStream(self.file);
  self.read(str);
};


//============================ ColorBookColor =================================

ColorBookColor = function() {
  var self = this;

  self.name   = '';
  self.key    = -1;
  self.color  = null;
  self.displayName = '';
  self.parent = null;
};
ColorBookColor.prototype.typename = "ColorBookColor";
ColorBookColor.prototype.toString = function() {
  var self = this;
  function colorToString(c) {
    var str;
    if (c.typename == "LabColor") {
      str = "{ Lab: [ " + c.l + ", " + c.a + ", " + c.b + " ]}";
    } else if (c.typename == "RGBColor") {
      str = "{ RGB: [ " + c.red + ", " + c.green +  ", " + c.blue + " ]}";
    } else if (c.typename == "CMYKColor") {
      str = "{ CMYK: [ " + c.cyan + ", " + c.magenta + ", " + c.yellow +
        ", " + c.black + " ]}";
    }
    return str;
  }
  var cstr = colorToString(self.color);
  return "{ name: \"" + self.displayName + "\", " + cstr + "}";
};

ColorBookColor.prototype.setColor = function(cbytes, ctype) {
  var self = this;

  switch (ctype) {
    case ColorBook.RGB_TYPE:
      self.color = new RGBColor();
      self.color.red   = cbytes[0];
      self.color.green = cbytes[1];
      self.color.blue  = cbytes[2];
      break;
    case ColorBook.CMYK_TYPE:
      self.color = new CMYKColor();
      self.color.cyan    = cbytes[0];
      self.color.magenta = cbytes[1];
      self.color.yellow  = cbytes[2];
      self.color.black   = cbytes[3];
      break;
    case ColorBook.LAB_TYPE:
      self.color = new LabColor();
      self.color.l = cbytes[0];
      self.color.a = cbytes[1];
      self.color.b = cbytes[2];
      break;
    default:
      throw "Bad color type specified for color book";
  }
};

ColorBookColor.prototype.getSolidColor = function() {
  var self = this;
  var c = self.color;
  var sc = new SolidColor();

  if (c.typename == "LabColor") {
    sc.lab = c;
  } else if (c.typename == "RGBColor") {
    sc.rgb = c;
  } else if (c.typename == "CMYKColor") {
    sc.cmyk = c;
  } else {
    throw "Unsupported color mode";
  }
  return sc;
};

ColorBookIDMap = {
  ANPA:                  3000,
  Focoltone:             3001,
  PantoneCoated:         3002,
  PantoneProcess:        3003,
  PantoneProSlim:        3004,
  PantoneUncoated:       3005,
  Toyo:                  3006,
  Trumatch:              3007,
  HKSE:                  3008,
  HKSK:                  3009,
  HKSN:                  3010,
  HKSZ:                  3011,
  DIC:                   3012,
  PantonePastelCoated:   3020,
  PantonePastelUncoated: 3021,
  PantoneMetallic:       3022
};
ColorBookIDMap.reverseLookup = function(id) {
  for (var prop in ColorBookIDMap) {
    if (ColorBookIDMap[prop] == id) {
      return prop;
    }
  }
  return undefined;
};

"ColorBook.js";
// EOF
