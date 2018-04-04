//
// Stream.js
// This file contains code necessary for reading and writing binary data with
// reasonably good performance. There is a lot that can be done to improve this
// but it works well enough for current purposes
//
// $Id: Stream.js,v 1.32 2014/12/19 01:18:09 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
Stream = function(str) {
  var self = this;
  self.str = (str ? str : []); // the actual bytes as String or Array of char
  self.ptr = 0;                // the current index into the stream
  self.byteOrder = Stream.DEFAULT_BYTEORDER;   // or Stream.LITTLE_ENDIAN
};
Stream.prototype.typename = "Stream";

Stream.BIG_ENDIAN = "BE";
Stream.LITTLE_ENDIAN = "LE";
Stream.DEFAULT_BYTEORDER = Stream.BIG_ENDIAN;

Stream.TWO_32= Math.pow(2, 32);
Stream.LARGE_LONG_MASK = 0x001FFFFFFFFFFFFF;  // Math.pow(2, 53) - 1;

Stream.RcsId = "$Revision: 1.32 $";

Stream.EOF = -1;

//
// some code for reading and writing files
//
Stream.writeToFile = function(fptr, str) {
  var file = Stream.convertFptr(fptr);
  file.open("w") || Error.runtimeError(9002, "Unable to open output file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';
  file.write(str);
  file.close();
};
Stream.readFromFile = function(fptr) {
  var file = Stream.convertFptr(fptr);
  file.open("r") || Error.runtimeError(9002, "Unable to open input file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';
  var str = '';
  str = file.read(file.length);
  file.close();
  return str;
};
Stream.readStream = function(fptr) {
  var str = new Stream();
  str.str = Stream.readFromFile(fptr);
  return str;
};
Stream.convertFptr = function(fptr) {
  var f;
  if (fptr.constructor == String) {
    f = File(fptr);
  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;
  } else {
    throw "Bad file \"" + fptr + "\" specified.";
  }
  return f;
};

//
// Convert the Stream to a String. Probably not the best choice of names here
//
Stream.prototype.toStream = function() {
  var s;
  if (this.str.constructor == String) {
    s = this.str.slice(0); 
  } else {
    s = this.str.join("");
  }
  return s;
};

Stream.prototype.appendToFile = function(fptr) {
  var self = this;

  fptr.write.apply(fptr, self.str);

  return;

//   this is surprising slow for large values of len
//   var len = self.str.length;
//   for (var i = 0; i < len; i++) {
//     fptr.write(self.str[i]);
//   }
};


Stream.prototype.writeToFile = function(fptr) {
  var self = this;
  var file = Stream.convertFptr(fptr);
  file.open("w") || Error.runtimeError(9002, "Unable to open output file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';
  // var str = self.toStream();
  // file.write(str);

  self.appendToFile(fptr);

  file.close();
};

Stream.prototype.seek = function(ptr, mode) {
  var self = this;
  var index;

  if (mode == undefined) {
    mode = 0;
  }

  if (mode == 0) {
    index = ptr;
  } else if (mode == 1) {
    index = self.ptr + ptr;
  } else if (mode == 2) {
    index = self.str.length - ptr;
  } else {
    throw "Bad seek mode.";
  }

  if (index < 0 || index > this.str.length) {
    throw "Attempt to seek in Stream out of range.";
  }
  self.ptr = index;
};
Stream.prototype.tell = function() {
  return this.ptr;
};
Stream.prototype.eof = function() {
  return this.ptr == this.str.length;
};
Stream.prototype.length = function() {
  return this.str.length;
};

//
// Write parts into the Stream
//
Stream.prototype.writeByte = function(b) {
  var self = this;
  self.str[self.ptr++] = String.fromCharCode(b);
  return self;
};
Stream.prototype.writeChar = function(c) {
  var self = this;
  self.str[self.ptr++] = c.charCodeAt(0);
  return self;
};
Stream.prototype.writeUnicodeChar = function(c) {
  var self = this;
  self.writeInt16(c.charCodeAt(0));
  return self;
};

Stream.prototype.writeInt16 = function(w) {
  var self = this;
  self.writeByte((w >> 8) & 0xFF);
  self.writeByte(w & 0xFF);
  return self;
};
Stream.prototype.writeShort = Stream.prototype.writeInt16;

Stream.prototype.writeWord = function(w) {
  var self = this;
  self.writeByte((w >> 24) & 0xFF);
  self.writeByte((w >> 16) & 0xFF);
  self.writeByte((w >> 8) & 0xFF);
  self.writeByte(w & 0xFF);
  return self;
};
Stream.prototype.writeLongWord = function(dw) {
  var self = this;
  var desc = new ActionDescriptor();
  desc.putLargeInteger(app.charIDToTypeID('Temp'), dw);
  var str = new Stream(desc.toStream());
  str.seek(8, 2);
  for (var i = 0; i < 8; i++) {
    self.writeByte(str.readByte());
  }
  return self;
};

Stream.prototype.writeDouble = function(d) {
  var self = this;
  var str = IEEE754.doubleToBin(d);
  self.writeRaw(str);
};
Stream.prototype.writeRaw = function(s) {
  var self = this;

  if (s.constructor == String) {
    for (var i = 0; i < s.length; i++) {
      //self.writeByte(s.charCodeAt(0));
      self.str[self.ptr++] = String.fromCharCode(s.charCodeAt(i));
    }
  } else {
    for (var i = 0; i < s.length; i++) {
      self.str[self.ptr++] = s[i];
    }
  }
};
Stream.prototype.writeString = function(s) {
  var self = this;
  for (var i = 0; i < s.length; i++) {
    //self.writeChar(s[i]);
    self.str[self.ptr++] = s[i];
  }
};
Stream.prototype.writeAscii = function(s) {
  var self = this;
  self.writeWord(s.length);
  for (var i = 0; i < s.length; i++) {
    //self.writeChar(s[i]);
    self.str[self.ptr++] = s[i];
  }
};
Stream.prototype.writeUnicode = function(s) {
  var self = this;
  self.writeWord(s.length + 1);
  for (var i = 0; i < s.length; i++) {
    //self.writeUnicodeChar(s[i]);
    self.writeInt16(s.charCodeAt(i));
  }
  self.writeInt16(0);  // null pad
};
Stream.prototype.writeUnicodeString = function(s) {
  var self = this;
  for (var i = 0; i < s.length; i++) {
    self.writeInt16(s.charCodeAt(i));
  }
  self.writeInt16(0);  // null pad
};
Stream.prototype.writeBoolean = function(b) {
  var self = this;
  self.writeByte(b ? 1 : 0);
};

//
// Read parts from the Stream
//
Stream.prototype.readByte = function() {
  var self = this;
  if (self.ptr >= self.str.length) {
    return Stream.EOF;
  }
  var ch = self.str[self.ptr++];
  var c = ch.charCodeAt(0);

  if (isNaN(c) && ch == 0) {
    c = 0;
  }

  return c;
};
Stream.prototype.readSignedByte = function() {
  b = this.readByte();
  if (b > 0x7F) {
    b = 0xFFFFFF00^b;
  };
  return b;
};
Stream.prototype.readByteChar = function() {
  var b = this.readByte();
  if (b != Stream.EOF) {
    b = String.fromCharCode(b);
  }
  return b;
};
Stream.prototype.readChar = function() {
  var self = this;
  if (self.ptr >= self.str.length) {
    return Stream.EOF;
  }
  var c = self.str[self.ptr++];
  return c;
};
Stream.prototype.readUnicodeChar = function() {
  var self = this;
  var i = self.readInt16();
  return String.fromCharCode(i);
};

Stream.prototype.readInt16 = function() {
  var self = this;
  var hi = self.readByte();
  var lo = self.readByte();
  if (self.byteOrder == Stream.BIG_ENDIAN) {
    return (hi << 8) + lo;
  } else {
    return (lo << 8) + hi;
  }
};
Stream.prototype.readShort = Stream.prototype.readInt16;

Stream.prototype.readSignedInt16 = function() {
  var i = this.readInt16();
  if(i > 0x7FFF){
    i = 0xFFFF0000^i;
  };
  return i;
};
Stream.prototype.readSignedShort = Stream.prototype.readSignedInt16;

Stream.prototype.readWord = function() {
  var self = this;
  var hi = self.readInt16();
  var lo = self.readInt16();
  var w;
  if (self.byteOrder == Stream.BIG_ENDIAN) {
    w = (hi << 16) + lo;
  } else {
    w = (lo << 16) + hi;
  }
  if (w < 0) {
    w = 0xFFFFFFFF + w + 1;
  }
  return w;
};

Stream.prototype.readSignedWord = function() {
  var w = this.readWord();
  if(w > 0x7FFFFFFF){
    w = 0xFFFFFFFF00000000^w;
  }
  return w;
};

Stream.prototype.readLongWord = function() {
  var self = this;
  var dw = self.readSignedLongWord();

  if (dw < 0) {
    self.ptr -= 8;
    var s = "0x";
    for (var i = 0; i < 8; i++) {
      var b = self.readByte();
      s += b.toString(16);
    }

    dw = parseInt(s);
  }

  return dw;
};

Stream.prototype.readSignedLongWord = function() {
  var self = this;
  var desc = new ActionDescriptor();
  desc.putLargeInteger(app.charIDToTypeID('Temp'), 0);
  var str = new Stream(desc.toStream().split(''));
  str.seek(8, 2);

  for (var i = 0; i < 8; i++) {
    var b = self.readByte();
    str.writeByte(b);
  }

  str.seek(0, 0);
  desc.fromStream(str.toStream());
  dw = desc.getLargeInteger(app.charIDToTypeID('Temp'));

  return dw;
};


Stream.prototype.readRaw = function(len) {
  var self = this;
  var str = self.str;

  var ar = str.slice(self.ptr, self.ptr+len);
  self.ptr += len;

  return ar;

// This is the original paranoid slower version
//   var ar = [];
//   for (var i = 0; i < len; i++) {
//     ar[i] = String.fromCharCode(self.readByte());
//   }
//   return ar.join("");
};

Stream.prototype.readDouble = function() {
  var self = this;
  var bin = self.readRaw(8);
  var v = IEEE754.binToDouble(bin);
  return v;
};

Stream.prototype.readFloat = function() {
  var self = this;
  var bin = self.readRaw(4);
  var ieee32 = new IEEE754(32);

  var v = IEEE754.binToDouble(bin);
  return v;
};

Stream.prototype.readString = function(len) {
  var self = this;
  var s = '';
  for (var i = 0; i < len; i++) {
    s += self.readChar();
  }
  return s;
};
Stream.prototype.readAscii = function() {
  var self = this;
  var len = self.readWord();
  return self.readString(len);
};
Stream.prototype.readUnicode = function(readPad) {
  var self = this;

  var len = self.readWord();
  if (readPad != false) {
    len--;
  }
  var s = '';
  for (var i = 0; i < len; i++) {
    //s += self.readUnicodeChar();
    var uc = self.readInt16();
    if (uc != 0) {
      s += String.fromCharCode(uc);
    }
  }
  if (readPad != false) {
    self.readInt16(); // null pad
  }
  return s;
};
Stream.prototype.readUnicodeZ = function() {
  return this.readUnicode(true);
};
Stream.prototype.readBoolean = function(b) {
  var self = this;
  return self.readByte() != 0;
};

Stream.prototype.toHex = function(start, len) {
  var self = this;
  if (start == undefined) {
    start = self.ptr;
    len = 16;
  }
  if (len == undefined) {
    len = start;
    start = self.ptr;
  }
  var s = self.str.slice(start, start+len);
  if (self.str instanceof Array) {
    s = s.join("");
  }
  return Stream.binToHex(s, true);
};
Stream.binToHex = function(s, whitespace) {
  function hexDigit(d) {
    if (d < 10) return d.toString();
    d -= 10;
    return String.fromCharCode('A'.charCodeAt(0) + d);
  }
  var str = '';

  for (var i = 0; i < s.length; i++) {
    if (i) {
      if (whitespace == true) {
        if (!(i & 0xf)) {
          str += '\n';
        } else if (!(i & 3)) {
          str += ' ';
        }
      }
    }
    var ch = s[i].charCodeAt(0);
    str += hexDigit(ch >> 4) + hexDigit(ch & 0xF);
  }
  return str;
};


Stream.prototype.dump = function(len) {
  return Stream.binToHex(this.str.slice(this.ptr, this.ptr + (len || 32)),
                         true);
};
Stream.prototype.rdump = function(start, len) {
  return Stream.binToHex(this.str.slice(start, start + (len || 32)),
                         true);
};
Stream.prototype.dumpAscii = function(len) {
  return this.str.slice(this.ptr, this.ptr + (len || 32)).replace(/\W/g, '.');
};

"Stream.js";
// EOF
