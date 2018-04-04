//
// metadata.js
//
// routines for manipulating IPTC, EXIF, XMP, and any other
// metadata
//
// Functions:
//
// History:
//  2005-01-20 v0.8 Name change
//  2004-09-27 v0.1 Creation date
//
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
// $Id: metadata.js,v 1.61 2012/08/11 17:27:26 anonymous Exp $
//

//================================= EXIF ===============================

EXIF = function EXIF(obj) {
  if (obj.typename == "Document") {
    this.exif = obj.info.exif;
  } else {
    this.exif = obj;
  }
  this.caseSensitive = false;
};
EXIF.prototype.get = function(tag) {
  var exif = this.exif;

  for (var i = 0; i < exif.length; i++) {
    var name = exif[i][0];
    if (name == tag) {
      return exif[i][1];
    }
  }

  if (!this.caseSensitive) {
    tag = tag.toLowerCase().replace(/\s/g, '');

    for (var i = 0; i < exif.length; i++) {
      var name = exif[i][0];
      name = name.toLowerCase().replace(/\s/g, '');
      if (name == tag) {
        return exif[i][1];
      }
    }
  }

  return '';
};
EXIF.prototype.toString = function(indent) {
  var exif = this.exif;

  var s = '';
  if (indent == undefined) { indent = ''; }
  for (var i = 0; i < exif.length; i++) {
    var name = exif[i][0];
    var val  = exif[i][1] || '';
    s += indent + name + ': ' + val + '\r\n';
  }

  return s;
};
EXIF.prototype.toXMLString = function() {
  return "<EXIF>\r\n<![CDATA" + "[\n" + this.toString() + "\r\n]" + "]>\r\n</EXIF>\r\n";
};


//================================== IPTC ===================================

IPTC = function IPTC(obj) {
  if (obj.typename == "Document") {
    this.info = obj.info;
  } else if (obj.typename == "DocumentInfo") {
    this.info = obj;
  }
};

IPTC._names = {
  creator: "author",
  creatorstitle: "authorPosition",
  description: "caption",
  descriptionwriter: "captionWriter",
  provinceorstate: "provinceState",
};
IPTC.prototype.get = function(tag) {
  var info = this.info;
  tag = tag.toLowerCase();

  var nm = IPTC._names[tag];

  if (nm) {
    tag = nm.toLowerCase();
  }

  for (var idx in info) {
    if (idx.toLowerCase() == tag) {
      return info[idx];
    }
  }
  return '';
};
IPTC.prototype.set = function(tag, value) {
  var info = this.info;

  for (var idx in info) {
    if (idx.toLowerCase() == tag) {
      info[idx] = value;
    }
  }
  return value;
};
IPTC.prototype.getCreationDate = function() {
  var info = this.info;

  var dstr = info.creationDate;
  var date;
  if (dstr && dstr.length != 0) {
    date = IPTC.DocumentInfoStringToDate(dstr);
  }
  return date;
};
IPTC.prototype.setCreationDate = function(date) {
  var info = this.info;

  var dstr = null;
  if (date) {
    if (typeof date != "object" || !(date instanceof Date)) {
      return null;
    }
    dstr = IPTC.DateToDocumentInfoString(date);
  } else {
    dstr = null;
  }
  return info.creationDate = dstr;
};
IPTC.prototype.getKeywords = function() {
  var info = this.info;

  return IPTC.get(info, "keywords");
};
IPTC.prototype.setKeywords = function(keywords) {
  var info = this.info;

  if (keywords == undefined) { keywords = []; }
  return info.keywords = keywords;
};
IPTC.prototype.addKeyword = function(keyword) {
  return this.addKeywords([keyword]);
};
IPTC.prototype.addKeywords = function(keywords) {
  var info = this.info;
  var keys = info.keywords;
  if (!keywords || keywords.length == 0) {
    return keys;
  }

  return info.keywords = Set.merge(keys, keywords);
};
IPTC.prototype.removeKeyword = function(keyword) {
  var info = this.info;

  if (!keyword) {
    return undefined;
  }
  return info.keywords = Set.remove(info.keywords, keyword);
};
IPTC.prototype.containsKeyword = function(keyword) {
  var info = this.info;

  if (!keyword) {
    return undefined;
  }
  return Set.contains(info.keywords, keyword);
};
IPTC.prototype.toString = function(exif) {
  var info = this.info;

  var str = '';
  if (exif == undefined) { exif = false; }
  for (var x in info) {
    if (x == "exif") {
      if (exif) {
        str += "exif:\r\n" + EXIF.toString(doc.info.exif, '\t');
      } else {
        str += "exif: [EXIF]\r\n";
      }
    } else if (x == "keywords" || x == "supplementalCategories") {
      str += x + ":\t\r\n";
      var list = info[x];
      for (var y in list){
        str += '\t' + list[y] + "\r\n";
      }
    } else if (x == "creationDate") {
      var dstr = IPTC.DocumentInfoStringToISODate(info[x]);
      str += x + ":\t" + dstr + "\r\n";
    } else {
      str += x + ":\t" + info[x] + "\r\n";
    }
  }
  return str;
};
IPTC.prototype.toXMLString = function(info) {
  return "<IPTC>\r\n<![CDATA" + "[\n" + IPTC.toString(info) + "\r\n]" + "]>\r\n</IPTC>\r\n";
};
IPTC.DateToDocumentInfoString = function(date) {
  if (!date) {
    date = new Date();
  } else if (typeof date != "object" || !(date instanceof Date)) {
    return undefined;
  }
  var str = '';
  function _zeroPad(val) { return (val < 10) ? '0' + val : val; }
  str = date.getFullYear() + '-' +
  _zeroPad(date.getMonth()+1,2) + '-' +
  _zeroPad(date.getDate(),2);

  return str;
};
// IPTC.DocumentInfoStringToDate("20060410");
IPTC.DocumentInfoStringToDate = function(str) {
  if (!str || str.length != 8) {
    return undefined;
  }
  return new Date(Number(str.substr(0, 4)),
                  Number(str.substr(4, 2))-1,
                  Number(str.substr(6,2)));
};
IPTC.DocumentInfoStringToISODate = function(str) {
  return str.substr(0, 4) + '-' + str.substr(4, 2) + '-' + str.substr(6,2);
};

//================================= XMP ==================================
// var str = doc.xmpMetadata.rawData;

// var rex = /<([^>]+)>([^<]+)<\/(?:\1)>/m;
// var m;

// while (m = rex.exec(str)) {
//   var tag = m[1];
//   var value = m[2];
//   alert(tag + ' : ' + value);

//   str = RegExp.rightContext;
// }
//  /<([^>]+)>([^<]+)<\/(?:\1)>/

XMPData = function(obj) {
  var self = this;

  if (obj.typename == "Document") {
    self.xmp = obj.xmpMetadata.rawData;

  } else if (obj.constructor == String) {
    self.xmp = obj;

  } else if (obj instanceof XMPMeta) {
    self.xmpMeta = obj;

  } else if (obj != undefined) {
    Error.runtimeError(9001, "XMPData constructor argument must be a " +
                       "Document or a String");
  }

  self.caseSensitive = false;
};

XMPData.SEPARATOR = ',';
XMPData.hasXMPTools = function() {
  if (!XMPData._hasXMPTools) {
    try {
      XMPTools;
      XMPData._hasXMPTools = XMPTools.isCompatible() && XMPTools.loadXMPScript();
    } catch (e) {
      XMPData._hasXMPTools = false;
    }
  }
  return XMPData._hasXMPTools;
}

XMPData._xmp_extract = function(str, startTag, stopTag) {
  var re = new RegExp(startTag, 'i');
  var start = str.match(re);

  if (!start) {
    return undefined;
  }

  var re = new RegExp(stopTag, 'i');
  var stop = str.match(re);
  if (!stop) {
    return undefined;
  }

  var startIndex = start.index + start[0].length;

  var val = str.substring(startIndex, stop.index).trim();

  // This takes care of matches against </rdf:Description> and others
  // which are not valid...
  if (val.match('^</rdf:[^>]+>')) {
    return undefined;
  }

  // Order/Unordered Arrays
  // return a comma delimited list
  //
  if (val.match('^<rdf:Seq>') || val.match('^<rdf:Bag>')) {
    var res = [];
    var rex = /<rdf:li>([^<]+)<\/rdf:li>/m;
    var m;
    while (m = rex.exec(val)) {
      res.push(m[1].trim());
      val = RegExp.rightContext;
    }

    var s = res.join(XMPData.SEPARATOR);
    return (res.length > 1) ? ('[' + s + ']') : s;
  }

  // Alternative Arrays
  // return the 'default' or first element
  //
  if (val.match('^<rdf:Alt>')) {
    var m = val.match(/<rdf:li.+default.+>([^<]+)<\/rdf:li>/);
    if (!m) {
      m = val.match(/<rdf:li.+>([^<]+)<\/rdf:li>/);
      if (!m) {
        return val;
      }
    }
    return m[1].trim();
  }

  // Structures
  // result looks like "{key1:"value1",key2:"value2"}
  // should probably use json for this
  //
  if (val.match('<stDim:')) {
    var res = [];
    var rex = /<stDim:([\w]+)>([^<]+)<\/stDim:\w+>/;
    var m;
    while (m = rex.exec(val)) {
      res.push(m[1] + ":\"" + m[2].trim() + '\"');
      val = RegExp.rightContext;
    }

    return '{' + res.join(XMPData.SEPARATOR) + '}';
  }

  return val;
};

XMPData.prototype.getFromXMPMeta = function(tag) {
  var self = this;
  var xmeta = self.xmpMeta;

  if (!xmeta) {
    xmeta = self.xmpMeta = new XMPMeta(self.xmp);
  }

  var val = XMPTools.getMetadataValue(xmeta, tag);
  return val;
};

// function _xmlFix(v) {
// };

// _xmlFix.run = function(v) {
//   if (v) {
//     var str = v;
//     var t = _xmlFix.table;
//     var result = '';
//     var rex = t._rex;
//     var m;

//     while (m = str.exec(str)) {
//     var pre = m[1];
//     var typ = m[2];
//     var post = m[3];
//     result += pre + cnvts[typ](t);
//     str = post;
//   }
//   result += str;
//   return result;
// };

// _xmlFix.table = {};
// _xmlFix.table._add = function(enc, hex) {
//   _xmlFix.table[enc] = hex;
// };
// _xmlFix.table._init = function() {
//   var t = _xmlFix.table;
//   t.add('quot', '\x22');
//   t.add('amp', '\x26');
//   t.add('lt', '\x3C');
//   t.add('gt', '\x3E');

//   var str = '';
//   for (var idx in t) {
//     if (!idx.startsWith('_')) {
//       str += "|" + idx;
//     }
//   }
//   str = "([^&])&(" + str.substring(1) + ");(.*)":

//   // fix this
//   t._rex = new RegExp(str);
// };
// _xmlFix.table._init();

XMPData.prototype.get = function(tag) {
  var self = this;

  if (XMPData.hasXMPTools()) {
    var val = self.getFromXMPMeta(tag);
    if (!val) {
      // try a reverse localization lookup to get the actual exif tag
    }
    return val;
  }

  //  $.level = 1; debugger;

  var hasNmSpc = tag.contains(':');

  if (hasNmSpc) {
    var val = XMPData._xmp_extract(self.xmp,
      '<' + tag + '>', '</' + tag + '>');

    if (val != undefined) {
      return val;
    }

  } else {
    // XXX Fix later
    // the startTag should probably look more like this:
    // '<[^(:|\\/)]+:' + tag + '>'
    var val = XMPData._xmp_extract(self.xmp,
      '<[^:]+:' + tag + '>',
      '</[^:]+:' + tag + '>');

    if (val != undefined) {
      return val;
    }
  }

  // handle embedded spaces
  if (tag.contains(' ')) {
    var t = tag.replace(' ', '');
    var val = self.get(t);
    if (val != undefined && val != '') {
      return val;
    }
  }

  // check for missing 'Value' suffix
  if (!tag.endsWith('Value')) {
    var t = tag + 'Value';
    var val = self.get(t);
    if (val != undefined && val != '') {
      return val;
    }
  }

  // The rest of this code handles metadata formatted in a non-canonical
  // format.

  // tag with a namespace
  var restr3 = "\\W" + tag + "=\"([^\"]*)\"";

  // tag without a namespace
  var restr4 = "\\W[\\w]+:" + tag + "=\"([^\"]*)\"";

  var re = new RegExp(restr3);
  var m = self.xmp.match(re);

  if (!m) {
    re = new RegExp(restr4);
    m = self.xmp.match(re);
  }

  if (!self.caseSensitive) {
    // now check the rex's again but case-insensitive
    if (!m) {
      re = new RegExp(restr3, 'i');
      m = self.xmp.match(re);
    }

    if (!m) {
      re = new RegExp(restr4, 'i');
      m = self.xmp.match(re);
    }
  }

  if (m) {
    return m[1];
  }

  // In CS4, Adobe will apparently use xmp* name spaces instead of xap*
  // namespaces. This code addresses that equivalence.

  var nm = undefined;
  var nmspc = undefined;
  var m = tag.split(':');

  if (m.length == 2) {
    nmspc = m[0];
    nm = m[1];
  }

  if (!nmspc || !nmspc.startsWith('xap')) {
    return '';
  }

  nmspc = nmspc.replace(/xap/, 'xmp');

  var rtag = nmspc + ':' + nm;

  // CS3 style
  var restr5 = '<' + rtag + '>(.+)</' + rtag + '>';

  // CS4 style
  var restr6 = "\\W" + rtag + "=\"([^\"]*)\"";

  var re = new RegExp(restr5);
  var m = self.xmp.match(re);

  if (!m) {
    re = new RegExp(restr6);
    m = self.xmp.match(re);
  }

  if (!self.caseSensitive) {
    // now check the rex's again but case-insensitive
    if (!m) {
      re = new RegExp(restr5, 'i');
      m = self.xmp.match(re);
    }

    if (!m) {
      re = new RegExp(restr6, 'i');
      m = self.xmp.match(re);
    }
  }

  if (m) {
    return m[1];
  }

  return '';
};


//================================= Metadata ==================================

Metadata = function(obj) {
  var self = this;

  if (obj != undefined) {
    if (obj.typename == "Document") {
      self.doc = obj;

    } else if (obj instanceof File) {
      self.file = obj;
      if (XMPData.hasXMPTools()) {
        self.xmpMeta = XMPTools.loadMetadata(self.file);
      }

    } else if (obj instanceof String || typeof(obj) == "string") {
      self.str = obj;
      if (XMPData.hasXMPTools()) {
        self.xmpMeta = new XMPMeta(self.str);
      }

    } else if (XMPData.hasXMPTools() && obj instanceof XMPMeta) {
      self.xmpMeta = obj;

    } else {
      var md = {};
      for (var idx in obj) {
        var v = obj[idx];
        if (typeof v != 'function') {
          md[idx] = v;
        }
      }
      self.obj = md;
      self.obj.get = function(tag) {
        return this.obj[tag] || '';
      };
    }
  }
  self.defaultDateTimeFormat = Metadata.defaultDateTimeFormat;
  self.defaultGPSFormat = Metadata.defaultGPSFormat;
};
Metadata.DEFAULT_DATE_FORMAT = "%Y-%m-%d";
Metadata.DEFAULT_GPS_FORMAT = "%d\u00B0 %d' %.2f\"";

Metadata.defaultDateTimeFormat = Metadata.DEFAULT_DATE_FORMAT;
Metadata.defaultGPSFormat = Metadata.DEFAULT_GPS_FORMAT;

Metadata.prototype.get = function(tags) {
  var self = this;

  var single = false;

  try {
    if (!tags) {
      return undefined;
    }
    if (tags.constructor == String) {
      tags = [tags];
      single = true;
    }
    if (!(tags instanceof Array)) {
      return undefined;
    }

    var res = {};
    var re = /\$\{([^:]+):(.+)\}/;
    var re2 = /([^:]+):(.+)/;

    if (self.obj) {
      for (var i = 0; i < tags.length; i++) {
        var tag = tags[i];
        var m = tag.match(re);
        if (m) {
          tag = m[2];
        }

        val = self.obj[tag];
        res[tag] = val || '';

        if (single) {
          single = val;
        }
      }

    } else if (self.str || self.xmpMeta) {
      var xmp;
      if (self.xmpMeta) {
        xmp = new XMPData(self.xmpMeta);
      } else {
        xmp = new XMPData(self.str);
      }

      for (var i = 0; i < tags.length; i++) {
        var tag = tags[i];
        var m = tag.match(re);
        if (!m) {
          m = tag.match(re2);
          if (!m) {
            Error.runtimeError(9001, "Bad tag value: " + tag);
          }
        }
        var type = m[1];
        var name = m[2];
        var val;

        if (type == 'EXIF') {
          val = xmp.get(name);

        } else if (type == 'IPTC') {
          val = xmp.get('Iptc4xmpCore:' + name);
          if (!val) {
            val = xmp.get(name);
          }
        } else if (type == 'XMP') {
          val = xmp.get(name);

        } else {
          Error.runtimeError(9001, "Bad tag type: " + type);
        }

        res[tag] = val || '';
        if (single) {
          single = val;
        }
      }

    } else if (self.doc && self.doc.typename == "Document") {
      var exif = new EXIF(self.doc);
      var iptc = new IPTC(self.doc);
      var xmp  = new XMPData(self.doc);

      for (var i = 0; i < tags.length; i++) {
        var tag = tags[i];
        var m = tag.match(re);

        if (!m) {
          m = tag.match(re2);
          if (!m) {
            Error.runtimeError(9001, "Bad tag value: " + tag);
          }
        }

        var type = m[1];
        var name = m[2];
        var val;

        if (type == 'EXIF') {
          val = exif.get(name);
          if (!val) {
            val = xmp.get(name);
          }
        } else if (type == 'IPTC') {
          val = iptc.get(name.toLowerCase());
          if (!val) {
            val = iptc.get(name);
          }
          if (!val) {
            val = xmp.get('Iptc4xmpCore:' + name);
          }
          if (!val) {
            val = xmp.get(name);
          }
        } else if (type == 'XMP') {
          val = xmp.get(name);
        } else {
          throw "Bad tag type " + type;
        }

        res[tag] = val || '';
        if (single) {
          single = val;
        }
      }

    } else {
      Error.runtimeError(9001, "Internal Error: Unable to determine " +
                         "metadata for images");
    }

  } catch (e) {
    Stdlib.logException(e);
    return undefined;
  }

  return (single ? single : res[0]);
};

Metadata.prototype.formatDate = function(date, fmt) {
  var self = this;
  var str = fmt || self.defaultDateTimeFormat;

  if (!str.contains('%')) {
    str = str.replace(/YYYY/g, '%Y');
    str = str.replace(/YY/g, '%y');
    str = str.replace(/MM/g, '%m');
    str = str.replace(/DD/g, '%d');
    str = str.replace(/H/g, '%H');
    str = str.replace(/I/g, '%I');
    str = str.replace(/M/g, '%M');
    str = str.replace(/S/g, '%S');
    str = str.replace(/P/g, '%p');
  }
  return date.strftime(str);
};

Metadata.prototype.strf = function(fmt) {
  var self = this;
  var ru = app.preferences.rulerUnits;
  app.preferences.rulerUnits = Units.PIXELS;
  
  try {
  var str = fmt;

  var doc = self.doc;
  var file;

  if (doc) {
    file = Stdlib.getDocumentName(doc);

  } else {
    file = self.file;
  }

  // just used as a boolean check
  var xmp = self.xmpMeta;
  
  var restrBase = "%|B|F|H|M|P|R|s|S|W|n|t| "; // d|e|f|p
  var restrC = "|C(?:\\{([^\\}]+)\\})?";
  var restrT = "|T(?:\\{([^\\}]+)\\})?";
  var restrN = "|N(?:\\{([^\\}]+)\\})?";
  var restrIE = "|(?:I|E)\\{([^\\}]+)\\}";
  var restrX = "|X\\{([^:]+)\:([^\\}]+)\\}";
  var restrX2 = "|X\\{([^\\}]+)\\}";

  var restrFile = "|(-)?(\\d+)?(\\.\\d+)?(d|e|f|p)";

  var restr = ("([^%]*)%(" + restrBase + restrC + restrT + restrN +
               restrIE + restrX + restrX2 + restrFile + ")(.*)");

  var re = new RegExp(restr);

  var dateFormat = self.defaultDateTimeFormat;

  var a = [];
  var b = [];

  //$.level = 1; debugger;

  var result = '';

  while (a = re.exec(str)) {
    var leftpart     = a[1];
    var pType        = a[2].charAt(0);

    var createTime   = a[3];
    var fileTime     = a[4];
    var currentTime  = a[5];
    var ieTag        = a[6];
    var xmpSpace     = a[7];
    var xmpTag       = a[8];
    var xTag         = a[9];
    var rightPart    = a[10];

    var fsig         = a[10];
    var flen         = a[11];
    var fign         = a[12];
    var ftyp         = a[13];

    var rightPart    = a[14];

    var subst = '';

    if (pType == '%') {
      subst = '%';

    } else if (ftyp) {
      subst = file.strf('%' + a[2]);

    } else {
      switch (pType) {
        case 'd':
          if (file) {
            subst = file.strf("%d");
          }
          break;
        case 'e':
          if (file) {
            subst =  file.strf("%e");
          }
          break;
        case 'p':
          if (file) {
            subst = decodeURI(file.parent.name);
          }
          break;
        case 'f':
          if (file) {
            subst = file.strf("%f");
          }
          break;
        case 'F':
          if (file) {
            subst = decodeURI(file.name);
          }
          break;
        case 's':
          if (file) {
            subst =  file.length;
          }
          break;
        case 'S':
          if (file) {
            var len = file.length;
            if (len > 1000000000) {
              subst = Math.round(len/1000000000) + 'G';
            } else if (len > 1000000) {
              subst = Math.round(len/1000000) + 'M';
            } else if (len > 1000) {
              subst = Math.round(len/1000) + 'K';
            } else {
              subst = len;
            }
          }
          break;
        case 'C':
          if (file) {
            subst = self.formatDate(file.created, createTime);
          }
          break;
        case 'T':
          if (file) {
            subst = self.formatDate(file.modified, fileTime);
          }
          break;
        case 'N':
          if (file) {
            subst = self.formatDate(new Date(), currentTime);
          }
          break;
        case 'W':
          if (doc) {
            subst = doc.width.value;
          } else if (xmp) {
            self.get('${XMP:ImageWidth}');
            if (!subst && self._width) {
              subst = self._width;
            }
          }
          break;
        case 'H':
          if (doc) {
            subst = doc.height.value;
          } else if (xmp) {
            subst = self.get('${XMP:ImageHeight}');
            if (!subst && self._height) {
              subst = self._height;
            }
          }
          break;
        case 'M':
          if (doc) {
            subst = Stdlib.colorModeString(doc.mode);

          } else if (xmp) {
            var mstr = self.get('${XMP:ColorMode}');
            if (mstr) {
              var cmode = toNumber(mstr);
              if (!isNaN(cmode)) {
                subst = Stdlib.colorModeString(cmode);
              }
            }
            if (!subst && self._mode) {
              subst = self._mode;
            }
          }
          break;
        case 'P':
          if (doc) {
            var lvl = $.level;
            try {
              $.level = 0;
              subst = doc.colorProfileName;
            } catch (e) {
              subst = '';
            }
            $.level = lvl;
          } else if (xmp) {
            subst = self.get('${XMP:ICCProfile}');
            if (!subst && self._profile) {
              subst = self._profile;
            }
          }
          break;
        case 'R':
          if (doc) {
            subst = doc.resolution;
          } else if (xmp) {
            subst = self.get('${XMP:XResolution}');
            if (!subst && self._resolution) {
              subst = self._resolution;
            }
          }
          break;
        case 'B':
          if (doc) {
            var bpc = doc.bitsPerChannel;
            if (bpc == BitsPerChannelType.ONE) {
              subst = "1";
            } else if (bpc == BitsPerChannelType.EIGHT) {
              subst = "8";
            } else if (bpc == BitsPerChannelType.SIXTEEN) {
              subst = "16";
            } else if (bpc == BitsPerChannelType.THIRTYTWO) {
              subst = "32";
            } else {
              Error.runtimeError(9001, "Bad bits per channel value");
            }
          } else if (xmp) {
            // BitDepth
            if (self._bitDepth) {
              subst = self._bitDepth;
            } else {
              var bpc = self.get("${XMP:BitsPerSample}");
              if (bpc) {
                var m = bpc.match(/d+/);
                if (m) {
                  subst = m[0];
                }
              } else  {
                subst = "8";  // just as a default
              }
            }
          }
          break;
        case 'I':
          if (doc || xmp) {
            var doFormat = true;
            if (ieTag.startsWith('-')) {
              doFormat = false;
              ieTag = ieTag.substring(1);
            }
            subst = self.get("${IPTC:" + ieTag + '}') || '';
            if (subst) {
              try {
                var itag = ieTag.toLowerCase();
                if (itag == "creationdate" && doFormat) {
                  var date = IPTC.DocumentInfoStringToDate(subst);
                  if (date) {
                    subst = self.formatDate(date, dateFormat);
                  }
                } else if (itag == "urgency") {
                  if (subst.toString().startsWith("Urgency.")) {
                    subst = Stdlib.urgencyString(subst);
                  }
                } else if (itag == "copyrighted") {
                  if (subst.toString().startsWith("CopyrightedType.")) {
                    subst = Stdlib.copyrightedString(subst);
                  }
                }
              } catch (e) {
              }
            }
          }
          break;
        case 'E':
          // md = new Metadata(doc); md.strf("%E{GPSLongitude}")
          if (doc || xmp) {
            var doFormat = true;
            if (ieTag.startsWith('-')) {
              doFormat = false;
              ieTag = ieTag.substring(1);
            }
            subst = self.get("${EXIF:" + ieTag + '}') || '';
            if (subst && doFormat) {
              if (ieTag.match(/date/i)) {
                var date = Stdlib.parseISODateString(subst);
                if (date) {
                  subst = self.formatDate(date, dateFormat);
                }
              } else if (ieTag.match(/gps/i)) {
                if (ieTag.match(/(longitude|latitude)/i)) {
                  subst = Stdlib.strfGPSstr(self.defaultGPSFormat, subst);
                }
              }
            }
          }
          break;
        case 'X':
          if (doc || xmp) {
            var doFormat = true;
            if (xmpTag && xmpTag.startsWith('-')) {
              doFormat = false;
              xmpTag = xmpTag.substring(1);
            }
            if (xTag && xTag.startsWith('-')) {
              doFormat = false;
              xTag = xTag.substring(1);
            }
            if (!xTag) {
              xTag = xmpSpace + ':' + xmpTag;
            }
            subst = self.get("${XMP:" + xTag + '}') || '';
            if (subst) {
              if (xTag.match(/date/i) && doFormat) {
                var date = Stdlib.parseISODateString(subst);
                if (date) {
                  subst = self.formatDate(date, dateFormat);
                }
              }
            }
          }
          break;
        case 'n':
          subst = '\n';
          break;
        case 't':
          subst = '\t';
          break;
        case ' ':
          subst = ' ';
          break;

        default:
          break;
      }
    }

    result += leftpart + subst;
    str = rightPart;
  }

  result += str;

  } catch (e) {
    Stdlib.logException(e);

  } finally {
    app.preferences = ru;
  }
  return result;
};

Metadata.strf = function(fmt, obj) {
  return new Metadata(obj).strf(fmt);
};

Metadata.test = function() {
  listProps = function(obj) {
    var s = '';
    for (var x in obj) {
      s += x + ":\t";
      try {
        var o = obj[x];
        s += (typeof o == "function") ? "[function]" : o;
      } catch (e) {
      }
      s += "\r\n";
    }
    return s;
  };

  var doc = app.activeDocument;
//   alert(new EXIF(doc).toString());
//   return;

  var md = new Metadata(doc);
  var tags = ["${EXIF:Exposure Time}",
              "${EXIF:GPS Latitude}",
              "${EXIF:ISO Speed Ratings}",
              "${IPTC:Author}"];
  var res = md.get(tags);

  alert(listProps(res));
};

//Metadata.test();

Metadata.mdTest = function() {
  var doc = app.activeDocument;
  var md = new Metadata(doc);

  var str1 = "%d %p %f %F %e %s";
  alert(md.strf(str1));

  var str2 = "%T{%y-%m-%d} %N{%D} %W %H %R %B";
  alert(md.strf(str2));

  var str3 = "%X{xap:CreateDate} %I{Author} %X{dc:format}";
  alert(md.strf(str3));
};

//md = new Metadata(doc.xmpMetadata.rawData)
//Metadata.mdTest();


"metadata.js";
// EOF
