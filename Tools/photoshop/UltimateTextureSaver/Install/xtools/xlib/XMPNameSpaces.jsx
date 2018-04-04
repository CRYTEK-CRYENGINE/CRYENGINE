//
// XMPNameSpaces.jsx
//
// $Id: XMPNameSpaces.jsx,v 1.11 2010/10/26 22:43:10 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

//
// This class contains info about a specific namespace
//
XMPNameSpace = function() {
  var self = this;

  self.name = '';
  self.prefix = '';
  self.uri = '';
  self.properties = [];
};
XMPNameSpace.prototype.typename = "XMPNameSpace";

//
// Returns true if this namespace has the given property
//
XMPNameSpace.prototype.hasProperty = function(p) {
  var self = this;

  // fixed so that the check is case-insensitive

  var rc = self.properties.indexOf(p) != -1;
  if (!rc) {
    p = p.toLowerCase();

    for (var i = 0; i < self.properties.length; i++) {
      var np = self.properties;
      rc = np.toLowerCase() == p;
      if (rc) {
        break;
      }
    }
  }
  return rc;
};

//
// Returns true if this namespace has any properties
//
XMPNameSpace.prototype.hasProperties = function() {
  return this.properties && this.properties.length;
};


//
// This 'class' provides access to known namespaces
//
// Note that namespaces can be addressed like this:
//    var exifNS = XMPNameSpaces["exif"];
//
XMPNameSpaces = function() {
};

//
// Preload the list of namespace names in a prioritized order
//
XMPNameSpaces.NAMESPACE_NAMES =
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

XMPNameSpaces.addNSName = function(name) {
  var nsNames = XMPNameSpaces.NAMESPACE_NAMES;

  for (var i = 0; i < nsNames.length; i++) {
    if (name == nsNames[i]) {
      // we already have this one in the table
      return;
    }
  }
  nsNames.push(name);
};
XMPNameSpaces.getNameSpace = function(name) {
  return XMPNameSpaces[name];
};
XMPNameSpaces.getURI = function(prefix) {
  var ns = XMPNameSpaces[prefix];
  return ns ? ns.uri : undefined;
};

XMPNameSpaces.add = function(name, prefix, uri, properties) {
  var self = this;
  var argError = "Illegal argument - argument ";

  if (name.constructor != String) {
    Error.runtimeError(1242, argError + "1");
  }

  if (prefix.constructor != String) {
    Error.runtimeError(1243, argError + "2");
  }

  if (uri.constructor != String) {
    Error.runtimeError(1244, argError + "3");
  }

  if (properties && !(properties instanceof Array)) {
    Error.runtimeError(1245, argError + "4");
  }

  var ns = new XMPNameSpace();
  ns.name = name;
  ns.properties = properties;
  ns.prefix = prefix;
  ns.uri = uri;

  self[prefix] = ns;
  self[prefix.toLowerCase()] = ns;
  self[uri] = ns;

  XMPNameSpaces.addNSName(prefix);

  return ns;
};

// Details of common namespaces

XMPNameSpaces._init = function() {
  function nsAlias(str, redirect) {
    str = new String(str);
    str.redirect = redirect;
    return str;
  }

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
                     "ThumbnailsFormat",
                     "ThumbnailsHeight",
                     "ThumbnailsImage",
                     "ThumbnailsWidth",
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
                     "InstanceID",
                     "LastURL",      // deprecated
                     "ManagedFrom",
                     "Manager",
                     "ManagerVariant",
                     "ManageTo",
                     "ManageUI",
                     "RenditionClass",
                     "RenditionOf",  // deprecated
                     "RenditionParams",
                     "SaveID",       // deprecated
                     "VersionID",
                     "Versions",
                     ]);

  XMPNameSpaces.add("XAP Media Management",
                    "xapMM",
                    "http://ns.adobe.com/xap/1.0/mm/",
                    XMPNameSpaces.xmpMM.properties);


  XMPNameSpaces.add("XMP Basic Job Ticket",
                    "xmpBJ",
                    "http://ns.adobe.com/xap/1.0/bj/",
                    ["JobRef",
                     "JobRefId",
                     "JobRefName",
                     "JobRefUrl",
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
                    ["absPeakAudioFilePath",
                     "album",
                     "altTapeName",
                     "altTimecode",
                     "altTimecodeFormat",
                     "altTimecodeValue",
                     "artist",
                     "audioChannelType",
                     "audioCompressor",
                     "audioModDate",
                     "audioSampleRate",
                     "audioSampleType",
                     "beatSpliceParams",
                     "composer",
                     "contributedMedia",
                     "copyright",
                     "duration",
                     "engineer",
                     "fileDataRate",
                     "genre",
                     "instrument",
                     "introTime",
                     "key",
                     "logComment",
                     "loop",
                     "markers",
                     "metadataModDate",
                     "numberOfBeats",
                     "outCue",
                     "projectRef",
                     "pullDown",
                     "relativePeakAudioFilePath",
                     "relativeTimestamp",
                     "releaseDate",
                     "resampleParams",
                     "scaleType",
                     "scene",
                     "shotDate",
                     "shotLocation",
                     "shotName",
                     "speakerPlacement",
                     "startTimecode",
                     "stretchMode",
                     "tapeName",
                     "tempo",
                     "timeScaleParams",
                     "timeSignature",
                     "trackNumber",
                     "videoAlphaMode",
                     "videoAlphaPremultipleColor",
                     "videoAlphaUnityIsTransparent",
                     "videoColorSpace",
                     "videoCompressor",
                     "videoFieldOrder",
                     "videoFrameSize",
                     "videoModDate",
                     "videoPixelAspectRatio",
                     "videoPixelDepth",
                     "vidoeFrameRate",
                     ]);

  XMPNameSpaces.add("Adobe PDF",
                    "pdf",
                    "http://ns.adobe.com/pdf/1.3/",
                    ["Author",
                     "CreationDate",
                     "Creator",
                     "Keywords",
                     "ModDate",
                     "PDFVersion",
                     "Producer",
                     "Subject",
                     "Title",
                     "Trapped"
                     ]);

  XMPNameSpaces.add("Photoshop",
                    "photoshop",
                    "http://ns.adobe.com/photoshop/1.0/",
                    ["AuthorsPosition",
                     "CaptionWriter",
                     "Category",
                     "City",
                     "ColorMode",
                     "Country",
                     "Credit",
                     "DateCreated",
                     "Headline",
                     "History",
                     "ICCProfileName",
                     "Instructions",
                     "LegacyIPTCDigest",
                     "SidecarForExtension",
                     "Source",
                     "State",
                     "SupplementalCategories",
                     "TransmissionReference",
                     "Urgency",
                     ]);

  XMPNameSpaces.add("Camera Raw Settings",
                    "crs",
                    "http://ns.adobe.com/camera-raw-settings/1.0/",
                    ["AlreadyApplied",
                     "AutoBrightness",
                     "AutoContrast",
                     "AutoExposure",
                     "AutoShadows",
                     "BlueHue",
                     "BlueSaturation",
                     "Brightness",
                     "CameraProfile",
                     "CameraProfileDigest",
                     "ChromaticAberrationB",
                     "ChromaticAberrationR",
                     "Clarity",
                     "ColorNoiseReduction",
                     "ConvertToGrayScale",
                     "Contrast",
                     "CropAngle",
                     "CropBottom",
                     "CropHeight",
                     "CropLeft",
                     "CropRight",
                     "CropTop",
                     "CropUnits",   // 0=px,2=in,1=cm
                     "CropWidth",
                     "Defringe",
                     "Exposure",
                     "FillLight",
                     "GrayMixerAqua",
                     "GrayMixerBlue",
                     "GrayMixerGreen",
                     "GrayMixerMagenta",
                     "GrayMixerOrange",
                     "GrayMixerPurple",
                     "GrayMixerRed",
                     "GrayMixerYellow",
                     "GreenHue",
                     "GreenSaturation",
                     "HasCrop",
                     "HasSettings",
                     "HighlightRecovery",
                     "HueMixerAqua",
                     "HueMixerBlue",
                     "HueMixerGreen",
                     "HueMixerMagenta",
                     "HueMixerOrange",
                     "HueMixerPurple",
                     "HueMixerRed",
                     "HueMixerYellow",
                     "IncrementalTemperature",
                     "IncrementalTint",
                     "LuminanceMixerAqua",
                     "LuminanceMixerBlue",
                     "LuminanceMixerGreen",
                     "LuminanceMixerMagenta",
                     "LuminanceMixerOrange",
                     "LuminanceMixerPurple",
                     "LuminanceMixerRed",
                     "LuminanceMixerYellow",
                     "LuminanceSmoothing",
                     "ParametricDarks",
                     "ParametricHiglights",
                     "ParametricHiglightsSplit",
                     "ParametricLights",
                     "ParametricMidtoneSplit",
                     "ParametricShadows",
                     "ParametricShadowsSplit",
                     "RawFileName",
                     "RedEyeInfo",
                     "RedHue",
                     "RedSaturation",
                     "RetouchInfo",
                     "Saturation",
                     "SaturationMixerAqua",
                     "SaturationMixerBlue",
                     "SaturationMixerGreen",
                     "SaturationMixerMagenta",
                     "SaturationMixerOrange",
                     "SaturationMixerPurple",
                     "SaturationMixerRed",
                     "SaturationMixerYellow",
                     "Shadows",
                     "ShadowTint",
                     "SharpenDetail",
                     "SharpenEdgeMasking",
                     "SharpenRadius",
                     "Sharpness",
                     "SplitToningBalance",
                     "SplitToningHighlightHue",
                     "SplitToningHighlightSaturation",
                     "SplitToningShadowHue",
                     "SplitToningShadowSaturation",
                     "Temperature",
                     "Tint",
                     "ToneCurve",
                     "ToneCurveName",
                     // Custom,Linear,Medium Contrast,Strong Contrast
                     "Version",
                     "Vibrance",
                     "VignetteAmount",
                     "VignetteMidpoint",
                     "WhiteBalance",
                     // As Shot,Auto,Cloudy,Custom,Daylight,Flash,Fluorescent,
                     // Shade,Tungsten
                     ]);

  // EXIF Schemas
  XMPNameSpaces.add("TIFF",
                    "tiff",
                    "http://ns.adobe.com/tiff/1.0/",
                    ["Artist",
                     "BitsPerSample",
                     "Compression",              //...
                     "Copyright",
                     "DateTime",
                     "ImageDescription",
                     "ImageLength",
                     "ImageHeight",
                     "ImageWidth",
                     "Make",
                     "Model",
                     "NativeDigest",
                     "Orientation",                //...
                     "PhotometricInterpretation",  //...
                     "PlanarConfiguration",        // 1=Chunky,2=Planar
                     "PrimaryChromaticities",
                     "ReferenceBlackWhite",
                     "ResolutionUnit",             // 1=None,2=in,3=cm
                     "SamplesPerPixel",
                     "Software",
                     "TransferFunction",
                     "WhitePoint",
                     "XResolution",
                     "YCbCrCoefficients",
                     "YCbCrPositioning",            // 1=Centered,2=Co-sited
                     "YCbCrSubSampling",            //...
                     "YResolution",
                     ]);

  XMPNameSpaces.add("EXIF",
                    "exif",
                    "http://ns.adobe.com/exif/1.0/",
                    ["ApetureValue",
                     "BrightnessValue",
                     "CFAPattern",
                     "CFAPatternColumns",
                     "CFAPatternRows",
                     "CFAPatternValues",
                     "ColorSpace",
                     // 1=sRGB,2=Adobe RGB,65535=Uncalibrated,4294967295=Uncalibrated
                     "ComponentsConfiguration", //...
                     "CompressedBitsPerPixel",
                     "Constrast",      // 0=Normal,1=Low,2=High
                     "CustomRendered", // 0=Normal,1=Custom
                     "DateTimeDigitized",
                     "DateTimeOriginal",
                     "DeviceSettingDescription",
                     "DeviceSettingDescriptionColumns",
                     "DeviceSettingDescriptionRows",
                     "DeviceSettingDescriptionSettings",
                     "DigitalZoomRatio",
                     "ExifVersion",
                     "ExposureCompenstation",
                     "ExposureBiasValue",
                     "ExposureIndex",
                     "ExposureMode", // 0=Auto,1=Manual,2=Auto bracket
                     "ExposureProgram", //...
                     "ExposureTime",
                     "FileSource",
                     // 1=Film Scanner,2=Reflection Print Scanner,3=Digital Camera
                     "Flash",
                     "FlashEnergy",
                     "FlashFired",
                     "FlashMode",    // 0=Unknown,1=On,2=Off,3=Auto
                     "FlashpixVersion",
                     "FlashRedEyeMode",
                     "FlashReturn",  //...
                     "FNumber",
                     "FocalLength",
                     "FocalLengthIn35mmFilm",
                     "FocalPlaneResolutionUnit",
                     "FocalPlaneXResolution",
                     "FocalPlaneYResolution",
                     "GainControl",  //...
                     "GPSAltitude",
                     "GPSAltitudeRef", // 0=Above Sea Level,1=Below Sea Level
                     "GPSAreaInformation",
                     "GPSDestBearing",
                     "GPSDestBearingRef",
                     "GPSDestDistance",
                     "GPSDestDistanceRef",
                     "GPSDestLatitude",
                     "GPSDestLongitude",
                     "GPSDifferential", // 0=No Correction,1=Differential Correction
                     "GPSDOP",
                     "GPSImgDirection",
                     "GPSImgDirectionRef",  // M=Magnetic North,T=True North
                     "GPSLatitude",
                     "GPSLongitude",
                     "GPSMapDatum",
                     "GPSMeasureMode",      // 2=2-Dimensional,3=3-Dimensional
                     "GPSProcessingMethod",
                     "GPSSatellites",
                     "GPSSpeed",
                     "GPSSpeedRef",         // K=km/h,M=mph,N=knots
                     "GPSStatus",     // A=Measurment Active,V=Measurment Void
                     "GPSTimeStamp",
                     // "GPSDateTime",  // see exiftool
                     "GPSTrack",
                     "GPSTrackRef",     // M=Magnetic North,T=True North
                     "GPSVersionID",
                     "ImageUniqueID",
                     "ISO",
                     "ISOSpeedRatings",
                     "LightSource",      //...
                     "MakerNote",
                     "MaxApetureValue",
                     "MeteringMode",     //...
                     "NativeDigest",
                     "OECF",
                     "OECFColumns",
                     "OECFNames",
                     "OECFRows",
                     "PixelXDimension",
                     // "ExifImageWidth", // see exiftool
                     "PixelYDimension",
                     // "ExifImageHeight", // see exiftool
                     "RelatedSoundFile",
                     "Saturation",         // 0=Normal,1=Low,2=High
                     "SceneCaptureType",   //...
                     "SceneType",          // 1=Directly Photographed
                     "SensingMethod",      //...
                     "Sharpness",          // 0=Normal,1=Soft,2=Hard
                     "ShutterSpeedValue",
                     "SpatialFrequencyResponse",
                     "SpatialFrequencyResponseColumns",
                     "SpatialFrequencyResponseNames",
                     "SpatialFrequencyResponseRows",
                     "SpatialFrequencyResponseValues",
                     "SpectralSensitivity",
                     "SubjectArea",
                     "SubjectDistance",
                     "SubjectDistanceRange",  //...
                     "SubjectLocation",
                     "UserComment",
                     "WhiteBalance",   // 0=Auto,1=Manual
                     ]);

  XMPNameSpaces.add("EXIF AUX",
                    "aux",
                    "http://ns.adobe.com/exif/1.0/aux/",
                    ["Firmware",
                     "FlashCompensation",
                     "ImageNumber",
                     "Lens",
                     "LensID",
                     "LensInfo",
                     "OwnerName",
                     "SerialNumber",
                     "Shutter",
                     ]);

  XMPNameSpaces.add("IPTC Core",
                    "Iptc4xmpCore",
                    "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                    [nsAlias("City", "photoshop:City"),
                     nsAlias("CopyrightNotice", "dc:rights"),
                     nsAlias("Country", "photoshop:Country"),
                     "CountryCode",
                     nsAlias("Creator", "dc:creator"),
                     "CreatorContactInfo",
                     nsAlias("CreatorJobTitle", "photoshop:AuthorsPosition"),
                     nsAlias("DateCreated", "photoshop:DateCreated"),
                     nsAlias("Description", "dc:description"),
                     nsAlias("DescriptionWriter", "photoshop:CaptionWriter"),
                     nsAlias("Headline", "photoshop:Headline"),
                     nsAlias("Instructions", "photoshop:instructions"),
                     "IntellectualGenre",
                     nsAlias("JobID", "photoshop:TransmissionReference"),
                     nsAlias("Keywords", "dc:subject"),
                     "Location",
                     nsAlias("Provider", "photoshop:Credit"),
                     nsAlias("Province-State", "photoshop:State"),
                     nsAlias("RightsUsageTerms", "xmpRights:UsageTerms"),
                     "Scene",
                     nsAlias("Source", "photoshop:Source"),
                     "SubjectCode",
                     nsAlias("Title", "dc:title"),
                     ]);

  XMPNameSpaces.add("IPTC Core",
                    "iptcCore",
                    "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
                    XMPNameSpaces.Iptc4xmpCore.properties);

  XMPNameSpaces.add("Creative Commons",
                    "cc",
                    "http://creativecommons.org/ns#",
                    ["License"]);

  XMPNameSpaces.add("Lightroom",
                    "lr",
                    "http://ns.adobe.com/lightroom/1.0/",
                    ["hierachicalSubject",
                     "privateRTKInfo"]);

  XMPNameSpaces.add("iView MediaPro",
                    "mediapro",
                    "http://ns.iview-multimedia.com/mediapro/1.0/",
                    ["CatalogSets",
                     "Event",
                     "Location",
                     "People",
                     "Status",
                     "UserFields"]);

  XMPNameSpaces.add("Microsoft Photo",
                    "MicrosoftPhoto",
                    "http://ns.microsoft.com/photo/1.0/",
                    ["CameraSerialNumber",
                     "DateAcquired",
                     "FlashManufacturer",
                     "FlashModel",
                     "LastKeywordIPTC",
                     "LastKeywordXMP",
                     "LensManufacturer",
                     "LensModel",
                     "RatingsPercent"]);

  String.prototype.startsWith = function(sub) {
    return this.indexOf(sub) == 0;
  };

};

XMPNameSpaces._init();


//
// Give a name like "tiff:Artist", "Artist"
//    return http://ns.adobe.com/tiff/1.0/Artist
// The property name is case insensitive
//
XMPNameSpaces.getCanonicalName = function(name) {
  function _ftn(uri, lname) {
    return uri + lname;
  }

  return XMPNameSpaces._getName(name, _ftn);
};

//
// Give a name like "tiff:Artist", "Artist"
//    return the QName for http://ns.adobe.com/tiff/1.0/, Artist
// The property name is case insensitive
//
XMPNameSpaces.getQName = function(name) {
  function _ftn(uri, lname) {
    return new QName(uri, lname);
  }

  return XMPNameSpaces._getName(name, _ftn);
};

XMPNameSpaces.convertQName = function(qname) {
  var ns = XMPNameSpaces[qname.uri];
  return (ns ? (ns.prefix + ':' + qname.localName) : undefined);
};

XMPNameSpaces._getName = function(name, _ftn) {
  if (name instanceof QName) {
    return _ftn(name.uri, name.localName);
  }

  if (name.startsWith("http://")) {
    return name;
  }

  var qname = undefined;
  var parts = name.split(':');
  var prefixes;
  var property;

  if (parts.length == 2) {
    prefixes = [parts[0]];
    property = parts[1];

  } else {
    prefixes = XMPNameSpaces.NAMESPACE_NAMES;
    property = name;
  }

  for (var i = 0; i < prefixes.length; i++) {
    var prefix = prefixes[i];

    var ns = XMPNameSpaces[prefix];

    if (!ns) {
      return undefined;
    }

    var redirect = undefined;
    var pname = undefined;
    var props = ns.properties;

    for (var j = 0; j < ns.properties.length; j++) {
      pname = props[j];

      if (pname == property) {
        break;
      }

      if (pname.toLowerCase() == property.toLowerCase()) {
        break;
      }

      pname = undefined;
    }

    if (!pname && prefixes.length == 1) {
      return undefined;
    }

    // We didn't find the property in this namespace,
    // so try the next
    if (!pname) {
      continue;
    }

    redirect = pname.redirect;

    // if it's a redirect, chase that down
    if (redirect) {
      qname = XMPNameSpaces._getName(redirect, _ftn);

    } else {
      // if not, were done
      qname = _ftn(ns.uri, pname);
    }
    break;
  }

  return qname;
};

// XXX reimplement to use getNameSpaceWithProperty and return the first value...
XMPNameSpaces.getNameSpaceWithProperty = function(property) {
  var prefixes = XMPNameSpaces.NAMESPACE_NAMES;
  var namespace = undefined;

  for (var i = 0; i < prefixes.length; i++) {
    var prefix = prefixes[i];

    var ns = XMPNameSpaces[prefix];

    if (!ns) {
      return undefined;
    }

    var redirect = undefined;
    var pname = undefined;
    var props = ns.properties;

    for (var j = 0; j < ns.properties.length; j++) {
      pname = props[j];

      if (pname == property) {
        break;
      }

      if (pname.toLowerCase() == property.toLowerCase()) {
        break;
      }

      pname = undefined;
    }

    // If we find the property in this namespace,
    if (pname) {
      var redirect = pname.redirect;

      // if it's a redirect, chase that down
      if (redirect) {
        ns = XMPNameSpaces.getNameSpaceWithProperty(redirect, _ftn);
      }

      return ns;
    }
  }

  return namespace;
};

XMPNameSpaces.getNameSpacesWithProperty = function(property) {
  var prefixes = XMPNameSpaces.NAMESPACE_NAMES;
  var namespaces = undefined;

  for (var i = 0; i < prefixes.length; i++) {
    var prefix = prefixes[i];

    var ns = XMPNameSpaces[prefix];

    if (!ns) {
      return undefined;
    }

    var redirect = undefined;
    var pname = undefined;
    var props = ns.properties;

    for (var j = 0; j < ns.properties.length; j++) {
      pname = props[j];

      if (pname == property) {
        break;
      }

      if (pname.toLowerCase() == property.toLowerCase()) {
        break;
      }

      pname = undefined;
    }

    // If we find the property in this namespace,
    if (pname) {
      var redirect = pname.redirect;

      // if it's a redirect, chase that down
      if (redirect) {
        ns = XMPNameSpaces.getNameSpaceWithProperty(redirect, _ftn);
      }

      namespaces.push(ns);
    }
  }

  return namespaces;
};

XMPNameSpaces._test = function() {
  var props = ["iptcCore:City",
               "iptcCore:city",
               "Artist",
               "artist",
               "City",
               "tiff:Artist",
               "tiff:artist",
               "http://ns.adobe.com/exif/1.0/aux/Shutter",
               "exif:badProperty",
               "badProperty"
               ];

  var str = '';
  for (var i = 0; i < props.length; i++) {
    var p = props[i];
    var cname = XMPNameSpaces.getCanonicalName(p) || "[unknown]";
    str += p + " = " + cname + '\r';
  }
  alert(str);
};

"XMPNameSpace.jsx";
// EOF
