//
// XMPData
//
// This class provides an interface to XMP information in a raw (string) form
// and a XMLDom object.
// The source of the XMP data can be one of the following:
//   - Document
//   - xmpMetadata
//   - String
//   - File (currently on XML and XMP files supported)
//
// If the source a String or File, it can be raw XML or inside and xpacket.
// $Id: XMPData.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
XMPData = function XMPData(obj, format, openFlags) {
  var self = this;

  self.format = format;
  self.openFlags = openFlags;
  self.xmlstr = '';
  self.dom = undefined;
  self.doc = undefined;
  self.pad = 0;
  self.encoding = '';
  self.packetLength = 0;
  self.file = undefined;
  self.length = 0;

  if (obj) {
    self.setSource(obj, format, openFlags);
  }
};


XMPData.XMP_BEGIN = "<x:xmpmeta"; 

// use a UTF-8 encoding
XMPData.XPACKET_BEGIN = '<?xpacket begin="﻿" id="W5M0MpCehiHzreSzNTczkc9d"?>';
XMPData.XPACKET_END = '<?xpacket end="w"?>';
XMPData.XPACKET_BEGIN_RE = /<\?xpacket begin="(.{0,3})" id="W5M0MpCehiHzreSzNTczkc9d"\?>/;
XMPData.XPACKET_END_RE = /<\?xpacket end="w"\?>/;

// This doesn't work
XMPData.XPACKET_RE =
  /(<\?xpacket begin="(.{0,3})" id="W5M0MpCehiHzreSzNTczkc9d"\?>)(.+)(\s*)(<\?xpacket end="w"\?>)/;


//
// XMPData.setSource
// Specify the source of XMP data to be 'obj'.
//
XMPData.prototype.setSource = function(obj, format, openFlags) {
  var self = this;

  if (obj == undefined) {
    Error.runtimeError(1220); // PSError.IllegalArgument
  }

  self.format = format;
  self.openFlags = openFlags;
  self.xmlstr = '';
  self.dom = undefined;
  self.pad = 0;
  self.encoding = '';
  self.packetLength;
  self.source = undefined;

  if (obj) {
    var str;

    // if it's a Document, the xmp data is right here
    if (obj.typename == "Document") {
      self.source = obj;
      dat = obj.xmpMetadata.rawData;
    }

    // if it's a xmpMetadata, the xmp data is right here
    if (obj.typename == "xmpMetadata") {
      self.source = obj;
      dat = obj.rawData;
    }

    // if it's a File, read it all in
    if (obj instanceof File) {
      self.source = obj;
      self.file = obj;
      obj.open("r") || throwFileError("Unable to open file", obj);
      obj.close();
      dat = Stdlib.readFromFile(obj, 'BINARY');
    }

    // if it's a string, assume it's either xmp or xml
    if (obj.constructor == String) {
      self.source = obj;
      dat = obj;
    }

    // make sure we actually found some raw xmp data
    if (!dat) {
      Error.runtimeError(1241); // PSError.UnsupportedType;
    }

    // if it's got an xpacket header
    var m = dat.match(XMPData.XPACKET_BEGIN_RE);
    if (m) {
      self.packetLength = dat.length;
      self.xpacketHeader = m[0]; 
      self.encoding = m[1];
      m = dat.match(XMPData.XPACKET_END_RE);
      if (!m) {
        throw "Unexpected xpacket trailer.";
      }
      self.xpacketTrailer = m[0];
      var str = dat.substring(self.xpacketHeader.length,
                              dat.length - self.xpacketHeader.length);
      self.xmlstr = str.trim();
      self.pad = str.length - self.xmlstr.length;

    } else {
      if (dat.startsWith('<?xpacket begin=')) {
        throw "Unexpected xpacket header.";
      }

      self.xmlstr = dat.trim();
    }
  }

  if (!self.xmlstr.startsWith(XMPData.XMP_BEGIN)) {
    throw "String is not in valid XMP/XML format.";
  }
  if (self.xmlstr) {
    self.dom = new XML(self.xmlstr);
  }
};

XMPData.prototype.toString = function() {
  return "[XMPData]";
  //return this.dom.toString().replace(/></g, ">\n<");
};

XMPData.prototype.toXML = function() {
  return this.dom.toString().replace(/></g, ">\n<");
};

XMPData.prototype.getDOM = function() {
  return this.dom;
};

// XXX fix this...
XMPData.prototype.toXPacket = function() {
  return (XMPData.XPACKET_BEGIN + '\n' +
          this.toXML() + '\n' +
          XMPData.XPACKET_END);
};


XMPData.prototype.setXMLString = function(str) {
  self.setSource(str);
};

"XMPData.jsx";
// EOF
