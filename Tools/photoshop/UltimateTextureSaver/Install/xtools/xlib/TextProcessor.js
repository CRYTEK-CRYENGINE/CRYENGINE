//
// TextProcessor
//   This class abstracts out the idea of iterating through a text file and
//   processing it one line at a time.
//
// $Id: TextProcessor.js,v 1.10 2010/04/27 22:08:28 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

String.prototype.trim = function() {
  return this.replace(/^[\s]+|[\s]+$/g, '');
};
String.prototype.startsWith = function(sub) {
  return this.indexOf(sub) == 0;
};
String.prototype.endsWith = function(sub) {
  return this.length >= sub.length &&
    this.substr(this.length - sub.length) == sub;
};

function throwFileError(f, msg) {
  if (msg == undefined) msg = '';
  Error.runtimeError(9002, msg + '\"' + f + "\": " + f.error + '.');
};

//
//=============================== TextProcessor ==============================
//
// The contructors creates a new object with the input and output files,
// and a function 'processor'. This processor function is not used in this
// version. All arguments are optional.
//
TextProcessor = function(infile, outfile, processor) {
  var self = this;
  self.infile = infile;
  self.outfile = outfile;
  self.encoding = undefined;

  if (typeof processor == "function") {
    self.processor = processor;
  }
};
TextProcessor.prototype.typename = "TextProcessor";

TextProcessorStatus = {};
TextProcessorStatus.OK   = "StatusOK";
TextProcessorStatus.DONE = "StatusDONE";
TextProcessorStatus.FAIL = "StatusFAIL";


//
// convertFptr
//   Return a File or Folder object given one of:
//    A File or Folder Object
//    A string literal or a String object that refers to either
//    a File or Folder
//
TextProcessor.convertFptr = function(fptr) {
  var f;
  if (fptr.constructor == String) {
    f = File(fptr);
  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;
  } else {
    Error.runtimeError(9002, "Bad file \"" + fptr + "\" specified.");
  }
  return f;
};

TextProcessor.writeToFile = function(fptr, str) {
  var file = TextProcessor.convertFptr(fptr);
  file.open("w") || throwFileError(file, "Unable to open output file ");
  file.lineFeed = 'unix';
  file.write(str);
  file.close();
};

TextProcessor.readFromFile = function(fptr) {
  var file = TextProcessor.convertFptr(fptr);
  file.open("r") || throwFileError(file, "Unable to open input file ");

  var str = file.read();

  // this insanity is to detect Character conversion errors
  if (str.length == 0 && file.length != 0) {
    file.close();
    file.encoding = (file.encoding == "UTF8" ? "ASCII" : "UTF8");
    file.open("r");
    str = file.read();
    if (str.length == 0 && file.length != 0) {
      throwFileError(f, "Unable to read from file");
    }
    file.close();

  } else {
    file.close();
  }
  return str;
};

//
// exec
//  This function is called to actually process the input file. If the input
//  file is not specified here, is must have been specified before. The output
//  file is completely optional.
//
//  This function loads the input text file, splits the contents into an
//  array of strings and calls the processor function. The processor function
//  handles the string as needed, possibly modifying some of the strings and
//  copying them to an output buffer.
//  When the end of the input file has been reached, the line handler is called
//  with the line argument set to undefined to indicate the end of the file.
//  When all the lines have been processed, the output buffer is written to the
//  output file. The number of lines processed is returned as a result. If
//  processing was stopped because of a STATUS_FAIL, -1 is returned.
//
TextProcessor.prototype.exec = function(infile, outfile) {
  var self = this;

  if (!self.processor) {
    self.processor = self.handleLine;
  }
  if (!self.handleLine) {
    throw "No processor function has been specified.";
  }

  if (infile) {
    self.infile = infile;
  }
  if (!self.infile) {
    throw "No input file has been specified.";
  }
  if (outfile) {
    self.outfile = outfile;
  }

  var str = TextProcessor.readFromFile(self.infile);
  self.lines = str.split("\n");

  var outputBuffer = [];

  // loop through the lines...
  for (var i = 0; i < self.lines.length; i++) {
    var line = self.lines[i];
    var rc = self.processor(line, i, outputBuffer);
    if (rc == TextProcessorStatus.OK || rc == true) {
      continue;
    }
    if (rc == TextProcessorStatus.DONE) {
      break;
    }
    if (rc == TextProcessorStatus.FAIL || rc == false) {
      return -1;
    }
    throw "Unexpected status code returned by line handler.";
  }

  self.processor(undefined, i, outputBuffer);
  self.outputBuffer = outputBuffer;

  // write out the results, if needed
  if (self.outfile) {
    var outStr = outputBuffer.join("\n");
    TextProcessor.writeToFile(self.outfile, outStr + '\n');
  }
  return i;
};

//
// handleLine
//   This is the function that will get called for each line.
//   It must be set for the processor to work. The processor
//   function takes these arguments:
// line         - the line to be processed
// index        - the line number
// outputBuffer - an array of strings representing the output buffer
//   The processor function should return STATUS_OK if everything is OK,
//   STATUS_DONE to stop processing and write out the results, or STATUS_FAIL
//   if the processing should be halted completely for this file.
//
//  The default handleLine method just copies the input to the output.
//
TextProcessor.prototype.handleLine = function(line, index, outputBuffer) {
  if (line != undefined) {
    outputBuffer.push(line);
  }
  return TextProcessorStatus.OK;
};

//============================== Demo Code ====================================
//
// lineNumbers
//   This is a _sample_ line handler. It just prepends the line with the
//   current line number and a colon and copies it to the outputBuffer
//   This function must be replaced/overridden
//
TextProcessor.lineNumbers = function(line, index, outputBuffer) {
  if (line != undefined) {
    outputBuffer.push('' + index + ": " + line);
  }
  return TextProcessorStatus.OK;
};

//
// This is a demo to show how the TextProcessor class can be used
//
TextProcessor.demo = function() {
  var proc = new TextProcessor();
  proc.handleLine = TextProcessor.lineNumbers;
  proc.exec("/c/work/mhale/Info.txt", "/c/work/mhale/Info.out");
};

//TextProcessor.demo();

"TextProcessor.js";
// EOF
