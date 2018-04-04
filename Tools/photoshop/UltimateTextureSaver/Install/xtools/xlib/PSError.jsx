//
// PSError
// Contains symbolic names for error numbers. This aids in I18N
//
// $Id: PSError.jsx,v 1.16 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//

// The first form doesn't really work...
// Error.runtimeError(PSError.IllegalArgument);
// Error.runtimeError(41, width);

/*
try {
  Error.runtimeError(48);

} catch (e) {
  alert(Stdlib.exceptionMessage(e));
}
*/

PSError = function(number, name, cls, msg) {
  if (typeof this == "function") {
    Error.runtimeError(number, name);
  }
  var self = this;

  self.cls = Error;
  self.number = number;
  self.name = name;
  self.msg = msg;
};
PSError.prototype.valueOf = function() {
  return this.number;
};
PSError.prototype.error = function(arg) {
  Error.runtimeError(this.number, arg);
};
PSError.rte = function(n, m) {
  Error.runtimeError(n, m);
};
PSError.add = function(number, cls, name, msg) {
  if (!msg) {
    msg = name;
    name = undefined;
  }
  PSError[number] = new PSError(number, name, cls, msg);
  if (name) {
    PSError[name] = number;
//     PSError[name] = function(m) {
//       Error.runtimeError(number, m);
//     }
//     PSError[name].number = number;
  }
  return number;
};

PSError.dump = function(file) {
  file.open("w");

  for (var i = 0; i < 10000; i++) {
    try {
      Error.runtimeError(i, "%1");
    } catch (e) {
      if (e) {
        if (e.message == "%1") {
          // file.writeln("// " + i);
        } else {
          file.writeln("PSError.add(" + e.number + ", " + e.name +
                       ", \"" + e.message + "\");");
//           file.writeln(e.number + ", " + e.name + ", \"" + e.message + "\"");
        }
      } else {
        //alert("e is undefined");
      }
    }
  }

  file.close("w");
};

//PSError.dump(new File("/c/tmp/Errors.jsx"));

// JavaScript Language Errors

PSError.add(2, ReferenceError,
            "isUndefined",
            "%1 is undefined");
PSError.add(3, ReferenceError,
            "CannotAssignValue",
            "Cannot assign value");
PSError.add(4, SyntaxError,
            "UnterminatedStringConstant",
            "Unterminated string constant");
PSError.add(5, SyntaxError, "Unterminated comment");
PSError.add(6, SyntaxError, "Bad digit in number");
PSError.add(7, SyntaxError, "Language feature '%1' is not supported");
PSError.add(8, SyntaxError, "Syntax error");
PSError.add(9, SyntaxError, "Illegal use of reserved word '%1'");
PSError.add(10, SyntaxError, "Illegal break or continue outside a loop");
PSError.add(11, SyntaxError, "Label not found");
PSError.add(12, SyntaxError, "Expression is not constant");
PSError.add(13, SyntaxError, "Too many closing braces");
PSError.add(14, SyntaxError, "No matching closing brace found");
PSError.add(15, SyntaxError, "Try without catch or finally");
PSError.add(16, SyntaxError, "catch/finally without try");
PSError.add(17, TypeError, "Variable name expected");
PSError.add(18, TypeError, "Variable or value expected");
PSError.add(19, TypeError,
            "BadArgument",
            "Bad argument %1");
PSError.add(20, TypeError,
            "BadArgumentList",
            "Bad argument list");
PSError.add(21, TypeError,
            "IsNotAnObject",
            "%1 is not an object");
PSError.add(22, ReferenceError, "%1 does not have a constructor");
PSError.add(23, ReferenceError, "%1 does not have a value");
PSError.add(24, ReferenceError,
            "IsNotAFunction",
            "%1 is not a function");
PSError.add(25, SyntaxError, "Expected: %1");
PSError.add(26, Error, "%1 cannot work with instances of this class");
PSError.add(27, InternalError, "Stack overrun");
PSError.add(28, FatalError, "Out of memory");
PSError.add(29, FatalError, "Uncaught exception %1");
PSError.add(30, SyntaxError, "Illegal 'return' outside of a function body");
PSError.add(31, URIError, "Bad or invalid URI: %1");
PSError.add(32, Error, "Cannot execute");
PSError.add(33, InternalError, "Internal error");
PSError.add(34, FatalError, "Execution halted");
PSError.add(35, FatalError, "Timeout while waiting for the engine");
PSError.add(36, FatalError,
            "NotImplemented",
            "Not implemented");
PSError.add(37, SyntaxError, "Character conversion error");
PSError.add(38, SyntaxError, "Partial multibyte character found");
PSError.add(39, SyntaxError, "More than one switch default");
PSError.add(40, TypeError, "%1 redeclared");
PSError.add(41, RangeError, "%1 is out of range");
PSError.add(42, SyntaxError, "Catch after unconditional catch");
PSError.add(43, EvalError, "Evaluation error");
PSError.add(44, TypeError, "Cannot convert");
PSError.add(45, ReferenceError, "Object is invalid");
PSError.add(46, ReferenceError, "%1 is read only");
PSError.add(47, TypeError,
            "DataTypeMismatch",
            "Data type mismatch");
PSError.add(48, IOError, "File or folder does not exist");
PSError.add(49, IOError, "File or folder already exists");
PSError.add(50, IOError, "I/O device is not open");
PSError.add(51, IOError, "Read past EOF");
PSError.add(52, IOError, "I/O error");
PSError.add(53, ReferenceError, "Permission denied");
PSError.add(55, Error, "Object does not support the property or method '%1'");
PSError.add(56, IOError, "Cannot connect to %1");
PSError.add(57, IOError, "Cannot resolve reference");
PSError.add(58, IOError, "I/O Timeout");
PSError.add(59, Error, "No response");


// ScriptUI Errors
PSError.add(501, Error, "Unknown UI element type '%1'");
PSError.add(502, Error, "UI element type is required");
PSError.add(503, Error, "Unknown list item type '%1'");
PSError.add(504, Error, "List item type is required");
PSError.add(505, Error, "selection target '%1' is not a list index or ListItem");
PSError.add(506, Error, "Error creating UI framework control for '%1'");
PSError.add(507, Error, "Error setting initial bounds for '%1'");
PSError.add(508, Error, "Error setting initial text for '%1'");
PSError.add(509, Error, "Invalid image data");
PSError.add(510, Error, "Incompatible version of Mondo '%1'");

PSError.add(1050, Error,
            "GenericMacOS",
            "Generic Mac OS error occurred");
PSError.add(1051, Error,
            "GenericMacOSOSA",
            "Generic Mac OS error occurred in the OSA subsystem");
PSError.add(1052, Error,
            "GenericWindows",
            "Generic Windows error occurred");
PSError.add(1200, Error,
            "Internal",
            "Internal error");
PSError.add(1201, Error,
            "Memory",
            "Memory Error");
PSError.add(1202, Error,
            "NotImplemented",
            "Not implemented");
PSError.add(1205, Error,
            "Unknown",
            "Unknown exception");
PSError.add(1206, Error,
            "HostBusy",
            "Host is busy");
PSError.add(1207, Error,
            "InternalRequire",
            "Internal error (require failed)");
PSError.add(1208, Error,
            "NULL",
            "NULL pointer was encountered");
PSError.add(1209, Error,
            "Clone",
            "clone not allowed");
PSError.add(1220, Error,
            "IllegalArgument",
            "Illegal Argument");
PSError.add(1221, Error,
            "WrongNumberArguments",
            "Wrong number of arguments");
PSError.add(1222, Error,
            "TooManyArguments",
            "Too many arguments");
PSError.add(1223, Error,
            "StringExpected",
            "String value expected");
PSError.add(1224, Error,
            "NumericExpected",
            "Numeric value expected");
PSError.add(1225, Error,
            "ObjectExpected",
            "Object expected");
PSError.add(1226, Error,
            "EnumeratedValueExpected",
            "Enumerated value expected");
PSError.add(1227, Error,
            "WrongEnumerated",
            "Wrong type of enumerated value");
PSError.add(1228, Error,
            "BooleanExpected",
            "Boolean value expected");
PSError.add(1229, Error,
            "ArrayExpected",
            "Array expected");
PSError.add(1230, Error,
            "FileFolderExpected",
            "File/Folder expected");
PSError.add(1231, Error,
            "Pointexpected",
            "Point value expected");
PSError.add(1232, Error,
            "RectangleExpected",
            "Rectangle value expected");
PSError.add(1233, Error,
            "ExpectedReference",
            "Expected a reference to an existing File/Folder");
PSError.add(1235, Error,
            "NumericUnitExpected",
            "Numeric value (optionally in units) expected");
PSError.add(1236, Error,
            "PointUnitExpected",
            "Point value (optionally in units) expected");
PSError.add(1237, Error,
            "RectangleUnitExpected",
            "Rectangle value (optionally in units) expected");
PSError.add(1238, Error,
            "RequiredValue",
            "Required value is missing");
PSError.add(1239, Error,
            "GreaterThanMaximum",
            "Specified value greater than maximum allowed value");
PSError.add(1240, Error,
            "LessThanMinimum",
            "Specified value less than minimum allowed value");
PSError.add(1241, Error,
            "UnsupportedType",
            "Unsupported type");
PSError.add(1242, Error,
            "Argument1",
            "Illegal argument - argument 1");
PSError.add(1243, Error,
            "Argument2",
            "Illegal argument - argument 2");
PSError.add(1244, Error,
            "Argument3",
            "Illegal argument - argument 3");
PSError.add(1245, Error,
            "Argument4",
            "Illegal argument - argument 4");
PSError.add(1246, Error,
            "Argument5",
            "Illegal argument - argument 5");
PSError.add(1247, Error,
            "Argument6",
            "Illegal argument - argument 6");
PSError.add(1248, Error,
            "Argument7",
            "Illegal argument - argument 7");
PSError.add(1249, Error,
            "Argument8",
            "Illegal argument - argument 8");
PSError.add(1250, Error,
            "IndexBounds",
            "Index out of bounds");
PSError.add(1251, Error,
            "JavaScriptNotInitialized",
            "JavaScript engine was not initialized");
PSError.add(1252, Error,
            "JavaScriptMissing",
            "JavaScript code was missing");
PSError.add(1261, Error,
            "NotSupported",
            "operation not supported on this object");
PSError.add(1262, Error,
            "NotAvailable",
            "requested property not available for this object");
PSError.add(1265, Error,
            "NotDeleted",
            "this object cannot be deleted");
PSError.add(1266, Error,
            "NewNotCreated",
            "the new object cannot be created at the specified location");
PSError.add(1267, Error,
            "NotMoved",
            "the object cannot be moved the specified location");
PSError.add(1268, Error,
            "NotCreated",
            "the object could not be created");
PSError.add(1280, Error,
            "PropertyInitialized",
            "The property was not initialized");
PSError.add(1281, Error,
            "PropertyRange",
            "Property value out of allowed range");
PSError.add(1282, Error,
            "PropertyReadOnly",
            "Property is read/only and cannot be changed");
PSError.add(1283, Error,
            "NotAProperty",
            "Not a property");
PSError.add(1284, Error,
            "NotAMethod",
            "Not a method");
PSError.add(1285, Error,
            "PropertyFound",
            "Property not found for this object");
PSError.add(1286, Error,
            "PropertyWrite",
            "Property is write/only");
PSError.add(1287, Error,
            "Method",
            "Method not found for this object");
PSError.add(1288, Error,
            "Command",
            "Command not found for this object");
PSError.add(1302, Error,
            "NoSuchElement",
            "No such element");
PSError.add(1304, Error,
            "CollectionSupport",
            "The collection does not support getting elements by name");
PSError.add(1305, Error,
            "Collection",
            "The specified collection does not exist");
PSError.add(1306, Error,
            "AccessID",
            "You cannot access elements of this type by ID");
PSError.add(1310, Error,
            "AddText",
            "You cannot add a text line");
PSError.add(1311, Error,
            "TextRange",
            "The specified text range is invalid");
PSError.add(1320, Error,
            "EnumerationValue",
            "Invalid enumeration value");
PSError.add(1321, Error,
            "EnumerationType",
            "Invalid enumeration type");
PSError.add(1330, Error,
            "InvalidType",
            "Invalid type");
PSError.add(1331, Error,
            "UnknownType",
            "Unknown type");
PSError.add(1333, Error,
            "ObjectExpected",
            "Object expected");
PSError.add(1351, Error,
            "DuplicateArguments",
            "Duplicate method got invalid arguments. If you omit the relative object then the location should also be omitted.");
PSError.add(1352, Error,
            "StringTooBig",
            "String provided is too big for intended use.");
PSError.add(1501, Error,
            "AppleUnknownObject",
            "Apple event contained an unknown object container type");
PSError.add(1503, Error,
            "AppleUnknownAbsolute",
            "Apple event contained an unknown absolute position form");
PSError.add(1504, Error,
            "AppleUnknownIndex",
            "Apple event contained an unknown index position form");
PSError.add(1505, Error,
            "AppleUnknownRelative",
            "Apple event contained an unknown relative position form");
PSError.add(2000, Error,
            "UnsupportedVariant",
            "Unsupported Variant type");
PSError.add(2001, Error,
            "OnlyArrays",
            "Only arrays with dimension 1 are supported");
PSError.add(2002, Error,
            "ArrayBound",
            "Upper array bound is less than lower array bound");
PSError.add(2003, Error,
            "ObjectyNotHost",
            "Provided Objecty does not belong to this host");
PSError.add(5000, Error,
            "ScriptEngineInitialized",
            "Script engine not initialized");
PSError.add(5001, Error,
            "JavaScriptExecution",
            "JavaScript execution error. %1");
PSError.add(5002, Error,
            "WindowsShellFailure",
            "Windows shell execution failure");
PSError.add(5003, Error,
            "NoAdditional",
            "<no additional error information>");
PSError.add(5050, Error,
            "SPNotFound",
            "SP suite not found");
PSError.add(5051, Error,
            "SPRegistrationFailed",
            "SP suite registration failed");
PSError.add(5052, Error,
            "SPNotInitialized",
            "SP suite not initialized");
PSError.add(5053, Error,
            "SPGeneral",
            "SP general interface error");
PSError.add(5100, Error,
            "RootNotFound",
            "Root Folder Not Found");
PSError.add(5149, Error,
            "WindowsFile",
            "Windows File System Error");

PSError.add(8000, Error,
            "IllegalOpenOptions",
            "Cannot open the file because the open options are incorrect");
PSError.add(8001, Error,
            "MissingOpenFormatSpecifications",
            "Cannot open the file because the format options are missing");
PSError.add(8002, Error,
            "ApplicationNotFrontmost",
            "Photoshop must be the front application when trying to cut, copy or paste");

//		psPrefTypeUnitOutOfRange				= 8003,
PSError.add(8004, Error,
            "PrefPIFolderDoesNotExist",
            "The specified folder for location of extra plug-ins does not exist.");
PSError.add(8005, Error,
            "PrefPIPathNotFolder",
            "The specified path for location of extra plug-ins is not a folder.");
PSError.add(8006, Error,
            "WinColorSettingsTooManyArgs",
            "You may not specify both a Color Settings name and a Color Settings file.");
PSError.add(8007, Error,
            "UserCancelled",
            "User cancelled the operation");
PSError.add(8008, Error,
            "NotifierAlreadyInstalled",
            "Notifier already installed");

// Document errors
PSError.add(8100, Error,
            "NotFrontmost",
            "The requested action requires that the target document is the frontmost document.");
PSError.add(8101, Error,
            "DontSetColorProfileCustom",
            "To set the document to custom color management, assign the name of the desired color profile to the 'color profile name' property");
PSError.add(8102, Error,
            "TryingToSetCurrentLayerToNonDocLayer",
            "You cannot set the current layer to a layer that is not part of the document.");
PSError.add(8103, Error,
            "DocumentNotYetSaved",
            "The document has not yet been saved.");
PSError.add(8104, Error,
            "ActiveHistorystateDocMismatch",
            "The provided history state belongs to a different document than the target document.");
PSError.add(8105, Error,
            "ChangeToColorSpaceNotSupported",
            "This version of ScriptingSupport does not support changing the document mode to duotone.");
PSError.add(8106, Error,
            "CannotSetDepthTo1Bit",
            "You cannot set the bit depth property to one bit. To change a document to one bit, change the document mode to Bitmap.");
PSError.add(8107, Error,
            "ConvertToIndexedRequiresOptions",
            "The specified conversion to indexed color mode requires conversion options.");
PSError.add(8108, Error,
            "ResizeWResampleNoneTooManyArgs",
            "If you don't resample, you may only specify one of 'width', 'height' or 'resolution'");
PSError.add(8109, Error,
            "ResizeWResampleNonePixels",
            "You may use pixels to specify 'height' and 'width' only when resampling");
PSError.add(8110, Error,
            "CannotSetDepthTo32Bit",
            "You cannot set the bit depth property to thirty two bit. To change a document to thirty two bit, change the document mode to Grayscale or RGB.");

// Selection Errors
PSError.add(8150, Error,
            "FillSelectionNoType",
            "You must specify either a fill color, the name of a pattern, or a history state when filling a selection.");
PSError.add(8151, Error,
            "FIllSelectionTooManyTypes",
            "You may only specify one of the following when applying a fill: fill color, fill pattern, or history state.");
PSError.add(8152, Error,
            "NoSelection",
            "The document does not contain a selection");

// Layer errors
PSError.add(8175, Error,
            "PassThroughOnlyAvailableForLayerSets",
            "The 'Pass Through' blend mode is only available for layer sets.");
PSError.add(8176, Error,
            "TryingToLinkLayersAcrossDocuments",
            "You cannot link to a layer that is in a different document.");
PSError.add(8177, Error,
            "LayerNotTextLayer",
            "Layer is not a text layer.");
PSError.add(8178, Error,
            "CannotMoveBackgroundLayer",
            "A background layer cannot be moved.");
PSError.add(8179, Error,
            "TryingToMoveLayersBetweenDocuments",
            "You cannot move layers between different documents.");
PSError.add(8180, Error,
            "CannotNestLayerSets",
            "cannot move a layer set into another layer set");
PSError.add(8181, Error,
            "BgLayerCannotContainText",
            "Background layers cannot contain text.");
PSError.add(8182, Error,
            "LayerCannotBeConvertedToText",
            "The layer cannot contain text.");
PSError.add(8183, Error,
            "LayerSetDoesNotSupportCCCP",
            "You cannot clear, copy, cut or paste when a layer set is active.");
PSError.add(8184, Error,
            "DocumentAlreadyHasBgLayer",
            "There is already a background layer in the document.");
PSError.add(8185, Error,
            "DuplicateToDocumentPosWrong",
            "When duplicating a layer to a document, you may only specify 'to beginning' or 'to end'.");
PSError.add(8186, Error,
            "DuplicateLayerSetInsideLayerSet",
            "You cannot duplicate a layer set to inside another layer set.");
PSError.add(8187, Error,
            "DuplicateToInsideArtLayer",
            "You cannot duplicate to inside an art layer.");
PSError.add(8188, Error,
            "CannotUnlockPixelsOnTextLayers",
            "You cannot unlock pixels or transparent pixels on text layers.");
PSError.add(8189, Error,
            "CannotDeleteAllTopLevelLayers",
            "You cannot delete all top level layers.");
PSError.add(8190, Error,
            "EnabledChannelsNotSupportedForMode",
            "Enabled channels are only available for RGB, CMYK and Lab documents.");
PSError.add(8191, Error,
            "EnabledChannelsRequiresComponents",
            "Enabled channels can only be set to component channels.");
PSError.add(8192, Error,
            "DuplicateDocumentNotFrontmost",
            "You can only duplicate layers from the frontmost document.");
PSError.add(8193, Error,
            "CanOnlyChangeBetweenNormalAndTextLayer",
            "You can only change the layer's kind to text or normal.");
PSError.add(8194, Error,
            "CannotFilterOrAdjustHiddenLayer",
            "You cannot apply a filter or an adjustment to a hidden layer");
PSError.add(8195, Error,
            "CannotAdjustTextLayer",
            "You cannot apply an adjustment to a text layer");
PSError.add(8196, Error,
            "TargetDocumentDifferentFromCurrent",
            "The specified target object belongs to a different document than the current action is invoked on");

// Open options errors
PSError.add(8200, Error,
            "MissingRAWOpenOptions",
            "You must specify width, height, number of channels, interleave channels and bits per channel when opening a raw format image");

// Color errors
PSError.add(8250, Error,
            "CannotChangeModel",
            "You cannot change the color model.");

// Save options error
PSError.add(8351, Error,
            "NotAvailableOnWindows",
            "Functionality is not available when running on Windows.");
PSError.add(8352, Error,
            "FormatNotSupportedForSave",
            "Format is not supported for save.");
PSError.add(8354, Error,
            "SaveAsCopyErrorNoFile",
            "You must specify a filename when saving as a copy.");
PSError.add(8355, Error,
            "PICTResolutionMissing",
            "Resolution parameter is missing.");
PSError.add(8356, Error,
            "BMPMustSpecifyBitDepth",
            "You must specify bit depth when saving as BMP");
PSError.add(8357, Error,
            "EPSMustSpecifyPreview",
            "You must specify preview type when saving as EPS");

// Color errors
PSError.add(8365, Error,
            "ColorUninitialized",
            "Color object is not initialized.");
PSError.add(8366, Error,
            "ColorBookColorNotSupported",
            "This version of the ScriptingSupport does not support custom colors such as PANTONE^R colors.");
PSError.add(8367, Error,
            "ColorCannotChangeColorModel",
            "You cannot change the color model of color with a color model that is different from 'none'.");

// Export Option errors
PSError.add(8375, Error,
            "AiExportNameAndNoNamedPath",
            "You may not specify a path name unless you are exporting a named path.");
PSError.add(8376, Error,
            "ExportOptionsAreMissing",
            "You must specify either an export type or export options.");

// Text errors
PSError.add(8400, Error,
            "CharacterHasAutokerning",
            "The text is set to auto-kerning and has no special kerning value.");
PSError.add(8401, Error,
            "CharacterHasAutoleading",
            "The text is set to auto-leading and has no special leading value.");
PSError.add(8402, Error,
            "NoTextOnLayer",
            "The layer contains no text.");
PSError.add(8403, Error,
            "NotParagraphText",
            "The requested operation is only applicable for paragraph text.");

// Channel errors
PSError.add(8450, Error,
            "NotValidForComponentChannel",
            "The operation is not valid for channels of type component.");
PSError.add(8451, Error,
            "ChannelDuplicateToDocumentPosWrong",
            "You may only duplicate a channel to the end of a document.");
PSError.add(8452, Error,
            "YouCanOnlyMergeSpotChannels",
            "Only spot channels can be merged.");
PSError.add(8453, Error,
            "HistogramRequiresVisible",
            "You can only get a histogram for visible channels.");
PSError.add(8454, Error,
            "CannotDeleteLastChannel",
            "You cannot delete the last channel in a document.");

// Filter errors
PSError.add(8475, Error,
            "GlassFilterNeedsTexture",
            "You must specify either a texture kind or a texture file.");
PSError.add(8476, Error,
            "GlassFilterBothTexturesProvided",
            "You may only specify either a texture kind or a texture file (not both).");

// General errors
PSError.add(8500, Error,
            "SpecifiedKeyDoesNotExist",
            "The requested property does not exist.");
PSError.add(8501, Error,
            "PropertyIsWriteOnly",
            "The property is write only and its value cannot be obtained.");
PSError.add(8502, Error,
            "PropertyNotSet",
            "You must set the value of this property before reading it.");
PSError.add(8503, Error,
            "GenericFileError",
            "Some file related error occurred.");

// Action manager errors
PSError.add(8600, Error,
            "EventKeyIsMissing",
            "You must provide either an event code or an event string.");
PSError.add(8601, Error,
            "EventKeyDefinedTwice",
            "You can only provide either an event code or an event string (not both).");
PSError.add(8602, Error,
            "ValueTypeMissing",
            "'value type' property is missing.");
PSError.add(8603, Error,
            "ValueMissing",
            "'value' property is missing.");
PSError.add(8604, Error,
            "ValueTypeAndValueMismatch",
            "The provided value does not match with the corresponding 'value type'");

PSError.add(8605, Error,
            "BothCodeAndStringDefined",
            "Both 'key string' and 'key code' were provided. You may only provide one of these properties.");
PSError.add(8606, Error,
            "NeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided.");
PSError.add(8607, Error,
            "EnumTypeBothCodeAndStringDefined",
            "Both 'key string' and 'key code' were provided for the type of an enumeration. You may only provide one of these properties.");
PSError.add(8608, Error,
            "EnumTypeNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the type of an enumeration.");
PSError.add(8609, Error,
            "EnumValueBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the value of an enumeration. You may only provide one of these properties.");
PSError.add(8610, Error,
            "EnumValueNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the value of an enumeration.");
PSError.add(8611, Error,
            "UnitTypeBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the type of a unit value. You may only provide one of these properties.");
PSError.add(8612, Error,
            "UnitTypeNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the type of a unit value.");
PSError.add(8613, Error,
            "ClassTypeBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the class type. You may only provide one of these properties.");
PSError.add(8614, Error,
            "ClassTypeNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the class type.");
PSError.add(8615, Error,
            "ObjectTypeBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the object type. You may only provide one of these properties.");
PSError.add(8616, Error,
            "ObjectTypeNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the object type.");
PSError.add(8617, Error,
            "ValueMissingInListItem",
            "Value is missing from the action list item.");
PSError.add(8618, Error,
            "ValueTypeMissingInListItem",
            "'value type' is missing from the action list item.");
PSError.add(8619, Error,
            "RefTypeBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the reference type. You may only provide one of these properties.");
PSError.add(8620, Error,
            "RefTypeNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the reference type.");
PSError.add(8621, Error,
            "ActionReferenceTypeMissing",
            "'value type' is missing from the action reference item.");
PSError.add(8622, Error,
            "ActionReferenceValueMissing",
            "='value' is missing from the action reference item.");
PSError.add(8623, Error,
            "RefPropBothCodeAndStringDefined",
            "Both 'value string' and 'value code' were provided for the action reference property type. You may only provide one of these properties.");
PSError.add(8624, Error,
            "RefPropNeitherCodeOrStringDefined",
            "Neither 'key string' nor 'key code' was provided for the action reference property type.");

PSError.add(8650, Error,
            "XMPUnknownPropertyFormatErr",
            "An XMP property with an unknow format was encountered.");
PSError.add(8651, Error,
            "XMPStructuredValueFormatErr",
            "an XMP structured value must be a list containing a name and value string.");
PSError.add(8652, Error,
            "XMPValueListExpected",
            "Expected an XMP value list.");
PSError.add(8653, Error,
            "XMPAlternatsValueListExpected",
            "Expected an XMP alternates value.");
PSError.add(8654, Error,
            "XMPOrderedValueListExpected",
            "Expected an XMP orderd value.");
PSError.add(8655, Error,
            "XMPUnorderedValueListExpected",
            "Expected an XMP unordered value.");
PSError.add(8656, Error,
            "XMPValuesMustBeStrings",
            "XMP values must be strings");

PSError.add(8659, Error,
            "XMPInternalXMPError",
            "Unexpected internal XMP error");

// General error
PSError.add(8800, Error,
            "GeneralError",
            "General Photoshop error occurred.");

"PSError.txt";
// EOF
