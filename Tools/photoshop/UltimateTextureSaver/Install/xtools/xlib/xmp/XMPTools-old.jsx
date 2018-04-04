//
// XMPTools.js
//
// $Id: XMPTools-old.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//@include "xlib/PSError.jsx"
//@include "xlib/xml/xmlsax.js"
//@include "xlib/xml/xmlw3cdom.js"
//@include "xlib/xml/xmlxpath.js"
//
//@include "xlib/JShell.js"
//

//
// W3C DOM API extensions
//
DOMNode.prototype.addAttribute = function(name, val) {
  var self = this;
  var attrNode = self.getAttributeNode(name);
  if (!attrNode) {
    attrNode = self.getOwnerDocument().createAttribute(name);
  }
  attrNode.setValue(val);
  self.setAttributeNode(attrNode);
};
DOMDocument.prototype.addAttribute = DOMNode.prototype.addAttribute;
DOMElement.prototype.addAttribute  = DOMNode.prototype.addAttribute;

DOMNode.prototype.getChildNode = function(name) {
  var self = this;
  var children = self.getChildNodes();
  var childCnt = children.getLength();

  for (var i = 0; i < childCnt; i++) {
    var child = children.item(i);
    if (child.getNodeType() == DOMNode.ELEMENT_NODE &&
        child.getTagName() == name) {
      return child;
    }
  }
  return undefined;
};
DOMDocument.prototype.getChildNode = DOMNode.prototype.getChildNode;
DOMElement.prototype.getChildNode  = DOMNode.prototype.getChildNode;

DOMNode.prototype.getElements = function(name) {
  var self = this;
  var children = self.getChildNodes();
  var childCnt = children.getLength();
  var elements = [];

  for (var i = 0; i < childCnt; i++) {
    var child = children.item(i);
    if (child.getNodeType() == DOMNode.ELEMENT_NODE && 
        (!name || child.getTagName() == name)) {
      elements.push(child);
    }
  }
  return elements;
};
DOMDocument.prototype.getElements = DOMNode.prototype.getElements;
DOMElement.prototype.getElements  = DOMNode.prototype.getElements;

//
// The original DOMElement.getAttribute call return a String object, which
// causes all kinds of havoc. We wrap the method here to make sure it returns
// a string primitive
//
DOMElement.prototype._getAttribute = function(name) {
  var v = this._orig_getAttribute(name);
  return String(v);
};

if (DOMElement.prototype.getAttribute != DOMElement.prototype._getAttribute) {
  DOMElement.prototype._orig_getAttribute = DOMElement.prototype.getAttribute;
  DOMElement.prototype.getAttribute = DOMElement.prototype._getAttribute;;
}

// this is missing from the w3c api
DOMNode.prototype.getAttribute = function(name) {
  var ret = "";
  var attr = this.attributes.getNamedItem(name);
  if (attr) {
    ret = String(attr.value);
  }
  return ret; // if Attribute exists, return its value, otherwise, return ""
};

// this is missing from the w3c api
DOMNode.prototype.getTagName = function() {
  return this.tagName;
};


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
//
XMPData = function(obj) {
  var self = this;

  self.xmlstr = '';
  self.dom = undefined;
  self.doc = undefined;
  self.pad = 0;
  self.encoding = '';
  self.packetLength = 0;
  self.file = undefined;

  if (obj) {
    self.setSource(obj);
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
XMPData.prototype.setSource = function(obj) {
  var self = this;

  if (obj == undefined) {
    Error.runtimeError(PSError.IllegalArgument);
  }

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
      Error.runtimeError(PSError.UnsupportedType);
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
    self.dom = new DOMImplementation().loadXML(self.xmlstr);
    self.doc = self.dom.getDocumentElement();
  }
};

XMPData.prototype.toString = function() {
  return "[XMPData]";
  //return this.dom.toString().replace(/></g, ">\n<");
};

XMPData.prototype.toXML = function() {
  return this.dom.toString().replace(/></g, ">\n<");
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

XMPData.prototype.selectNodeSet = function(str) {
  return this.doc.selectNodeSet(str);
};

var rdfuri = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";

XMPData.prototype.getRDFRoot = function() {
//   var xmpmeta = this.dom.getChildNode("x:xmpmeta");
//   return xmpmeta.getChildNode("rdf:RDF");
  var self = this;
  if (!self.rdfRoot) {
    var res = this.doc.selectNodeSet("//rdf:RDF");
    self.rdfRoot = (res ? res.item(0) : undefined);
  }
  return self.rdfRoot;
};

XMPData.prototype.getRDFs = function() {
  var self = this;
  var rdf = self.getRDFRoot();
  return rdf.getElements("rdf:Description");
};

XMPData.prototype.getXMPNamespace = function(name) {
  var self = this;
  var nodes = self.getRDFs();

  for (var i = 0; i < nodes.length; i++) {
    var node = nodes[i];
    if (node._namespaces.getNamedItem(name)) {
      return new XMPNode(node, name);
    }
  }
  return undefined;
};

XMPData.prototype.addXMPNamespace = function(name, prefix, uri, properties) {
  var self = this;
  var doc = self.dom;

  var root = self.getRDFRoot();

  var node = doc.createElementNS(rdfuri, "rdf:Description");
  var attr = doc.createAttributeNS(rdfuri, "rdf:about");
  attr.setValue("");

  node.setAttributeNodeNS(attr);
  root.appendChild(node);

  var nsname;
  if (prefix.startsWith("xmlns:")) {
    nsname = prefix;
  } else {
    nsname = "xmlns:" + prefix;
  }
  var ns = doc.createNamespace(nsname);
  ns.setValue(uri);
  node._namespaces.setNamedItem(ns);

  var xmpns = XMPNameSpaces[prefix];

  if (!xmpns) {
    xmpns = XMPNameSpaces.add(name, prefix, uri, properties);
  }

  return new XMPNode(node, prefix);
};

XMPData.prototype.removeXMPNamespace = function(name) {
  var self = this;

  var ns = self.getXMPNamespace(name);

  if (ns) {
    var node = ns.node;
    var parent = node.getParentNode();
    parent.removeChild(node);
    return true;
  }

  return false;
};


//================================= XMPNode ==================================
XMPNode = function(node, name) {
  var self = this;
  self.node = node;
  self.name = name;
  self.ns = XMPNameSpaces[name];
};

XMPNode.prototype.toString = function() {
  return self.name;
};

XMPNode.prototype.addProperty = function(tag, value) {
  var self = this;
  var node = self.node;
  var doc = node.getOwnerDocument();
  var child = doc.createElementNS(self.ns.uri, self.ns.prefix + ":" + tag);

  if (typeof value == "object") {
    if (value instanceof Array) {
      var seq = doc.createElementNS(rdfuri, "rdf:Seq");

      for (var i = 0; i < value.length; i++) {
        var el = doc.createElementNS(rdfuri, "rdf:li");
        var textNode = doc.createTextNode(value[i]);
        el.appendChild(textNode);
        seq.appendChild(el);
      }
      child.appendChild(seq);

    } else if (value instanceof String) {
      var textNode = doc.createTextNode(value);
      child.appendChild(textNode);

    } else {
      for (var idx in value) {
        var el = doc.createElementNS(self.ns.uri, self.ns.prefix + ":" + idx);
        var textNode = doc.createTextNode(value[idx].toString());
        el.appendChild(textNode);
        child.appendChild(el);
      }
    }
  } else {
    var textNode = doc.createTextNode(value);
    child.appendChild(textNode);
  }
  node.appendChild(child);
};

//============================== XMPTool ===============================

XMPTool = function(data) {
  var self = this;
  self.data = data;
  self.rdfNodes = {};
  self.strictMode = true;   // if a namespace has properities, the property
                            // must exist when setting its value
};

XMPTool.namespaceNames =  // prioritization
  ["exif",
   "tiff",
   "dc",
   "xmp",
   "photoshop",
   "aux",
   "crs",
   "Iptc4xmpCore",
   "pdf",
   "xapMM",
   "xmpDM",
   "xmpTPg",
   "xmpRights",
   ];

XMPTool.addNSName = function(name) {
  for (var i = 0; i < XMPTool.namespaceNames.length; i++) {
    if (name == XMPTool.namespaceNames[i]) {
      return;
    }
  }
  XMPTool.namespaceNames.push(name);
};

function pad(str, len) {
  while (str.length < len) {
    str += ' ';
  }
  return str;
};

XMPTool.prototype._select = function(ftn) {

};

//
//  select("dc:creator")
//  select("creator")
//  select("*:creator")
//  select("
//
XMPTool.prototype.select = function(ns, tag) {
  var self = this;

  //$.level = 1; debugger;
  //debugger;

  if (!ns && !tag) {
    Error.runtimeError(PSError.isUndefined, "tag");    
  }

  var isSingle = false;

  if (!tag) {
    tag = ns;
    if (!tag.contains(':')) { // select("creator")
      ns = "*";
      isSingle = true;
    } else {
      ns = undefined;
    }
  }

  var nspaces = XMPTool.namespaceNames;

  if (!ns && tag.contains(':')) {
    var p = tag.split(':');
    ns = p[0] || "*";
    tag = p[1] || "*";
  } else if (ns != "*") {
    Error.runtimeError(33, "bad select format"); // PSError.InternalError
  }

  if (ns != "*") {
    nspaces = [ns];
  }

  var wildcard = (tag == '*' || ns == '*');
  var nodes = [];

  // loop through collecting nodes that match the namespace and tag
  for (var i = 0; i < nspaces.length; i++) {
    var ns = nspaces[i];

    var nsnode = self.rdfNodes[ns];       // rdfNodes is a cache

    if (!nsnode) {
      nsnode = self.data.getXMPNamespace(ns);
      self.rdfNodes[ns] = nsnode;
    }

    if (!nsnode) {
      continue;
    }

    var foundOne = false;
    var children = nsnode.node.getChildNodes();
    var childCnt = children.getLength();
    var tagNS = ns + ':' + tag;

    for (var j = 0; j < childCnt && !foundOne; j++) {
      var child = children.item(j);

      if (child.getNodeType() == DOMNode.ELEMENT_NODE && 
          (tag == "*" || child.getTagName() == tagNS)) {
        nodes.push(child);

        // if we were looking for an exact match or a first match
        // we're done
        if (!wildcard || isSingle) {
          foundOne = true;
        }
      }
    }
    if (foundOne) {
      break;
    }
  }

  return nodes;
};


XMPTool.prototype.selectNode = function(ns, tag) {
  var self = this;
  var nodes = self.select(ns, tag);
  if (nodes.length > 0) {
    return nodes[0];
  } else { 
    return undefined;
  }
};


//
// Retrieve a simple value
//
XMPTool.prototype.extractValue = function(ns, tag) {
  var self = this;

  var node = self.selectNode(ns, tag);
  if (node) {
    var chnode = node.getFirstChild(); //getNodeValue();
    return chnode ? chnode.toString() : '';
  } else {
    return undefined;
  }
};


//
// Extract information for the specified tags.
// If 'out' is an array, the values for the tags are inserted into the array.
// If 'out' is an object, the values are added to the object using the tags as
//   property names.
// If 'out' is not specified, a display string is returned.
//
XMPTool.prototype.extract = function(tags, out) {
  var self = this;
  var str = '';

  if (!tags) {
    Error.runtimeError(PSError.isUndefined, "tags");    
  }

  var isArray = (out instanceof Array);
  var isObject = (typeof(out) == "object");

  if (tags.constructor == String) {
    tags = [tags];
  }

  for (var i = 0; i < tags.length; i++) {
    var tag = tags[i];
    var nodes = self.select(tag);

    //$.level = 1; debugger;

    for (var j = 0; j < nodes.length; j++) {
      var node = nodes[j];
      var chnode = node.getFirstChild(); //getNodeValue();

      var val = chnode ? chnode.toString() : '';

      if (isArray) {
        out.push(val);
      } else if (isObject) {
        out[node.getTagName()] = val;
      } else {
        str += pad(tag, 40) + " " + val + "\n";
      }
    }
  }

  var res;
  if (isArray || isObject) {
    res = out;
  } else {
    res = str;
  }

  return res;
};


XMPTool.prototype.insertValue = function(ns, tag, value) {
  var self = this;

  if (arguments.length == 2) {
    value = tag;
    tag = ns;
    ns = undefined;
  } else if (arguments.length == 3) {
    tag = ns + ':' + tag;
    ns = undefined;
  } else {
    Error.runtimeError(PSError.WrongNumberOfArguments);    
  }

  if (!tag) {
    Error.runtimeError(PSError.isUndefined, "tag");    
  }

  var nspaces;
  var explicitNS = undefined;

  if (tag.contains(':')) {
    var p = tag.split(':');
    explicitNS = p[0];
    tag = p[1];
    nspaces = [explicitNS];

  } else {
    nspaces = XMPTool.namespaceNames;
  }

  var setOK = false;

  for (var i = 0; i < nspaces.length; i++) {
    var ns = nspaces[i];
    var xmpns = XMPNameSpaces[ns];
    var nsnode = self.rdfNodes[ns];  // this is a cache for namespace nodes

    if (!nsnode) {
      // cache the namespace
      nsnode = self.data.getXMPNamespace(ns);

      if (!nsnode && xmpns) {

        // we found a new name space.

        nsnode = self.data.addXMPNamespace(xmpns.name,
                                           xmpns.prefix,
                                           xmns.uri);
      }
      self.rdfNodes[ns] = nsnode;
    }

    if (!nsnode) {
      continue;
    }

    if (!xmpns) {
      throw "Unknown namespace: " + ns;
    }

    if (ns == explicitNS) {
      if (self.strictMode &&
          xmpns.hasProperties() && !xmpns.hasProperty(tag)) {
        throw "The namespace " + ns + " does not have the property " + tag;
      }
      nsnode.addProperty(tag, value);
      setOK = true;

    } else if (xmpns.hasProperty(tag)) {
      nsnode.addProperty(tag, value);
    }
  }

  if (!setOK) {
    throw "Unable to set property " + tag + " in namespace " + ns;
  }

  return value;
};


//
// XMPTool.insert([namespace,] properties)
//
XMPTool.prototype.insert = function(ns, properties) {
  var self = this;
  
  if (arguments.length == 1) {
    properties = ns;
    ns = undefined;
  } else if (arguments.length == 2) {
    if (ns.constructor != String) {
      Error.runtimeError(PSError.StringExpected, "ns");    
    }
  } else {
    Error.runtimeError(PSError.WrongNumberOfArguments);    
  }
  
  if (!(properties instanceof Array)) {
    Error.runtimeError(PSError.ArrayExpected, "properties");
  }

  for (var idx in properties) {
    var val = properties[idx];
    var tag = idx;
    if (idx.contains(':')) {
      if (ns && !idx.match(ns + ':')) {
        throw "Property namespace does not match specified namespace.";
      }
    } else if (ns) {
      tag = ns + ':' + idx;
    }

    self.insertValue(tag, value);
  }
};

//  var res = this.xmp.selectNodeSet("//exif:SceneCaptureType");
XMPTool.prototype.remove = function(ns, tag) {
  var self = this;

  var nodes = self.select(ns, tag);

  for (var i = 0; i < nodes.length; i++) {
    var child = nodes[i];
    var parent = child.getParentNode();
    parent.removeChild(child);
  }

  return nodes.length;
};

XMPTool.prototype.selectNodeSet = function(expr) {
  return this.xmp.doc.selectNodeSet(expr);
};


//================================ XMPNameSpace =============================

XMPNameSpace = function() {
  var self = this;
  self.name = '';
  self.prefix = '';
  self.uri = '';
  self.properties = [];
};

XMPNameSpace.prototype.hasProperty = function(p) {
  var self = this;

  var str = "+" + this.properties.join("+") + "+";
  return str.contains("+" + p + "+");
};
XMPNameSpace.prototype.hasProperties = function() {
  return this.properties && this.properties.length;
};

XMPNameSpaces = function() {};

XMPNameSpaces._elements = [];

XMPNameSpaces.add = function(name, prefix, uri, properties) {
  self = this;

  if (name.constructor != String) {
    Error.runtimeError(PSError.Argument1);
  }

  if (prefix.constructor != String) {
    Error.runtimeError(PSError.Argument2);
  }

  if (uri.constructor != String) {
    Error.runtimeError(PSError.Argument3);
  }

  if (properties && !(properties instanceof Array)) {
    Error.runtimeError(PSError.Argument4);
  }

  var ns = new XMPNameSpace();
  ns.name = name;
  ns.properties = properties;
  ns.prefix = prefix;
  ns.uri = uri;

  self[prefix] = ns;
  self[prefix.toLowerCase()] = ns;

  XMPTool.addNSName(prefix);

  return ns;
};

// Details of common namespaces

XMPNameSpaces.add("Dublin Core",
                  "dc",
                  "http://purl.org/dc/elements/1.1/",
                  ["contributor",
                   "coverage",
                   "creator",
                   "date",
                   "description",
                   "format",
                   "identifier",
                   "language",
                   "publisher",
                   "relation",
                   "rights",
                   "source",
                   "subject",
                   "title",
                   "type",
                   ]);

XMPNameSpaces.add("XMP Basic",
                  "xmp",
                  "http://ns.adobe.com/xap/1.0/",
                  ["Advisory",
                   "BaseURL",
                   "CreateDate",
                   "CreatorTool",
                   "Identifier",
                   "Label",
                   "MetadataDate",
                   "ModifyDate",
                   "Nickname",
                   "Rating",
                   "Thumbnail",
                   "Thumbnails",
                   ]);

XMPNameSpaces.add("XAP Basic",
                  "xap",
                  "http://ns.adobe.com/xap/1.0/",
                  XMPNameSpaces.xmp.properties);


XMPNameSpaces.add("XMP Rights Management",
                  "xmpRights",
                  "http://ns.adobe.com/xap/1.0/rights/",
                  ["Certificate",
                   "Marked",
                   "Owner",
                   "UsageTerms",
                   "WebStatement"
                   ]);

XMPNameSpaces.add("XAP Rights",
                  "xapRights",
                  "http://ns.adobe.com/xap/1.0/rights/",
                  XMPNameSpaces.xmpRights.properties);

  
XMPNameSpaces.add("XMP Media Management",
                  "xmpMM",
                  "http://ns.adobe.com/xap/1.0/mm/",
                  ["DerivedFrom",
                   "DocumentID",
                   "History",
                   "ManagedFrom",
                   "Manager",
                   "ManageTo",
                   "ManageUI",
                   "ManagerVariant",
                   "RenditionClass",
                   "RenditionParams",
                   "VersionID",
                   "Versions",
//                     "LastURL",      // deprecated
//                     "RenditionOf",  // deprecated
//                     "SaveID",       // deprecated
                   ]);

XMPNameSpaces.add("XAP Media Management",
                  "xapMM",
                  "http://ns.adobe.com/xap/1.0/mm/",
                  XMPNameSpaces.xmpMM.properties);


XMPNameSpaces.add("XMP Basic Job Ticket",
                  "xmpBJ",
                  "http://ns.adobe.com/xap/1.0/bj/",
                  ["JobRef",
                   ]);

XMPNameSpaces.add("XAP Basic Job Ticket",
                  "xapBJ",
                  "http://ns.adobe.com/xap/1.0/bj/",
                  XMPNameSpaces.xmpBJ.properties);

XMPNameSpaces.add("XMP Paged-Text",
                  "xmpTPg",
                  "http://ns.adobe.com/xap/1.0/pg/",
                  ["MaxPageSize",
                   "NPages",
                   "Fonts",
                   "Colorants",
                   "PlateNames",
                   ]);

XMPNameSpaces.add("XMP Dynamic Media",
                  "xmpDM",
                  "http://ns.adobe.com/xmp/1.0/DynamicMedia/",
                  ["projectRef",
                   "vidoeFrameRate",
                   "videoFrameSize",
                   "videoPixelAspectRatio",
                   "videoPixelDepth",
                   "videoColorSpace",
                   "videoAlphaMode",
                   "videoAlphaPremultipleColor",
                   "videoAlphaUnityIsTransparent",
                   "videoCompressor",
                   "videoFieldOrder",
                   "pullDown",
                   "audioSampleRate",
                   "audioSampleType",
                   "audioChannelType",
                   "audioCompressor",
                   "speakerPlacement",
                   "fileDataRate",
                   "tapeName",
                   "altTapeName",
                   "startTimecode",
                   "altTimecode",
                   "duration",
                   "scene",
                   "shotName",
                   "shotDate",
                   "shotLocation",
                   "logComment",
                   "markers",
                   "contributedMedia",
                   "absPeakAudioFilePath",
                   "relativePeakAudioFilePath",
                   "videoModDate",
                   "audioModDate",
                   "metadataModDate",
                   "artist",
                   "album",
                   "trackNumber",
                   "genre",
                   "copyright",
                   "releaseDate",
                   "composer",
                   "engineer",
                   "tempo",
                   "instrument",
                   "introTime",
                   "outCue",
                   "relativeTimestamp",
                   "loop",
                   "numberOfBeats",
                   "key",
                   "stretchMode",
                   "timeScaleParams",
                   "resampleParams",
                   "beatSpliceParams",
                   "timeSignature",
                   "scaleType",
                   ]);

XMPNameSpaces.add("Adobe PDF",
                  "pdf",
                  "http://ns.adobe.com/pdf/1.3/",
                  ["Keywords",
                   "PDFVersion",
                   "Producer",
                   ]);

XMPNameSpaces.add("Photoshop",
                  "photoshop",
                  "http://ns.adobe.com/photoshop/1.0/",
                  ["AuthorsPosition",
                   "CaptionWriter",
                   "Category",
                   "City",
                   "Country",
                   "Credit",
                   "DateCreated",
                   "Headline",
                   "Instructions",
                   "Source",
                   "State",
                   "SupplementalCategories",
                   "TransmissionReference",
                   "Urgency",
                   ]);

XMPNameSpaces.add("Camera Raw",
                  "crs",
                  "http://ns.adobe.com/camera-raw-settings/1.0/",
                  ["AutoBrightness",
                   "AutoContrast",
                   "AutoExposure",
                   "AutoShadows",
                   "BlueHue",
                   "BlueSaturation",
                   "Brightness",
                   "CameraProfile",
                   "ChromaticAberrationB",
                   "ChromaticAberrationR",
                   "ColorNoiseReduction",
                   "Contrast",
                   "CropTop",
                   "CropLeft",
                   "CropBottom",
                   "CropRight",
                   "CropAngle",
                   "CropWidth",
                   "CropHeight",
                   "CropUnits",
                   "Exposure",
                   "GreenHue",
                   "GreenSaturation",
                   "HasCrop",
                   "HasSettings",
                   "LuminanceSmoothing",
                   "RawFileName",
                   "RedHue",
                   "RedSaturation",
                   "Saturation",
                   "Shadows",
                   "ShadowTint",
                   "Sharpness",
                   "Temperature",
                   "Tint",
                   "ToneCurve",
                   "ToneCurveName",
                   "Version",
                   "VignetteAmount",
                   "VignetteMidpoint",
                   "WhiteBalance",
                   ]);

// EXIF Schemas
XMPNameSpaces.add("TIFF",
                  "tiff",
                  "http://ns.adobe.com/tiff/1.0/",
                  ["ImageWidth",
                   "ImageLength",
                   "BitsPerSample",
                   "Compression",
                   "PhotometricInterpretation",
                   "Orientation",
                   "SamplesPerPixel",
                   "PlanarConfiguration",
                   "YCbCrSubSampling",
                   "XResolution",
                   "YResolution",
                   "ResolutionUnit",
                   "TransferFunction",
                   "WhitePoint",
                   "PrimaryChromaticities",
                   "YCbCrCoefficients",
                   "ReferenceBlackWhite",
                   "DateTime",
                   "ImageDescription",
                   "Make",
                   "Model",
                   "Software",
                   "Artist",
                   "Copyright",
                   ]);

XMPNameSpaces.add("EXIF",
                  "exif",
                  "http://ns.adobe.com/exif/1.0/",
                  ["ExifVersion",
                   "FlashpixVersion",
                   "ColorSpace",
                   "ComponentsConfiguration",
                   "CompressedBitsPerPixel",
                   "PixelXDimension",
                   "PixelYDimension",
                   "UserComment",
                   "RelatedSoundFile",
                   "DateTimeOriginal",
                   "DateTimeDigitized",
                   "ExposureTime",
                   "FNumber",
                   "ExposureProgram",
                   "SpectralSensitivity",
                   "ISOSpeedRatings",
                   "OECF",
                   "ShutterSpeedValue",
                   "ApetureValue",
                   "BrightnessValue",
                   "ExposureBiasValue",
                   "MaxApetureValue",
                   "SubjectDistance",
                   "MeteringMode",
                   "LightSource",
                   "Flash",
                   "FocalLength",
                   "SubjectArea",
                   "FlashEnergy",
                   "SpatialFrequencyResponse",
                   "FocalPlaneXResolution",
                   "FocalPlaneYResolution",
                   "FocalPlaneResolutionUnit",
                   "SubjectLocation",
                   "ExposureIndex",
                   "SensingMethod",
                   "FileSource",
                   "SceneType",
                   "CFAPattern",
                   "CustomRendered",
                   "ExposureMode",
                   "WhiteBalance",
                   "DigitalZoomRatio",
                   "FocalLengthIn35mmFilm",
                   "SceneCaptureType",
                   "GainControl",
                   "Constrast",
                   "Saturation",
                   "Sharpness",
                   "DeviceSettingDescription",
                   "SubjectDistanceRange",
                   "ImageUniqueID",
                   "GPSVersionID",
                   "GPSLatitude",
                   "GPSLongitude",
                   "GPSAltitudeRef",
                   "GPSAltitude",
                   "GPSTimeStamp",
                   "GPSSatellites",
                   "GPSStatus",
                   "GPSMeasureMode",
                   "GPSDOP",
                   "GPSSpeedRef",
                   "GPSSpeed",
                   "GPSTrackRef",
                   "GPSTrack",
                   "GPSImgDirectionRef",
                   "GPSImgDirection",
                   "GPSMapDatum",
                   "GPSDestLatitude",
                   "GPSDestLongitude",
                   "GPSDestBearingRef",
                   "GPSDestBearing",
                   "GPSDestDistanceRef",
                   "GPSDestDistance",
                   "GPSProcessingMethod",
                   "GPSAreaInformation",
                   "GPSDifferential",
                   ]);

XMPNameSpaces.add("EXIF AUX",
                  "aux",
                  "http://ns.adobe.com/exif/1.0/aux",
                  ["Lens",
                   "Shutter",
                   ]);

XMPNameSpaces.add("IPTC Core",
                  "Iptc4xmpCore",
                  "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                  ["City",               // photoshop:City
                   "CopyrightNotice",    // dc:rights
                   "Country",            // photoshop:Country
                   "CountryCode",
                   "Creator",            // dc:creator
                   "CreatorContactInfo",
                   "CreatorJobTitle",    // photoshop:AuthorsPosition
                   "DateCreated",        // photoshop:DateCreated
                   "Description",        // dc:description
                   "DescriptionWriter",  // photoshop:CaptionWriter
                   "Headline",           // photoshop:Headline
                   "Instructions",       // photoshop:instructions
                   "IntellectualGenre",
                   "JobID",              // photoshop:TransmissionReference
                   "Keywords",           // dc:subject
                   "Location",
                   "Provider",           // photoshop:Credit
                   "Province-State",     // photoshop:State
                   "RightsUsageTerms",   // xmpRights:UsageTerms
                   "Scene",
                   "Source",             // photoshop:Source
                   "SubjectCode",
                   "Title",              // dc:title
                   ]);

XMPNameSpaces.add("IPTC Core",
                  "iptcCore",
                  "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                  XMPNameSpaces.Iptc4xmpCore.properties);

XMPNameSpaces.add("Creative Commons",
                  "cc",
                  "http://web.resource.org/cc/",
                  ["License"]);

//
// RDFContainer
//  ctype    one of 'Seq', 'Bag', 'Alt'
//  ary      Array
//
RDFContainer = function RDFContainer(type, ary) {
  if (typeof(this) == "function") {
    return new RDFContainer(type, ary);
  }
  var self = this;
  self.type = type;
  if (ary) {
    for (var i = 0; i < ary.length; i++) {
      self[i] = ary[i];
    }
  }
};
RDFContainer.prototype = new Array();

RDFContainer.prototype.asNode = function(doc) {
  var self = this;
  var node = doc.createElementNS(rdfuri, "rdf:" + self.type);
  for (var i = 0; i < self.length; i++) {
    var el = doc.createElementNS(rdfuri, "rdf:li");
  }
};

//
// Test working with XMPTool.extract
//
function testXMPextract() {
  // read in an xmp file
  var xmp = new XMPData();
  xmp.setSource(new File('/c/work/scratch/test.xmp'));

  // pass the data to our exiftool-like accessor 
  var xmpTool = new XMPTool(xmp);

  // define some properties
  var props = ["dc:creator", "DateTimeOriginal"];

  // extract the properties into an object
  var obj = {};
  xmpTool.extract(props, obj);
  alert(listProps(obj));

  // extract the property values into an array
  var ary = [];
  xmpTool.extract(props, ary);
  alert(ary);

  // extract the properties into an display string
  var str = xmpTool.extract(props);
  alert(str);

  return;
};

//
// Test working with XMPTool.insert
//
function testXMPinsert() {
  // read in an xmp file
  var xmp = new XMPData();
  xmp.setSource(new File('/c/work/scratch/test.xmp'));

  // pass the data to our exiftool-like accessor 
  var xmpTool = new XMPTool(xmp);

  // define some properties
  var props = ["dc:creator", "DateTimeOriginal"];
};

function main() {
  testXMPextract();

  return;

  var testStr = Stdlib.readFromFile('/c/work/scratch/test.xmp');

  xmp = new XMPData(testStr);

  var exif = xmp.getXMPNamespace("exif");

  xmpTool = new XMPTool(xmp);

  doc = xmp.doc;

  //  $.level = 1; debugger;
  var res = xmpTool.extract(["Source",
                             "dc:creator",
                             "exif:ExposureTime",
                             "DateTimeOriginal"]);
  //alert(res);

  var obj = {};
  xmpTool.extract("photoshop:", obj);
  //alert(listProps(obj));

  xmp.addXMPNamespace("ps-scripts.com", "psscriptsns",
                      "http://ps-scripts.com/ns/");

  xmpTool.insertValue("psscriptsns:myScalar", "some value");
  xmpTool.insertValue("psscriptsns:myArray", [1, 3, "blue"]);
  var obj = {
    name: "Bob",
    age: 32
  };
  xmpTool.insertValue("psscriptsns:myObject", obj);
  xmpTool.insertValue("psscriptsns:Source", "http://google.com");

  //$.level = 1;
  xmpTool.select("photoshop:");
  xmpTool.select(":Source");
  
  xmp.selectNodeSet("//psscriptsns:myObject");
  xmp.selectNodeSet("//psscriptsns:name");
  xmp.selectNodeSet("//exif:FocalLength");

  xmpTool.remove("exif:Flash");
  xmpTool.remove("ShutterSpeedValue");

  //JShell.exec();
 
  Stdlib.writeToFile('/c/work/scratch/test2.xml',
                     xmp.toXML());

  xmp.removeXMPNamespace("psscriptsns");

  Stdlib.writeToFile('/c/work/scratch/test3.xml',
                     xmp.toXML());

  Stdlib.writeToFile('/c/work/scratch/test3.xmp',
                     xmp.toXPacket());
};

main();

"XMPTools.jsx";
// EOF
