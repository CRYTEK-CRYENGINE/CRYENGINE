//
// XMPMeta.jsx
//
// $Id: XMPMeta.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPMeta = function XMPMeta(arg) {
  // arg is one of packet/String or buffer/Array of Number, or undefined
  var self = this;

  self.dom = undefined;

  if (arg) {
    if (arg instanceof XML) {
      self.dom = arg;
    }

    if (arg instanceof Array) {
      if (arg[0].constructor == Number) {
        for (var i = 0; i < arg.length; i++) {
          arg[i] = String.fromCharCode(arg[i]);
        }
        arg = arg.join('');
      } else {
        throw "Expected Array of Numbers";
      }
    }

    if (arg.constructor == String) {
      var m = arg.match(XMPData.XPACKET_BEGIN_RE);
      if (m) {
        var packetLength = arg.length;
        var xpacketHeader = m[0]; 
        var encoding = m[1];
        m = arg.match(XMPData.XPACKET_END_RE);
        if (!m) {
          throw "Unexpected xpacket trailer.";
        }
        var xpacketTrailer = m[0];
        var str = arg.substring(xpacketHeader.length,
                                arg.length - xpacketHeader.length);
        self.xmlstr = str.trim();
        self.dom = new XML(xmlstr);

      } else {
        if (arg.startsWith('<?xpacket begin=')) {
          throw "Unexpected xpacket header.";
        }
        
        self.xmlstr = arg.trim();
        self.dom = new XML(self.xmlstr);
      }
    }
  }
};
XMPMeta.version = "$Id: XMPMeta.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $";

XMPMeta.deleteAlias = function(aliasNS, aliasProp) {
  throw "Not yet implemented.";
  return undefined;
};
XMPMeta.deleteNamespace = function(namespaceURI) {
  throw "Not yet implemented.";
  return undefined;
};
XMPMeta.dumpAliases = function() {
  var str = "Dumping alias name to actual path map\n";

  for (var i = 0; i < XMPMeta._aliases.length; i++) {
    var al = XMPMeta._aliases[i];
    str += al.alias + "\t\t=> " + al.actual;
    if (al.form != XMPConst.ALIAS_TO_SIMPLE_PROP) {
      str += " (" + "0x%X".sprintf(al.form) + ")";
    }
    str += '\n';
  }

  return str;
};
XMPMeta.dumpNamespaces = function() {
  var str = "Dumping namespace prefix to URI map\n";

  for (var i = 0; i < XMPMeta._namespaces.length; i++) {
    var ns = XMPMeta._namespaces[i];
    str += ns.prefix + "\t\t=> " + ns.uri + '\n';
  }

  return str;
};
XMPMeta.getNamespacePrefix = function(namespaceURI) {
  var ns = Stdlib.getByProperty(XMPMeta._namespaces, "uri", namespaceURI);
  if (!ns) {
    throw "!Unregistered namespaceURI: " + namespaceURI;
  }
  return ns.prefix;
};
XMPMeta.getNamespaceURI = function(namespacePrefix) {
  namespacePrefix = namespacePrefix.replace(/:$/, '');

  var ns = Stdlib.getByProperty(XMPMeta._namespaces, "prefix",
                                namespacePrefix);
  if (!ns) {
    throw "!Unregistered namespacePrefix: " + namespacePrefix;
  }
  return ns.uri;
};
XMPMeta.registerAlias = function(aliasNS, aliasProp,
                                 actualNS, actualProp,
                                 arrayForm) {
  if (!aliasNS || !aliasProp || !actualNS || !actualProp) {
    return undefined;
  }

  if (!XMPMeta._aliases) {
    XMPMeta._aliases = [];
  }

  var alias = XMPMeta.getNamespacePrefix(aliasNS) + ':' + aliasProp;
  var actual = XMPMeta.getNamespacePrefix(actualNS) + ':' + actualProp;

  if (arrayForm == undefined) {
    arrayForm = XMPConst.ALIAS_TO_SIMPLE_PROP;
  }

  if (Stdlib.getByProperty(XMPMeta._aliases, "alias", alias)) {
    throw "Alias " + alias + " already defined.";
  }
  
  XMPMeta._aliases.push({alias: alias, actual: actual, form: arrayForm});
                            
  return undefined;
};
XMPMeta.registerNamespace = function(namespaceURI, suggestedPrefix) {
  if (!namespaceURI || !suggestedPrefix) {
    throw "Empty namespace URI or prefix";
  }
  if (!XMPMeta._namespaces) {
    XMPMeta._namespaces = [];
  }
  namespaceURI = namespaceURI.toString();
  suggestedPrefix = suggestedPrefix.replace(/:$/, '');

  var ns = Stdlib.getByProperty(XMPMeta._namespaces, "prefix",
                                suggestedPrefix);
  if (ns) {
    if (ns.uri != namespaceURI) {
      // if this prefix was already used for a different URI,
      // generate a new prefix by appending a 2 digit sequence number
      var cnt = 1;
      var prefix = "%s%02".sprintf(suggestedPrefix, cnt);

      while (Stdlib.getByProperty(XMPMeta._namespaces, "prefix", prefix)) {
        cnt++;
        prefix = "%s%02".sprintf(suggestedPrefix, cnt);
        if (cnt > 20) {
          throw "Exceeded duplicate prefix threshold";
        }
      }
      XMPMeta._namespaces.push({uri: namespaceURI, prefix: prefix});
      suggestedPrefix = prefix;
    }

  } else {
    XMPMeta._namespaces.push({uri: namespaceURI, prefix: suggestedPrefix});
  }

  return suggestedPrefix;
};
XMPMeta.resolveAlias = function(aliasNS, aliasProp) {
  aliasNS = aliasNS.toString();
  var alias;
  if (aliasProp) {
    alias = XMPMeta.getNamespacePrefix(aliasNS) + ':' + aliasProp;
  } else {
    alias = aliasNS;
  }

  var al = Stdlib.getByProperty(XMPMeta._aliases, "alias", alias);

  if (!al) {
    return undefined;
  }

  var ns = al.actual.split(':');
  if (ns.length < 2) {
    throw "Bad actual format: " + actual;
  }
  var actualPrefix = ns.shift();
  var actualNS = XMPMeta.getNamespaceURI(actualPrefix);

  var ai = new XMPAliasInfo();
  ai.name = ns.join(':');
  ai.namespace = actualNS;
  ai.arrayForm = al.form;

  return ai;
};

XMPMeta.prototype.toString = function() {
  var self = this;

  if (self.xmlstr) {
    return "[XMPMeta " +
      self.xmlstr.substr(0, Math.min(32, self.xmlstr.length)) + "...]";
  } else {
    return "[XMPMeta]";
  }
};

XMPMeta.prototype.appendArrayItem = function(schemaNS, arrayName, itemValue,
                                             itemOptions, arrayOptions) {
  return undefined;
};
XMPMeta.prototype.countArrayItems = function(schemaNS, arrayName) {
  return 0;
};
XMPMeta.prototype.deleteArrayItem = function(schemaNS, arrayName,
                                             itemIndex) {
  return undefined;
};
XMPMeta.prototype.deleteProperty = function(schemaNS, propName) {
  return undefined;
};
XMPMeta.prototype.deleteStructField = function(schemaNS, structName,
                                               fieldNS, fieldName) {
  return undefined;
};
XMPMeta.prototype.deleteQualifier = function(schemaNS, structName, qualNS,
                                             qualName) {
  return undefined;
};

XMPMeta.prototype.doesPropertyExist = function(schemaNS, propName) {
  var self = this;
  var alias = XMPMeta.resolveAlias(schemaNS, propName);
  debugger;
  if (alias) {
    schemaNS = alias.namespace;
    propName = alias.name;
  }

  var prop = self.getProperty(schemaNS, propName);
  return prop != undefined;
};
XMPMeta.prototype.doesArrayItemExist = function(schemaNS, arrayName,
                                                itemIndex) {
  var self = this;
  var itemPath = self._composeArrayItemPath(schemaNS, arrayName, itemIndex);
  return self.doesPropertyExist(schemaNS, itemPath);
};
XMPMeta.prototype.doesStructFieldExist = function(schemaNS, structName,
                                                  fieldNS, fieldName) {
  var self = this;
  var fieldPath = self._composeStructFieldPath(schemaNS, structName,
                                               fieldNS, fieldName);
  return self.doesPropertyExist(schemaNS, fieldPath);
};
XMPMeta.prototype.doesQualifierExist = function(schemaNS, structName,
                                                qualNS, qualName) {
  return false;
};

XMPMeta.prototype.dumpObject = function() {
  return '';
};
XMPMeta.prototype._composeArrayItemPath = function(schemaNS,
                                                   arrayName, itemIndex) {
  var self = this;
  var index;
  
  if (itemIndex == XMPConst.ARRAY_LAST_ITEM) {
    index = "[last()]";

  } else {
    var idx = Number(itemIndex);
    if (isNaN(idx)) {
      throw "Invalid array index";
    }
    index = '[' + itemIndex + ']';
  }
  return arrayName + index;
};
XMPMeta.prototype._composeStructFieldPath = function(schemaNS, structName,
                                                     fieldNS, fieldName) {
  var self = this;
  if (fieldNS) {
    var fieldPath = XMPMeta.getNamespacePrefix(fieldNS) + ':' + fieldName;
  } else {
    var fieldPath = fieldName;
  }
  return structName + '/' + fieldPath;
};


XMPMeta.prototype.getProperty = function(schemaNS, propName, valueType) {
  var self = this;

  var xp = ("/rdf:RDF/rdf:Description/" +
            XMPMeta.getNamespacePrefix(schemaNS) + ':' + propName);

  // this is hardwired for simple top-level properties
  var res = self.dom.xpath(xp);
  if (!res) {
    return undefined;
  }

  var prop = new XMPProperty();
  prop.namespace = schemaNS;
  prop.locale = 'en';
  prop.path = propName;
  prop.value = res;

  if (valueType != undefined) {
    res = res.toString();

    if (valueType == XMPConst.INTEGER || valueType == XMPConst.NUMBER) {
      prop.value = Number(res);

    } else if (valueType == XMPConst.BOOLEAN) {
      prop.value = (res.toLowerCase() == "true");

    } else if (valueType == XMPConst.XMPDATE) {
      var date = Stdlib.parseISODateString(res);
      prop.value = new XMPDateTime(date);

    } else {
      prop.value = res;
    }
  }
  return prop;
};
XMPMeta.prototype.getArrayItem = function(schemaNS, arrayName, itemIndex) {
  var self = this;
  var itemPath = self._composeArrayItemPath(schemaNS, arrayName, itemIndex);

  return self.getProperty(schemaNS, itemPath);
};
XMPMeta.prototype.getLocalizedText = function(schemaNS, altTextName,
                                              genericLang, specificLang) {
  return undefined; // String
};

XMPMeta.prototype.getStructField = function(schemaNS, structName,
                                            fieldNS, fieldName) {
  var self = this;
  var fieldPath = self._composeStructFieldPath(schemaNS, structName,
                                               fieldNS, fieldName);
  return self.getProperty(schemaNS, fieldPath);
};
XMPMeta.prototype.getQualifierField = function(schemaNS, structName,
                                               qualNS, qualName) {
  return undefined; // XMPProperty
};
XMPMeta.prototype.insertArrayItem = function(schemaNS, arrayName,
                                             itemIndex, itemValue,
                                             itemOptions) {
  return undefined;
};
XMPMeta.prototype.iterator = function(options, schemaNS, propName) {
  return new XMPIterator();
};
XMPMeta.prototype.serialize = function(options, padding, indent,
                                       newline, baseIndent) {
  var self = this;

  if (!self.dom) {
    return '';
  }
  return self.dom.toString();
};
XMPMeta.prototype.serializeToArray = function(options, padding, indent,
                                              newline, baseIndent) {
  return []; // Array of Numbers
};
XMPMeta.prototype.setArrayItem = function(schemaNS, arrayName, itemIndex,
                                          itemValue, itemOptions) {
  return undefined;
};
XMPMeta.prototype.setLocalizedText = function(schemaNS, altTextName,
                                              genericLang, specificLang,
                                              itemValue, setOptions) {
  return undefined;
};
XMPMeta.prototype.setStructField = function(schemaNS, structName, fieldNS,
                                            fieldName, fieldValue, options) {
  return undefined;
};
XMPMeta.prototype.setQualifier = function(schemaNS, propName,
                                          qualNS, qualName,
                                          qualValue, options) {
  return undefined;
};
XMPMeta.prototype.setProperty = function(schemaNS, propName, propValue,
                                         setOptions, valueType) {
  return undefined;
};

XMPMeta._init = function() {
  XMPMeta.registerNamespace("http://ns.adobe.com/DICOM/", 'DICOM');
  XMPMeta.registerNamespace("http://ns.adobe.com/fileinfolibpreferences/1.0/",
                            'FileInfoLibPrefs');
  XMPMeta.registerNamespace(XMPConst.NS_IPTC_CORE, 'Iptc4xmpCore');
  XMPMeta.registerNamespace(XMPConst.NS_PS_ALBUM, 'album');
  XMPMeta.registerNamespace(XMPConst.NS_ASF, 'asf');
  XMPMeta.registerNamespace(XMPConst.NS_EXIF_AUX, 'aux');
  XMPMeta.registerNamespace(XMPConst.NS_ADOBE_STOCK_PHOTO, 'bmsp');
  XMPMeta.registerNamespace("http://ns.adobe.com/StockPhoto/Resolutions/1.0/",
                            'bmsr');
  XMPMeta.registerNamespace(XMPConst.NS_CAMERA_RAW, 'crs');
  XMPMeta.registerNamespace(XMPConst.NS_DC, 'dc');
  XMPMeta.registerNamespace(XMPConst.NS_EXIF, 'exif');
  XMPMeta.registerNamespace("http://ns.adobe.com/iX/1.0/", 'iX');
  XMPMeta.registerNamespace(XMPConst.NS_JP2K, 'jp2k');
  XMPMeta.registerNamespace(XMPConst.NS_JPEG, 'jpeg');
  XMPMeta.registerNamespace("http://ns.adobe.com/lightroom/1.0/", 'lr');
  XMPMeta.registerNamespace(XMPConst.NS_PDF, 'pdf');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_EXTENSION, 'pdfaExtension');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_FIELD, 'pdfaField');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_PROPERTY, 'pdfaProperty');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_SCHEMA, 'pdfaSchema');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_TYPE, 'pdfaType');
  XMPMeta.registerNamespace(XMPConst.TYPE_PDFA_ID, 'pdfaid');
  XMPMeta.registerNamespace(XMPConst.NS_PDF, 'pdfx');
  XMPMeta.registerNamespace("http://www.npes.org/pdfx/ns/id/", 'pdfxid');
  XMPMeta.registerNamespace(XMPConst.NS_PHOTOSHOP, 'photoshop');
  XMPMeta.registerNamespace(XMPConst.NS_PNG, 'png');
  XMPMeta.registerNamespace("http://ns.adobe.com/plain-xmp/1.0/", 'pxmp');
  XMPMeta.registerNamespace(XMPConst.NS_RDF, 'rdf');
  XMPMeta.registerNamespace(XMPConst.TYPE_DIMENSIONS, 'stDim');
  XMPMeta.registerNamespace(XMPConst.TYPE_RESOURCE_EVENT, 'stEvt');
  XMPMeta.registerNamespace(XMPConst.TYPE_FONT, 'stFnt');
  XMPMeta.registerNamespace(XMPConst.TYPE_ST_JOB, 'stJob');
  XMPMeta.registerNamespace(XMPConst.TYPE_MANIFEST_ITEM, 'stMfs');
  XMPMeta.registerNamespace(XMPConst.TYPE_RESOURCE_REF, 'stRef');
  XMPMeta.registerNamespace(XMPConst.TYPE_ST_VERSION, 'stVer');
  XMPMeta.registerNamespace(XMPConst.NS_SWF, 'swf');
  XMPMeta.registerNamespace(XMPConst.NS_TIFF, 'tiff');
  XMPMeta.registerNamespace("http://ns.adobe.com/xmp/wav/1.0/", 'wav');
  XMPMeta.registerNamespace("adobe:ns:meta/", 'x');
  XMPMeta.registerNamespace(XMPConst.NS_XMP, 'xap');
  XMPMeta.registerNamespace(XMPConst.NS_XMP_BJ, 'xapBJ');
  XMPMeta.registerNamespace(XMPConst.TYPE_GRAPHICS, 'xapG');
  XMPMeta.registerNamespace(XMPConst.TYPE_IMAGE, 'xapGImg');
  XMPMeta.registerNamespace("http://ns.adobe.com/xap/1.0/g", 'xapG_1_');
  XMPMeta.registerNamespace(XMPConst.NS_XMP_MM, 'xapMM');
  XMPMeta.registerNamespace(XMPConst.NS_XMP_RIGHTS, 'xapRights');
  XMPMeta.registerNamespace(XMPConst.TYPE_TEXT, 'xapT');
  XMPMeta.registerNamespace(XMPConst.TYPE_PAGEDFILE, 'xapTPg');
  XMPMeta.registerNamespace(XMPConst.NS_XML, 'xml');
  XMPMeta.registerNamespace(XMPConst.NS_DM, 'xmpDM');
  XMPMeta.registerNamespace(XMPConst.NS_XMP_NOTE, 'xmpNote');
  XMPMeta.registerNamespace(XMPConst.TYPE_IDENTIFIER_QUAL,'xmpidq');
  XMPMeta.registerNamespace("http://ns.adobe.com/xmp/transient/1.0/", 'xmpx');

  function _registerAlias(alias, actual, arrayForm) {
    var ns = alias.split(':');
    if (ns.length != 2) {
      throw "Bad alias format: " + alias;
    }
    var aliasNS = XMPMeta.getNamespaceURI(ns[0] + ':');
    if (!aliasNS) {
      throw "Unregistered namespacePrefix: " + ns[0];
    }
    var aliasProp = ns[1];

    var ns = actual.split(':');
    if (ns.length < 2) {
      throw "Bad actual format: " + actual;
    }
    var actualPrefix = ns.shift() + ':';
    var actualNS = XMPMeta.getNamespaceURI(actualPrefix);
    if (!actualNS) {
      throw "Unregistered namespacePrefix: " + actualPrefix;
    }
    var actualProp = ns.join(':');
    if (arrayForm == undefined) {
      arrayForm = XMPConst.ALIAS_TO_SIMPLE_PROP;
    }
    XMPMeta.registerAlias(aliasNS, aliasProp,
                          actualNS, actualProp,
                          arrayForm);
  }

  x1E00 = (XMPConst.ALIAS_TO_ALT_TEXT | XMPConst.ALIAS_TO_ALT_ARRAY |
           XMPConst.ALIAS_TO_ORDERED_ARRAY | XMPConst.ALIAS_TO_ARRAY);

  x600 = (XMPConst.ALIAS_TO_ORDERED_ARRAY | XMPConst.ALIAS_TO_ARRAY);

  // NS_XMP
  _registerAlias("xap:Author", "dc:creator[1]", x600);
  _registerAlias("xap:Authors", "dc:creator");
  _registerAlias("xap:Description", "dc:description");
  _registerAlias("xap:Format", "dc:format");
  _registerAlias("xap:Keywords", "dc:subject");
  _registerAlias("xap:Locale", "dc:language");
  _registerAlias("xap:Title", "dc:title");
  _registerAlias("xapRights:Copyright", "dc:rights");

  // NS_PDF
  _registerAlias("pdf:Author", "dc:creator[1]", x600);
  _registerAlias("pdf:BaseURL", "xap:BaseURL");
  _registerAlias("pdf:CreationDate", "xap:CreateDate");
  _registerAlias("pdf:Creator", "xap:CreatorTool");
  _registerAlias("pdf:ModDate", "xap:ModifyDate");
  _registerAlias("pdf:Subject", "dc:description[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("pdf:Title", "dc:title[?xml:lang=\"x-default\"]", x1E00);

  // NS_PHOTOSHOP
  _registerAlias("photoshop:Author", "dc:creator[1]", x600);
  _registerAlias("photoshop:Caption",
                 "dc:description[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("photoshop:Copyright",
                 "dc:rights[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("photoshop:Keywords", "dc:subject");
  _registerAlias("photoshop:Marked", "xapRights:Marked");
  _registerAlias("photoshop:Title",
                 "dc:title[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("photoshop:WebStatement", "xapRights:WebStatement");

  // NS_TIFF
  _registerAlias("tiff:Artist", "dc:creator[1]", x600);
  _registerAlias("tiff:Copyright", "dc:rights");
  _registerAlias("tiff:DateTime", "xap:ModifyDate");
  _registerAlias("tiff:ImageDescription", "dc:description");
  _registerAlias("tiff:Software", "xap:CreatorTool");

  // NS_PNG
  _registerAlias("png:Author", "dc:creator[1]", x600);
  _registerAlias("png:Copyright", "dc:rights[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("png:CreationTime", "xap:CreateDate");
  _registerAlias("png:Description", "dc:description[?xml:lang=\"x-default\"]",
                 x1E00);
  _registerAlias("png:ModificationTime", "xap:ModifyDate");
  _registerAlias("png:Software", "xap:CreatorTool");
  _registerAlias("png:Title", "dc:title[?xml:lang=\"x-default\"]",
                 x1E00);


  delete XMPMeta["_init"];
};
XMPMeta._init();

"XMPMeta.jsx";
// EOF
