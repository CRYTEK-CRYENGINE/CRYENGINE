//
// ShapesFile
//
// $Id: ShapesFile.js,v 1.7 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2008, xbytor
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
//include "xlib/Stream.js"
//include "xlib/PresetsManager.jsx"
//

CustomShapeInfo = function() {
  var self = this;
  self.name = '';
  self.id = '';
  self.x = 0;
  self.x = 0;
  self.w = 0;
  self.h = 0;
};
CustomShapeInfo.prototype.typename = "CustomShapeInfo";
CustomShapeInfo.prototype.toString = function() {
  var self = this;
  var str = "{ name: \"" + self.name + "\", ";
  str += "id: \"" + this.id + "\", ";
  str += "[" + self.x + ", " + self.y + ", ";
  str += (self.x + self.w) + ", " + (self.y + self.h) + "] }";
  return str;
};

CustomShapeInfo.massageName = function(name) {
  var n = localize(name);
  n = n.replace(/\s+/g, '');  // remove any embedded spaces
  n = n.replace(/\W/g, '_');  // replace any non-word characters with '_'
  n = n.replace(/[_]+$/, ''); // remove any trailing '_'
  return n;
};

CustomShapeInfo.getShapeInfo = function(name) {
  var pm = new PresetsManager();
  var csm = new PTM(pm, PresetType.CUSTOM_SHAPES);

  var tempFile = new File(Folder.temp + '/' + CustomShapeInfo.massageName(name) + ".csh");
  csm.saveElement(name, tempFile);

  var shapesFile = new ShapesFile();
  shapesFile.read(tempFile);
  var info = shapesFile.shapes[0];
  tempFile.remove();

  return info || {};
};

ShapesFile = function() {
  var self = this;
  self.file = null;
  self.shapes = [];
};
ShapesFile.prototype.typename = "ShapesFile";

ShapesFile.prototype.toString = function() {
  var self = this;

  var str = "[ShapesFile " + decodeURI(self.file) + "\r\n";
  var shapes = self.shapes;
  for (var i = 0; i < shapes.length; i++) {
    str += shapes[i] + ",\r\n";
  }
  str += "]\r\n";

  return str;
};

ShapesFile.prototype.readFirstShape = function(fptr) {
  var self = this;

//   debugger;
  var idLen = 37;

  self.file = File(fptr);

  var str = Stream.readStream(self.file);

  var fileID = str.readKey();

  if (fileID != 'cush') {
    throw "File is not a custom shape file";
  }

  var n = str.readWord(); // 2??
  var cnt = str.readWord();
  var name = str.readUnicode();
  if (str.ptr % 4) {
    str.readUnicodeChar(); // pad
  }

  var idx = str.readWord();
  var len = str.readWord();
  var id = str.readString(idLen);

  var shapeInfo = {
    name: name,
    file: self.file,
    id: id,
    index: idx
  };
  return shapeInfo;
};

ShapesFile.prototype.read = function(fptr) {
  var self = this;

  self.file = File(fptr);
  self.shapes = [];

  var str = Stream.readStream(self.file);

  var fileID = str.readKey();

  if (fileID != 'cush') {
    throw "File is not a custom shape file";
  }

  var re = /(\x00\w|\x00\d)(\x00\w|\x00\s|\x00\d|\x00\.|\x00\'|\x00\-)+(.|\n){10}\$[-a-z\d]+/g;//'

  var raw = str.str;
  var parts = raw.match(re);
  if (parts == null) {
    re = /(\x00\w|\x00\d)(\x00\w|\x00\s|\x00\d|\x00\.|\x00\'|\x00\-)+(.|\n){12}\$[-a-z\d]+/g;//'
    parts = raw.match(re);

    if (!parts) {
//       debugger;
      return undefined;
    }
  }

  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    var sp = p.replace(/\x00/g, '').split('$');
    
    sp[0] = sp[0].match(/[\w\d][\w\s\d\.\'\-]+/)[0]; //'

    var shape = new CustomShapeInfo();
    shape.name = sp[0];
    var id = sp[1];
    shape.id = '$' + sp[1];
    var m = str.str.match(sp[1]);
    str.ptr = m.index + m[0].length;
    shape.y = str.readWord() + 2;
    shape.x = str.readWord() + 2;
    shape.h = str.readWord() - shape.y - 2;
    shape.w = str.readWord() - shape.x - 2;
    self.shapes.push(shape);
  }

  return self.shapes;
};


if (!Stream.prototype.readAsciiOrKey) {
  //===========================================================================
  //   Stream Extensions
  //===========================================================================
  Stream.prototype.readAsciiOrKey = function() {
    var self = this;
    var len = self.readWord();
    if (len > 20000) {
      throw "Read of string longer than 20K requested.";
    }
    return self.readString(len ? len : 4);
  };

  Stream.prototype.readKey = function() {
    return this.readString(4);
  };
  Stream.prototype.readUnicodeString = function(len, readPad) {
    var self = this;
    var s = '';
    for (var i = 0; i < len; i++) {
      var uc = self.readInt16();
      s += String.fromCharCode(uc);
    }
    if (readPad == true) {
      self.readInt16();     // null pad
    }
    return s;
  };
  Stream.prototype.readUnicodeOrKey = function() {
    var self = this;
    //var len = self.int16();
    var len = self.readWord();
    if (len > 20000) {
      throw "Read of string longer than 20K requested.";
    }
    var v;
    if (len == 0) {
      v = self.readString(4);
    } else {
      //self.ptr -= 4;
      v = self.readUnicodeString(len);
    }
    return v;
  };
};

ShapesFile.test = function() {
  if (false) {
    var shapeInfo = CustomShapeInfo.getShapeInfo("EricaXHeart");
    alert(listProps(shapeInfo));

    return;

    var pm = new PresetsManager();
    var names = pm.getNames(PresetType.CUSTOM_SHAPES);

    var str = '';
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      var shapeInfo = CustomShapeInfo.getShapeInfo(name);
      str += name + '::' + shapeInfo.toString() + '\r';
//       if (!confirm(shapeInfo + "\rContinue?")) {
//         break;
//       }
    }
    alert(str);
  }

  if (false) {
    var shapeFile = "/c/work/xes/test/presets/Square100x200.csh";
    var sf = new ShapesFile();
    sf.read(shapeFile);
    alert(sf);
    return;
  }

  if (false) {
    var shapeFolder = new Folder(app.path + "/Presets/Custom Shapes");
    var shapeFiles = shapeFolder.getFiles("*.csh");

    var str = '';
    for (var i = 0; i < shapeFiles.length; i++) {
      var file = shapeFiles[i];
      var shapes = new ShapesFile();
      shapes.read(file);
      str += shapes;
    }
    
    alert(str);
  }
};

// ShapesFile.test();

"ShapesFile.js";
// EOF
