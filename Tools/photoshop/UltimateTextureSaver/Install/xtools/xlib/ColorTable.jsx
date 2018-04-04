//
// ColorTable
//   This class reads and writes color table files from disk.
//   This assumes that the index color table is (at most) 256 colors long.
//   If the file contains less, toString will pad with 0,0,0 entries.
//
// $Id: ColorTable.jsx,v 1.7 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//

//============================= ColorTable ====================================

ColorTable = function() {
  var self = this;

  self.file      = null;
  self.colors = [];
};
ColorTable.RcsId = "$Id: ColorTable.jsx,v 1.7 2010/03/29 02:23:23 anonymous Exp $";

ColorTable.prototype.typename = "ColorTable";

//
// toString always returns 256 colors
//
ColorTable.prototype.toString = function() {
  var self = this;
  var str = '';

  if (!self.colors || self.colors.length == 0) {
    return undefined;
  }

  for (var i = 0; i < 256; i++) {
    if (i >= self.colors.length) {
      str += "0,0,0\n";
    } else {
      var sc = self.colors[i];
      var rgb = sc.rgb;
      str += rgb.red + ',' + rgb.green + ',' + rgb.blue + '\n';
    }
  };

  return str;
};

ColorTable.prototype.read = function(str) {
  var self = this;

  self.colors = [];

  for (var i = 0; i < 256; i++) {
    var sc = new SolidColor();
    var rgb = sc.rgb;

    rgb.red = str.readByte();
    rgb.green = str.readByte();
    rgb.blue = str.readByte();
    self.colors.push(sc);

    if (str.eof()) {
      break;
    }
  }
};

ColorTable.prototype.readFromFile = function(fptr) {
  var self = this;

  if (fptr.constructor == String) {
    fptr = File(fptr);
  }

  var file = self.file = fptr;

  file.open("r");
  file.encoding = 'BINARY';
  var s = file.read();
  file.close();

  var str = {
    str: s,
    ptr: 0
  };

  str.readByte = function() {
    var self = this;
    if (self.ptr >= self.str.length) {
      return -1;
    }
    return self.str.charCodeAt(self.ptr++);
  };

  str.eof = function() {
    return this.ptr >= this.str.length;
  };

  self.read(str);
};

ColorTable.prototype.apply = function(fptr) {
  var self = this;

  function cTID(s) { return app.charIDToTypeID(s); };
   function sTID(s) { return app.stringIDToTypeID(s); };

  self.readFromFile(fptr);
  var desc  = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putProperty(cTID('Clr '), cTID('ClrT'));
  desc.putReference(cTID('null'), ref);

  var list = new ActionList();

  var colors = self.colors;

  for (var i = 0; i < colors.length; i++) {
    var color = colors[i];
    var cdesc = new ActionDescriptor();
    cdesc.putDouble(cTID('Rd  '), color.rgb.red);
    cdesc.putDouble(cTID('Grn '), color.rgb.green);
    cdesc.putDouble(cTID('Bl  '), color.rgb.blue);
    list.putObject(cTID('RGBC'), cdesc);
  }

  desc.putList(cTID('T   '), list);
  return executeAction(cTID('setd'), desc, DialogModes.NO);
};

ColorTable.prototype.writeToFile = function(file) {
  var self = this;

  if (file.constructor == String) {
    file = File(file);
  }

  file.open("w") || Error.runtimeError(9002, "Unable to open output file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';

  var str = {
    str: [],
    ptr: 0
  };

  str.writeByte = function(b) {
    var self = this;
    self.str[self.ptr++] = String.fromCharCode(b);
    return self;
  };

  var len = Math.min(colors.length, 256);

  for (var i = 0; i < len; i++) {
    var color = colors[i];
    str.writeByte(color.rgb.red);
    str.writeByte(color.rgb.green);
    str.writeByte(color.rgb.blue);
  }

  var len = str.str.length;
  for (var i = 0; i < len; i++) {
    file.write(str.str[i]);
  }

  file.close();
};

ColorTable.saveCurrentTable = function(file) {
  var self = this;

  function cTID(s) { return app.charIDToTypeID(s); };
  function cTID(s) { return app.stringIDToTypeID(s); };

  if (file.constructor == String) {
    file = File(file);
  }

  // var desc = new ActionDescriptor();
  // desc.putPath( cTID('null'), file);

  // var ref = new ActionReference();
  // ref.putProperty(cTID('Clr '), cTID('ClrT'));
  // desc.putReference(cTID('null'), ref);

  // executeAction(cTID('setd'), desc, DialogModes.NO );

};

/*
Sample Usage

var clrTbl = new ColorTable();
clrTbl.apply("~/Desktop/temp/bianca.act");

var clrTbl = new ColorTable();
clrTbl.readFromFile("~/someTable.act");
var str = clrTbl.toString();
alert(str);
*/

// for testing
ColorTable.main = function() {
  var clrTbl = new ColorTable();
  clrTbl.apply("~/Desktop/temp/bianca.act");
};
//ColorTable.main();



"ColorTable.jsx";
// EOF
