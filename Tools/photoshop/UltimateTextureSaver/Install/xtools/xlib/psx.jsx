//
// psx.jsx
//   This file contains a collection code extracted from other parts
//   of xtools for use in production scripts written for Adobe.
//
// $Id: psx.jsx,v 1.80 2014/12/15 20:26:17 anonymous Exp $
// Copyright: (c)2011, xbytor
// Author: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

//
// cTID and sTID are wrappers for standard PS ID conversion functions.
// They require fewer keystrokes and are easier to read.
// Their implementations cache the calls to the underlying PS DOM
// functions making them fractionally faster than the underlying functions
// in some boundary cases.
//
cTID = function(s) { return cTID[s] || (cTID[s] = app.charIDToTypeID(s)); };
sTID = function(s) { return sTID[s] || (sTID[s] = app.stringIDToTypeID(s)); };

// return an ID for whatever s might be
xTID = function(s) {
  if (s.constructor == Number) {
    return s;
  }
  try {
    if (s instanceof XML) {
      var k = s.nodeKind();
      if (k == 'text' || k == 'attribute') {
        s = s.toString();
      }
    }
  } catch (e) {
  }

  if (s.constructor == String) {
    if (s.length > 0) {
      if (s.length != 4) return sTID(s);
      try { return cTID(s); } catch (e) { return sTID(s); }
    }
  }
  Error.runtimeError(19, s);  // Bad Argument

  return undefined;
};

//
// Convert a 32 bit ID back to either a 4 character representation or the
// mapped string representation.
//
id2char = function(s) {
  if (isNaN(Number(s))){
    return '';
  }
  var v;

  var lvl = $.level;
  $.level = 0;
  try {
    if (!v) {
      try { v = app.typeIDToCharID(s); } catch (e) {}
    }
    if (!v) {
      try { v = app.typeIDToStringID(s); } catch (e) {}
    }
  } catch (e) {
  }
  $.level = lvl;

  if (!v) {
    // neither of the builtin PS functions know about this ID so we
    // force the matter
    v = psx.numberToAscii(s);
  }

  return v ? v : s;
};


//
// What platform are we on?
//
isWindows = function() { return $.os.match(/windows/i); };
isMac = function() { return !isWindows(); };

//
// Which app are we running in?
//
isPhotoshop = function() { return !!app.name.match(/photoshop/i); };
isBridge = function() { return !!app.name.match(/bridge/i); };

//
// Which CS version is this?
//
CSVersion = function() {
  var rev = Number(app.version.match(/^\d+/)[0]);
  return isPhotoshop() ? (rev - 7) : rev;
};
CSVersion._version = CSVersion();

// not happy about the CS7+ definitions
isCC2014 = function()  { return CSVersion._version == 8; }; 
isCC  = function()  { return CSVersion._version == 7; }; 
isCS7 = function()  { return CSVersion._version == 7; };
isCS6 = function()  { return CSVersion._version == 6; };
isCS5 = function()  { return CSVersion._version == 5; };
isCS4 = function()  { return CSVersion._version == 4; };
isCS3 = function()  { return CSVersion._version == 3; };
isCS2 = function()  { return CSVersion._version == 2; };
isCS  = function()  { return CSVersion._version == 1; };

//
// ZStrs is a container for (mostly) localized strings used in psx
// or elsewhere
//
try {
  var _lvl = $.level;
  $.level = 0;
  ZStrs;
} catch (e) {
  ZStrs = {};
} finally {
  $.level = _lvl;
  delete _lvl;
}

ZStrs.SelectFolder = 
  localize("$$$/JavaScripts/psx/SelectFolder=Select a folder");

ZStrs.SelectFile = 
  localize("$$$/JavaScripts/psx/SelectFile=Select a file");

ZStrs.FileErrorStr = 
  localize("$$$/JavaScripts/psx/FileError=File Error: ");

ZStrs.BadFileSpecified = 
  localize("$$$/JavaScripts/psx/BadFileSpecified=Bad file specified");

ZStrs.UnableToOpenLogFile =
  localize("$$$/JavaScripts/psx/UnableToOpenLogFile=Unable to open log file %%s : %%s");

ZStrs.UnableToWriteLogFile =
  localize("$$$/JavaScripts/psx/UnableToWriteLogFile=Unable to write to log file %%s : %%s");

ZStrs.UnableToOpenFile =
  localize("$$$/JavaScripts/psx/UnableToOpenFile=Unable to open file");

ZStrs.UnableToOpenInputFile =
  localize("$$$/JavaScripts/psx/UnableToOpenInputFile=Unable to open input file");

ZStrs.UnableToOpenOutputFile =
  localize("$$$/JavaScripts/psx/UnableToOpenOutputFile=Unable to open output file");

ZStrs.CharacterConversionError =
  localize("$$$/JavaScripts/psx/CharacterConversionError=Probable character conversion error");

// need to break up the ZString prefix to avoid
// the ZString harvester
ZStrs.InstalledScripts = 
  localize('$' + '$' + '$' + '/' +
           (isCS6() ? "private/" : "") +
           "ScriptingSupport/InstalledScripts=Presets/Scripts");

ZStrs.DocumentName =
  localize("$$$/JavaScripts/psx/DocumentName=Document Name");

ZStrs.LCDocumentName =
  localize("$$$/JavaScripts/psx/LCDocumentName=document name");

ZStrs.UCDocumentName =
  localize("$$$/JavaScripts/psx/UCDocumentName=DOCUMENT NAME");

ZStrs.FN1Digit =
  localize("$$$/JavaScripts/psx/FN1Digit=1 Digit Serial Number");

ZStrs.FN2Digit =
  localize("$$$/JavaScripts/psx/FN2Digit=2 Digit Serial Number");

ZStrs.FN3Digit =
  localize("$$$/JavaScripts/psx/FN3Digit=3 Digit Serial Number");

ZStrs.FN4Digit =
  localize("$$$/JavaScripts/psx/FN4Digit=4 Digit Serial Number");

ZStrs.FN5Digit =
  localize("$$$/JavaScripts/psx/FN5Digit=5 Digit Serial Number");

ZStrs.LCSerial =
  localize("$$$/JavaScripts/psx/LCSerial=Serial Letter (a, b, c...)");

ZStrs.UCSerial =
  localize("$$$/JavaScripts/psx/UCSerial=Serial Letter (A, B, C...)");

ZStrs.Date_mmddyy =
  localize("$$$/JavaScripts/psx/Date/mmddyy=mmddyy (date)");

ZStrs.Date_mmdd =
  localize("$$$/JavaScripts/psx/Date/mmdd=mmdd (date)");

ZStrs.Date_yyyymmdd =
  localize("$$$/JavaScripts/psx/Date/yyyymmdd=yyyymmdd (date)");

ZStrs.Date_yymmdd =
  localize("$$$/JavaScripts/psx/Date/yymmdd=yymmdd (date)");

ZStrs.Date_yyddmm =
  localize("$$$/JavaScripts/psx/Date/yyddmm=yyddmm (date)");

ZStrs.Date_ddmmyy =
  localize("$$$/JavaScripts/psx/Date/ddmmyy=ddmmyy (date)");

ZStrs.Date_ddmm =
  localize("$$$/JavaScripts/psx/Date/ddmm=ddmm (date)");

ZStrs.Extension =
  localize("$$$/JavaScripts/psx/Extension=Extension");

ZStrs.LCExtension =
  localize("$$$/JavaScripts/psx/LCextension=extension");

ZStrs.UCExtension =
  localize("$$$/JavaScripts/psx/UCextension=EXTENSION");

ZStrs.FileNaming =
  localize("$$$/JavaScripts/psx/FileNaming=File Naming");

ZStrs.ExampleLabel =
  localize("$$$/JavaScripts/psx/ExampleLabel=Example:");

ZStrs.StartingSerialNumber =
  localize("$$$/JavaScripts/psx/StartingSerialNumber=Starting Serial #:");

ZStrs.CompatibilityPrompt =
  localize("$$$/JavaScripts/psx/CompatibilityPrompt=Compatibilty:");

ZStrs.Windows =
  localize("$$$/JavaScripts/psx/Windows=Windows");

ZStrs.MacOS =
  localize("$$$/JavaScripts/psx/MacOS=MacOS");

ZStrs.Unix =
  localize("$$$/JavaScripts/psx/Unix=Unix");

ZStrs.CustomTextEditor =
  localize("$$$/JavaScripts/psx/CustomTextEditor=Custom Text Editor");

ZStrs.CreateCustomText =
  localize("$$$/JavaScripts/psx/CreateCustomText=Create Custom Text");

ZStrs.EditCustomText =
  localize("$$$/JavaScripts/psx/EditCustomText=Edit Custom Text");

ZStrs.DeleteCustomText =
  localize("$$$/JavaScripts/psx/DeleteCustomText=Delete Custom Text");

ZStrs.DeleteCustomTextPrompt =
  localize("$$$/JavaScripts/psx/DeleteCustomTextPrompt=Do you really want to remove %%s?");

ZStrs.CustomTextPrompt = 
  localize("$$$/JavaScripts/psx/CustomTextPrompt=Please enter the desired Custom Text: ");

ZStrs.Cancel = 
  localize("$$$/JavaScripts/psx/Cancel=Cancel");

ZStrs.Save = 
  localize("$$$/JavaScripts/psx/Save=Save");

ZStrs.SameAsSource = 
  localize("$$$/JavaScripts/psx/SameAsSource=Same as Source");

ZStrs.UserCancelled = 
  localize("$$$/ScriptingSupport/Error/UserCancelled=User cancelled the operation");

// Units
ZStrs.UnitsPX = 
  localize("$$$/UnitSuffixes/Short/Px=px");

ZStrs.UnitsIN = 
  localize("$$$/UnitSuffixes/Short/In=in");

ZStrs.Units_IN = 
  localize("$$$/UnitSuffixes/Short/IN=in");

ZStrs.UnitsCM = 
  localize("$$$/UnitSuffixes/Short/Cm=cm");

ZStrs.Units_CM = 
  localize("$$$/UnitSuffixes/Short/CM=cm");

ZStrs.UnitsMM = 
  localize("$$$/UnitSuffixes/Short/MM=mm");

ZStrs.UnitsPercent = 
  localize("$$$/UnitSuffixes/Short/Percent=%");

ZStrs.UnitsPica = 
  localize("$$$/UnitSuffixes/Short/Pica=pica");

ZStrs.UnitsPT =
  localize("$$$/UnitSuffixes/Short/Pt=pt");

ZStrs.UnitsShortCM =
  localize("$$$/UnitSuffixes/Short/CM=cm");

ZStrs.UnitsShortIn =
  localize("$$$/UnitSuffixes/Short/In=in");

ZStrs.UnitsShortIN =
  localize("$$$/UnitSuffixes/Short/IN=in");

ZStrs.UnitsShortMM = 
  localize("$$$/UnitSuffixes/Short/MM=mm");

ZStrs.UnitsShortPercent = 
  localize("$$$/UnitSuffixes/Short/Percent=%");

ZStrs.UnitsShortPica = 
  localize("$$$/UnitSuffixes/Short/Pica=pica");

ZStrs.UnitsShortPT =
  localize("$$$/UnitSuffixes/Short/Pt=pt");

ZStrs.UnitsShortPx = 
  localize("$$$/UnitSuffixes/Short/Px=px");

ZStrs.UnitsShortMMs = 
  localize("$$$/UnitSuffixes/Short/MMs=mm");

ZStrs.UnitsShortPluralCMS =
  localize("$$$/UnitSuffixes/ShortPlural/CMS=cms");

ZStrs.UnitsShortPluralIns =
  localize("$$$/UnitSuffixes/ShortPlural/Ins=ins");

ZStrs.UnitsShortPluralPercent =
  localize("$$$/UnitSuffixes/ShortPlural/Percent=%");

ZStrs.UnitsShortPluralPicas =
  localize("$$$/UnitSuffixes/ShortPlural/Picas=picas");

ZStrs.UnitsShortPluralPts =
  localize("$$$/UnitSuffixes/ShortPlural/Pts=pts");

ZStrs.UnitsShortPluralPx =
  localize("$$$/UnitSuffixes/ShortPlural/Px=px");

ZStrs.UnitsVerboseCentimeter =
  localize("$$$/UnitSuffixes/Verbose/Centimeter=centimeter");

ZStrs.UnitsVerboseInch =
  localize("$$$/UnitSuffixes/Verbose/Inch=inch");

ZStrs.UnitsVerboseMillimeter =
  localize("$$$/UnitSuffixes/Verbose/Millimeter=millimeter");

ZStrs.UnitsVerbosePercent =
  localize("$$$/UnitSuffixes/Verbose/Percent=percent");

ZStrs.UnitsVerbosePica =
  localize("$$$/UnitSuffixes/Verbose/Pica=pica");

ZStrs.UnitsVerbosePixel =
  localize("$$$/UnitSuffixes/Verbose/Pixel=pixel");

ZStrs.UnitsVerbosePoint =
  localize("$$$/UnitSuffixes/Verbose/Point=point");

ZStrs.UnitsVerbosePluralCentimeters =
  localize("$$$/UnitSuffixes/VerbosePlural/Centimeters=Centimeters");

ZStrs.UnitsVerbosePluralInches =
  localize("$$$/UnitSuffixes/VerbosePlural/Inches=Inches");

ZStrs.UnitsVerbosePluralMillimeters =
  localize("$$$/UnitSuffixes/VerbosePlural/Millimeters=Millimeters");

ZStrs.UnitsVerbosePluralPercent =
  localize("$$$/UnitSuffixes/VerbosePlural/Percent=Percent");

ZStrs.UnitsVerbosePluralPicas =
  localize("$$$/UnitSuffixes/VerbosePlural/Picas=Picas");

ZStrs.UnitsVerbosePluralPixels =
  localize("$$$/UnitSuffixes/VerbosePlural/Pixels=Pixels");

ZStrs.UnitsVerbosePluralPoints =
  localize("$$$/UnitSuffixes/VerbosePlural/Points=Points");

ZStrs.FontLabel =
  localize("$$$/JavaScripts/psx/FontLabel=Font:");

ZStrs.FontTip =
  localize("$$$/JavaScripts/psx/FontTip=Select the font");

ZStrs.FontStyleTip =
  localize("$$$/JavaScripts/psx/FontStyleTip=Select the font style");

ZStrs.FontSizeTip =
  localize("$$$/JavaScripts/psx/FontSizeTip=Select the font size");


//
// Colors
//
ZStrs.black =
  localize("$$$/Actions/Enum/Black=black");
ZStrs.white =
  localize("$$$/Actions/Enum/White=white");
ZStrs.foreground =
  localize("$$$/JavaScripts/psx/Color/foreground=foreground");
ZStrs.background =
  localize("$$$/Actions/Enum/Background=background");
ZStrs.gray =
  localize("$$$/Actions/Enum/Gray=gray");
ZStrs.grey =
  localize("$$$/JavaScripts/psx/Color/grey=grey");
ZStrs.red =
  localize("$$$/Actions/Enum/Red=red");
ZStrs.green =
  localize("$$$/Actions/Enum/Green=green");
ZStrs.blue =
  localize("$$$/Actions/Enum/Blue=blue");

//
// Days of the week
//
ZStrs.Monday = 
  localize("$$$/JavaScripts/psx/Date/Monday=Monday");
ZStrs.Mon = 
  localize("$$$/JavaScripts/psx/Date/Mon=Mon");
ZStrs.Tuesday =
  localize("$$$/JavaScripts/psx/Date/Tuesday=Tuesday");
ZStrs.Tue =
  localize("$$$/JavaScripts/psx/Date/Tue=Tue");
ZStrs.Wednesday =
  localize("$$$/JavaScripts/psx/Date/Wednesday=Wednesday");
ZStrs.Wed =
  localize("$$$/JavaScripts/psx/Date/Wed=Wed");
ZStrs.Thursday =
  localize("$$$/JavaScripts/psx/Date/Thursday=Thursday");
ZStrs.Thu =
  localize("$$$/JavaScripts/psx/Date/Thu=Thu");
ZStrs.Friday =
  localize("$$$/JavaScripts/psx/Date/Friday=Friday");
ZStrs.Fri =
  localize("$$$/JavaScripts/psx/Date/Fri=Fri");
ZStrs.Saturday =
  localize("$$$/JavaScripts/psx/Date/Saturday=Saturday");
ZStrs.Sat =
  localize("$$$/JavaScripts/psx/Date/Sat=Sat");
ZStrs.Sunday =
  localize("$$$/JavaScripts/psx/Date/Sunday=Sunday");
ZStrs.Sun =
  localize("$$$/JavaScripts/psx/Date/Sun=Sun");


//
// Months
//
ZStrs.January =
  localize("$$$/JavaScripts/psx/Date/January=January");
ZStrs.Jan =
  localize("$$$/JavaScripts/psx/Date/Jan=Jan");
ZStrs.February =
  localize("$$$/JavaScripts/psx/Date/February=February");
ZStrs.Feb =
  localize("$$$/JavaScripts/psx/Date/Feb=Feb");
ZStrs.March =
  localize("$$$/JavaScripts/psx/Date/March=March");
ZStrs.Mar =
  localize("$$$/JavaScripts/psx/Date/Mar=Mar");
ZStrs.April =
  localize("$$$/JavaScripts/psx/Date/April=April");
ZStrs.Apr =
  localize("$$$/JavaScripts/psx/Date/Apr=Apr");
ZStrs.May =
  localize("$$$/JavaScripts/psx/Date/May=May");
ZStrs.June =
  localize("$$$/JavaScripts/psx/Date/June=June");
ZStrs.Jun =
  localize("$$$/JavaScripts/psx/Date/Jun=Jun");
ZStrs.July =
  localize("$$$/JavaScripts/psx/Date/July=July");
ZStrs.Jul =
  localize("$$$/JavaScripts/psx/Date/Jul=Jul");
ZStrs.August =
  localize("$$$/JavaScripts/psx/Date/August=August");
ZStrs.Aug =
  localize("$$$/JavaScripts/psx/Date/Aug=Aug");
ZStrs.September =
  localize("$$$/JavaScripts/psx/Date/September=September");
ZStrs.Sep =
  localize("$$$/JavaScripts/psx/Date/Sep=Sep");
ZStrs.October =
  localize("$$$/JavaScripts/psx/Date/October=October");
ZStrs.Oct =
  localize("$$$/JavaScripts/psx/Date/Oct=Oct");
ZStrs.November =
  localize("$$$/JavaScripts/psx/Date/November=November");
ZStrs.Nov =
  localize("$$$/JavaScripts/psx/Date/Nov=Nov");
ZStrs.December =
  localize("$$$/JavaScripts/psx/Date/December=December");
ZStrs.Dec =
  localize("$$$/JavaScripts/psx/Date/Dec=Dec");

ZStrs.AM =
  localize("$$$/JavaScripts/psx/Date/AM=AM");
ZStrs.PM =
  localize("$$$/JavaScripts/psx/Date/PM=PM");

//
// Color Profiles
//
ZStrs.ProfileAdobeRGB = 
  localize("$$$/Menu/Primaries/AdobeRGB1998=Adobe RGB (1998)");

ZStrs.ProfileAppleRGB = 
  localize("$$$/Actions/Enum/AppleRGB=Apple RGB");

ZStrs.ProfileProPhotoRGB = 
  localize("$$$/JavaScripts/ContactSheet2/Profile/ProPhotoRGB=ProPhoto RGB");

ZStrs.ProfileSRGB = 
  localize("$$$/JavaScripts/ContactSheet2/Profile/sRGB=sRGB IEC61966-2.1");

ZStrs.ProfileColorMatchRGB = 
  localize("$$$/Actions/Enum/ColorMatch=ColorMatch RGB");

ZStrs.ProfileWideGamutRGB = 
  localize("$$$/Actions/Enum/WideGamut=Wide Gamut RGB");

ZStrs.ProfileLab = 
  localize("$$$/Actions/Enum/Lab=Lab");

// tpr not used
ZStrs.ProfileWorkingCMYK = 
  localize("$$$/Actions/Key/ColorSettings/WorkingCMYK=Working CMYK");

// tpr not used
ZStrs.ProfileWorkingGray = 
  localize("$$$/Actions/Key/ColorSettings/WorkingGray=Working Gray");

// tpr not used
ZStrs.ProfileWorkingRGB = 
  localize("$$$/Actions/Key/ColorSettings/WorkingRGB=Working RGB");

//
// Color Modes
//
ZStrs.CMYKMode =
  localize("$$$/Menu/ModePopup/CMYKColor=CMYK Color");

ZStrs.GrayscaleMode =
  localize("$$$/Menu/ModePopup/Grayscale=Grayscale");

ZStrs.LabMode =
  localize("$$$/Menu/ModePopup/LabColor=Lab Color");

ZStrs.RGBMode =
  localize("$$$/Menu/ModePopup/RGBColor=RGB Color");

//
// psx works as a namespace for commonly used functions
//
psx = function() {};

// If IOEXCEPTIONS_ENABLED is true, psx File I/O operations
// perform strict error checking and throw IO_ERROR_CODE exceptions
// when errors are detected
psx.IOEXCEPTIONS_ENABLED = true;

// Generic psx error number
psx.ERROR_CODE = 9001;

// File IO error number used by psx functions
psx.IO_ERROR_CODE = 9002;

//
// Convert a 4 byte number back to a 4 character ASCII string.
//
psx.numberToAscii = function(n) {
  if (isNaN(n)) {
    return n;
  }
  var str = (String.fromCharCode(n >> 24) +
             String.fromCharCode((n >> 16) & 0xFF) +
             String.fromCharCode((n >> 8) & 0xFF) +
             String.fromCharCode(n & 0xFF));

  return (psx.isAscii(str[0]) && psx.isAscii(str[1]) &&
          psx.isAscii(str[2]) && psx.isAscii(str[3])) ? str : n;
};

//
// Character types...
//
psx.ASCII_SPECIAL = "\r\n !\"#$%&'()*+,-./:;<=>?@[\]^_`{|}~";
psx.isSpecialChar = function(c) {
  return psx.ASCII_SPECIAL.contains(c[0]);
};
psx.isAscii = function(c) {
  return !!(c.match(/[\w\s]/) || psx.isSpecialChar(c));
};

//
// Define mappings between localized UnitValue type strings and strings
// acceptable to UnitValue constructors
//
psx._units = undefined;
psx._unitsInit = function() {
  if (!isPhotoshop()) {
    return;
  }
  psx._units = app.preferences.rulerUnits.toString();

  // map ruler units to localized strings
  psx._unitMap = {};
  psx._unitMap[Units.CM.toString()] =      ZStrs.UnitsCM;
  psx._unitMap[Units.INCHES.toString()] =  ZStrs.UnitsIN;
  psx._unitMap[Units.MM.toString()] =      ZStrs.UnitsMM;
  psx._unitMap[Units.PERCENT.toString()] = ZStrs.UnitsPercent;
  psx._unitMap[Units.PICAS.toString()] =   ZStrs.UnitsPica;
  psx._unitMap[Units.PIXELS.toString()] =  ZStrs.UnitsPX;
  psx._unitMap[Units.POINTS.toString()] =  ZStrs.UnitsPT;

  // since these are only used for construction UnitValue objects
  // don't bother with plural or verbose variants
  psx._unitStrMap = {};
  psx._reverseMap = {};

  function _addEntry(local, en) {
    psx._unitStrMap[local] = en;
    psx._unitStrMap[local.toLowerCase()] = en;
    psx._reverseMap[en.toLowerCase()] = local;
  }

  _addEntry(ZStrs.UnitsCM, "cm");
  _addEntry(ZStrs.UnitsShortCM, "cm");
  // _addEntry(ZStrs.UnitsShortPluralCMS, "cm");
  _addEntry(ZStrs.UnitsVerboseCentimeter, "centimeter");
  _addEntry(ZStrs.UnitsVerbosePluralCentimeters, "Centimeters");

  _addEntry(ZStrs.UnitsIN, "in");
  _addEntry(ZStrs.UnitsShortIN, "in");
  _addEntry(ZStrs.UnitsShortIn, "in");
  // _addEntry(ZStrs.UnitsShortPluralIns, "ins");
  _addEntry(ZStrs.UnitsVerboseInch, "inch");
  _addEntry(ZStrs.UnitsVerbosePluralInches, "Inches");

  _addEntry(ZStrs.UnitsMM, "mm");
  _addEntry(ZStrs.UnitsShortMM, "mm");
  // _addEntry(ZStrs.UnitsShortPluralMMs, "mm");
  _addEntry(ZStrs.UnitsVerboseMillimeter, "millimeter");
  _addEntry(ZStrs.UnitsVerbosePluralMillimeters, "Millimeters");

  _addEntry(ZStrs.UnitsPercent, "%");
  _addEntry(ZStrs.UnitsShortPercent, "%");
  _addEntry(ZStrs.UnitsShortPluralPercent, "%");
  _addEntry(ZStrs.UnitsVerbosePercent, "percent");
  _addEntry(ZStrs.UnitsVerbosePluralPercent, "Percent");

  _addEntry(ZStrs.UnitsPica, "pc");
  _addEntry(ZStrs.UnitsShortPica, "pc");
  _addEntry(ZStrs.UnitsShortPluralPicas, "picas");
  _addEntry(ZStrs.UnitsVerbosePica, "pica");
  _addEntry(ZStrs.UnitsVerbosePluralPicas, "Picas");

  _addEntry(ZStrs.UnitsPX, "px");
  _addEntry(ZStrs.UnitsShortPx, "px");
  _addEntry(ZStrs.UnitsShortPluralPx, "px");
  _addEntry(ZStrs.UnitsVerbosePixel, "pixel");
  _addEntry(ZStrs.UnitsVerbosePluralPixels, "Pixel");

  _addEntry(ZStrs.UnitsPT, "pt");
  _addEntry(ZStrs.UnitsShortPT, "pt");
  // _addEntry(ZStrs.UnitsShortPluralPts, "pt");
  _addEntry(ZStrs.UnitsVerbosePoint, "points");
  _addEntry(ZStrs.UnitsVerbosePluralPoints, "Points");
};
psx._unitsInit();


//
// Function: localizeUnitValue
// Description: Convert a UnitValue object to a localized string
// Input: un - UnitValue
// Return: a localized string
//
psx.localizeUnitValue = function(un) {
  var obj = {};
  obj.toString = function() {
    return this.value + ' ' + this.type;
  }
  obj.value = psx.localizeNumber(un.value);
  obj.type = un.type;

  var map = psx._unitStrMap;
  for (var idx in map) {
    if (un.type == map[idx]) {
      obj.type = idx;
      break;
    }
  }
  return obj;
};

//
// Function: localizeUnitType
// Description: Convert a UnitValue type string to a localized string
// Input: txt - UnitValue type string
// Return: a localized string
//
psx.localizeUnitType = function(txt) {
  var type = psx._reverseMap[txt.toLowerCase()];
  return type;
};

//
// Function: delocalizeUnitType
// Description: Convert a localized type to a UnitValue type string
// Input: txt - a localized type string
// Return: a UnitValue type string
//
psx.delocalizeUnitType = function(txt) {
  var type = psx._unitStrMap[txt.toLowerCase()];
  if (!type) {
    type = psx._unitStrMap[txt];
  }
  return type;
};


//
// Function: delocalizeUnitValue
// Description: Convert a localized UnitValue string into a UnitValue object
// Input: localized UnitValue string
// Return: a UnitValue object or undefined if there was a problem
//
psx.delocalizeUnitValue = function(str) {
  var un = undefined;
  var ar = str.split(/\s+/);
  if (ar.length == 2) {
    var n = psx.delocalizeNumber(ar[0]);
    var val = psx.delocalizeUnitType(ar[1]);
    un = UnitValue(n, val);
  } 
  return un;
};

//
// Function: getDefaultUnits
// Description: gets the default ruler units as localized string
// Input: <input>
// Return: the default ruler units as localized string
//
psx.getDefaultUnits = function() {
  return psx._unitMap[psx._units];
};

//
// Function: getDefaultUnitsString
// Description: Get the ruler unit default Unit type
// Input: <none>
// Return: the default ruler unit as a UnitValue type
//
psx.getDefaultUnitsString = function() {
  return psx._unitStrMap[psx._unitMap[psx._units]];
};
psx.getDefaultRulerUnitsString = psx.getDefaultUnitsString;

//
// Function: validateUnitValue
// Description: Convert string to a UnitValue object
// Input: str - the string to be converted
//        bu  - the base UnitValue to use for conversion (opt)
//        ru  - the Unit type to use if one is not specified (opt)
//
// If bu is a Document, ru is set to the docs type and the
// docs resolution is used to determine the base UnitValue
//
// If ru is not specified, the default ruler unit type is used.
//
// If bu is not specified, a resolution of 1/72 is used.
//
// Note: this does not handle localized Unit value strings
//
// Return: A UnitValue object or undefined if it's not a valid string
//
psx.validateUnitValue = function(str, bu, ru) {
  var self = this;

  if (str instanceof UnitValue) {
    return str;
  }

  if (bu && bu instanceof Document) {
    var doc = bu;
    ru = doc.width.type;
    bu = UnitValue(1/doc.resolution, ru);

  } else {
    if (!ru) {
      ru = psx.getDefaultRulerUnitsString();
    }
    if (!bu) {
      UnitValue.baseUnit = UnitValue(1/72, ru);
    }
  }
  str = str.toString().toLowerCase();

  var zero = new UnitValue("0 " + ru);
  var un = zero;
  if (!str.match(/[a-z%]+/)) {
    str += ' ' + ru.units;
  }
  str = psx.delocalizeNumber(s);
  un = new UnitValue(str);

  if (isNaN(un.value) || un.type == '?') {
    return undefined;
  }

  if (un.value == 0) {
    un = zero;
  }

  return un;
};


//
// Function: doEvent
// Description: Invoke a Photoshop Event with no arguments
// Input:  doc - the target document (opt: undefined)
//         eid - the event ID
//         interactive - do we run the event interactively (opt: true)
//         noDesc - do we pass in an empty descriptor (opt: true)
// Return: the result descriptor
//
psx.doEvent = function(doc, eid, interactive, noDesc) {
  var id;

  if (doc != undefined && eid == undefined) {
    if (doc.constructor == Number) {
      eid = doc.valueOf();
    } else if (doc.constructor == String) {
      eid = doc;
    }
    doc = undefined;
  }

  if (!eid) {
    Error.runtimeError(8600); // Event key is missing "No event id specified");
  }

  if (eid.constructor != Number) {
    if (eid.length < 4) {
      // "Event id must be at least 4 characters long"
      Error.runtimeError(19, "eventID");
    }

    if (eid.length == 4) {
      id = cTID(eid);
    } else {
      id = sTID(eid);
    }
  } else {
    id  = eid;
  }

  interactive = (interactive == true);
  noDesc = (noDesc == true);

  function _ftn(id) {
    var dmode = (interactive ? DialogModes.ALL : DialogModes.NO);
    var desc = (noDesc ? undefined : new ActionDescriptor());
    return app.executeAction(id, desc, dmode);
  }

  return _ftn(id);
};


//
// Function: hist
// Description: Move back and forth through the history stack.
// Input: dir - "Prvs" or "Nxt "
// Return: <none>
//
psx.hist = function(dir) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putEnumerated(cTID("HstS"), cTID("Ordn"), cTID(dir));
    desc.putReference(cTID("null"), ref);
    executeAction(cTID("slct"), desc, DialogModes.NO);
  }

  _ftn();
};

//
// Function: back
// Description: Move back through the history stack.
// Input: <none>
// Return: <none>
//
psx.undo = function () {
  psx.hist("Prvs");
};
//
// Function: redo
// Description: Move forward through the history stack.
// Input: <none>
// Return: <none>
//
psx.redo = function () {
  psx.hist("Nxt ");
};
//
// Function: Undo
// Description: Do an "Undo"
// Input: <none>
// Return: <none>
//
psx.Undo = function () {
  psx.doEvent("undo");
};
//
// Function: Redo
// Description: do a Redo
// Input: <none>
// Return: <none>
//
psx.Redo = function () {
  psx.doEvent(sTID('redo'));
};

//
// Function: waitForRedraw
// Description: wait until the PS UI has finished redrawing the UI
// Input: <none>
// Return: <none>
//
psx.waitForRedraw = function() {
  if (app.refresh) {
    app.refresh();
  } else {
    var desc = new ActionDescriptor();
    desc.putEnumerated(cTID("Stte"), cTID("Stte"), cTID("RdCm"));
    executeAction(cTID("Wait"), desc, DialogModes.NO);
  }
};


//
// Function: delocalizeColorMode
// Description: Convert a localized mode string into a non-localized string.
//   This is useful for constructing API constants.
//   ex:
//     var mode = psx.delocalizeColorMode(ZStrs.LabMode);
//     doc.changeMode(eval("ChangeMode." + mode));
//   ex:
//     var mode = psx.delocalizeColorMode(ZStrs.LabMode);
//     var doc = Documents.add(UnitValue("6 in"), UnitValue("4 in"),
//                             300, "NoName", eval("NewDocumentMode." + mode));
// Input: a localized color mode string
// Return: a delocalized color mode string
//
psx.delocalizeColorMode = function(str) {
  var mode = str;

  switch (str) {
    case ZStrs.RGBMode:       mode = "RGB"; break;
    case ZStrs.CMYKMode:      mode = "CMYK"; break;
    case ZStrs.LabMode:       mode = "Lab"; break;
    case ZStrs.GrayscaleMode: mode = "Grayscale"; break;
  }

  return mode;
};

//=========================== PS Paths =============================

//
// Some PS folder constants
//
// need to break up the ZString prefix to avoid
// the ZString harvester because the ZString definition changed
// in CS6
//
psx.PRESETS_FOLDER =
  new Folder(app.path + '/' +
             localize('$' + '$' + '$' + '/' +
                      (isCS6() ? "private/" : "") +
                      "ApplicationPresetsFolder/Presets=Presets"));

psx.SCRIPTS_FOLDER =
  new Folder(app.path + '/' + ZStrs.InstalledScripts);

psx.USER_PRESETS_FOLDER =
  new Folder(Folder.userData + '/' +
             localize("$$$/private/AdobeSystemFolder/Adobe=Adobe") + '/' +
             localize("$$$/private/FolderNames/AdobePhotoshopProductVersionFolder") + '/' +
             localize("$$$/private/FolderName/UserPresetsFolder/Presets=Presets"));


//======================= File functions ===========================

//
// Function: fileError
// Description: Format a standard File/Folder error string
// Input: f - File
//        msg - an error message (opt: '')
// Return: an File I/O error string
//
psx.fileError = function(f, msg) {
 return (ZStrs.FileErrorStr + (msg || '') + " \"" + decodeURI(f) +
         "\": " +  f.error + '.');
};

//
// Function: convertFptr
// Description: convert something into a File/Folder object
// Input: fptr - a String, XML object, or existing File/Folder object
// Return: a File/Folder object
//
psx.convertFptr = function(fptr) {
  var f;
  try { if (fptr instanceof XML) fptr = fptr.toString(); } catch (e) {}

  if (fptr.constructor == String) {
    f = File(fptr);

  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;

  } else {
    Error.runtimeError(19, "fptr");
  }
  return f;
};

//
// Function: writeToFile
// Description: Open a file, write a string into it, then close it
// Input: fptr - a file reference
//        str - a String
//        encoding - the encoding (opt)
//        lineFeed - the lineFeed (opt) 
// Return: <none>
//
psx.writeToFile = function(fptr, str, encoding, lineFeed) {
  var xfile = psx.convertFptr(fptr);
  var rc;

  if (encoding) {
    xfile.encoding = encoding;
  }

  rc = xfile.open("w");
  if (!rc) {
    Error.runtimeError(psx.IO_ERROR_CODE,
                       psx.fileError(xfile, ZStrs.UnableToOpenOutputFile));
  }

  if (lineFeed) {
    xfile.lineFeed = lineFeed;
  }

  rc = xfile.write(str);
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, ZStrs.fileError(xfile));
  }

  rc = xfile.close();
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, ZStrs.fileError(xfile));
  }
};

//
// Function: readFromFile
// Description: Read the entire contents of a file as a string
// Input:  fptr - a file reference
//         encoding - the encoding (opt) 
//         lineFeed - the lineFeed (opt) 
// Returns: a String
// Note: there are some subtleties involved in handling
// some character conversions errors
//
psx.readFromFile = function(fptr, encoding, lineFeed) {
  var file = psx.convertFptr(fptr);
  var rc;

  rc = file.open("r");
  if (!rc) {
    Error.runtimeError(psx.IO_ERROR_CODE,
                       psx.fileError(file, ZStrs.UnableToOpenInputFile));
  }
  if (encoding) {
    file.encoding = encoding;
  }
  if (lineFeed) {
    file.lineFeed = lineFeed;
  }
  var str = file.read();

  // In some situations, read() will set the file.error to
  // 'Character conversion error' but read the file anyway
  // In other situations it won't read anything at all from the file
  // we ignore the error if we were able to read the file anyway
  if (str.length == 0 && file.length != 0) {
    if (!file.error) {
      file.error = ZStrs.CharacterConversionError;
    }
    if (psx.IOEXCEPTIONS_ENABLED) {
      Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
    }
  } else {
    // if (file.error) {
    //   Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
    // }
  }

  rc = file.close();
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  return str;
};


//
// Function: readXMLFile
// Description: Reads a text file and returns an XML object.
//              psx assumes UTF8 with \n
// Input:  fptr - a reference to a file
// Return: an XML object
//
psx.readXMLFile = function(fptr) {
  var rc;
  var file = psx.convertFptr(fptr);
  if (!file.exists) {
    Error.runtimeError(48); // File/Folder does not exist
  }

  // Always work with UTF8/unix
  file.encoding = "UTF8";
  file.lineFeed = "unix";

  rc = file.open("r", "TEXT", "????");
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  var str = file.read();
  // Need additional error checking here...

  rc = file.close();
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  return new XML(str);
};

//
// Function: writeXMLFile
// Description: Writes an XML object to a file
//              psx uses UTF8 with \n
// Input:  fptr - a file reference
//         xml - an XML object
// Return: a File object
//
psx.writeXMLFile = function(fptr, xml) {
  var rc;
  if (!(xml instanceof XML)) {
    Error.runtimeError(19, "xml"); // "Bad XML parameter";
  }

  var file = psx.convertFptr(fptr);

  // Always work with UTF8/unix
  file.encoding = "UTF8";

  rc = file.open("w", "TEXT", "????");
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  // file.write("\uFEFF");
  // unicode signature, this is UTF16 but will convert to UTF8 "EF BB BF"
  // optional and not used since it confuses most programming editors
  // and command line tools

  file.lineFeed = "unix";

  file.writeln('<?xml version="1.0" encoding="utf-8"?>');

  rc = file.write(xml.toXMLString());
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  rc = file.close();
  if (!rc && psx.IOEXCEPTIONS_ENABLED) {
    Error.runtimeError(psx.IO_ERROR_CODE, psx.fileError(file));
  }

  return file;
};


//
// Function: psx.createFileSelect
// Description: File 'open' functions take a string of the format
//              "JPEG Files: *.jpg" on Windows and a function on
//              OS X. This function takes a Windows-style select string
//              a returns the OS X select-function on Mac.
//   ex:
//     var sel = psx.createFileSelect("XML Files: *.xml");
//     var file = psx.selectFileOpen(promptStr, sel, Folder.desktop);
// Input:  str - a Windows-style select string
// Return: The orignal select-string on Windows, or a select-function
//         for the select-string on OS X
//
psx.createFileSelect = function(str) {
  if (isWindows()) {
    return str;
  }

  if (!str.constructor == String) {
    return str;
  }

  var exts = [];
  var rex = /\*\.(\*|[\w]+)(.*)/;
  var m;
  while (m = rex.exec(str)) {
    exts.push(m[1].toLowerCase());
    str = m[2];
  }

  function macSelect(f) {
    var name = decodeURI(f.absoluteURI).toLowerCase();
    var _exts = macSelect.exts;

    // alert(name);

    while (f.alias) {
      try {
        f = f.resolve();
      } catch (e) {
        f = null;
      }

      if (f == null) {
        return false;
      }
    }

    if (f instanceof Folder) {
      return true;
    }

    for (var i = 0; i < _exts.length; i++) {
      var ext = _exts[i];
      if (ext == '.*') {
        return true;
      }
      if (name.match(RegExp("\\." + ext + "$", "i")) != null) {
        return true;
      }
    }
    return false;
  }

  macSelect.exts = exts;
  return macSelect;
};

//
// Function: selectFileOpen, selectFileSave
// Description: Open a dialog to prompt the user to select a file.
//              An initial file or folder can optionally be specified
//              Change the current directory reference if we it
//              seems appropriate.
//    ex: var file = psx.selectFileOpen("Choose a file to open",
//                                      "JPEG Files: *.jpg", "/c/tmp")
//    ex: var file = psx.selectFileSave("Choose a file to save",
//                                      "JPEG Files: *.jpg",
//                                      File("/c/tmp/tmp.jpg"))
// Input:  prompt - a prompt for the dialog (opt)
//         select - a select-string (opt)
//         start  - the initial directory
// Return: a File or undefined if the user canceled
//
psx.selectFileOpen = function(prompt, select, start) {
  return psx._selectFile(prompt, select, start, true);
};
psx.selectFileSave = function(prompt, select, start) {
  return psx._selectFile(prompt, select, start, false);
};
psx.selectFile = psx.selectFileOpen;

psx._selectFile = function(prompt, select, start, open) {
  var file;

  if (!prompt) {
    prompt = ZStrs.SelectFile;
  }

  if (start) {
    start = psx.convertFptr(start);
  } else {
    start = Folder.desktop;
  }

  var classFtn = (open ? File.openDialog : File.saveDialog);

  if (!start) {
    file = classFtn(prompt, select);

  } else {
    if (start instanceof Folder) {
      while (start && !start.exists) {
        start = start.parent;
      }

      var files = start.getFiles(select);
      if (!files || files.length == 0) {
        files = start.getFiles();
      }
      for (var i = 0; i < files.length; i++) {
        if (files[i] instanceof File) {
          start = files[i];
          break;
        }
      }
      if (start instanceof Folder) {
        start = new File(start + "/file");
      }
    }

    while (true) {
      if (start instanceof File) {
        var instanceFtn = (open ? "openDlg" : "saveDlg");
        file = start[instanceFtn](prompt, select);

      } else {
        file = Folder.selectDialog(prompt);
      }

      if (open && file && !file.exists) {
        continue;
      }

      break;
    }
  }

  if (file) {
    Folder.current = file.parent;
  }

  return file;
};

//
// Function: selectFolder
// Description: Open a dialog to select a folder
// Input:  prompt - (opt: "Select a Folder")
//         start - the initial folder
// Return: a Folder object or undefined if the user canceled
//
psx.selectFolder = function(prompt, start) {
  var folder;

  if (!prompt) {
    prompt = ZStrs.SelectFolder;
  }

  if (start) {
    start = psx.convertFptr(start);
    while (start && !start.exists) {
      start = start.parent;
    }
  }

  if (!start) {
    folder = Folder.selectDialog(prompt);

  } else {
    if (start instanceof File) {
      start = start.parent;
    }

    folder = start.selectDlg(prompt);
  }

  return folder;
};

//
// Function: getFiles
// Description: Get a set of files from a folder
// Input:  folder - a Folder
//         mask - a file mask pattern or RegExp (opt: undefined)
// Return: an array of Files
//
psx.getFiles = function(folder, mask) {
  var files = [];

  folder = psx.convertFptr(folder);

  if (folder.alias) {
    folder = folder.resolve();
  }

  return folder.getFiles(mask);
};

//
// Function: getFolders
// Description: Get a set of folders from a folder
// Input:  folder - a Folder
// Return: an array of Folders
//
psx.getFolders = function(folder) {
  folder = psx.convertFptr(folder);

  if (folder.alias) {
    folder = folder.resolve();
  }
  var folders = psx.getFiles(folder,
                             function(f) { return f instanceof Folder; });
  return folders;
};

//
// Function: findFiles
// Description: Find a set of files from a folder recursively
// Input:  folder - a Folder
//         mask - a file mask pattern or RegExp (opt: undefined)
// Return: an array of Files
//
psx.findFiles = function(folder, mask) {
  folder = psx.convertFptr(folder);

  if (folder.alias) {
    folder = folder.resolve();
  }
  var files = psx.getFiles(folder, mask);
  var folders = psx.getFolders(folder);

  for (var i = 0; i < folders.length; i++) {
    var f = folders[i];
    var ffs = psx.findFiles(f, mask);
    // files.concat(ffs); This occasionally fails for some unknown reason (aka
    // interpreter Bug) so we do it manually instead
    while (ffs.length > 0) {
      files.push(ffs.shift());
    }
  }
  return files;
};

//
// Function: exceptionMessage
// Description: create a useful error message based on an exception
// Input: e - an Exception
// Return: a String
//
// Thanks to Bob Stucky for this...
//
psx.exceptionMessage = function(e) {
  var str = '';
  var fname = (!e.fileName ? '???' : decodeURI(e.fileName));
  str += "   Message: " + e.message + '\n';
  str += "   File: " + fname + '\n';
  str += "   Line: " + (e.line || '???') + '\n';
  str += "   Error Name: " + e.name + '\n';
  str += "   Error Number: " + e.number + '\n';

  if (e.source) {
    var srcArray = e.source.split("\n");
    var a = e.line - 10;
    var b = e.line + 10;
    var c = e.line - 1;
    if (a < 0) {
      a = 0;
    }
    if (b > srcArray.length) {
      b = srcArray.length;
    }
    for ( var i = a; i < b; i++ ) {
      if ( i == c ) {
        str += "   Line: (" + (i + 1) + ") >> " + srcArray[i] + '\n';
      } else {
        str += "   Line: (" + (i + 1) + ")    " + srcArray[i] + '\n';
      }
    }
  }

  try {
    if ($.stack) {
      str += '\n' + $.stack + '\n';
    }
  } catch (e) {
  }

  if (str.length > psx.exceptionMessage._maxMsgLen) {
    str = str.substring(0, psx.exceptionMessage._maxMsgLen) + '...';
  }

  if (LogFile.defaultLog.fptr) {
    str += "\nLog File:" + LogFile.defaultLog.fptr.toUIString();
  }

  return str;
};
psx.exceptionMessage._maxMsgLen = 5000;

//============================ LogFile =================================

//
// Class: LogFile
// Description: provides a interface for logging information
// Input: fname - a file name
//
LogFile = function(fname) {
  var self = this;
  
  self.filename = fname;
  self.enabled = fname != undefined;
  self.encoding = "UTF8";
  self.append = false;
  self.fptr = undefined;
};

//
// Function: LogFile.setFilename
// Description: set the name of the log file. The log file is
//              enabled if a filename is passed in.
// Input: filename - the log filename or undefined
//        encoding - the file encoding (opt: "UTF8")
// Return: <none>
//
LogFile.prototype.setFilename = function(filename, encoding) {
  var self = this;
  self.filename = filename;
  self.enabled = filename != undefined;
  self.encoding = encoding || "UTF8";
  self.fptr = undefined;
};

//
// Function LogFile.write
// Description: Writes a string to a log file if the log is enabled
//              and it has a valid filename. The log file is opened
//              and closed for each write in order to flush the
//              message to disk.
// Input: msg - a message for the log file
// Return: <none>
//
LogFile.prototype.write = function(msg) {
  var self = this;
  var file;

  if (!self.enabled) {
    return;
  }

  if (!self.filename) {
    return;
  }

  if (!self.fptr) {
    file = new File(self.filename);
    if (self.append && file.exists) {
      if (!file.open("e", "TEXT", "????"))  {
        var err = ZStrs.UnableToOpenLogFile.sprintf(file.toUIString(),
                                                    file.error);
        Error.runtimeError(psx.IO_ERROR_CODE, err);
      }
      file.seek(0, 2); // jump to the end of the file

    } else {
      if (!file.open("w", "TEXT", "????")) {
        if (!file.open("e", "TEXT", "????")) {
          var err = ZStrs.UnableToOpenLogFile.sprintf(file.toUIString(),
                                                      file.error);
          Error.runtimeError(psx.IO_ERROR_CODE, err);
        }
        file.seek(0, 0); // jump to the beginning of the file
      }
    }
    self.fptr = file;

  } else {
    file = self.fptr;
    if (!file.open("e", "TEXT", "????"))  {
      var err = ZStrs.UnableToOpenLogFile.sprintf(file.toUIString(),
                                                  file.error);
      Error.runtimeError(psx.IO_ERROR_CODE, err);
    }
    file.seek(0, 2); // jump to the end of the file
  }

  if (isMac()) {
    file.lineFeed = "Unix";
  }

  if (self.encoding) {
    file.encoding = self.encoding;
  }

  if (msg) {
    msg = msg.toString();
  }

  if (!file.writeln(new Date().toISODateString() + " - " + msg)) {
    var err = ZStrs.UnableToOpenLogFile.sprintf(file.toUIString(),
                                                file.error);
    Error.runtimeError(psx.IO_ERROR_CODE, err);
  }

  file.close();
};

//
// Function: LogFile.defaultLog 
// Description: This is the default log file
//
LogFile.defaultLog = new LogFile(Folder.userData + "/stdout.log");

//
// Function: LogFile.setFilename
// Description: sets the name of the default log file
// Input:  fptr - a file name
//         encoding - the encoding for the file (opt)
// Return: <none>
//
LogFile.setFilename = function(fptr, encoding) {
  LogFile.defaultLog.setFilename(fptr, encoding);
};

//
// Function: LogFile.write
// Description: write a message to the default log file
// Input:  msg - a message for the log file
// Return: <none>
//
LogFile.write = function(msg) {
  LogFile.defaultLog.write(msg);
};

//
// Function: LogFile.logException
// Description: log a formatted message based on an exception
// Input:  e - an Exception
//         msg - a message for the log file (opt)
//         doAlert - open an alert with the formatted message (opt: false)
// Return: <none>
//
LogFile.logException = function(e, msg, doAlert) {
  var log = LogFile.defaultLog;
  if (!log || !log.enabled) {
    return;
  }

  if (doAlert == undefined) {
    doAlert = false;

    if (msg == undefined) {
      msg = '';
    } else if (isBoolean(msg)) {
      doAlert = msg;
      msg = '';
    }
  }

  doAlert = !!doAlert;

  var str = ((msg || '') + "\n" +
             "==============Exception==============\n" +
             psx.exceptionMessage(e) +
             "\n==============End Exception==============\n");

  log.write(str);

  if (doAlert) {
    str += ("\r\r" + ZStrs.LogFileReferences + "\r" +
            "    " + log.fptr.toUIString());

    alert(str);
  }
};

//
// Function: toBoolean
// Description: convert something to a boolean
// Input:  s - the thing to convert
// Return: a boolean
//
function toBoolean(s) {
  if (s == undefined) { return false; }
  if (s.constructor == Boolean) { return s.valueOf(); }
  try { if (s instanceof XML) s = s.toString(); } catch (e) {}
  if (s.constructor == String)  { return s.toLowerCase() == "true"; }

  return Boolean(s);
};

//
// Function: isBoolean
// Description: determine if something is a boolean
// Input:  s - the thing to test
// Return: true if s is boolean, false if not
//
function isBoolean(s) {
  return (s != undefined && s.constructor == Boolean);
};

//
// Description: Should the PS locale be used to determine the
//              decimal point or should the OS locale be used.
//              PS uses the OS locale so scripts may not match
//              the PS UI.
//
psx.USE_PS_LOCALE_FOR_DECIMAL_PT = true;

// 
// Function: determineDecimalPoint
// Description: determine what to use for the decimal point
// Input:  <none>
// Return: a locale-specific decimal point
//
// Note: Currently there is no way to determine what decimal
//       point is being used in the PS UI so this always returns
//       the decimal point for the PS locale
//
psx.determineDecimalPoint = function() {
//   if (psx.USE_PS_LOCALE_FOR_DECIMAL_PT) {
    psx.decimalPoint = $.decimalPoint;
//   }
  return psx.decimalPoint;
};
psx.determineDecimalPoint();

//
// Function: localizeNumber
// Description: convert a number to a string with a localized decimal point
// Input: n - a number or UnitValue
// Return: a number as a localized string
//
psx.localizeNumber = function(n) {
  return n.toString().replace('.', psx.decimalPoint);
};

//
// Function: delocalizeNumber
// Description: convert a string containing a localized number to
//              a "standard" number string
// Input:  a localized numeric string
// Return: a numeric string with a EN decimal point
//
psx.delocalizeNumber = function(n) {
  return n.toString().replace(psx.decimalPoint, '.');
};


//
// Function: toNumber
// Description: convert a something to a number
// Input: s - some representation of a number
//        def - a value to use if s cannot be parsed
// Return: a number or NaN if there was a problem and no default was specified
//
function toNumber(s, def) {
  if (s == undefined) { return def || NaN; }
  try { if (s instanceof XML) s = s.toString(); } catch (e) {}
  if (s.constructor == String && s.length == 0) { return def || NaN; }
  if (s.constructor == Number) { return s.valueOf(); }
  try {
    var n = Number(psx.delocalizeNumber(s.toString()));
  } catch (e) {
    // $.level = 1; debugger;
  }
  return (isNaN(n) ? (def || NaN) : n);
};

//
// Function: isNumber
// Description: see if something is a number
// Input: s - some representation of a number
//        def - a value to use if s cannot be parsed
// Return: true if s is a number, false if not
//
function isNumber(s) {
  try { if (s instanceof XML) s = s.toString(); }
  catch (e) {}
  return !isNaN(psx.delocalizeNumber(s));
};

//
// Function: isNumber
// Description: see if something is a String
// Input: s - something
// Return: true if s is a String, false if not
//
function isString(s) {
  return (s != undefined && s.constructor == String);
};

//
// Function: toFont
// Description: convert something to a font name
// Input: fs - a TextFont or a string
// Return: a font name that can be used with TextItem.font
//
function toFont(fs) {
  if (fs.typename == "TextFont") { return fs.postScriptName; }

  var str = fs.toString();
  var f = psx.determineFont(str);  // first, check by PS name

  return (f ? f.postScriptName : undefined);
};

// 
// Function: getXMLValue
// Description: returns the value of an xml object as a string if it
//              is not undefined else it returns a default value
// Input: xml - an XML object
//        def - a default value (opt: undefined)
// Return: a String or undefined
//
psx.getXMLValue = function(xml, def) {
  return (xml == undefined) ? def : xml.toString();
}

// 
// Function: getByName
// Description: Get an element in the container with a desired name property
// Input: container - an Array or something with a [] interface
//        value - the name of the element being sought
//        all - get all elements with the given name
// Return: an object, array of objects, or undefined
//
psx.getByName = function(container, value, all) {
  return psx.getByProperty(container, "name", value, all);
};

// 
// Function: getByProperty
// Description: Get an element in the container with a desired property
// Input: container - an Array or something with a [] interface
//        prop - the name of the property
//        value - the value of the property of the element being sought
//        all - get all elements that match
// Return: an object, array of objects, or undefined
//
psx.getByProperty = function(container, prop, value, all) {
  // check for a bad index
  if (prop == undefined) {
    Error.runtimeError(2, "prop");
  }
  if (value == undefined) {
    Error.runtimeError(2, "value");
  }
  var matchFtn;

  all = !!all;

  if (value instanceof RegExp) {
    matchFtn = function(s1, re) { return s1.match(re) != null; };
  } else {
    matchFtn = function(s1, s2) { return s1 == s2; };
  }

  var obj = [];

  for (var i = 0; i < container.length; i++) {
    if (matchFtn(container[i][prop], value)) {
      if (!all) {
        return container[i];     // there can be only one!
      }
      obj.push(container[i]);    // add it to the list
    }
  }

  return all ? obj : undefined;
};

//
// Function: determineFont
// Description: find a font based on a name
// Input: str - a font name or postScriptName
// Return: a TextFont or undefined
//
psx.determineFont = function(str) {
  return (psx.getByName(app.fonts, str) ||
          psx.getByProperty(app.fonts, 'postScriptName', str));
};

//
// Function: getDefaultFont
// Description: Attempt to find a resonable locale-specific font
// Input:  <none>
// Return: TextFont or undefined
//
psx.getDefaultFont = function() {
  var str;

  if (isMac()) {
    str = localize("$$$/Project/Effects/Icon/Font/Name/Mac=Lucida Grande");
  } else {
    str = localize("$$$/Project/Effects/Icon/Font/Name/Win=Tahoma");
  }

  var font = psx.determineFont(str);

  if (!font) {
    var f = psx.getApplicationProperty(sTID('fontLargeName'));
    if (f != undefined) {
      font = psx.determineFont(f);
    }
  }

  return font;
};

// 
// Function: psx.getDefaultTypeToolFont
// Description: This attemps gets the default Type Tool font. Since there is no
//         direct API for this, we have to save the current type tool settings,
//         reset the settings, then restore the saved settings.
//         This will fail if there already exists a tool preset called
//         "__temp__". Working around this shortcoming would make things even
//         more complex than they already are
// Input:  <none>
// Return: TextFont or undefined
//
psx.getDefaultTypeToolFont = function() {
  var str = undefined;
  var typeTool = "typeCreateOrEditTool";

  try {
    // get the current tool
    var ref = new ActionReference();
    ref.putEnumerated(cTID("capp"), cTID("Ordn"), cTID("Trgt") );
    var desc = executeActionGet(ref);
    var tid = desc.getEnumerationType(sTID('tool'));
    var currentTool = typeIDToStringID(tid);

    // switch to the type tool
    if (currentTool != typeTool) {
      var desc = new ActionDescriptor();
      var ref = new ActionReference();
      ref.putClass(sTID(typeTool));
      desc.putReference(cTID('null'), ref);
      executeAction(cTID('slct'), desc, DialogModes.NO);
    }

    var ref = new ActionReference();
    ref.putEnumerated(cTID("capp"), cTID("Ordn"), cTID("Trgt") );
    var desc = executeActionGet(ref);
    var tdesc = desc.hasKey(cTID('CrnT')) ?
      desc.getObjectValue(cTID('CrnT')) : undefined;

    if (tdesc) {
      // save the current type tool settings
      var desc4 = new ActionDescriptor();
      var ref4 = new ActionReference();
      ref4.putClass( sTID('toolPreset') );
      desc4.putReference( cTID('null'), ref4 );
      var ref5 = new ActionReference();
      ref5.putProperty( cTID('Prpr'), cTID('CrnT') );
      ref5.putEnumerated( cTID('capp'), cTID('Ordn'), cTID('Trgt') );
      desc4.putReference( cTID('Usng'), ref5 );
      desc4.putString( cTID('Nm  '), "__temp__" );

      // this will fail if there is already a preset called __temp__
      executeAction( cTID('Mk  '), desc4, DialogModes.NO );

      // reset the type tool
      var desc2 = new ActionDescriptor();
      var ref2 = new ActionReference();
      ref2.putProperty( cTID('Prpr'), cTID('CrnT') );
      ref2.putEnumerated( cTID('capp'), cTID('Ordn'), cTID('Trgt') );
      desc2.putReference( cTID('null'), ref2 );
      executeAction( cTID('Rset'), desc2, DialogModes.NO );

      // get the current type tool settings
      var ref = new ActionReference();
      ref.putEnumerated(cTID("capp"), cTID("Ordn"), cTID("Trgt") );
      var desc = executeActionGet(ref);
      var tdesc = desc.getObjectValue(cTID('CrnT'));

      // get the default type tool font
      var charOpts = tdesc.getObjectValue(sTID("textToolCharacterOptions"));
      var styleOpts = charOpts.getObjectValue(cTID("TxtS"));
      str = styleOpts.getString(sTID("fontPostScriptName"));

      // restore the type tool settings
      var desc9 = new ActionDescriptor();
      var ref10 = new ActionReference();
      ref10.putName( sTID('toolPreset'), "__temp__" );
      desc9.putReference( cTID('null'), ref10 );
      executeAction( cTID('slct'), desc9, DialogModes.NO );

      // delete the temp setting
      var desc11 = new ActionDescriptor();
      var ref12 = new ActionReference();
      ref12.putEnumerated( sTID('toolPreset'), cTID('Ordn'), cTID('Trgt') );
      desc11.putReference( cTID('null'), ref12 );
      executeAction( cTID('Dlt '), desc11, DialogModes.NO );
    }

    // switch back to the original tool
    if (currentTool != typeTool) {
      var desc = new ActionDescriptor();
      var ref = new ActionReference();
      ref.putClass(tid);
      desc.putReference(cTID('null'), ref);
      executeAction(cTID('slct'), desc, DialogModes.NO);
    }
  } catch (e) {
    return undefined;
  }

  return str;
};

//
// Function: trim
// Description: Trim leading and trailing whitepace from a string
// Input: value - String
// Return: String
//
psx.trim = function(value) {
   return value.replace(/^[\s]+|[\s]+$/g, '');
};


//
// Function: copyFromTo
// Description: copy the properties from one object to another. functions
//              and 'typename' are skipped
// Input: from - object
//        to - Object
// Return: <none>
//
psx.copyFromTo = function(from, to) {
  if (!from || !to) {
    return;
  }
  for (var idx in from) {
    var v = from[idx];
    if (typeof v == 'function') {
      continue;
    }
    if (v == 'typename'){
      continue;
    }

    to[idx] = v;
  }
};

//
// Function: listProps
// Description: create a string with name-value pairs for each property
//              in an object. Functions are skipped.
// Input: obj - object
// Return: String
//
psx.listProps = function(obj) {
  var s = [];
  var sep = (isBridge() ? "\r" : "\r\n");

  for (var x in obj) {
    var str = x + ":\t";
    try {
      var o = obj[x];
      str += (typeof o == "function") ? "[function]" : o;
    } catch (e) {
    }
    s.push(str);
  }
  s.sort();

  return s.join(sep);
};


//
//============================ Strings  Extensions ===========================
//

//
// Function: String.contains
// Description: Determines if a string contains a substring
// Input: sub - a string
// Return: true if sub is a part of a string, false if not
//
String.prototype.contains = function(sub) {
  return this.indexOf(sub) != -1;
};

//
// Function: String.containsWord
// Description: Determines if a string contains a word
// Input: str - a word
// Return: true if str is word in a string, false if not
//
String.prototype.containsWord = function(str) {
  return this.match(new RegExp("\\b" + str + "\\b")) != null;
};

//
// Function: String.endsWith
// Description: Determines if a string ends with a substring
// Input: sub - a string
// Return: true if a string ends with sub, false if not
//
String.prototype.endsWith = function(sub) {
  return this.length >= sub.length &&
    this.slice(this.length - sub.length) == sub;
};

//
// Function: String.reverse
// Description: Creates a string with characters in reverse order
// Input:  <none>
// Return: the string with the characters in reverse order
//
String.prototype.reverse = function() {
  var ar = this.split('');
  ar.reverse();
  return ar.join('');
};

//
// Function: String.startsWith
// Description: Determines if a string starts with a substring
// Input: sub - a string
// Return: true if a string starts with sub, false if not
//
String.prototype.startsWith = function(sub) {
  return this.indexOf(sub) == 0;
};

//
// Function: String.trim
// Description: Trims whitespace from the beginning and end of a string
// Input:  <none>
// Return: the string with whitespace trimmed off
//
String.prototype.trim = function() {
  return this.replace(/^[\s]+|[\s]+$/g, '');
};
//
// Function: String.ltrim
// Description: Trims whitespace off the beginning of the string
// Input:  <none>
// Return: the string with whitespace trimmed off the start of the string
//
String.prototype.ltrim = function() {
  return this.replace(/^[\s]+/g, '');
};
//
// Function: String.rtrim
// Description: Trims whitespace off the end of a string
// Input:  <none>
// Return: the string with whitespace of the end of the string
//
String.prototype.rtrim = function() {
  return this.replace(/[\s]+$/g, '');
};


//========================= String formatting ================================
//
// Function: String.sprintf
// Description: Creates a formatted string
// Input: the format specification and values to be used in formatting
// Return: a formatted string
//
// Documentation:
//   http://www.opengroup.org/onlinepubs/007908799/xsh/fprintf.html
//
// From these sites:
//   http://forums.devshed.com/html-programming-1/sprintf-39065.html
//   http://jan.moesen.nu/code/javascript/sprintf-and-printf-in-javascript/
//
// Example:    var idx = 1;
//             while (file.exists) {
//                var newFname = "%s/%s_%02d.%s".sprintf(dir, fname,
//                                                       idx++, ext);
//                file = File(newFname);
//              }
//
String.prototype.sprintf = function() {
  var args = [this];
  for (var i = 0; i < arguments.length; i++) {
    args.push(arguments[i]);
  }
  return String.sprintf.apply(null, args);
};
String.sprintf = function() {
  function _sprintf() {
    if (!arguments || arguments.length < 1 || !RegExp)  {
      return "Error";
    }
    var str = arguments[0];
    var re = /([^%]*)%('.|0|\x20)?(-)?(\d+)?(\.\d+)?(%|b|c|d|u|f|o|s|x|X)/m;
            //') /* for xemacs auto-indent  */
    var a = b = [], numSubstitutions = 0, numMatches = 0;
    var result = '';

    while (a = re.exec(str)) {
      var leftpart = a[1], pPad = a[2], pJustify = a[3], pMinLength = a[4];
      var pPrecision = a[5], pType = a[6], rightPart = a[7];

      rightPart = str.slice(a[0].length);

      numMatches++;

      if (pType == '%') {
        subst = '%';
      } else {
        numSubstitutions++;
        if (numSubstitutions >= arguments.length) {
          alert('Error! Not enough function arguments (' +
                (arguments.length - 1)
                + ', excluding the string)\n'
                + 'for the number of substitution parameters in string ('
                + numSubstitutions + ' so far).');
        }
        var param = arguments[numSubstitutions];
        var pad = '';
        if (pPad && pPad.slice(0,1) == "'") {
          pad = leftpart.slice(1,2);
        } else if (pPad) {
          pad = pPad;
        }
        var justifyRight = true;
        if (pJustify && pJustify === "-") {
          justifyRight = false;
        }
        var minLength = -1;
        if (pMinLength) {
          minLength = toNumber(pMinLength);
        }
        var precision = -1;
        if (pPrecision && pType == 'f') {
          precision = toNumber(pPrecision.substring(1));
        }
        var subst = param;
        switch (pType) {
        case 'b':
          subst = toNumber(param).toString(2);
          break;
        case 'c':
          subst = String.fromCharCode(toNumber(param));
          break;
        case 'd':
          subst = toNumber(param) ? Math.round(toNumber(param)) : 0;
          break;
        case 'u':
          subst = Math.abs(Math.round(toNumber(param)));
          break;
        case 'f':
          if (precision == -1) {
            precision = 6;
          }
          var n = Number(parseFloat(param).toFixed(Math.min(precision, 20)));
          subst = psx.localizeNumber(n);
//             ? Math.round(parseFloat(param) * Math.pow(10, precision))
//             / Math.pow(10, precision)
//             : ;
            break;
        case 'o':
          subst = toNumber(param).toString(8);
          break;
        case 's':
          subst = param;
          break;
        case 'x':
          subst = ('' + toNumber(param).toString(16)).toLowerCase();
          break;
        case 'X':
          subst = ('' + toNumber(param).toString(16)).toUpperCase();
          break;
        }
        var padLeft = minLength - subst.toString().length;
        if (padLeft > 0) {
          var arrTmp = new Array(padLeft+1);
          var padding = arrTmp.join(pad?pad:" ");
        } else {
          var padding = "";
        }
      }
      result += leftpart + padding + subst;
      str = rightPart;
    }
    result += str;
    return result;
  };

  return _sprintf.apply(null, arguments);
};


//========================= Date formatting ================================
//
// Function: Date.strftime
// Description:
//    This is a third generation implementation. This is a JavaScript
//    implementation of C the library function 'strftime'. It supports all
//    format specifiers except U, W, z, Z, G, g, O, E, and V.
//    For a full description of this function, go here:
//       http://www.opengroup.org/onlinepubs/007908799/xsh/strftime.html
//    Donating implementations can be found here:
//      http://redhanded.hobix.com/inspect/showingPerfectTime.html
//    and here:
//      http://wiki.osafoundation.org/bin/view/Documentation/JavaScriptStrftime
//  Input:  the date object and the format specification
//  Return: a formatted string
//
//  Example: var date = new Date(); alert(date.strftime("%Y-%m-%d"));
//
// Object Method
Date.prototype.strftime = function (fmt) {
  return Date.strftime(this, fmt);
};

// Class Function
Date.strftime = function(date, fmt) {
  var t = date;
  var cnvts = Date.prototype.strftime._cnvt;
  var str = fmt;
  var m;
  var rex = /([^%]*)%([%aAbBcCdDehHIjmMprRStTuwxXyYZ]{1})(.*)/;

  var result = '';
  while (m = rex.exec(str)) {
    var pre = m[1];
    var typ = m[2];
    var post = m[3];
    result += pre + cnvts[typ](t);
    str = post;
  }
  result += str;
  return result;
};

// the specifier conversion function table
Date.prototype.strftime._cnvt = {
  zeropad: function( n ){ return n>9 ? n : '0'+n; },
  spacepad: function( n ){ return n>9 ? n : ' '+n; },
  ytd: function(t) {
    var first = new Date(t.getFullYear(), 0, 1).getTime();
    var diff = t.getTime() - first;
    return parseInt(((((diff/1000)/60)/60)/24))+1;
  },
  a: function(t) {
    return [ZStrs.Sun,ZStrs.Mon,ZStrs.Tue,ZStrs.Wed,ZStrs.Thu,
            ZStrs.Fri,ZStrs.Sat][t.getDay()];
  },
  A: function(t) {
    return [ZStrs.Sunday,ZStrs.Monday,ZStrs.Tuesdsay,ZStrs.Wednesday,
            ZStrs.Thursday,ZStrs.Friday,
            ZStrs.Saturday][t.getDay()];
  },
  b: function(t) {
    return [ZStrs.Jan,ZStrs.Feb,ZStrs.Mar,ZStrs.Apr,ZStrs.May,ZStrs.Jun,
            ZStrs.Jul,ZStrs.Aug,ZStrs.Sep,ZStrs.Oct,
            ZStrs.Nov,ZStrs.Dec][t.getMonth()]; },
  B: function(t) {
    return [ZStrs.January,ZStrs.February,ZStrs.March,ZStrs.April,ZStrs.May,
            ZStrs.June,ZStrs.July,ZStrs.August,
            ZStrs.September,ZStrs.October,ZStrs.November,
            ZStrs.December][t.getMonth()]; },
  c: function(t) {
    return (this.a(t) + ' ' + this.b(t) + ' ' + this.e(t) + ' ' +
            this.H(t) + ':' + this.M(t) + ':' + this.S(t) + ' ' + this.Y(t));
  },
  C: function(t) { return this.Y(t).slice(0, 2); },
  d: function(t) { return this.zeropad(t.getDate()); },
  D: function(t) { return this.m(t) + '/' + this.d(t) + '/' + this.y(t); },
  e: function(t) { return this.spacepad(t.getDate()); },
  // E: function(t) { return '-' },
  F: function(t) { return this.Y(t) + '-' + this.m(t) + '-' + this.d(t); },
  g: function(t) { return '-'; },
  G: function(t) { return '-'; },
  h: function(t) { return this.b(t); },
  H: function(t) { return this.zeropad(t.getHours()); },
  I: function(t) {
    var s = this.zeropad((t.getHours() + 12) % 12);
    return (s == "00") ? "12" : s;
  },
  j: function(t) { return this.ytd(t); },
  k: function(t) { return this.spacepad(t.getHours()); },
  l: function(t) {
    var s = this.spacepad((t.getHours() + 12) % 12);
    return (s == " 0") ? "12" : s;
  },
  m: function(t) { return this.zeropad(t.getMonth()+1); }, // month-1
  M: function(t) { return this.zeropad(t.getMinutes()); },
  n: function(t) { return '\n'; },
  // O: function(t) { return '-' },
  p: function(t) { return this.H(t) < 12 ? ZStrs.AM : ZStrs.PM; },
  r: function(t) {
    return this.I(t) + ':' + this.M(t) + ':' + this.S(t) + ' ' + this.p(t);
  },
  R: function(t) { return this.H(t) + ':' + this.M(t); },
  S: function(t) { return this.zeropad(t.getSeconds()); },
  t: function(t) { return '\t'; },
  T: function(t) {
    return this.H(t) + ':' + this.M(t) + ':' + this.S(t) + ' ' + this.p(t);
  },
  u: function(t) {return t.getDay() ? t.getDay()+1 : 7; },
  U: function(t) { return '-'; },
  w: function(t) { return t.getDay(); }, // 0..6 == sun..sat
  W: function(t) { return '-'; },       // not available
  x: function(t) { return this.D(t); },
  X: function(t) { return this.T(t); },
  y: function(t) { return this.zeropad(this.Y(t) % 100); },
  Y: function(t) { return t.getFullYear().toString(); },
  z: function(t) { return ''; },
  Z: function(t) { return ''; },
  '%': function(t) { return '%'; }
};

// this needs to be worked on...
function _weekNumber(date) {
  var ytd = toNumber(date.strftime("%j"));
  var week = Math.floor(ytd/7);
  if (new Date(date.getFullYear(), 0, 1).getDay() < 4) {
    week++;
  }
  return week;
};

//
// Format a Date object into a proper ISO 8601 date string
//
psx.toISODateString = function(date, timeDesignator, dateOnly, precision) {
  if (!date) date = new Date();
  var str = '';
  if (timeDesignator == undefined) { timeDesignator = 'T'; };
  function _zeroPad(val) { return (val < 10) ? '0' + val : val; }
  if (date instanceof Date) {
    str = (date.getFullYear() + '-' +
           _zeroPad(date.getMonth()+1,2) + '-' +
           _zeroPad(date.getDate(),2));
    if (!dateOnly) {
      str += (timeDesignator +
              _zeroPad(date.getHours(),2) + ':' +
              _zeroPad(date.getMinutes(),2) + ':' +
              _zeroPad(date.getSeconds(),2));
      if (precision && typeof(precision) == "number") {
        var ms = date.getMilliseconds();
        if (ms) {
          var millis = _zeroPad(ms.toString(),precision);
          var s = millis.slice(0, Math.min(precision, millis.length));
          str += "." + s;
        }
      }
    }
  }
  return str;
};

//
// Make it a Date object method
//
Date.prototype.toISODateString = function(timeDesignator, dateOnly, precision) {
  return psx.toISODateString(this, timeDesignator, dateOnly, precision);
};

// some ISO8601 formats
Date.strftime.iso8601_date = "%Y-%m-%d";
Date.strftime.iso8601_full = "%Y-%m-%dT%H:%M:%S";
Date.strftime.iso8601      = "%Y-%m-%d %H:%M:%S";
Date.strftime.iso8601_time = "%H:%M:%S";

Date.prototype.toISO = function() {
  return this.strftime(Date.strftime.iso8601);
};

Date.prototype.toISOString = Date.prototype.toISODateString;


//
//============================ Array  Extensions ===========================
//

//
// Function: Array.contains
// Description: Determines if an array contains a specific element
// Input:  the array and the element to search for
// Return: true if the element is found, false if not
//
Array.contains = function(ar, el) {
  for (var i = 0; i < ar.length; i++) {
    if (ar[i] == el) {
      return true;
    }
  }
  return false;
};
if (!Array.prototype.contains) {
  // define the instance method
  Array.prototype.contains = function(el) {
    for (var i = 0; i < this.length; i++) {
      if (this[i] == el) {
        return true;
      }
    }
    return false;
  };
}

//
// Function: Array.indexOf
// Description: Determines the index of an element in an array
// Input:  the array and the element to search for
// Return: the index of the element or -1 if not found
//
if (!Array.prototype.indexOf) {
  Array.prototype.indexOf = function(el) {
    for (var i = 0; i < this.length; i++) {
      if (this[i] == el) {
        return i;
      }
    }
    return -1;
  };
}

//
// Function: Array.lastIndexOf
// Description: Determines the last index of an element in an array
// Input:  the array and the element to search for
// Return: the last index of the element or -1 if not found
//
if (!Array.prototype.lastIndexOf) {
  Array.prototype.indexOf = function(el) {
    for (var i = this.length-1; i >= 0; i--) {
      if (this[i] == el) {
        return i;
      }
    }
  return -1;
  };
}


//
// Class: Timer
// Description: a simple timer with start and stop methods
//
Timer = function() {
  var self = this;
  self.startTime = 0;
  self.stopTime  = 0;
  self.elapsed = 0;
  self.cummulative = 0;
  self.count = 0;
};

Timer.prototype.start = function() {
  this.startTime = new Date().getTime();
};
Timer.prototype.stop = function() {
  var self = this;
  self.stopTime = new Date().getTime();
  self.elapsed = (self.stopTime - self.startTime)/1000.00;
  self.cummulative += self.elapsed;
  self.count++;
  self.per = self.cummulative/self.count;
};


//
//======================== File and Folder ===================================
//
// Function: toUIString
// Description: the name of a File/Folder suitable for use in a UI
// Input:  <none>
// Return: the formatted file name
//
File.prototype.toUIString = function() {
  return decodeURI(this.fsName);
};
Folder.prototype.toUIString = function() {
  return decodeURI(this.fsName);
};

//========================= Filename formatting ===============================
//
// Function: strf
// Input: File/Folder and the format string
// File.strf(fmt [, fs])
// Folder.strf(fmt [, fs])
//   This is based of the file name formatting facility in exiftool. Part of
//   the description is copied directly from there. You can find exiftool at:
//      http://www.sno.phy.queensu.ca/~phil/exiftool/
//
// Description:
//   Format a file string using a printf-like format string
//
// fmt is a string where the following substitutions occur
//   %d - the directory name (no trailing /)
//   %f - the file name without the extension
//   %e - the file extension without the leading '.'
//   %p - the name of the parent folder
//   %% - the '%' character
//
// if fs is true the folder is in local file system format
//   (e.g. C:\images instead of /c/images)
//
// Examples:
//
// Reformat the file name:
// var f = new File("/c/work/test.jpg");
// f.strf("%d/%f_%e.txt") == "/c/work/test_jpg.txt"
//
// Change the file extension
// f.strf("%d/%f.psd") == "/c/work/test.psd"
//
// Convert to a file name in a subdirectory named after the extension
// f.strf("%d/%e/%f.%e") == "/c/work/jpg/test.jpg"
//
// Change the file extension and convert to a file name in a subdirectory named
//   after the new extension
// f.strf("%d/psd/%f.psd") == "/c/work/psd/test.psd"
//
// var f = new File("~/.bashrc");
// f.strf("%f") == ".bashrc"
// f.strf("%e") == ""
//
// Advanced Substitution
//   A substring of the original file name, directory or extension may be
//   taken by specifying a string length immediately following the % character.
//   If the length is negative, the substring is taken from the end. The
//   substring position (characters to ignore at the start or end of the
//   string) may be given by a second optional value after a decimal point.
// For example:
//
// var f = new File("Picture-123.jpg");
//
// f.strf("%7f.psd") == "Picture.psd"
// f.strf("%-.4f.psd") == "Picture.psd"
// f.strf("%7f.%-3f") == "Picture.123"
// f.strf("Meta%-3.1f.xmp") == "Meta12.xmp"
//
File.prototype.strf = function(fmt, fs) {
  var self = this;
  var name = decodeURI(self.name);
  //var name = (self.name);

  // get the portions of the full path name

  // extension
  var m = name.match(/.+\.([^\.\/]+)$/);
  var e = m ? m[1] : '';

  // basename
  m = name.match(/(.+)\.[^\.\/]+$/);
  var f = m ? m[1] : name;

  fs |= !($.os.match(/windows/i)); // fs only matters on Windows
  // fs |= isMac();

  // full path...
  var d = decodeURI((fs ? self.parent.fsName : self.parent.absoluteURI));

  // parent directory...
  var p = decodeURI(self.parent.name);

  //var d = ((fs ? self.parent.fsName : self.parent.toString()));

  var str = fmt;

  // a regexp for the format specifiers

  var rex = /([^%]*)%(-)?(\d+)?(\.\d+)?(%|d|e|f|p)(.*)/;

  var result = '';

  while (m = rex.exec(str)) {
    var pre = m[1];
    var sig = m[2];
    var len = m[3];
    var ign = m[4];
    var typ = m[5];
    var post = m[6];

    var subst = '';

    if (typ == '%') {
      subst = '%';
    } else {
      var s = '';
      switch (typ) {
        case 'd': s = d; break;
        case 'e': s = e; break;
        case 'f': s = f; break;
        case 'p': s = p; break;
        // default: s = "%" + typ; break; // let others pass through
      }

      var strlen = s.length;

      if (strlen && (len || ign)) {
        ign = (ign ? Number(ign.slice(1)) : 0);
        if (len) {
          len = Number(len);
          if (sig) {
            var _idx = strlen - len - ign;
            subst = s.slice(_idx, _idx+len);
          } else {
            subst = s.slice(ign, ign+len);
          }
        } else {
          if (sig) {
            subst = s.slice(0, strlen-ign);
          } else {
            subst = s.slice(ign);
          }
        }

      } else {
        subst = s;
      }
    }

    result += pre + subst;
    str = post;
  }

  result += str;

  return result;
};
Folder.prototype.strf = File.prototype.strf;


//=========================== PS Functions ===============================

//
// Function: getApplicationProperty
// Description: Get a value from the Application descriptor
// Input:  key - an ID
// Return: The value or undefined
//
psx.getApplicationProperty = function(key) {
  var ref = ref = new ActionReference();
  ref.putProperty(cTID("Prpr"), key);
  ref.putEnumerated(cTID('capp'), cTID("Ordn"), cTID("Trgt") );
  var desc;
  try {
    desc = executeActionGet(ref);
  } catch (e) {
    return undefined;
  }
  var val = undefined;
  if (desc.hasKey(key)) {
    var typ = desc.getType(key);
    switch (typ) {
    case DescValueType.ALIASTYPE:
      val = desc.getPath(key); break;
    case DescValueType.BOOLEANTYPE:
      val = desc.getBoolean(key); break;
    case DescValueType.CLASSTYPE:
      val = desc.getClass(key); break;
    case DescValueType.DOUBLETYPE:
      val = desc.getDouble(key); break;
    case DescValueType.ENUMERATEDTYPE:
      val = desc.getEnumeratedValue(key); break;
    case DescValueType.INTEGERTYPE:
      val = desc.getInteger(key); break;
    case DescValueType.LISTTYPE:
      val = desc.getList(key); break;
    case DescValueType.OBJECTTYPE:
      val = desc.getObjectValue(key); break;
    case DescValueType.RAWTYPE:
      val = desc.getData(key); break;
    case DescValueType.REFERENCETYPE:
      val = desc.getReference(key); break;
    case DescValueType.STRINGTYPE:
      val = desc.getString(key); break;
    case DescValueType.UNITDOUBLE:
      val = desc.getUnitDoubleValue(key); break;
    default:
      try {
        if (typ == DescValueType.LARGEINTEGERTYPE) {
          val = desc.getLargeInteger(key);
        }
      }  catch (e) {
      }
      break;
    }
  }
  return val;
};

//
// Class: ColorProfileNames
// Description: a holder for common color profile names
//
ColorProfileNames = {};
ColorProfileNames.ADOBE_RGB      = "Adobe RGB (1998)";
ColorProfileNames.APPLE_RGB      = "Apple RGB";
ColorProfileNames.PROPHOTO_RGB   = "ProPhoto RGB";
ColorProfileNames.SRGB           = "sRGB IEC61966-2.1";
ColorProfileNames.COLORMATCH_RGB = "ColorMatch RGB";
ColorProfileNames.WIDEGAMUT_RGB  = "Wide Gamut RGB";

//
// Function: delocalizeProfile
// Description: converts a localized color profile name to a name
//              that can be used in the PS DOM API
// Input:  profile - localized color profile name
// Return: a color profile name in EN
//
psx.delocalizeProfile = function(profile) {
  var p = profile;

  switch (profile) {
    case ZStrs.ProfileAdobeRGB:      p = ColorProfileNames.ADOBE_RGB; break;
    case ZStrs.ProfileAppleRGB:      p = ColorProfileNames.APPLE_RGB; break;
    case ZStrs.ProfileProPhotoRGB:   p = ColorProfileNames.PROPHOTO_RGB; break;
    case ZStrs.ProfileSRGB:          p = ColorProfileNames.SRGB; break;
    case ZStrs.ProfileColorMatchRGB: p = ColorProfileNames.COLORMATCH_RGB; break;
    case ZStrs.ProfileWideGamutRGB:  p = ColorProfileNames.WIDEGAMUT_RGB; break;
    case ZStrs.ProfileLab:           profile = "Lab"; break;
    case ZStrs.ProfileWorkingCMYK:   profile = "Working CMYK"; break;
    case ZStrs.ProfileWorkingGray:   profile = "Working Gray"; break;
    case ZStrs.ProfileWorkingRGB:    profile = "Working RGB"; break;
  }

  return p;
};

//
// Function: convertProfile
// Description: converts a document's color profile
// Input:  doc - a Document
//         profile - a color profile name (possibly localized)
//         flatten - should the document be flattened (opt)
// Return: <none>
//
// tpr, why can't we use the DOM for this call?
psx.convertProfile = function(doc, profile, flatten) {
  profile = profile.replace(/\.icc$/i, '');

  profile = psx.delocalizeProfile(profile);

  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putEnumerated( cTID('Dcmn'), cTID('Ordn'), cTID('Trgt') );
    desc.putReference( cTID('null'), ref);

    if (profile == 'Working RGB' || profile == 'Working CMYK') {
      var idTargetMode = cTID('TMd ');
      var idTargetModeValue = profile == 'Working RGB' ? cTID('RGBM') : cTID('CMYM');
      desc.putClass( idTargetMode, idTargetModeValue );
    } else {
        desc.putString( cTID('T   '), profile );
    }
    desc.putEnumerated( cTID('Inte'), cTID('Inte'), cTID('Clrm') );
    desc.putBoolean( cTID('MpBl'), true );
    desc.putBoolean( cTID('Dthr'), false );
    desc.putInteger( cTID('sdwM'), -1 );
    if (flatten != undefined) {
      desc.putBoolean( cTID('Fltt'), !!flatten);
    }
    executeAction( sTID('convertToProfile'), desc, DialogModes.NO );
  }

  _ftn();
};


//
// Function: getDocumentDescriptor
// Description: Gets the ActionDescriptor of a Document
// Input:  doc - a Document
// Return: an ActionDescriptor
//
psx.getDocumentDescriptor = function(doc) {
  var active = undefined;
  if (doc && doc != app.activeDocument) {
    active = app.activeDocument;
    app.activeDocument = doc;
  }
  var ref = new ActionReference();
  ref.putEnumerated( cTID("Dcmn"),
                     cTID("Ordn"),
                     cTID("Trgt") );  //activeDoc
  var desc = executeActionGet(ref);

  if (active) {
    app.activeDocument = active;
  }

  return desc;
};

//
// Function: getDocumentIndex
// Description: Gets the index of a Document in app.documents
// Input:  doc - a Document
// Return: The index of the document or -1 if not found
//
psx.getDocumentIndex = function(doc) {
  var docs = app.documents;
  for (var i = 0; i < docs.length; i++) {
    if (docs[i] == doc) {
      return i+1;
    }
  }

  return -1;

//   return psx.getDocumentDescriptor(doc).getInteger(cTID('ItmI'));
};

//
// Function: revertDocument
// Description: Reverts a document to it's original state
// Input:  doc - a Document
// Return: <none>
//
psx.revertDocument = function(doc) {
  psx.doEvent(doc, "Rvrt");
};


//
// Function: getXMPValue
// Description: Get the XMP value for (tag) from the object (obj).
//              obj can be a String, XML, or Document. Support for
//              Files will be added later.
//              Based on getXMPTagFromXML from Adobe's StackSupport.jsx
// Input:  obj - an object containing XMP data
//         tag - the name of an XMP field
// Return: the value of an XMP field as a String
//
psx.getXMPValue = function(obj, tag) {
  var xmp;

  if (obj.constructor == String) {
    xmp = new XML(obj);

  } else if (obj.typename == "Document") {
    xmp = new XML(obj.xmpMetadata.rawData);

  } else if (obj instanceof XML) {
    xmp = obj;

  // } else if (obj instanceof File) {
  // add support for Files

  } else {
    Error.runtimeError(19, "obj");
  }

  var s;
  
  // Ugly special case
  if (tag == "ISOSpeedRatings") {
    s = String(xmp.*::RDF.*::Description.*::ISOSpeedRatings.*::Seq.*::li);

  }  else {
    s = String(eval("xmp.*::RDF.*::Description.*::" + tag));
  }

  return s;
};


//
// Function: getDocumentName
// Description: Gets the name of the document. Doing it this way
//              avoids the side effect recomputing the histogram.
// Input:  doc - a Document
// Return: the name of the Document
//
psx.getDocumentName = function(doc) {
  function _ftn() {
    var ref = new ActionReference();
    ref.putProperty(cTID('Prpr'), cTID('FilR'));
    ref.putEnumerated(cTID('Dcmn'), cTID('Ordn'), cTID('Trgt'));
    var desc = executeActionGet(ref);
    return desc.hasKey(cTID('FilR')) ? desc.getPath(cTID('FilR')) : undefined;
  }
  return _ftn();
};

//
// Function: hasBackgruond
// Description: Determines if a Document has a background
// Input:  doc - a Document
// Return: true if the document has a background, false if not
//
psx.hasBackground = function(doc) {
   return doc.layers[doc.layers.length-1].isBackgroundLayer;
};

//
// Function: copyLayerToDocument
// Description: Copies a layer from on Document to another
// Input:  doc   - a Document
//         layer - a Layer
//         otherDocument - a Document
// Return: <none>
//
psx.copyLayerToDocument = function(doc, layer, otherDoc) {
  var desc = new ActionDescriptor();
  var fref = new ActionReference();
  fref.putEnumerated(cTID('Lyr '), cTID('Ordn'), cTID('Trgt'));
  desc.putReference(cTID('null'), fref);
  var tref = new ActionReference();
  tref.putIndex(cTID('Dcmn'), psx.getDocumentIndex(otherDoc));
  // tref.putName(cTID('Dcmn'), otherDoc.name);
  desc.putReference(cTID('T   '), tref);
  desc.putInteger(cTID('Vrsn'), 2 );
  executeAction(cTID('Dplc'), desc, DialogModes.NO);
};

//
// Function: getLayerDescriptor
// Description: Gets the ActionDescriptor for a layer
// Input:  doc   - a Document
//         layer - a Layer
// Return: an ActionDescriptor
//
psx.getLayerDescriptor = function(doc, layer) {
  var ref = new ActionReference();
  ref.putEnumerated(cTID("Lyr "), cTID("Ordn"), cTID("Trgt"));
  return executeActionGet(ref);
};

//
// Function: createLayerMask
// Description: Creates a layer mask for a layer optionally from the
//              current selection
// Input:  doc   - a Document
//         layer - a Layer
//         fromSelection - should mask be made from the current selection (opt)
// Return: <none>
//
psx.createLayerMask = function(doc, layer, fromSelection) {
  var desc = new ActionDescriptor();
  desc.putClass(cTID("Nw  "), cTID("Chnl"));
  var ref = new ActionReference();
  ref.putEnumerated(cTID("Chnl"), cTID("Chnl"), cTID("Msk "));
  desc.putReference(cTID("At  "), ref);
  if (fromSelection == true) {
    desc.putEnumerated(cTID("Usng"), cTID("UsrM"), cTID("RvlS"));
  } else {
    desc.putEnumerated(cTID("Usng"), cTID("UsrM"), cTID("RvlA"));
  }
  executeAction(cTID("Mk  "), desc, DialogModes.NO);
};

//
// Function: hasLayerMask
//           isLayerMaskEnabled
//           disableLayerMask
//           enableLayerMask
//           setLayerMaskEnabledState
// Description: A collection of functions dealing with the state
//              of a layer's mask.
//              
// Input:  doc   - a Document
//         layer - a Layer
// Return: boolean or <none>
//
psx.hasLayerMask = function(doc, layer) {
  return psx.getLayerDescriptor().hasKey(cTID("UsrM"));
};
psx.isLayerMaskEnabled = function(doc, layer) {
  var desc = psx.getLayerDescriptor(doc, layer);
  return (desc.hasKey(cTID("UsrM")) && desc.getBoolean(cTID("UsrM")));
};
psx.disableLayerMask = function(doc, layer) {
  psx.setLayerMaskEnabledState(doc, layer, false);
};
psx.enableLayerMask = function(doc, layer) {
  psx.setLayerMaskEnabledState(doc, layer, true);
};
psx.setLayerMaskEnabledState = function(doc, layer, state) {
  if (state == undefined) {
    state = false;
  }
  var desc = new ActionDescriptor();

  var ref = new ActionReference();
  ref.putEnumerated(cTID('Lyr '), cTID('Ordn'), cTID('Trgt'));
  desc.putReference(cTID('null'), ref );

  var tdesc = new ActionDescriptor();
  tdesc.putBoolean(cTID('UsrM'), state);
  desc.putObject(cTID('T   '), cTID('Lyr '), tdesc);

  executeAction(cTID('setd'), desc, DialogModes.NO );
};

//
// Function: mergeVisible
//           mergeSelected
//           mergeDown
//           mergeAllLayers
// Description: A collection of functions for merging layers
// Input:  doc - a Document
// Return: <none>
//
psx.mergeVisible = function(doc) {
  psx.doEvent(doc, "MrgV");  // "MergeVisible"
};
psx.mergeSelected = function(doc) {
  psx.doEvent(doc, "Mrg2");  // "MergeLayers"
};
psx.mergeDown = function(doc) {
  psx.doEvent(doc, "Mrg2");  // "MergeLayers"
};
psx.mergeAllLayers = function(doc) {
  psx.selectAllLayers(doc);

  if (psx.hasBackground(doc)) {
    psx.appendLayerToSelectionByName(doc, doc.backgroundLayer.name);
  }
  psx.mergeSelected(doc);
};

//
// Function: appendLayerToSelectionByName
// Description: Adds a layer to the current set of selected layers
// Input:  doc  - a Document
//         name - a layer's name
// Return: <none>
//
psx.appendLayerToSelectionByName = function(doc, name) {
  var desc25 = new ActionDescriptor();
  var ref18 = new ActionReference();
  ref18.putName( cTID('Lyr '), name );
  desc25.putReference( cTID('null'), ref18 );
  desc25.putEnumerated( sTID('selectionModifier'),
                        sTID('selectionModifierType'),
                        sTID('addToSelection') );
  desc25.putBoolean( cTID('MkVs'), false );
  executeAction( cTID('slct'), desc25, DialogModes.NO );
};

//
// Function: selectAllLayers
// Description: Select all layers in a document
// Input:  doc  - a Document
// Return: <none>
//
psx.selectAllLayers = function(doc) {
  var desc18 = new ActionDescriptor();
  var ref11 = new ActionReference();
  ref11.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
  desc18.putReference( cTID('null'), ref11 );
  executeAction( sTID('selectAllLayers'), desc18, DialogModes.NO );
};

//
// Function: getLayerBounds
// Description: Gets the bounds of the layer content (without the mask)
// Input:  doc  - a Document
//         layer - a Layer
// Return: the bounding rectangle of the layer's content
//
psx.getLayerBounds = function(doc, layer) {
  var ru = app.preferences.rulerUnits;

  var reenable = false;
  var st;
  if (psx.hasLayerMask(doc, layer) &&
      psx.isLayerMaskEnabled(doc, layer)) {
      st = doc.activeHistoryState;
    psx.disableLayerMask(doc, layer);
    reenable = true;
  }

  var lbnds = layer.bounds;
  
  // fix this to modify the history state
  if (reenable) {
    psx.enableLayerMask(doc, layer);
  }
  for (var i = 0; i < 4; i++) {
    lbnds[i] = lbnds[i].as("px");
  }

  return lbnds;
};

//
// Function: selectBounds
// Description: Create a selection using the given bounds
// Input:  doc  - a Document
//         b - bounding rectangle (in pixels)
//         type - the selection type
//         feather - the amount of feather (opt: 0)
//         antialias - should antialias be used (opt: false)
// Return: <none>
//
psx.selectBounds = function(doc, b, type, feather, antialias) {
  if (feather == undefined) {
    feather = 0;
  }

  if (antialias == undefined) {
    antialias = false;
  }
  
  doc.selection.select([[ b[0], b[1] ],
                        [ b[2], b[1] ],
                        [ b[2], b[3] ],
                        [ b[0], b[3] ]],
                       type, feather, antialias);
};

//
// Function: getSelectionBounds
// Description: Get the bounds of the current selection
// Input:  doc  - a Document
// Return: a bound rectangle (in pixels)
//
psx.getSelectionBounds = function(doc) {
  var bnds = [];
  var sbnds = doc.selection.bounds;
  for (var i = 0; i < sbnds.length; i++) {
    bnds[i] = sbnds[i].as("px");
  }
  return bnds;
};

//
// Function: hasSelection
// Input:  doc  - a Document
// Return: returns true if the document has as selection, false if not
//
psx.hasSelection = function(doc) {
  var res = false;

  var as = doc.activeHistoryState;
  doc.selection.deselect();
  if (as != doc.activeHistoryState) {
    res = true;
    doc.activeHistoryState = as;
  }

  return res;
};

//
// Function: rgbToString
// Description: Convert a SolidColor into a RGB string
// Input:  c - SolidColor
// Return: an RGB string (e.g. "[128,255,128]")
//
psx.rgbToString = function(c) {
  return "[" + c.rgb.red + "," + c.rgb.green + "," + c.rgb.blue + "]";
};
//
// Function: rgbToArray
// Description: Convert a SolidColor into a RGB array
// Input:  c - SolidColor
// Return: an RGB array (e.g. [128,255,128])
//
psx.rgbToArray = function(c) {
  return [c.rgb.red, c.rgb.green, c.rgb.blue];
};
//
// Function: rgbFromString
// Description: Converts an RBG string to a SolidColor
// Input:  str - an RGB string
// Return: a SolidColor
//
psx.rgbFromString = function(str) {
  var rex = /([\d\.]+),([\d\.]+),([\d\.]+)/;
  var m = str.match(rex);
  if (m) {
    return psx.createRGBColor(Number(m[1]),
                              Number(m[2]),
                              Number(m[3]));
  }
  return undefined;
};
//
// Function: createRGBColor
// Description: Creates a SolidColor from RGB values
// Input:  r - red
//         g - green
//         b - blue
// Return: a SolidColor
//
psx.createRGBColor = function(r, g, b) {
  var c = new RGBColor();
  if (r instanceof Array) {
    b = r[2]; g = r[1]; r = r[0];
  }
  c.red = parseInt(r); c.green = parseInt(g); c.blue = parseInt(b);
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

//
// Predefine some common colors
//
psx.COLOR_BLACK = psx.createRGBColor(0, 0, 0);
psx.COLOR_RED = psx.createRGBColor(255, 0, 0);
psx.COLOR_GREEN = psx.createRGBColor(0, 255, 0);
psx.COLOR_BLUE = psx.createRGBColor(0, 0, 255);
psx.COLOR_GRAY = psx.createRGBColor(128, 128, 128);
psx.COLOR_WHITE = psx.createRGBColor(255, 255, 255);

//
// Function: colorFromString
// Description: Creates a SolidColor from an RGBString
// Input:  str - an RGB string
// Return: a SolidColor
//
psx.colorFromString = function(str) {
  var c = psx.rgbFromString(str);
  if (!c) {
    str = str.toLowerCase();
    if (str == ZStrs.black.toLowerCase()) {
      c = psx.COLOR_BLACK;
    } else if (str == ZStrs.white.toLowerCase()) {
      c = psx.COLOR_WHITE;
    } else if (str == ZStrs.foreground.toLowerCase()) {
      c = app.foregroundColor;
    } else if (str == ZStrs.background.toLowerCase()) {
      c = app.backgroundColor;
    } else if (str == ZStrs.gray.toLowerCase() ||
               str == ZStrs.grey.toLowerCase()) {
      c = psx.COLOR_GRAY;
    } else if (str == ZStrs.red.toLowerCase()) {
      c = psx.COLOR_RED;
    } else if (str == ZStrs.green.toLowerCase()) {
      c = psx.COLOR_GREEN;
    } else if (str == ZStrs.blue.toLowerCase()) {
      c = psx.COLOR_BLUE;
    }
  }
  return c;
};


//
// Function: winFileSelection
// Description: Determines if a File is an image file (checks file extension)
// Input:  f - File
// Return: true if f is an image file, false if not
//
psx.winFileSelection = function(f) {
  var suffix = f.name.match(/[.](\w+)$/);
  var t;
  
  if (suffix && suffix.length == 2)  {
    suffix = suffix[1].toUpperCase();
    for (t in app.windowsFileTypes) {
      if (suffix == app.windowsFileTypes[t]) {
        // Ignore mac-generated system thumbnails
        if (f.name.slice(0,2) != "._") {
          return true;
        }
      }
    }
  }

  return false;
};

//
// Function: macFileSelection
// Description: Determines if a File is an image file (by file type/extension)
// Input:  f - File
// Return: true if f is an image file, false if not
//
psx.macFileSelection = function(f) {
  var t;
  for (t in app.macintoshFileTypes) {
    if (f.type == app.macintoshFileTypes[t]) {
      return true;
    }
  }
  
  // Also check windows suffixes...
  return winFileSelection( f );
}


//
// Function: isValidImageFile
// Description: Determines if a File is an image file (by file type/extension)
// Input:  f - File
// Return: true if f is an image file, false if not
//
psx.isValidImageFile = function(f) {
  function _winCheck(f) {
    // skip mac system files
    if (f.name.startsWith("._")) {
      return false;
    }

    var ext = f.strf('%e').toUpperCase();
    return (ext.length > 0) && app.windowsFileTypes.contains(ext);
  }
  function _macCheck(f) {
    return app.macintoshFileTypes.contains(f.type) || _winCheck(f);
  }

  return ((f instanceof File) &&
          (((File.fs == "Macintosh") && _macCheck(f)) ||
           ((File.fs == "Windows") && _winCheck(f))));
};



//============================ Actions =====================================

//
// Function: getActionSets
// Description: Returns the ActionSets in the Actions palette
// Input:  <none>
// Return: an array of objects that have these properties
//         name - the name of the ActionSet
//         actions - an array of the names of the actions in this set
//
psx.getActionSets = function() {
  var i = 1;
  var sets = [];

  while (true) {
    var ref = new ActionReference();
    ref.putIndex(cTID("ASet"), i);
    var desc;
    var lvl = $.level;
    $.level = 0;
    try {
      desc = executeActionGet(ref);
    } catch (e) {
      break;    // all done
    } finally {
      $.level = lvl;
    }
    if (desc.hasKey(cTID("Nm  "))) {
      var set = {};
      set.index = i;
      set.name = desc.getString(cTID("Nm  "));
      set.toString = function() { return this.name; };
      set.count = desc.getInteger(cTID("NmbC"));
      set.actions = [];
      for (var j = 1; j <= set.count; j++) {
        var ref = new ActionReference();
        ref.putIndex(cTID('Actn'), j);
        ref.putIndex(cTID('ASet'), set.index);
        var adesc = executeActionGet(ref);
        var actName = adesc.getString(cTID('Nm  '));
        set.actions.push(actName);
      }
      sets.push(set);
    }
    i++;
  }

  return sets;
};


//============================= ScriptUI stuff =============================

psxui = function() {}

// XXX - Need to check to see if decimalPoint is a special RegEx character
// that needs to be escaped. Currently, we only handle '.'
psxui.dpREStr = (psx.decimalPoint == '.' ? "\\." : psx.decimalPoint);

//
// Function: numberKeystrokeFilter
//           positiveNumberKeystrokeFilter
//           numericKeystrokeFilter
//           unitValueKeystrokeFilter
// Description: A collection of KeyStroke filters that can be used with
//              EditText widgets
// Input:  <none>
// Return: <none>
//
psxui.numberKeystrokeFilter = function() {
  var ftn = psxui.numberKeystrokeFilter;

  if (this.text.match(ftn.matchRE)) {
    if (!ftn.replaceRE) {
      ftn.replaceRE = RegExp(ftn.matchRE.toString().replace(/\//g, ''), "g");
    }
    this.text = this.text.replace(ftn.replaceRE.toString(), '');
  }
};
psxui.numberKeystrokeFilter.matchRE = RegExp("[^\\-" + psxui.dpREStr + "\\d]");

psxui.positiveNumberKeystrokeFilter = function() {
  var ftn = psxui.positiveNumberKeystrokeFilter;

  if (this.text.match(ftn.matchRE)) {
    if (!ftn.replaceRE) {
      ftn.replaceRE = RegExp(ftn.matchRE.toString().replace(/\//g, ''), "g");
    }
    this.text = this.text.replace(ftn.replaceRE, '');
  }
};
psxui.positiveNumberKeystrokeFilter.matchRE =
  RegExp("[^" + psxui.dpREStr + "\\d]");

psxui.numericKeystrokeFilter = function() {
  if (this.text.match(/[^\d]/)) {
    this.text = this.text.replace(/[^\d]/g, '');
  }
};

psxui.unitValueKeystrokeFilter = function() {
  var ftn = psxui.unitValueKeystrokeFilter;

  if (this.text.match(ftn.matchRE)) {
    if (!ftn.replaceRE) {
      ftn.replaceRE = RegExp(ftn.matchRE.toString().replace(/\//g, ''), "g");
    }
    this.text = this.text.toLowerCase().replace(ftn.replaceRE, '');
  }
};
psxui.unitValueKeystrokeFilter.matchRE =
  RegExp("[^\\w% " + psxui.dpREStr + "\\d]");

psxui.unitValueKeystrokeFilter.replaceRE =
  RegExp("[^\\w% " + psxui.dpREStr + "\\d]", "gi");


//
// Function: setMenuSelection
// Description: Select an item on a menu
// Input:  menu - a Menu UI widget
//         txt - text of the menu element
//         def - an element to use if the desired one can't be found
//         ignoreCase - whether or not case insensitive comparison is used
// Return: <none>
//
psxui.setMenuSelection = function(menu, txt, def, ignoreCase) {
  var it = menu.find(txt);
  var last = menu.selection;

  if (!it && ignoreCase) {
    var items = menu.items;
    txt = txt.toLowerCase();
    for (var i = 0; i < items.length; i++) {
      if (txt == items[i].text.toLowerCase()) {
        it = items[i];
        break;
      }
    }
  }

  if (!it) {
    if (def != undefined) {
      var n = toNumber(def);
      if (!isNaN(n)) {
        it = def;

      } else {
        it = menu.find(def);
      }
    }
  }

  if (it != undefined) {
    menu.selection = it;
    menu._last = last;

  } else {
    // XXX debug only...
    if (Folder("/Users/xbytor").exists) {
      $.level = 1; debugger;
    }
    // alert("DEBUG: " + txt + " not found in menu");
  }
};

//============================ File Save =====================================
//
// FileSave is only available in Photoshop
//
FileSaveOptions = function(obj) {
  var self = this;

  self.saveDocumentType = undefined; // SaveDocumentType
  self.fileType = "jpg";             // file extension

  self._saveOpts = undefined;

  self.saveForWeb = false; // gif, png, jpg

  self.flattenImage = false; // psd, tif

  self.bmpAlphaChannels = true;
  self.bmpDepth = BMPDepthType.TWENTYFOUR;
  self.bmpRLECompression = false;

  self.gifTransparency = true;
  self.gifInterlaced = false;
  self.gifColors = 256;

  self.jpgQuality = 10;
  self.jpgEmbedColorProfile = true;
  self.jpgFormat = FormatOptions.STANDARDBASELINE;
  self.jpgConvertToSRGB = false;          // requires code

  self.epsEncoding = SaveEncoding.BINARY;
  self.epsEmbedColorProfile = true;

  self.pdfEncoding = PDFEncoding.JPEG;
  self.pdfEmbedColorProfile = true;

  self.psdAlphaChannels = true;
  self.psdEmbedColorProfile = true;
  self.psdLayers = true;
  self.psdMaximizeCompatibility = true;
  self.psdBPC = BitsPerChannelType.SIXTEEN;

  self.pngInterlaced = false;

  self.tgaAlphaChannels = true;
  self.tgaDepth = TargaBitsPerPixels.TWENTYFOUR;
  self.tgaRLECompression = true;

  self.tiffEncoding = TIFFEncoding.NONE;
  self.tiffByteOrder = (isWindows() ? ByteOrder.IBM : ByteOrder.MACOS);
  self.tiffEmbedColorProfile = true;

  if (obj) {
    for (var idx in self) {
      if (idx in obj) {       // only copy in FSO settings
        self[idx] = obj[idx];
      }
    }
    if (!obj.fileType) {
      self.fileType = obj.fileSaveType;
      if (self.fileType == "tiff") {
        self.fileType = "tif";
      }
    }
  }
};

//FileSaveOptions.prototype.typename = "FileSaveOptions";
FileSaveOptions._enableDNG = false;
FileSaveOptions._enableSAS = true; // Same as Source

FileSaveOptions.SASDefaultJPGQuality = 12;
FileSaveOptions.DefaultJPGScans = 3;

FileSaveOptions.convertBitsPerChannelType = function(bpc) {
  var v = 0;
  switch (bpc) {
    case BitsPerChannelType.ONE:       v = 1; break;
    case BitsPerChannelType.EIGHT:     v = 8; break;
    case BitsPerChannelType.SIXTEEN:   v = 16; break;
    case BitsPerChannelType.THIRTYTWO: v = 32; break;
  }

  return v;
};

FileSaveOptions.convertBMPDepthType = function(bpc) {
  var v = 0;
  switch (bpc) {
    case BMPDepthType.ONE:        v = 1; break;
    case BMPDepthType.FOUR:       v = 4; break;
    case BMPDepthType.EIGHT:      v = 8; break;
    case BMPDepthType.SIXTEEN:    v = 16; break;
    case BMPDepthType.TWENTYFOUR: v = 24; break;
    case BMPDepthType.THIRTYTWO:  v = 32; break;
  }

  return v;
};

FileSaveOptions.convertTargaBitsPerPixels = function(bpc) {
  var v = 0;
  switch (bpc) {
    case TargaBitsPerPixels.SIXTEEN:    v = 16; break;
    case TargaBitsPerPixels.TWENTYFOUR: v = 24; break;
    case TargaBitsPerPixels.THIRTYTWO:  v = 32; break;
  }
  return v;
};

FileSaveOptions.convert = function(fsOpts) {
  var fsType = fsOpts.fileType;
  if (!fsType) {
    fsType = fsOpts.fileSaveType;
  }
  if (fsType == "SaS") {
    var file = fsOpts.sourceFile;
    if (file == undefined) {
      return undefined;
    }

    fsType = file.strf("%e").toLowerCase();

    if (fsType == "jpeg") {
      fsType = "jpg";
    }
    if (fsType == "tiff") {
      fsType = "tif";
    }
    if (!FileSaveOptionsTypes[fsType]) {
      fsType = "psd";
    }

    var fs = FileSaveOptionsTypes[fsType];

    saveOpts = new fs.optionsType();

    if (fsType == "jpg") {
      saveOpts.quality = FileSaveOptions.SASDefaultJPGQuality;
    }

    return saveOpts;
  }

  if (fsType == "jpeg") {
    fsType = "jpg";
  }

  if (fsType == "tiff") {
    fsType = "tif";
  }
  if (!FileSaveOptionsTypes[fsType]) {
    fsType = "psd";
  }

  var fs = FileSaveOptionsTypes[fsType];
  if (!fs.optionsType) {
    return undefined;
  }
  var saveOpts = new fs.optionsType();

  // convert and xml node to a string. return '' if the node is undefined
  function _toString(x) {
    return (x != undefined) ? x.toString() : '';
  }

  switch (fsType) {
    case "bmp": {
      saveOpts.rleCompression = toBoolean(fsOpts.bmpRLECompression);

      var value = BMPDepthType.TWENTYFOUR;
      var str = _toString(fsOpts.bmpDepth);

      // we have to match 16 before 1 
     if (str == "1" || str.match(/one/i)) {
        value = BMPDepthType.ONE;
      } else if (str.match(/24|twentyfour/i)) {
        // we have to match 24 before 4
        value = BMPDepthType.TWENTYFOUR;
      } else if (str.match(/4|four/i)) {
        value = BMPDepthType.FOUR;
      } else if (str.match(/8|eight/i)) {
        value = BMPDepthType.EIGHT;
      } else if (str.match(/16|sixteen/i)) {
        value = BMPDepthType.SIXTEEN;
      } else if (str.match(/32|thirtytwo/i)) {
        value = BMPDepthType.THIRTYTWO;
      }
      saveOpts.depth = value;
      saveOpts.alphaChannels = toBoolean(fsOpts.bmpAlphaChannels);

      saveOpts._flatten = true;
      saveOpts._bpc = BitsPerChannelType.EIGHT;
      break;
    }
    case "gif": {
      saveOpts.transparency = toBoolean(fsOpts.gifTransparency);
      saveOpts.interlaced = toBoolean(fsOpts.gifInterlaced);
      saveOpts.colors = toNumber(fsOpts.gifColors);

      saveOpts._convertToIndexed = true;
      saveOpts._flatten = true;
      saveOpts._bpc = BitsPerChannelType.EIGHT;
      saveOpts._saveForWeb = toBoolean(fsOpts.saveForWeb);
      break;
    }
    case "jpg": {
      saveOpts.quality = toNumber(fsOpts.jpgQuality);
      saveOpts.embedColorProfile = toBoolean(fsOpts.jpgEmbedColorProfile);
      var value = FormatOptions.STANDARDBASELINE;
      var str = _toString(fsOpts.jpgFormat);
      if (str.match(/standard/i)) {
        value = FormatOptions.STANDARDBASELINE;
      } else if (str.match(/progressive/i)) {
        value = FormatOptions.PROGRESSIVE;
      } else if (str.match(/optimized/i)) {
        value = FormatOptions.OPTIMIZEDBASELINE;
      }
      saveOpts.formatOptions = value;

      if (value == FormatOptions.PROGRESSIVE) {
        saveOpts.scans = FileSaveOptions.DefaultJPGScans;
      }

      saveOpts._convertToSRGB = toBoolean(fsOpts.jpgConvertToSRGB);
      saveOpts._flatten = true;
      saveOpts._bpc = BitsPerChannelType.EIGHT;
      saveOpts._saveForWeb = toBoolean(fsOpts.saveForWeb);
      break;
    }
    case "psd": {
      saveOpts.alphaChannels = toBoolean(fsOpts.psdAlphaChannels);
      saveOpts.embedColorProfile = toBoolean(fsOpts.psdEmbedColorProfile);
      saveOpts.layers = toBoolean(fsOpts.psdLayers);
      saveOpts.maximizeCompatibility =
        toBoolean(fsOpts.psdMaximizeCompatibility);
      saveOpts.flattenImage = toBoolean(fsOpts.flattenImage);

      var value = BitsPerChannelType.SIXTEEN;
      var str = _toString(fsOpts.psdBPC);
      if (str.match(/16|sixteen/i)) {
        value = BitsPerChannelType.SIXTEEN;
      } else if (str.match(/1|one/i)) {
        value = BitsPerChannelType.ONE;
      } else if (str.match(/8|eight/i)) {
        value = BitsPerChannelType.EIGHT;
      } else if (str.match(/32|thirtytwo/i)) {
        value = BitsPerChannelType.THIRTYTWO;
      }
      saveOpts._bpc = value;

      break;
    }
    case "eps": {
      var value = SaveEncoding.BINARY;
      var str = _toString(fsOpts.epsEncoding);
      if (str.match(/ascii/i)) {
        value = SaveEncoding.ASCII;
      } else if (str.match(/binary/i)) {
        value = SaveEncoding.BINARY;
      } else if (str.match(/jpg|jpeg/i)) {
        if (str.match(/high/i)) {
          value = SaveEncoding.JPEGHIGH;
        } else if (str.match(/low/i)) {
          value = SaveEncoding.JPEGLOW;
        } else if (str.match(/max/i)) {
          value = SaveEncoding.JPEGMAXIMUM;
        } else if (str.match(/med/i)) {
          value = SaveEncoding.JPEGMEDIUM;
        }
      }
      saveOpts.encoding = value;
      saveOpts.embedColorProfile = toBoolean(fsOpts.epsEmbedColorProfile);

      saveOpts._flatten = true;
      saveOpts._bpc = BitsPerChannelType.EIGHT;
      break;
    }
    case "pdf": {
      saveOpts.embedColorProfile = toBoolean(fsOpts.pdfEmbedColorProfile);
      break;
    }
    case "png": {
      saveOpts.interlaced = toBoolean(fsOpts.pngInterlaced);

      saveOpts._flatten = true;
      saveOpts._saveForWeb = toBoolean(fsOpts.saveForWeb);
      break;
    }
    case "tga": {
      saveOpts.alphaChannels = toBoolean(fsOpts.tgaAlphaChannels);

      var value = TargaBitsPerPixels.TWENTYFOUR;
      var str = _toString(fsOpts.tgaDepth);
      if (str.match(/16|sixteen/i)) {
        value = TargaBitsPerPixels.SIXTEEN;
      } if (str.match(/24|twentyfour/i)) {
        value = TargaBitsPerPixels.TWENTYFOUR;
      } else if (str.match(/32|thirtytwo/i)) {
        value = TargaBitsPerPixels.THIRTYTWO;
      }
      saveOpts.resolution = value;

      saveOpts.rleCompression = toBoolean(fsOpts.tgaRLECompression);

      // saveOpts._flatten = true;
      saveOpts._bpc = BitsPerChannelType.EIGHT;
      break;
    }
    case "tiff":
    case "tif": {
      var value = (isWindows() ? ByteOrder.IBM : ByteOrder.MACOS);
      var str = _toString(fsOpts.tiffByteOrder);
      if (str.match(/ibm|pc/i)) {
        value = ByteOrder.IBM;
      } else if (str.match(/mac/i)) {
        value = ByteOrder.MACOS;
      }
      saveOpts.byteOrder = value;

      var value = TIFFEncoding.NONE;
      var str = _toString(fsOpts.tiffEncoding)
      if (str.match(/none/i)) {
        value = TIFFEncoding.NONE;
      } else if (str.match(/lzw/i)) {
        value = TIFFEncoding.TIFFLZW;
      } else if (str.match(/zip/i)) {
        value = TIFFEncoding.TIFFZIP;
      } else if (str.match(/jpg|jpeg/i)) {
        value = TIFFEncoding.JPEG;
      }
      saveOpts.imageCompression = value;

      saveOpts.embedColorProfile = toBoolean(fsOpts.tiffEmbedColorProfile);
      saveOpts.flattenImage = toBoolean(fsOpts.flattenImage);
      break;
    }
    case "dng": {
    }
    default: {
      Error.runtimeError(9001, "Internal Error: Unknown file type: " +
                         fs.fileType);
    }
  }

  return saveOpts;
};

FileSaveOptionsType = function(fileType, menu, saveType, optionsType) {
  var self = this;

  self.fileType = fileType;    // the file extension
  self.menu = menu;
  self.saveType = saveType;
  self.optionsType = optionsType;
};
FileSaveOptionsType.prototype.typename = "FileSaveOptionsType";

FileSaveOptionsTypes = [];
FileSaveOptionsTypes._add = function(fileType, menu, saveType, optionsType) {
  var fsot = new FileSaveOptionsType(fileType, menu, saveType, optionsType);
  FileSaveOptionsTypes.push(fsot);
  FileSaveOptionsTypes[fileType] = fsot;
};
FileSaveOptionsTypes._init = function() {
  if (!isPhotoshop()) {
    return;
  }

  if (FileSaveOptions._enableSAS) {
    FileSaveOptionsTypes._add("SaS", "Same as Source", undefined, undefined);
  }

  FileSaveOptionsTypes._add("bmp", "Bitmap (BMP)", SaveDocumentType.BMP,
                            BMPSaveOptions);
  FileSaveOptionsTypes._add("gif", "GIF", SaveDocumentType.COMPUSERVEGIF,
                            GIFSaveOptions);
  FileSaveOptionsTypes._add("jpg", "JPEG", SaveDocumentType.JPEG,
                            JPEGSaveOptions);
  FileSaveOptionsTypes._add("psd", "Photoshop PSD", SaveDocumentType.PHOTOSHOP,
                            PhotoshopSaveOptions);
  FileSaveOptionsTypes._add("eps", "Photoshop EPS",
                            SaveDocumentType.PHOTOSHOPEPS, EPSSaveOptions);
  FileSaveOptionsTypes._add("pdf", "Photoshop PDF",
                            SaveDocumentType.PHOTOSHOPPDF, PDFSaveOptions);
  FileSaveOptionsTypes._add("png", "PNG", SaveDocumentType.PNG,
                            PNGSaveOptions);
  FileSaveOptionsTypes._add("tga", "Targa", SaveDocumentType.TARGA,
                            TargaSaveOptions);
  FileSaveOptionsTypes._add("tif", "TIFF", SaveDocumentType.TIFF,
                            TiffSaveOptions);

  if (FileSaveOptions._enableDNG) {
    FileSaveOptionsTypes._add("dng", "DNG", undefined, undefined);
  }
};
FileSaveOptionsTypes._init();

// XXX remove file types _before_ creating a FS panel!
FileSaveOptionsTypes.remove = function(ext) {
  var ar = FileSaveOptionsTypes;
  var fsot = ar[ext];
  if (fsot) {
    for (var i = 0; i < ar.length; i++) {
      if (ar[i] == fsot) {
        ar.splice(i, 1);
        break;
      }
    }
    delete ar[ext];
  }
};

FileSaveOptions.createFileSavePanel = function(pnl, ini) {
  var win = pnl.window;
  pnl.mgr = this;

  var menuElements = [];

  for (var i = 0; i < FileSaveOptionsTypes.length; i++) {
    menuElements.push(FileSaveOptionsTypes[i].menu);
  }

  var w = pnl.bounds[2] - pnl.bounds[0];
  var xofs = 0;
  var y = 0;

  var opts = new FileSaveOptions(ini);

  if (pnl.type == 'panel') {
    xofs += 5;
    y += 10;
  }
  pnl.text = "Save Options";

  var tOfs = 3;

  var x = xofs;
  // tpr needs zstring
  var w = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs], 'File Type:');
  x += w.bounds.width + (isWindows() ? 5 : 15);
  pnl.fileType = pnl.add('dropdownlist', [x,y,x+150,y+22], menuElements);

  var ftype = opts.fileType || opts.fileSaveType || "jpg";

  var ft = psx.getByProperty(FileSaveOptionsTypes,
                             "fileType",
                             ftype);
  pnl.fileType.selection = pnl.fileType.find(ft.menu);

  x += pnl.fileType.bounds.width + 10;
  // tpr needs zstring
  pnl.saveForWeb = pnl.add('checkbox', [x,y,x+135,y+22], 'Save for Web');
  pnl.saveForWeb.visible = false;
  pnl.saveForWeb.value = false;
  // tpr needs zstring
  pnl.flattenImage = pnl.add('checkbox', [x,y,x+135,y+22], 'Flatten Image');
  pnl.flattenImage.visible = false;
  pnl.flattenImage.value = false;

  y += 30;
  var yofs = y;

  x = xofs;

  //=========================== Same as Source ===========================
  if (FileSaveOptionsTypes["SaS"]) {
    pnl["SaS"] = [];
  }
  
  //=============================== Bitmap ===============================
  if (FileSaveOptionsTypes["bmp"]) {
    pnl.bmpAlphaChannels = pnl.add('checkbox', [x,y,x+125,y+22],
                                   "Alpha Channels");

    x += 150;
    var bmpDepthMenu = ["1", "4", "8", "16", "24", "32"];
    // tpr needs zstring
    pnl.bmpDepthLabel = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs],
                                'Bit Depth:');
    x += 65;
    pnl.bmpDepth = pnl.add('dropdownlist', [x,y,x+55,y+22], bmpDepthMenu);
    pnl.bmpDepth.selection = pnl.bmpDepth.find("24");

    pnl.bmpDepth.find("1")._value = BMPDepthType.ONE;
    pnl.bmpDepth.find("4")._value = BMPDepthType.FOUR;
    pnl.bmpDepth.find("8")._value = BMPDepthType.EIGHT;
    pnl.bmpDepth.find("16")._value = BMPDepthType.SIXTEEN;
    pnl.bmpDepth.find("24")._value = BMPDepthType.TWENTYFOUR;
    pnl.bmpDepth.find("32")._value = BMPDepthType.THIRTYTWO;

    x = xofs;
    y += 30;
    // tpr needs zstring
    pnl.bmpRLECompression = pnl.add('checkbox', [x,y,x+145,y+22],
                                    "RLE Compression");

    pnl.bmp = ["bmpAlphaChannels", "bmpDepthLabel", "bmpDepth",
               "bmpRLECompression"];

    pnl.bmpAlphaChannels.value = toBoolean(opts.bmpAlphaChannels);
    var it = pnl.bmpDepth.find(opts.bmpDepth.toString());
    if (it) {
      pnl.bmpDepth.selection = it;
    }
    pnl.bmpRLECompression.value = toBoolean(opts.bmpRLECompression);

    y = yofs;
    x = xofs;
  }


  //=============================== GIF ===============================
  if (FileSaveOptionsTypes["gif"]) {
    pnl.gifTransparency = pnl.add('checkbox', [x,y,x+125,y+22],
                                  "Transparency");

    x += 125;
    pnl.gifInterlaced = pnl.add('checkbox', [x,y,x+125,y+22],
                                "Interlaced");

    x += 125;
    pnl.gifColorsLabel = pnl.add('statictext', [x,y+tOfs,x+55,y+22+tOfs],
                                  'Colors:');

    x += 60;
    pnl.gifColors = pnl.add('edittext', [x,y,x+55,y+22], "256");
    pnl.gifColors.onChanging = psx.numericKeystrokeFilter;
    pnl.gifColors.onChange = function() {
      var pnl = this.parent;
      var n = toNumber(pnl.gifColors.text || 256);
      if (n < 2)   { n = 2; }
      if (n > 256) { n = 256; }
      pnl.gifColors.text = n;
    }

    pnl.gif = ["gifTransparency", "gifInterlaced", "gifColors", "gifColorsLabel",
               "saveForWeb"];

    pnl.gifTransparency.value = toBoolean(opts.gifTransparency);
    pnl.gifInterlaced.value = toBoolean(opts.gifInterlaced);
    pnl.gifColors.text = toNumber(opts.gifColors || 256);

    pnl.saveForWeb.value = toBoolean(opts.saveForWeb);
    y = yofs;
    x = xofs;
  }


  //=============================== JPG ===============================
  if (FileSaveOptionsTypes["jpg"]) {
    pnl.jpgQualityLabel = pnl.add('statictext', [x,y+tOfs,x+55,y+22+tOfs],
                                  'Quality:');
    x += isWindows() ? 65 : 75;
    var jpqQualityMenu = ["1","2","3","4","5","6","7","8","9","10","11","12"];
    pnl.jpgQuality = pnl.add('dropdownlist', [x,y,x+55,y+22], jpqQualityMenu);
    pnl.jpgQuality.selection = pnl.jpgQuality.find("10");

    y += 30;
    x = xofs;
    pnl.jpgEmbedColorProfile = pnl.add('checkbox', [x,y,x+155,y+22],
                                       "Embed Color Profile");

    y = yofs;
    x += 150;

    var jpgFormatMenu = ["Standard", "Progressive", "Optimized"];
    pnl.jpgFormatLabel = pnl.add('statictext', [x,y+tOfs,x+50,y+22+tOfs],
                                 'Format:');
    x += 55;
    pnl.jpgFormat = pnl.add('dropdownlist', [x,y,x+110,y+22], jpgFormatMenu);
    pnl.jpgFormat.selection = pnl.jpgFormat.find("Standard");

    pnl.jpgFormat.find("Standard")._value = FormatOptions.STANDARDBASELINE;
    pnl.jpgFormat.find("Progressive")._value = FormatOptions.PROGRESSIVE;
    pnl.jpgFormat.find("Optimized")._value = FormatOptions.OPTIMIZEDBASELINE;

    y += 30;
    x = xofs + 150;
    pnl.jpgConvertToSRGB = pnl.add('checkbox', [x,y,x+145,y+22],
                                   "Convert to sRGB");

    pnl.jpg = ["jpgQualityLabel", "jpgQuality", "jpgEmbedColorProfile",
               "jpgFormatLabel", "jpgFormat", "jpgConvertToSRGB", "saveForWeb" ];

    var it = pnl.jpgQuality.find(opts.jpgQuality.toString());
    if (it) {
      pnl.jpgQuality.selection = it;
    }
    pnl.jpgEmbedColorProfile.value = toBoolean(opts.jpgEmbedColorProfile);
    var it = pnl.jpgFormat.find(opts.jpgFormat);
    if (it) {
      pnl.jpgFormat.selection = it;
    }
    pnl.jpgConvertToSRGB.value = toBoolean(opts.jpgConvertToSRGB);

    pnl.saveForWeb.value = toBoolean(opts.saveForWeb);

    x = xofs;
    y = yofs;
  }


  //=============================== PSD ===============================
  if (FileSaveOptionsTypes["psd"]) {
    pnl.psdAlphaChannels = pnl.add('checkbox', [x,y,x+125,y+22],
                                   "Alpha Channels");

    y += 30;
    pnl.psdEmbedColorProfile = pnl.add('checkbox', [x,y,x+155,y+22],
                                       "Embed Color Profile");

    y = yofs;
    x = xofs + 150;

    pnl.psdLayers = pnl.add('checkbox', [x,y,x+125,y+22],
                          "Layers");

    x += 95;
    var psdBPCMenu = ["1", "8", "16", "32"];
    // tpr needs zstring
    pnl.psdBPCLabel = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs],
                                'Bit Depth:');
    x += 65;
    pnl.psdBPC = pnl.add('dropdownlist', [x,y,x+55,y+22], psdBPCMenu);
    pnl.psdBPC.selection = pnl.psdBPC.find("16");

    pnl.psdBPC.find("1")._value  = BitsPerChannelType.ONE;
    pnl.psdBPC.find("8")._value  = BitsPerChannelType.EIGHT;
    pnl.psdBPC.find("16")._value = BitsPerChannelType.SIXTEEN;
    pnl.psdBPC.find("32")._value = BitsPerChannelType.THIRTYTWO;

    x = xofs + 150;
    y += 30;
    pnl.psdMaximizeCompatibility = pnl.add('checkbox', [x,y,x+175,y+22],
                                           "Maximize Compatibility");

    pnl.psd = ["psdAlphaChannels", "psdEmbedColorProfile",
               "psdLayers", "psdMaximizeCompatibility",
               "psdBPCLabel", "psdBPC",
               "flattenImage"];

    pnl.psdAlphaChannels.value = toBoolean(opts.psdAlphaChannels);
    pnl.psdEmbedColorProfile.value = toBoolean(opts.psdEmbedColorProfile);
    pnl.psdLayers.value = toBoolean(opts.psdLayers);
    pnl.psdMaximizeCompatibility.value =
       toBoolean(opts.psdMaximizeCompatibility);

    pnl.flattenImage.value = toBoolean(opts.flattenImage);

    x = xofs;
    y = yofs;
  }

  //=============================== EPS ===============================
  if (FileSaveOptionsTypes["eps"]) {
    var epsEncodingMenu = ["ASCII", "Binary", "JPEG High", "JPEG Med",
                           "JPEG Low", "JPEG Max"];
    pnl.epsEncodingLabel = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs],
                                 'Encoding:');
    x += 65;
    pnl.epsEncoding = pnl.add('dropdownlist',
                              [x,y,x+100,y+22],
                              epsEncodingMenu);
    pnl.epsEncoding.selection = pnl.epsEncoding.find("Binary");

    pnl.epsEncoding.find("ASCII")._value = SaveEncoding.ASCII;
    pnl.epsEncoding.find("Binary")._value = SaveEncoding.BINARY;
    pnl.epsEncoding.find("JPEG High")._value = SaveEncoding.JPEGHIGH;
    pnl.epsEncoding.find("JPEG Low")._value = SaveEncoding.JPEGLOW;
    pnl.epsEncoding.find("JPEG Max")._value = SaveEncoding.JPEGMAXIMUM;
    pnl.epsEncoding.find("JPEG Med")._value = SaveEncoding.JPEGMEDIUM;

    x = xofs;
    y += 30;
    pnl.epsEmbedColorProfile = pnl.add('checkbox', [x,y,x+155,y+22],
                                       "Embed Color Profile");

    pnl.eps = ["epsEncodingLabel", "epsEncoding", "epsEmbedColorProfile"];

    var it = pnl.epsEncoding.find(opts.epsEncoding);
    if (it) {
      pnl.epsEncoding.selection = it;
    }
    pnl.epsEmbedColorProfile.value = toBoolean(opts.epsEmbedColorProfile);

    x = xofs;
    y = yofs;
  }


  //=============================== PDF ===============================
  if (FileSaveOptionsTypes["pdf"]) {
    pnl.pdf = ["pdfEmbedColorProfile"];

    x = xofs;
    y = yofs;

    x = xofs;
    y += 30;
    pnl.pdfEmbedColorProfile = pnl.add('checkbox', [x,y,x+155,y+22],
                                       "Embed Color Profile");
    pnl.pdfEmbedColorProfile.value = toBoolean(opts.pdfEmbedColorProfile);

    x = xofs;
    y = yofs;
  }


  //=============================== PNG ===============================
  if (FileSaveOptionsTypes["png"]) {
    pnl.pngInterlaced = pnl.add('checkbox', [x,y,x+125,y+22],
                                "Interlaced");

    pnl.png = ["pngInterlaced", "saveForWeb"];

    pnl.pngInterlaced.value = toBoolean(opts.pngInterlaced);

    pnl.saveForWeb.value = toBoolean(opts.saveForWeb);

    x = xofs;
    y = yofs;
  }


  //=============================== TGA ===============================
  if (FileSaveOptionsTypes["tga"]) {
    pnl.tgaAlphaChannels = pnl.add('checkbox', [x,y,x+125,y+22],
                                   "Alpha Channels");

    x += 150;
    var tgaDepthMenu = ["16", "24", "32"];
    // tpr needs zstring
    pnl.tgaDepthLabel = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs],
                                'Bit Depth:');
    x += 65;
    pnl.tgaDepth = pnl.add('dropdownlist', [x,y,x+55,y+22], tgaDepthMenu);
    pnl.tgaDepth.selection = pnl.tgaDepth.find("24");

    pnl.tgaDepth.find("16")._value = TargaBitsPerPixels.SIXTEEN;
    pnl.tgaDepth.find("24")._value = TargaBitsPerPixels.TWENTYFOUR;
    pnl.tgaDepth.find("32")._value = TargaBitsPerPixels.THIRTYTWO;

    x = xofs; 
    y += 30;

    pnl.tgaRLECompression = pnl.add('checkbox', [x,y,x+145,y+22],
                                    "RLE Compression");

    pnl.tga = ["tgaAlphaChannels", "tgaDepthLabel", "tgaDepth",
               "tgaRLECompression"];

    pnl.tgaAlphaChannels.value = toBoolean(opts.tgaAlphaChannels);
    pnl.tgaRLECompression.value = toBoolean(opts.tgaRLECompression);

    x = xofs;
    y = yofs;
  }


  //=============================== TIFF ===============================
  if (FileSaveOptionsTypes["tif"]) {
    var tiffEncodingMenu = ["None", "LZW", "ZIP", "JPEG"];
    pnl.tiffEncodingLabel = pnl.add('statictext', [x,y+tOfs,x+60,y+22+tOfs],
                                    'Encoding:');
    x += 65;
    pnl.tiffEncoding = pnl.add('dropdownlist', [x,y,x+75,y+22],
                               tiffEncodingMenu);
    pnl.tiffEncoding.selection = pnl.tiffEncoding.find("None");

    pnl.tiffEncoding.find("None")._value = TIFFEncoding.NONE;
    pnl.tiffEncoding.find("LZW")._value = TIFFEncoding.TIFFLZW;
    pnl.tiffEncoding.find("ZIP")._value = TIFFEncoding.TIFFZIP;
    pnl.tiffEncoding.find("JPEG")._value = TIFFEncoding.JPEG;

    x += 90;

    var tiffByteOrderMenu = ["IBM", "MacOS"];
    pnl.tiffByteOrderLabel = pnl.add('statictext', [x,y+tOfs,x+65,y+22+tOfs],
                                     'ByteOrder:');
    x += 70;
    pnl.tiffByteOrder = pnl.add('dropdownlist', [x,y,x+85,y+22],
                                tiffByteOrderMenu);
    var bo = (isWindows() ? "IBM" : "MacOS");
    pnl.tiffByteOrder.selection = pnl.tiffByteOrder.find(bo);

    pnl.tiffByteOrder.find("IBM")._value = ByteOrder.IBM;
    pnl.tiffByteOrder.find("MacOS")._value = ByteOrder.MACOS;

    x = xofs;
    y += 30;
    pnl.tiffEmbedColorProfile = pnl.add('checkbox', [x,y,x+155,y+22],
                                        "Embed Color Profile");

    pnl.tif = ["tiffEncodingLabel", "tiffEncoding", "tiffByteOrderLabel",
               "tiffByteOrder", "tiffEmbedColorProfile", "flattenImage"];

    pnl.dng = [];

    var it = pnl.tiffEncoding.find(opts.tiffEncoding);
    if (it) {
      pnl.tiffEncoding.selection = it;
    }
    var it = pnl.tiffByteOrder.find(opts.tiffByteOrder);
    if (it) {
      pnl.tiffByteOrder.selection = it;
    }
    pnl.tiffEmbedColorProfile.value = toBoolean(opts.tiffEmbedColorProfile);

    pnl.flattenImage.value = toBoolean(opts.flattenImage);
  }
  
  pnl.fileType.onChange = function() {
    var pnl = this.parent;
    var ftsel = pnl.fileType.selection.index;
    var ft = FileSaveOptionsTypes[ftsel];

    for (var i = 0; i < FileSaveOptionsTypes.length; i++) {
      var fsType = FileSaveOptionsTypes[i];
      var parts = pnl[fsType.fileType];

      for (var j = 0; j < parts.length; j++) {
        var part = parts[j];
        pnl[part].visible = (fsType == ft);
      }
    }

    var fsType = ft.fileType;
    pnl.saveForWeb.visible = (pnl[fsType].contains("saveForWeb"));
    pnl.flattenImage.visible = (pnl[fsType].contains("flattenImage"));
    pnl._onChange();
  };

  pnl._onChange = function() {
    var self = this;
    if (self.onChange) {
      self.onChange();
    }
  };

  if (false) {
    y = yofs;
    x = 300;
    var btn = pnl.add('button', [x,y,x+50,y+22], "Test");
    btn.onClick = function() {
      try {
        var pnl = this.parent;
        var mgr = pnl.mgr;

        var opts = {};
        mgr.validateFileSavePanel(pnl, opts);
        alert(listProps(opts));
        alert(listProps(FileSaveOptions.convert(opts)));

      } catch (e) {
        var msg = psx.exceptionMessage(e);
        LogFile.write(msg);
        alert(msg);
      }
    };
  }

  pnl.fileType.onChange();

  pnl.getFileSaveType = function() {
    var pnl = this;
    var fstype = '';
    if (pnl.fileType.selection) {
      var fsSel = pnl.fileType.selection.index;
      var fs = FileSaveOptionsTypes[fsSel];
      fstype = fs.fileType;
    }
    return fstype;
  };

  pnl.updateSettings = function(ini) {
    var pnl = this;

    function _select(m, s, def) {
      var it = m.find(s.toString());
      if (!it && def != undefined) {
        it = m.items[def];
      }
      if (it) {
        m.selection = it;
      }
    }

    var opts = new FileSaveOptions(ini);
    var ftype = opts.fileType || opts.fileSaveType || "jpg";

    var ft = psx.getByProperty(FileSaveOptionsTypes,
                               "fileType",
                               ftype);

    try {
      pnl.fileType.selection = pnl.fileType.find(ft.menu);

    } catch (e) {
      var lstr = ("fileType: %s\nfileSaveType: %s\nfy: %s",
                  opts.fileType || "<>", opts.fileSaveType || "<>",
                  ft || "<>");
      LogFile.write(lstr);
      alert("Debug info has been written to the Image Processor Pro.log file" +
      "Please email that file and you settings XML file to xbytor@gmail.com");
      throw e;
    }

    if (FileSaveOptionsTypes["bmp"]) {
      pnl.bmpAlphaChannels.value = toBoolean(opts.bmpAlphaChannels);
      _select(pnl.bmpDepth, opts.bmpDepth.toString(), 0);
      pnl.bmpRLECompression.value = toBoolean(opts.bmpRLECompression);
    }

    if (FileSaveOptionsTypes["gif"]) {
      pnl.gifTransparency.value = toBoolean(opts.gifTransparency);
      pnl.gifInterlaced.value = toBoolean(opts.gifInterlaced);
      pnl.gifColors.text = toNumber(opts.gifColors || 256);
      pnl.saveForWeb.value = toBoolean(opts.saveForWeb);
    }

    if (FileSaveOptionsTypes["jpg"]) {
      _select(pnl.jpgQuality, opts.jpgQuality.toString(), 8);
      pnl.jpgEmbedColorProfile.value = toBoolean(opts.jpgEmbedColorProfile);
      _select(pnl.jpgFormat, opts.jpgFormat, 0);
      pnl.jpgConvertToSRGB.value = toBoolean(opts.jpgConvertToSRGB);
      pnl.saveForWeb.value = toBoolean(opts.saveForWeb);
    }

    if (FileSaveOptionsTypes["psd"]) {
      pnl.psdAlphaChannels.value = toBoolean(opts.psdAlphaChannels);
      pnl.psdEmbedColorProfile.value = toBoolean(opts.psdEmbedColorProfile);
      pnl.psdLayers.value = toBoolean(opts.psdLayers);
      pnl.psdMaximizeCompatibility.value =
          toBoolean(opts.psdMaximizeCompatibility);
      pnl.flattenImage.value = toBoolean(opts.flattenImage);
      _select(pnl.psdBPC, opts.psdBPC.toString(), 2);
    }
    
    if (FileSaveOptionsTypes["eps"]) {
      _select(pnl.epsEncoding, opts.epsEncoding, 0);
      pnl.epsEmbedColorProfile.value = toBoolean(opts.epsEmbedColorProfile);
    }
    
    if (FileSaveOptionsTypes["pdf"]) {
      pnl.pdfEmbedColorProfile.value = toBoolean(opts.pdfEmbedColorProfile);
    }
    
    if (FileSaveOptionsTypes["png"]) {
      pnl.pngInterlaced.value = toBoolean(opts.pngInterlaced);
      pnl.saveForWeb.value = toBoolean(opts.saveForWeb);
    }
    
    if (FileSaveOptionsTypes["tga"]) {
      pnl.tgaAlphaChannels.value = toBoolean(opts.tgaAlphaChannels);
      _select(pnl.tgaDepth, opts.tgaDepth.toString(), 1);
      pnl.tgaRLECompression.value = toBoolean(opts.tgaRLECompression);
    }
    
    if (FileSaveOptionsTypes["tif"]) {
      _select(pnl.tiffEncoding, opts.tiffEncoding, 0);
      _select(pnl.tiffByteOrder, opts.tiffByteOrder, 0);
      pnl.tiffEmbedColorProfile.value = toBoolean(opts.tiffEmbedColorProfile);
      pnl.flattenImage.value = toBoolean(opts.flattenImage);
    }

    pnl.fileType.onChange();
  }

  return pnl;
};
FileSaveOptions.validateFileSavePanel = function(pnl, opts) {
  var win = pnl.window;

  // XXX This function needs to remove any prior file save
  // options and only set the ones needed for the
  // selected file type

  var fsOpts = new FileSaveOptions();
  for (var idx in fsOpts) {
    if (idx in opts) {
      delete opts[idx];
    }
  }

  var fsSel = pnl.fileType.selection.index;
  var fs = FileSaveOptionsTypes[fsSel];

  opts.fileSaveType = fs.fileType;
  opts._saveDocumentType = fs.saveType;

  if (!fs.optionsType) {
    opts._saveOpts = undefined;
    return;
  }

  var saveOpts = new fs.optionsType();

  switch (fs.fileType) {
    case "bmp": {
      saveOpts.rleCompression = pnl.bmpRLECompression.value;
      var bpc = pnl.bmpDepth.selection._value;
      saveOpts.depth = bpc;
      saveOpts.alphaChannels = pnl.bmpAlphaChannels.value;

      opts.bmpRLECompression = pnl.bmpRLECompression.value;
      opts.bmpDepth = Number(pnl.bmpDepth.selection.text);
      opts.bmpAlphaChannels = pnl.bmpAlphaChannels.value;
      break;
    }
    case "gif": {
      saveOpts.transparency = pnl.gifTransparency.value;
      saveOpts.interlaced = pnl.gifInterlaced.value;
      var colors = toNumber(pnl.gifColors.text || 256);
      if (colors < 2)   { colors = 2; }
      if (colors > 256) { colors = 256; }
      saveOpts.colors = colors; 
      saveOpts._saveForWeb = pnl.saveForWeb.value;

      opts.gifTransparency = pnl.gifTransparency.value;
      opts.gifInterlaced = pnl.gifInterlaced.value;
      opts.gifColors = colors;
      opts.saveForWeb = pnl.saveForWeb.value;
      break;
    }
    case "jpg": {
      saveOpts.quality = Number(pnl.jpgQuality.selection.text);
      saveOpts.embedColorProfile = pnl.jpgEmbedColorProfile.value;
      saveOpts.formatOptions = pnl.jpgFormat.selection._value;
      saveOpts._convertToSRGB = pnl.jpgConvertToSRGB.value;
      saveOpts._saveForWeb = pnl.saveForWeb.value;

      opts.jpgQuality = Number(pnl.jpgQuality.selection.text);
      opts.jpgEmbedColorProfile = pnl.jpgEmbedColorProfile.value;
      opts.jpgFormat = pnl.jpgFormat.selection.text;
      opts.jpgConvertToSRGB = pnl.jpgConvertToSRGB.value;
      opts.saveForWeb = pnl.saveForWeb.value;
      break;
    }
    case "psd": {
      saveOpts.alphaChannels = pnl.psdAlphaChannels.value;
      saveOpts.embedColorProfile = pnl.psdEmbedColorProfile.value;
      saveOpts.layers = pnl.psdLayers.value;
      saveOpts.maximizeCompatibility = pnl.psdMaximizeCompatibility.value;

      opts.psdAlphaChannels = pnl.psdAlphaChannels.value;
      opts.psdEmbedColorProfile = pnl.psdEmbedColorProfile.value;
      opts.psdLayers = pnl.psdLayers.value;
      opts.psdMaximizeCompatibility = pnl.psdMaximizeCompatibility.value;
      opts.flattenImage = pnl.flattenImage.value;
      opts._bpc = pnl.psdBPC.selection._value;
      opts.psdBPC = FileSaveOptions.convertBitsPerChannelType(opts._bpc);
      break;
    }
    case "eps": {
      saveOpts.encoding = pnl.epsEncoding.selection._value;
      saveOpts.embedColorProfile = pnl.epsEmbedColorProfile.value;

      opts.epsEncoding = pnl.epsEncoding.selection.text;
      opts.epsEmbedColorProfile = pnl.epsEmbedColorProfile.value;
      break;
    }
    case "pdf": {
      saveOpts.embedColorProfile = pnl.pdfEmbedColorProfile.value;

      opts.pdfEmbedColorProfile = pnl.pdfEmbedColorProfile.value;
      break;
    }
    case "png": {
      saveOpts.interlaced = pnl.pngInterlaced.value;
      saveOpts._saveForWeb = pnl.saveForWeb.value;

      opts.pngInterlaced = pnl.pngInterlaced.value;
      opts.saveForWeb = pnl.saveForWeb.value;
      break;
    }
    case "tga": {
      saveOpts.alphaChannels = pnl.tgaAlphaChannels.value;
      var bpc = pnl.tgaDepth.selection._value;
      saveOpts.resolution = bpc;
      saveOpts.rleCompression = pnl.tgaRLECompression.value;

      opts.tgaAlphaChannels = pnl.tgaAlphaChannels.value;
      opts.tgaDepth = Number(pnl.tgaDepth.selection.text);
      opts.tgaRLECompression = pnl.tgaRLECompression.value;
      break;
    }
    case "tif": {
      saveOpts.byteOrder = pnl.tiffByteOrder.selection._value;
      saveOpts.imageCompression = pnl.tiffEncoding.selection._value;
      saveOpts.embedColorProfile = pnl.tiffEmbedColorProfile.value;

      opts.tiffByteOrder = pnl.tiffByteOrder.selection.text;
      opts.tiffEncoding = pnl.tiffEncoding.selection.text;
      opts.tiffEmbedColorProfile = pnl.tiffEmbedColorProfile.value;
      opts.flattenImage = pnl.flattenImage.value;
      break;
    }
    default:
      Error.runtimeError(9001, "Internal Error: Unknown file type: " +
                         fs.fileType);
  }

  opts._saveOpts = saveOpts;

  return;
};

//============================= FileNaming ====================================
//
// Function: _getFontArray
// Description: 
// Input:  <none> 
// Return: an array of font info objects created by _getFontTable
//
//
FileNamingOptions = function(obj, prefix) {
  var self = this;

  self.fileNaming = [];      // array of FileNamingType and/or String
  self.startingSerial = 1;
  self.windowsCompatible = isWindows();
  self.macintoshCompatible = isMac();
  self.unixCompatible = true;

  if (obj) {
    if (prefix == undefined) {
      prefix = '';
    }
    var props = FileNamingOptions.props;
    for (var i = 0; i < props.length; i++) {
      var name = props[i];
      var oname = prefix + name;
      if (oname in obj) {
        self[name] = obj[oname];
      }
    }

    if (self.fileNaming.constructor == String) {
      self.fileNaming = self.fileNaming.split(',');

      // remove "'s from around custom text
    }
  }
};
FileNamingOptions.prototype.typename = FileNamingOptions;
FileNamingOptions.props = ["fileNaming", "startingSerial", "windowsCompatible",
                           "macintoshCompatible", "unixCompatible"];

FileNamingOptions.prototype.format = function(file, cdate) {
  var self = this;
  var str  = '';

  file = psx.convertFptr(file);

  if (!cdate) {
    cdate = file.created || new Date();
  }

  var fname = file.strf("%f");
  var ext = file.strf("%e");

  var parts = self.fileNaming;

  if (parts.constructor == String) {
    parts = parts.split(',');
  }

  var serial = self.startingSerial;
  var aCode = 'a'.charCodeAt(0);
  var ACode = 'A'.charCodeAt(0);
  var hasSerial = false;

  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    var fnel = FileNamingElements.getByName(p);

    if (!fnel) {
      str += p;
      continue;
    }

    var s = '';
    switch (fnel.type) {
    case FileNamingType.DOCUMENTNAMEMIXED: s = fname; break;
    case FileNamingType.DOCUMENTNAMELOWER: s = fname.toLowerCase(); break;
    case FileNamingType.DOCUMENTNAMEUPPER: s = fname.toUpperCase(); break;
    case FileNamingType.SERIALNUMBER1:
      s = "%d".sprintf(serial);
      hasSerial = true;
      break;
    case FileNamingType.SERIALNUMBER2:
      s = "%02d".sprintf(serial);
      hasSerial = true;
      break;
    case FileNamingType.SERIALNUMBER3:
      s = "%03d".sprintf(serial);
      hasSerial = true;
      break;
    case FileNamingType.SERIALNUMBER4:
      s = "%04d".sprintf(serial);
      hasSerial = true;
      break;
    case FileNamingElement.SERIALNUMBER5:
      s = "%05d".sprintf(serial);
      hasSerial = true;
      break;
    case FileNamingType.EXTENSIONLOWER:    s = '.' + ext.toLowerCase(); break;
    case FileNamingType.EXTENSIONUPPER:    s = '.' + ext.toUpperCase(); break;
    case FileNamingType.SERIALLETTERLOWER:
      s = FileNamingOptions.nextAlphaIndex(aCode, serial);
      hasSerial = true;
      break;
    case FileNamingType.SERIALLETTERUPPER:
      s = FileNamingOptions.nextAlphaIndex(ACode, serial);
      hasSerial = true;
      break;
    }

    if (s) {
      str += s;
      continue;
    }

    var fmt = '';
    switch (fnel.type) {
    case FileNamingType.MMDDYY:   fmt = "%m%d%y"; break;
    case FileNamingType.MMDD:     fmt = "%m%d"; break;
    case FileNamingType.YYYYMMDD: fmt = "%Y%m%d"; break;
    case FileNamingType.YYMMDD:   fmt = "%y%m%d"; break;
    case FileNamingType.YYDDMM:   fmt = "%y%d%m"; break;
    case FileNamingType.DDMMYY:   fmt = "%d%m%y"; break;
    case FileNamingType.DDMM:     fmt = "%d%m"; break;
    }

    if (fmt) {
      str += cdate.strftime(fmt);
      continue;
    }
  }

  if (hasSerial) {
    serial++;
  }

  self._serial = serial;

  return str;
};

FileNamingOptions.nextAlphaIndex = function(base, idx) {
  var str = '';

  while (idx > 0) {
    idx--;
    var m = idx % 26;
    var idx = Math.floor(idx / 26);

    str = String.fromCharCode(m + base) + str;
  }

  return str;
};

FileNamingOptions.prototype.copyTo = function(opts, prefix) {
  var self = this;
  var props = FileNamingOptions.props;

  for (var i = 0; i < props.length; i++) {
    var name = props[i];
    var oname = prefix + name;
    opts[oname] = self[name];
    if (name == 'fileNaming' && self[name] instanceof Array) {
      opts[oname] = self[name].join(',');
    } else {
      opts[oname] = self[name];
    }
  }
};


// this array is folder into FileNamingElement
FileNamingOptions._examples =
  [ "",
    "Document",
    "document",
    "DOCUMENT",
    "1",
    "01",
    "001",
    "0001",
    "a",
    "A",
    "103107",
    "1031",
    "20071031",
    "071031",
    "073110",
    "311007",
    "3110",
    ".Psd",
    ".psd",
    ".PSD"
    ];

FileNamingOptions.prototype.getExample = function() {
  var self = this;
  var str = '';
  return str;
};

FileNamingElement = function(name, menu, type, sm, example) {
  var self = this;
  self.name = name;
  self.menu = menu;
  self.type = type;
  self.smallMenu = sm;
  self.example = (example || '');
};
FileNamingElement.prototype.typename = FileNamingElement;

FileNamingElements = [];
FileNamingElements._add = function(name, menu, type, sm, ex) {
  FileNamingElements.push(new FileNamingElement(name, menu, type, sm, ex));
}

FileNamingElement.NONE = "(None)";

FileNamingElement.SERIALNUMBER5 = {
  toString: function() { return "FileNamingElement.SERIALNUMBER5"; }
};

FileNamingElements._init = function() {

  FileNamingElements._add("", "", "", "", ""); // Same as (None)

  try {
    FileNamingType;
  } catch (e) {
    return;
  }

  // the names here correspond to the sTID symbols used when making
  // a Batch request via the ActionManager interface. Except for "Name",
  // which should be "Nm  ".
  // the names should be the values used when serializing to and from
  // an INI file.
  // A FileNamingOptions object needs to be defined.
  FileNamingElements._add("Name", ZStrs.DocumentName,
                          FileNamingType.DOCUMENTNAMEMIXED,
                          "Name", "Document");
  FileNamingElements._add("lowerCase", ZStrs.LCDocumentName,
                          FileNamingType.DOCUMENTNAMELOWER,
                          "name", "document");
  FileNamingElements._add("upperCase", ZStrs.UCDocumentName,
                          FileNamingType.DOCUMENTNAMEUPPER,
                          "NAME", "DOCUMENT");
  FileNamingElements._add("oneDigit", ZStrs.FN1Digit,
                          FileNamingType.SERIALNUMBER1,
                          "Serial #", "1");
  FileNamingElements._add("twoDigit", ZStrs.FN2Digit,
                          FileNamingType.SERIALNUMBER2,
                          "Serial ##", "01");
  FileNamingElements._add("threeDigit", ZStrs.FN3Digit,
                          FileNamingType.SERIALNUMBER3,
                          "Serial ###", "001");
  FileNamingElements._add("fourDigit", ZStrs.FN4Digit,
                          FileNamingType.SERIALNUMBER4,
                          "Serial ####", "0001");
  FileNamingElements._add("fiveDigit", ZStrs.FN5Digit,
                          FileNamingElement.SERIALNUMBER5,
                          "Serial #####", "00001");
  FileNamingElements._add("lowerCaseSerial", ZStrs.LCSerial,
                          FileNamingType.SERIALLETTERLOWER,
                          "Serial a", "a");
  FileNamingElements._add("upperCaseSerial", ZStrs.UCSerial,
                          FileNamingType.SERIALLETTERUPPER,
                          "Serial A", "A");
  FileNamingElements._add("mmddyy", ZStrs.Date_mmddyy,
                          FileNamingType.MMDDYY,
                          "mmddyy", "103107");
  FileNamingElements._add("mmdd", ZStrs.Date_mmdd,
                          FileNamingType.MMDD,
                          "mmdd", "1031");
  FileNamingElements._add("yyyymmdd", ZStrs.Date_yyyymmdd,
                          FileNamingType.YYYYMMDD,
                          "yyyymmdd", "20071031");
  FileNamingElements._add("yymmdd", ZStrs.Date_yymmdd,
                          FileNamingType.YYMMDD,
                          "yymmdd", "071031");
  FileNamingElements._add("yyddmm", ZStrs.Date_yyddmm,
                          FileNamingType.YYDDMM,
                          "yyddmm", "073110");
  FileNamingElements._add("ddmmyy", ZStrs.Date_ddmmyy,
                          FileNamingType.DDMMYY,
                          "ddmmyy", "311007");
  FileNamingElements._add("ddmm", ZStrs.Date_ddmm,
                          FileNamingType.DDMM,
                          "ddmm", "3110");
  FileNamingElements._add("lowerCaseExtension", ZStrs.LCExtension,
                          FileNamingType.EXTENSIONLOWER,
                          "ext", ".psd");
  FileNamingElements._add("upperCaseExtension", ZStrs.UCExtension,
                          FileNamingType.EXTENSIONUPPER,
                          "EXT", ".PSD");
};
FileNamingElements._init();
FileNamingElements.getByName = function(name) {
  return psx.getByName(FileNamingElements, name);
};

FileNamingOptions.CUSTOM_TEXT_CREATE = "Create";
FileNamingOptions.CUSTOM_TEXT_DELETE = "Delete";
FileNamingOptions.CUSTOM_TEXT_EDIT = "Edit";

FileNamingOptions.createFileNamingPanel = function(pnl, ini,
                                                   prefix,
                                                   useSerial,
                                                   useCompatibility,
                                                   columns) {
  var win = pnl.window;
  if (useSerial == undefined) {
    useSerial = false;
  }
  if (useCompatibility == undefined) {
    useCompatibility = false;
  }
  if (columns == undefined) {
    columns = 3;
  } else {
    if (columns != 2 && columns != 3) {
      Error.runtimeError(9001, "Internal Error: Bad column spec for " +
                         "FileNaming panel");
    }
  }

  pnl.fnmenuElements = [];
  for (var i = 0; i < FileNamingElements.length; i++) {
    var fnel = FileNamingElements[i];
    pnl.fnmenuElements.push(fnel.menu);
  }
  var extrasMenuEls = [
    "-",
    ZStrs.CreateCustomText,
    ZStrs.EditCustomText,
//     ZStrs.DeleteCustomText,
    "-",
    FileNamingElement.NONE,
    ];
  for (var i = 0; i < extrasMenuEls.length; i++) {
    pnl.fnmenuElements.push(extrasMenuEls[i]);
  }

  pnl.win = win;
  if (prefix == undefined) {
    prefix = '';
  }
  pnl.prefix = prefix;

  var w = pnl.bounds[2] - pnl.bounds[0];
  var xofs = 0;
  var y = 0;

  if (pnl.type == 'panel') {
    xofs += 5;
    y += 10;
  }
  pnl.text = ZStrs.FileNaming;

  var tOfs = 3;

  if (columns == 2) {
    var menuW = (w - 50)/2;

  } else {
    var menuW = (w - 65)/3;
  }

  var opts = new FileNamingOptions(ini, pnl.prefix);

  x = xofs;

  pnl.exampleLabel = pnl.add('statictext', [x,y+tOfs,x+70,y+22+tOfs],
                             ZStrs.ExampleLabel);
  x += 70;
  pnl.example = pnl.add('statictext', [x,y+tOfs,x+250,y+22+tOfs], '');
  y += 30;
  x = xofs;

  pnl.menus = [];

  pnl.menus[0]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  x += menuW + 5;
  pnl.add('statictext', [x,y+tOfs,x+10,y+22+tOfs], '+');

  x += 15;

  pnl.menus[1]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  x += menuW + 5;
  pnl.add('statictext', [x,y+tOfs,x+10,y+22+tOfs], '+');

  if (columns == 2) {
    y += 30;
    x = xofs;
  } else {
    x += 15;
  }

  pnl.menus[2]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  x += menuW + 5;
  pnl.add('statictext', [x,y+tOfs,x+10,y+22+tOfs], '+');

  if (columns == 3) {
    y += 30;
    x = xofs;

  } else {
    x += 15;
  }

  pnl.menus[3]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  x += menuW + 5;
  pnl.add('statictext', [x,y+tOfs,x+10,y+22+tOfs], '+');

  if (columns == 2) {
    y += 30;
    x = xofs;

  } else {
    x += 15;
  }

  pnl.menus[4]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  x += menuW + 5;
  pnl.add('statictext', [x,y+tOfs,x+10,y+22+tOfs], '+');

  x += 15;

  pnl.menus[5]  = pnl.add('dropdownlist', [x,y,x+menuW,y+22],
                          pnl.fnmenuElements);
  y += 30;
  x = xofs;

  pnl.addMenuElement = function(text) {
    var pnl = this;
    for (var i = 0; i < 6; i++) {
      var vmenu = pnl.menus[i];
      vmenu.add('item', text);
    }
  }

  pnl.addCustomMenuElement = function(text) {
    var pnl = this;
    if (text == '-') {
      text = '- ';
    }
    for (var i = 0; i < 6; i++) {
      var vmenu = pnl.menus[i];
      var it = menu.find(text);
      if (it == undefined) {
        vmenu.add('item', text);
      }
    }
  }

  pnl.useSerial = useSerial;
  if (useSerial) {
    pnl.startingSerialLbl = pnl.add('statictext', [x,y+tOfs,x+80,y+22+tOfs],
                                    ZStrs.StartingSerialNumber);
    x += 90;
    pnl.startingSerial = pnl.add('edittext', [x,y,x+50,y+22],
                                 opts.startingSerial);
    y += 30;
    x = xofs;
    pnl.startingSerial.onChanging = psx.numberKeystrokeFilter;
    pnl.startingSerial.onChange = function() {
      var pnl = this.parent;
      pnl.onChange();
    }
  }

  pnl.useCompatibility = useCompatibility;
  if (useCompatibility) {
    pnl.add('statictext', [x,y+tOfs,x+80,y+22+tOfs], ZStrs.CompatibilityPrompt);
    x += 90;
    pnl.compatWindows = pnl.add('checkbox', [x,y,x+70,y+22], ZStrs.Windows);
    x += 80;
    pnl.compatMac = pnl.add('checkbox', [x,y,x+70,y+22], ZStrs.MacOS);
    x += 80;
    pnl.compatUnix = pnl.add('checkbox', [x,y,x+70,y+22], ZStrs.Unix);

    pnl.compatWindows.value = opts.windowsCompatible;
    pnl.compatMac.value = opts.macintoshCompatible;
    pnl.compatUnix.value = opts.unixCompatible;
  }

  function menuOnChange() {
    var pnl = this.parent;
    var win = pnl.window;
    if (pnl.processing) {
      return;
    }
    pnl.processing = true;
    try {
      var menu = this;
      if (!menu.selection) {
        return;
      }

      var currentSelection = menu.selection.index;
      var lastSelection = menu.lastMenuSelection;

      menu.lastMenuSelection = menu.selection.index;

      var lastWasCustomText = (lastSelection >= pnl.fnmenuElements.length);

      var sel = menu.selection.text;
      if (sel == FileNamingElement.NONE) {
        menu.selection = menu.items[0];
        sel = menu.selection.text;
      }

      if (sel == ZStrs.CreateCustomText ||
          (sel == ZStrs.EditCustomText && !lastWasCustomText)) {
        var text = FileNamingOptions.createCustomTextDialog(win,
                                                    ZStrs.CreateCustomText,
                                                    "new");
        if (text) {
          if (text == '-') {
            text = '- ';
          }
          if (!menu.find(text)) {
            pnl.addMenuElement(text);
          }

          var it = menu.find(text);
          menu.selection = it;

        } else {
          if (lastSelection >= 0) {
            menu.selection = menu.items[lastSelection];
          } else {
            menu.selection = menu.items[0];
          }
        }

        if (pnl.notifyCustomText) {
          pnl.notifyCustomText(FileNamingOptions.CUSTOM_TEXT_CREATE, text);
        }

      } else if (lastWasCustomText) {
        if (sel == ZStrs.EditCustomText) {
          var lastText = menu.items[lastSelection].text;
          if (lastText == '- ') {
            lastText = '-'
          }
          var text = FileNamingOptions.createCustomTextDialog(win,
                                                      ZStrs.EditCustomText,
                                                      "edit",
                                                      lastText);
          if (text) {
            if (text == '-') {
              text = '- ';
            }
            for (var i = 0; i < 6; i++) {
              var vmenu = pnl.menus[i];
              vmenu.items[lastSelection].text = text;
            }

            var it = menu.find(text);
            menu.selection = it;

            if (pnl.notifyCustomText) {
              if (lastText == '-') {
                lastText = '- ';
              }

              pnl.notifyCustomText(FileNamingOptions.CUSTOM_TEXT_EDIT, text,
                                   lastText);
            }

          } else {
            if (lastSelection >= 0) {
              menu.selection = menu.items[lastSelection];
            } else {
              menu.selection = menu.items[0];
            }
          }

        } else if (sel == ZStrs.DeleteCustomText) {
          var lastText = menu.items[lastSelection].text;
          if (confirm(ZStrs.DeleteCustomTextPrompt.sprintf(lastText))) {
            for (var i = 0; i < 6; i++) {
              var vmenu = pnl.menus[i];
              vmenu.remove(lastSelection);
            }
            menu.selection = menu.items[0];

          } else {
            menu.selection = menu.items[lastSelection];
          }

          if (pnl.notifyCustomText) {
            pnl.notifyCustomText(FileNamingOptions.CUSTOM_TEXT_DELETE, lastText);
          }

        } else {
          //alert("Internal error, Custom Text request");
        }

      } else {
        if (lastSelection >= 0 && (sel == ZStrs.EditCustomText ||
                                   sel == ZStrs.DeleteCustomText)) {
          menu.selection = menu.items[lastSelection];
        }
      }

      menu.lastMenuSelection = menu.selection.index;

      var example = '';
      var format = [];

      for (var i = 0; i < 6; i++) {
        var vmenu = pnl.menus[i];
        if (vmenu.selection) {
          var fmt = '';
          var text = vmenu.selection.text;
          var fne = psx.getByProperty(FileNamingElements, "menu", text);
          if (fne) {
            text = fne.example;
            fmt = fne.name;
          } else {
            fmt = text;
          }

          if (text) {
            if (text == '- ') {
              text = '-';
            }
            example += text;
          }

          if (fmt) {
            if (fmt == '- ') {
              fmt = '-';
            }
            format.push(fmt);
          }
        }
      }
      if (pnl.example) {
        pnl.example.text = example;
      }
      format = format.join(",");
      var win = pnl.window;
      if (win.mgr.updateNamingFormat) {
        win.mgr.updateNamingFormat(format, example);
      }

    } finally {
      pnl.processing = false;
    }

    if (pnl.onChange) {
      pnl.onChange();
    }
  }

  // default all slots to ''
  for (var i = 0; i < 6; i++) {
    var menu = pnl.menus[i];
    menu.selection = menu.items[0];
    menu.lastMenuSelection = 0;
  }

  for (var i = 0; i < 6; i++) {
    var name = opts.fileNaming[i];
    if (name) {
      var fne = FileNamingElements.getByName(name);
      var it;

      if (!fne) {
        if (name == '- ') {
          name = '-';
        }
        it = pnl.menus[i].find(name);
        if (!it) {
          pnl.addMenuElement(name);
          it = pnl.menus[i].find(name);
        }
      } else {
        it = pnl.menus[i].find(fne.menu);
      }
      pnl.menus[i].selection = it;
    }
  }

//   pnl.menus[0].selection = pnl.menus[0].find("document name");
//   pnl.menus[0].lastMenuSelection = pnl.menus[0].selection.index;
//   pnl.menus[1].selection = pnl.menus[1].find("extension");
//   pnl.menus[1].lastMenuSelection = pnl.menus[1].selection.index;

  for (var i = 0; i < 6; i++) {
    var menu = pnl.menus[i];
    menu.onChange = menuOnChange;
  }

  pnl.getFileNamingOptions = function(ini) {
    var pnl = this;
    var fileNaming = [];

    for (var i = 0; i < 6; i++) {
      var menu = pnl.menus[i];

      if (menu.selection) {
        var idx = menu.selection.index;

        if (idx) {
          // [0] is the "" item so we ignore it
          var fnel = FileNamingElements[idx];
          if (fnel) {
            fileNaming.push(fnel.name);

          } else {
            // its a custom naming option
            var txt = menu.selection.text;
            if (txt == '- ') {
              txt = '-';
            }

            // txt = '"' + text + '"';
            fileNaming.push(txt);
          }
        }
      }
    }

    var prefix = pnl.prefix;
    var opts = new FileNamingOptions(ini, prefix);
    opts.fileNaming = fileNaming;

    if (pnl.startingSerial) {
      opts.startingSerial = Number(pnl.startingSerial.text);
    }
    if (pnl.compatWindows) {
      opts.windowsCompatible = pnl.compatWindows.value;
    }
    if (pnl.compatMac) {
      opts.macintoshCompatible = pnl.compatMac.value;
    }
    if (pnl.compatUnix) {
      opts.unixCompatible = pnl.compatUnix.value;
    }
    return opts;
  }
  pnl.getFilenamingOptions = pnl.getFileNamingOptions;

  pnl.updateSettings = function(ini) {
    var pnl = this;

    var opts = new FileNamingOptions(ini, pnl.prefix);

    if (pnl.useSerial) {
      pnl.startingSerial.text = opts.startingSerial;
    }

    if (pnl.useCompatibility) {
      pnl.compatWindows.value = opts.windowsCompatible;
      pnl.compatMac.value = opts.macintoshCompatible;
      pnl.compatUnix.value = opts.unixCompatible;
    }

    // default all slots to ''
    for (var i = 0; i < 6; i++) {
      var menu = pnl.menus[i];
      menu.selection = menu.items[0];
      menu.lastMenuSelection = 0;
    }

    for (var i = 0; i < 6; i++) {
      var name = opts.fileNaming[i];
      if (name) {
        var fne = FileNamingElements.getByName(name);
        var it;

        if (!fne) {
          if (name == '-') {
            name = '- ';
          }
          it = pnl.menus[i].find(name);
          if (!it) {
            pnl.addMenuElement(name);
            it = pnl.menus[i].find(name);
          }
        } else {
          it = pnl.menus[i].find(fne.menu);
        }
        pnl.menus[i].selection = it;
      }
    }

    for (var i = 0; i < 6; i++) {
      var menu = pnl.menus[i];
      menu.onChange = menuOnChange;
    }

    pnl.menus[0].onChange();

    if (pnl.onChange) {
      pnl.onChange();
    }
  }

  pnl.updateCustomText = function(event, text, oldText) {
    var pnl = this;

    if (!event || !text) {
      return;
    }

    if (event == FileNamingOptions.CUSTOM_TEXT_CREATE) {
      if (!pnl.menus[0].find(text)) {
        pnl.addMenuElement(text);
      }

    } else if (event == FileNamingOptions.CUSTOM_TEXT_DELETE) {
      var it = pnl.menus[0].find(text);
      if (it) {
        var idx = it.index;
        for (var i = 0; i < 6; i++) {
          var vmenu = pnl.menus[i];
          vmenu.remove(idx);
        }
      }

    } else if (event == FileNamingOptions.CUSTOM_TEXT_EDIT) {
      if (oldText) {
        var idx = -1;
        var it = pnl.menus[0].find(oldText);
        if (it) {
          idx = it.index;

          for (var i = 0; i < 6; i++) {
            var vmenu = pnl.menus[i];
            vmenu.items[idx].text = text;
          }
        } else {
          pnl.updateCustomText(FileNamingOptions.CUSTOM_TEXT_CREATE,
                               text);
        }
      } else {
        pnl.updateCustomText(FileNamingOptions.CUSTOM_TEXT_CREATE,
                             text);
      }
    }

    if (pnl.onChange) {
      pnl.onChange();
    }
  }

  pnl.menus[0].onChange();

  if (pnl.onChange) {
    pnl.onChange();
  }

  return pnl;
};
FileNamingOptions.createCustomTextDialog = function(win, title, mode, init) {
  var rect = {
    x: 200,
    y: 200,
    w: 350,
    h: 150
  };

  function rectToBounds(r) {
    return[r.x, r.y, r.x+r.w, r.y+r.h];
  };

  var cwin = new Window('dialog', title || ZStrs.CustomTextEditor,
                        rectToBounds(rect));

  cwin.text = title || ZStrs.CustomTextEditor;
  if (win) {
    cwin.center(win);
  }

  var xofs = 10;
  var y = 10;
  var x = xofs;

  var tOfs = 3;

  cwin.add('statictext', [x,y+tOfs,x+300,y+22+tOfs], ZStrs.CustomTextPrompt);

  y += 30;
  cwin.customText = cwin.add('edittext', [x,y,x+330,y+22]);

  cwin.customText.onChanging = function() {
    cwin = this.parent;
    var text = cwin.customText.text;

    if (cwin.initText) {
      cwin.saveBtn.enabled = (text.length > 0) && (text != cwin.initText);
    } else {
      cwin.saveBtn.enabled = (text.length > 0);
    }
  }

  if (init) {
    cwin.customText.text = init;
    cwin.initText = init;
  }

  y += 50;
  x += 100;
  cwin.saveBtn = cwin.add('button', [x,y,x+70,y+22], ZStrs.Save);
  cwin.saveBtn.enabled = false;

  x += 100;
  cwin.cancelBtn = cwin.add('button', [x,y,x+70,y+22], ZStrs.Cancel);

  cwin.defaultElement = cwin.saveBtn;

  cwin.customText.active = true;

  cwin.onShow = function() {
    this.customText.active = true;
  }

  var res = cwin.show();
  return (res == 1) ? cwin.customText.text : undefined;
};

FileNamingOptions.validateFileNamingPanel = function(pnl, opts) {
  var self = this;
  var win = pnl.window;
  var fopts = pnl.getFileNamingOptions(opts);

  if (fopts.fileNaming.length == 0) {
    return self.errorPrompt("You must specify a name for the files.");
  }

  fopts.copyTo(opts, pnl.prefix);

  return opts;
};


//======================== Font Panel =================================
//
// Function: createFontPanel
// Description: Creates a font selector panel
// Input: pnl - the panel that will be populated
//        ini - an object that contains initial values (not used)
//        label  - the lable for the panel (opt)
//        lwidth - the width to use for the lable in the UI
// Return: the panel
//
psxui.createFontPanel = function(pnl, ini, label, lwidth) {
  var win = pnl.window;

  pnl.win = win;

  var w = pnl.bounds[2] - pnl.bounds[0];
  var xofs = 0;
  var y = 0;

  if (pnl.type == 'panel') {
    xofs += 5;
    y += 5;
  }

  var tOfs = 3;
  var x = xofs;

  if (label == undefined) {
    label = ZStrs.FontLabel;
    lwidth = pnl.graphics.measureString(label)[0] + 5;
  }

  if (label != '') {
    pnl.label = pnl.add('statictext', [x,y+tOfs,x+lwidth,y+22+tOfs], label);
    pnl.label.helpTip = ZStrs.FontTip;
    x += lwidth;
  }
  pnl.family = pnl.add('dropdownlist', [x,y,x+180,y+22]);
  pnl.family.helpTip = ZStrs.FontTip;
  x += 185;
  pnl.style  = pnl.add('dropdownlist', [x,y,x+110,y+22]);
  pnl.style.helpTip = ZStrs.FontStyleTip; 
  x += 115;
  pnl.fontSize  = pnl.add('edittext', [x,y,x+30,y+22], "12");
  pnl.fontSize.helpTip = ZStrs.FontSizeTip; 
  x += 34;
  pnl.sizeLabel = pnl.add('statictext', [x,y+tOfs,x+15,y+22+tOfs],
                          ZStrs.UnitsPT);

  var lbl = pnl.sizeLabel;
  lbl.bounds.width = pnl.graphics.measureString(ZStrs.UnitsPT)[0] + 3;

  // make adjustments if panel is not wide enough to display all of the
  // controls steal space from the family and style dropdown menus
  var pw = pnl.bounds.width;
  var slMax = pnl.sizeLabel.bounds.right;
  var diff = slMax - pw;
  if (diff > 0) {
    diff += 6; // for padding on the right side
    var delta = Math.ceil(diff/2);
    pnl.family.bounds.width -= delta;
    pnl.style.bounds.left -= delta;
    delta *= 2;
    pnl.style.bounds.width -= delta;
    pnl.fontSize.bounds.left -= delta;
    pnl.fontSize.bounds.right -= delta;
    pnl.sizeLabel.bounds.left -= delta;
    pnl.sizeLabel.bounds.right -= delta;
  }

  pnl.fontTable = psxui._getFontTable();
  var names = [];
  for (var idx in pnl.fontTable) {
    names.push(idx);
  }
  // names.sort();
  for (var i = 0; i < names.length; i++) {
    pnl.family.add('item', names[i]);
  }

  pnl.family.onChange = function() {
    var pnl = this.parent;
    var sel = pnl.family.selection.text;
    var family = pnl.fontTable[sel];

    pnl.style.removeAll();

    var styles = family.styles;

    for (var i = 0; i < styles.length; i++) {
      var it = pnl.style.add('item', styles[i].style);
      it.font = styles[i].font;
    }
    if (pnl._defaultStyle) {
      var it = pnl.style.find(pnl._defaultStyle);
      pnl._defaultStyle = undefined;
      if (it) {
        it.selected = true;
      } else {
        pnl.style.items[0].selected = true;
      }
    } else {
      pnl.style.items[0].selected = true;
    }
  };
  pnl.family.items[0].selected = true;

  pnl.fontSize.onChanging = psxui.numericKeystrokeFilter;

//
// Function: setFont
// Description: set the font and font size
// Input: str  - TextFont or the font name
//        size - the font size in points 
// Return: <none>
//
  pnl.setFont = function(str, size) {
    var pnl = this;
    if (!str) {
      return;
    }
    var font = (str.typename == "TextFont") ? str : psx.determineFont(str);
    if (!font) {
      font = psx.getDefaultFont();
    }
    if (font) {
      var it = pnl.family.find(font.family);
      if (it) {
        it.selected = true;
        pnl._defaultStyle = font.style;
      }
    }
    pnl.fontSize.text = size;
    pnl._fontSize = size;
    pnl.family.onChange();
  };

//
// Function: getFont
// Description: Gets the current font and font size
// Input:  <none> 
// Return: an object containing the font and size
//
  pnl.getFont = function() {
    var pnl = this;
    var font = pnl.style.selection.font;
    return { font: font.postScriptName, size: toNumber(pnl.fontSize.text) };

    var fsel = pnl.family.selection.text;
    var ssel = pnl.style.selection.text;
    var family = pnl.fontTable[sel];
    var styles = familyStyles;
    var font = undefined;

    for (var i = 0; i < styles.length && font == undefined; i++) {
      if (styles[i].style == ssel) {
        font = styles[i].font;
      }
    }
    return { font: font, size: toNumber(font.fontSize) };
  }

  return pnl;
};

//
// Function: _getFontTable
// Description: Used by the Font Panel. Creates a table that
//              maps font names to their styles
// Input:  <none> 
// Return: an object where the names are font.family and the
//         values are objects containing the font family and styles
//
psxui._getFontTable = function() {
  var fonts = app.fonts;
  var fontTable = {};
  for (var i = 0; i < fonts.length; i++) {
    var font = fonts[i];
    var entry = fontTable[font.family];
    if (!entry) {
      entry = { family: font.family, styles: [] };
      fontTable[font.family] = entry;
    }
    entry.styles.push({ style: font.style, font: font });
  }
  return fontTable;
};

//
// Function: _getFontArray
// Description: 
// Input:  <none> 
// Return: an array of font info objects created by _getFontTable
//
psxui._getFontArray = function() {
  var fontTable = psxui._getFontTable();
  var fonts = [];
  for (var idx in fontTable) {
    var f = fontTable[idx];
    fonts.push(f);
  }
  return fonts;
};

//
// Function: createProgressPalette
// Description: Opens up a palette window with a progress bar that can be
//              'asynchronously' while the script continues running
// Input:
//   title     the window title
//   min       the minimum value for the progress bar
//   max       the maximum value for the progress bar
//   parent    the parent ScriptUI window (opt)
//   useCancel flag for having a Cancel button (opt)
//   msg        message that can be displayed and changed in the palette (opt)
//
//   onCancel  This method will be called when the Cancel button is pressed.
//             This method should return 'true' to close the progress window
// Return: The palette window
//
psxui.createProgressPalette = function(title, min, max,
                                       parent, useCancel, msg) {
  var opts = {
    closeButton: false,
    maximizeButton: false,
    minimizeButton: false
  };
  var win = new Window('palette', title, undefined, opts);
  win.bar = win.add('progressbar', undefined, min, max);
  if (msg) {
    win.msg = win.add('statictext');
    win.msg.text = msg;
  }
  win.bar.preferredSize = [500, 20];

  win.parentWin = undefined;
  win.recenter = false;
  win.isDone = false;

  if (parent) {
    if (parent instanceof Window) {
      win.parentWin = parent;
    } else if (useCancel == undefined) {
      useCancel = !!parent;
    }
  }

  if (useCancel) {
    win.onCancel = function() {
      this.isDone = true;
      return true;  // return 'true' to close the window
    };

    win.cancel = win.add('button', undefined, ZStrs.Cancel);

    win.cancel.onClick = function() {
      var win = this.parent;
      try {
        win.isDone = true;
        if (win.onCancel) {
          var rc = win.onCancel();
          if (rc != false) {
            if (!win.onClose || win.onClose()) {
              win.close();
            }
          }
        } else {
          if (!win.onClose || win.onClose()) {
            win.close();
          }
        }
      } catch (e) {
        LogFile.logException(e, '', true);
      }
    };
  }

  win.onClose = function() {
    this.isDone = true;
    return true;
  };

  win.updateProgress = function(val) {
    var win = this;

    if (val != undefined) {
      win.bar.value = val;
    }
//     else {
//       win.bar.value++;
//     }

    if (win.recenter) {
      win.center(win.parentWin);
    }

    win.update();
    win.show();
    // win.hide();
    // win.show();
  };

  win.recenter = true;
  win.center(win.parent);

  return win;
};

"psx.jsx";
// EOF
