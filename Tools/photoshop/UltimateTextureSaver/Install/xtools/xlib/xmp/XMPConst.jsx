//
// XMPConst.jsx
//
// $Id: XMPConst.jsx,v 1.2 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

XMPConst = function XMPConst() {
};
XMPConst._new = function(val, str) {
  var v = new XMPConst();
  v._val = val;
  v._str = (str || val.toString());
  v.toString = function() { return this._str; }
  v.valueOf = function()  { return this._val; }
  return v;
};


// XMP Namespaces

XMPConst.NS_DC = XMPConst._new("http://purl.org/dc/elements/1.1/");
XMPConst.NS_IPTC_CORE = XMPConst._new("http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/");
XMPConst.NS_RDF = XMPConst._new("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
XMPConst.NS_XML = XMPConst._new("http://www.w3.org/XML/1998/namespace");
XMPConst.NS_XMP = XMPConst._new("http://ns.adobe.com/xap/1.0/");
XMPConst.NS_XMP_RIGHTS = XMPConst._new("http://ns.adobe.com/xap/1.0/rights/");
XMPConst.NS_XMP_MM = XMPConst._new("http://ns.adobe.com/xap/1.0/mm/");
XMPConst.NS_XMP_BJ = XMPConst._new("http://ns.adobe.com/xap/1.0/bj/");
XMPConst.NS_XMP_NOTE = XMPConst._new("http://ns.adobe.com/xmp/note/");

XMPConst.NS_PDF = XMPConst._new("http://ns.adobe.com/pdf/1.3/");
XMPConst.NS_PDFX = XMPConst._new("http://ns.adobe.com/pdfx/1.3/");
XMPConst.NS_PHOTOSHOP = XMPConst._new("http://ns.adobe.com/photoshop/1.0/");
XMPConst.NS_PS_ALBUM = XMPConst._new("http://ns.adobe.com/album/1.0/");
XMPConst.NS_EXIF = XMPConst._new("http://ns.adobe.com/exif/1.0/");
XMPConst.NS_EXIF_AUX = XMPConst._new("http://ns.adobe.com/exif/1.0/aux/");

XMPConst.NS_TIFF = XMPConst._new("http://ns.adobe.com/tiff/1.0/");
XMPConst.NS_PNG = XMPConst._new("http://ns.adobe.com/png/1.0/");
XMPConst.NS_JPEG = XMPConst._new("http://ns.adobe.com/jpeg/1.0/");
XMPConst.NS_SWF = XMPConst._new("http://ns.adobe.com/swf/1.0/");
XMPConst.NS_JP2K = XMPConst._new("http://ns.adobe.com/jp2k/1.0/");
XMPConst.NS_CAMERA_RAW =
  XMPConst._new("http://ns.adobe.com/camera-raw-settings/1.0/");
XMPConst.NS_DM = XMPConst._new("http://ns.adobe.com/xmp/1.0/DynamicMedia/");
XMPConst.NS_ADOBE_STOCK_PHOTO =
  XMPConst._new("http://ns.adobe.com/StockPhoto/1.0/");
XMPConst.NS_ASF = XMPConst._new("http://ns.adobe.com/asf/1.0/");


// XMP Types

XMPConst.TYPE_IDENTIFIER_QUAL =
  XMPConst._new("http://ns.adobe.com/xmp/Identifier/qual/1.0/");
XMPConst.TYPE_DIMENSIONS =
  XMPConst._new("http://ns.adobe.com/xap/1.0/sType/Dimensions#");
XMPConst.TYPE_TEXT = XMPConst._new("http://ns.adobe.com/xap/1.0/t/");
XMPConst.TYPE_PAGEDFILE = XMPConst._new("http://ns.adobe.com/xap/1.0/t/pg/");
XMPConst.TYPE_GRAPHICS = XMPConst._new("http://ns.adobe.com/xap/1.0/g/");
XMPConst.TYPE_IMAGE = XMPConst._new("http://ns.adobe.com/xap/1.0/g/img/");
XMPConst.TYPE_FONT = XMPConst._new("http://ns.adobe.com/xap/1.0/sType/Font#");
XMPConst.TYPE_RESOURCE_EVENT =
  XMPConst._new("http://ns.adobe.com/xap/1.0/sType/ResourceEvent#");
XMPConst.TYPE_RESOURCE_REF =
  XMPConst._new("http://ns.adobe.com/xap/1.0/sType/ResourceRef#");
XMPConst.TYPE_ST_VERSION =
  XMPConst._new("http://ns.adobe.com/xap/1.0/sType/Version#");
XMPConst.TYPE_ST_JOB = XMPConst._new("http://ns.adobe.com/xap/1.0/sType/Job#");
XMPConst.TYPE_MANIFEST_ITEM =
  XMPConst._new("http://ns.adobe.com/xap/1.0/sType/ManifestItem#");
XMPConst.TYPE_PDFA_SCHEMA =
  XMPConst._new("http://www.aiim.org/pdfa/ns/schema#");
XMPConst.TYPE_PDFA_PROPERTY =
  XMPConst._new("http://www.aiim.org/pdfa/ns/property#");
XMPConst.TYPE_PDFA_TYPE = XMPConst._new("http://www.aiim.org/pdfa/ns/type#");
XMPConst.TYPE_PDFA_FIELD = XMPConst._new("http://www.aiim.org/pdfa/ns/field#");
XMPConst.TYPE_PDFA_ID = XMPConst._new("http://www.aiim.org/pdfa/ns/id/");
XMPConst.TYPE_PDFA_EXTENSION =
  XMPConst._new("http://www.aiim.org/pdfa/ns/extension/");


// XMP Files

XMPConst.FILE_UNKNOWN = XMPConst._new(538976288, "XMPConst.FILE_UNKNOWN");
XMPConst.FILE_PDF = XMPConst._new(1346651680, "XMPConst.FILE_PDF");
XMPConst.FILE_POSTSCRIPT =
  XMPConst._new(1347624992, "XMPConst.FILE_POSTSCRIPT");
XMPConst.FILE_EPS = XMPConst._new(1162892064, "XMPConst.FILE_EPS");
XMPConst.FILE_JPEG = XMPConst._new(1246774599, "XMPConst.FILE_JPEG");
XMPConst.FILE_JPEG2K = XMPConst._new(1246779424, "XMPConst.FILE_JPEG2K");
XMPConst.FILE_TIFF = XMPConst._new(1414088262, "XMPConst.FILE_TIFF");
XMPConst.FILE_GIF = XMPConst._new(1195984416, "XMPConst.FILE_GIF");
XMPConst.FILE_PNG = XMPConst._new(1347307296, "XMPConst.FILE_PNG");
XMPConst.FILE_SWF = XMPConst._new(1398228512, "XMPConst.FILE_SWF");
XMPConst.FILE_FLA = XMPConst._new(1179402528, "XMPConst.FILE_FLA");
XMPConst.FILE_FLV = XMPConst._new(1179407904, "XMPConst.FILE_FLV");
XMPConst.FILE_MOV = XMPConst._new(1297045024, "XMPConst.FILE_MOV");
XMPConst.FILE_AVI = XMPConst._new(1096173856, "XMPConst.FILE_AVI");
XMPConst.FILE_CIN = XMPConst._new(1128877600, "XMPConst.FILE_CIN");
XMPConst.FILE_WAV = XMPConst._new(1463899680, "XMPConst.FILE_WAV");
XMPConst.FILE_MP3 = XMPConst._new(1297101600, "XMPConst.FILE_MP3");
XMPConst.FILE_SES = XMPConst._new(1397052192, "XMPConst.FILE_SES");
XMPConst.FILE_CEL = XMPConst._new(1128614944, "XMPConst.FILE_CEL");
XMPConst.FILE_MPEG = XMPConst._new(1297106247, "XMPConst.FILE_MPEG");
XMPConst.FILE_MPEG2 = XMPConst._new(1297101344, "XMPConst.FILE_MPEG2");
XMPConst.FILE_MPEG4 = XMPConst._new(1297101856, "XMPConst.FILE_MPEG4");
XMPConst.FILE_WMAV = XMPConst._new(1464680790, "XMPConst.FILE_WMAV");
XMPConst.FILE_AIFF = XMPConst._new(1095321158, "XMPConst.FILE_AIFF");
XMPConst.FILE_HTML = XMPConst._new(1213484364, "XMPConst.FILE_HTML");
XMPConst.FILE_XML = XMPConst._new(1481460768, "XMPConst.FILE_XML");
XMPConst.FILE_TEXT = XMPConst._new(1952807028, "XMPConst.FILE_TEXT");
XMPConst.FILE_PHOTOSHOP = XMPConst._new(1347634208, "XMPConst.FILE_PHOTOSHOP");
XMPConst.FILE_ILLUSTRATOR =
  XMPConst._new(1095311392, "XMPConst.FILE_ILLUSTRATOR");
XMPConst.FILE_INDESIGN = XMPConst._new(1229866052, "XMPConst.FILE_INDESIGN");
XMPConst.FILE_AE_PROJECT =
  XMPConst._new(1095061536, "XMPConst.FILE_AE_PROJECT");
XMPConst.FILE_AE_PROJECT_TEMPLATE =
  XMPConst._new(1095062560, "XMPConst.FILE_AE_PROJECT_TEMPLATE");
XMPConst.FILE_AE_FILTER_PRESET =
  XMPConst._new(1179015200, "XMPConst.FILE_AE_FILTER_PRESET");
XMPConst.FILE_ENCORE_PROJECT =
  XMPConst._new(1313034066, "XMPConst.FILE_ENCORE_PROJECT");
XMPConst.FILE_PREMIERE_PROJECT =
  XMPConst._new(1347571786, "XMPConst.FILE_PREMIERE_PROJECT");
XMPConst.FILE_PREMIERE_TITLE =
  XMPConst._new(1347572812, "XMPConst.FILE_PREMIERE_TITLE");

// XMPFile openFlags

XMPConst.OPEN_FOR_READ =
  XMPConst._new(1, "XMPConst.OPEN_FOR_READ");
XMPConst.OPEN_FOR_UPDATE =
  XMPConst._new(2, "XMPConst.OPEN_FOR_UPDATE");
XMPConst.OPEN_ONLY_XMP =
  XMPConst._new(4, "XMPConst.OPEN_ONLY_XMP");
XMPConst.OPEN_STRICTLY =
  XMPConst._new(16, "XMPConst.OPEN_STRICTLY");
XMPConst.OPEN_USE_SMART_HANDLER =
  XMPConst._new(32, "XMPConst.OPEN_USE_SMART_HANDLER");
XMPConst.OPEN_USE_PACKET_SCANNING =
  XMPConst._new(64, "XMPConst.OPEN_USE_PACKET_SCANNING");
XMPConst.OPEN_LIMITED_SCANNING =
  XMPConst._new(128, "XMPConst.OPEN_LIMITED_SCANNING");

XMPConst.CLOSE_UPDATE_SAFELY =
  XMPConst._new(1, "XMPConst.CLOSE_UPDATE_SAFELY");

// XMPFile formatInfo

XMPConst.HANDLER_CAN_INJECT_XMP =
  XMPConst._new(1, "XMPConst.HANDLER_CAN_INJECT_XMP");
XMPConst.HANDLER_CAN_EXPAND =
  XMPConst._new(2, "XMPConst.HANDLER_CAN_EXPAND");
XMPConst.HANDLER_CAN_REWRITE =
  XMPConst._new(4, "XMPConst.HANDLER_CAN_REWRITE");
XMPConst.HANDLER_PPEFERS_IN_PLACE =
  XMPConst._new(8, "XMPConst.HANDLER_PPEFERS_IN_PLACE");
XMPConst.HANDLER_CAN_RECONCILE =
  XMPConst._new(16, "XMPConst.HANDLER_CAN_RECONCILE");
XMPConst.HANDLER_ALLOWS_ONLY_XMP =
  XMPConst._new(32, "XMPConst.HANDLER_ALLOWS_ONLY_XMP");
XMPConst.HANDLER_RETURNS_RAW_PACKETS =
  XMPConst._new(64, "XMPConst.HANDLER_RETURNS_RAW_PACKETS");
XMPConst.HANDLER_RETURNS_TNAIL =
  XMPConst._new(128, "XMPConst.HANDLER_RETURNS_TNAIL");
XMPConst.HANDLER_OWNS_FILE =
  XMPConst._new(256, "XMPConst.HANDLER_OWNS_FILE");
XMPConst.HANDLER_NEEDS_READONLY_PACKET =
  XMPConst._new(512, "XMPConst.HANDLER_NEEDS_READONLY_PACKET");
XMPConst.HANDLER_USES_SIDECAR_XMP =
  XMPConst._new(1024, "XMPConst.HANDLER_USES_SIDECAR_XMP");


// XMPMeta Alias form

XMPConst.ALIAS_TO_SIMPLE_PROP =
  XMPConst._new(0, "XMPConst.ALIAS_TO_SIMPLE_PROP");
XMPConst.ALIAS_TO_ARRAY =
  XMPConst._new(512, "XMPConst.ALIAS_TO_ARRAY");
XMPConst.ALIAS_TO_ORDERED_ARRAY =
  XMPConst._new(1024, "XMPConst.ALIAS_TO_ORDERED_ARRAY");
XMPConst.ALIAS_TO_ALT_ARRAY =
  XMPConst._new(2048, "XMPConst.ALIAS_TO_ALT_ARRAY");
XMPConst.ALIAS_TO_ALT_TEXT =
  XMPConst._new(4096, "XMPConst.ALIAS_TO_ALT_TEXT");

// XMPMeta itemOptions
XMPConst.PROP_IS_SIMPLE = XMPConst._new(0, "XMPConst.PROP_IS_SIMPLE");
XMPConst.PROP_IS_ARRAY = XMPConst._new(512, "XMPConst.PROP_IS_ARRAY");
XMPConst.PROP_IS_STRUCT = XMPConst._new(256, "XMPConst.PROP_IS_STRUCT");

// XMPMeta arrayOptions
XMPConst.ARRAY_IS_ORDERED =
  XMPConst._new(1024, "XMPConst.ARRAY_IS_ORDERED");
XMPConst.ARRAY_IS_ALTERNATIVE =
  XMPConst._new(2048, "XMPConst.ARRAY_IS_ALTERNATIVE");

XMPConst.ARRAY_LAST_ITEM = XMPConst._new(-1, "XMPConst.ARRAY_LAST_ITEM");


// XMPProperty data type
XMPConst.STRING  = XMPConst._new("string");
XMPConst.INTEGER = XMPConst._new("integer");
XMPConst.NUMBER  = XMPConst._new("number");
XMPConst.BOOLEAN = XMPConst._new("boolean");
XMPConst.XMPDATE = XMPConst._new("xmpdate");

XMPConst.APPEND_ALL_PROPERTIES =
  XMPConst._new(1, "XMPConst.APPEND_ALL_PROPERTIES");
XMPConst.APPEND_REPLACE_OLD_VALUES = 
  XMPConst._new(2, "XMPConst.APPEND_REPLACE_OLD_VALUES");
XMPConst.APPEND_DELETE_EMPTY_VALUES =
  XMPConst._new(4, "XMPConst.APPEND_DELETE_EMPTY_VALUES");

XMPConst.SEPARATE_ALLOW_COMMAS =
  XMPConst._new(268435456, "XMPConst.SEPARATE_ALLOW_COMMAS");

XMPConst.REMOVE_ALL_PROPERTIES =
  XMPConst._new(1, "XMPConst.REMOVE_ALL_PROPERTIES");
XMPConst.REMOVE_INCLUDE_ALIASES =
  XMPConst._new(2048, "XMPConst.REMOVE_INCLUDE_ALIASES");

XMPConst.ITERATOR_JUST_CHILDREN =
  XMPConst._new(256, "XMPConst.ITERATOR_JUST_CHILDREN");
XMPConst.ITERATOR_JUST_LEAFNODES =
  XMPConst._new(512, "XMPConst.ITERATOR_JUST_LEAFNODES");
XMPConst.ITERATOR_JUST_LEAFNAMES  =
  XMPConst._new(1024, "XMPConst.ITERATOR_JUST_LEAFNAMES ");
XMPConst.ITERATOR_INCLUDE_ALIASES =
  XMPConst._new(2048, "XMPConst.ITERATOR_INCLUDE_ALIASES");
XMPConst.ITERATOR_OMIT_QUALIFIERS =
  XMPConst._new(4096, "XMPConst.ITERATOR_OMIT_QUALIFIERS");

XMPConst.SERIALIZE_OMIT_PACKET_WRAPPER =
  XMPConst._new(16, "XMPConst.SERIALIZE_OMIT_PACKET_WRAPPER");
XMPConst.SERIALIZE_READ_ONLY_PACKET =
  XMPConst._new(32, "XMPConst.SERIALIZE_READ_ONLY_PACKET");
XMPConst.SERIALIZE_USE_COMPACT_FORMAT =
  XMPConst._new(64, "XMPConst.SERIALIZE_USE_COMPACT_FORMAT");
XMPConst.SERIALIZE_USE_PLAIN_XMP  =
  XMPConst._new(128, "XMPConst.SERIALIZE_USE_PLAIN_XMP ");
XMPConst.SERIALIZE_INCLUDE_THUMBNAIL_PAD =
  XMPConst._new(256, "XMPConst.SERIALIZE_INCLUDE_THUMBNAIL_PAD");
XMPConst.SERIALIZE_EXACT_PACKET_LENGTH =
  XMPConst._new(512, "XMPConst.SERIALIZE_EXACT_PACKET_LENGTH");
XMPConst.SERIALIZE_WRITE_ALIAS_COMMENTS =
  XMPConst._new(1024, "XMPConst.SERIALIZE_WRITE_ALIAS_COMMENTS");

delete XMPConst["_new"];

// XMPConst._test = function() {
// };

"XMPConst.jsx";
// EOF
