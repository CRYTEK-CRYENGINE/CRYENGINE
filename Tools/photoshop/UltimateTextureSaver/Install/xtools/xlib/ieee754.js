//
// IEEE754
// This file defines a class that can convert from doubles to hex and back.
// The primary interfaces to this class are:
//   IEEE754.hexToDouble(hex) - convert a JS double to a 16 char hex string
//   IEEE754.doubleToHex(val) - convert a 16 char hex string to a JS double
//   IEEE754.binToHex(bin) - converts an 8 byte binary string to
//                           a 16 char hex string
//   IEEE754.hexToBin(hex) - converts a 16 char hex string to
//                           an byte binary string
//   IEEE754.binToDouble(bin) - converts an 8 byte binary string to a JS double
//   IEEE754.doubleToBin(val) - converts a JS double to an 8 binary string
// 
//
// The primary purpose of this code is to enable reading and writing IEEE754
//   encoded numbers disk in Javascript.
//
// This work is reformatted and repackaged version of the code here:
//   http://babbage.cs.qc.edu/courses/cs341/IEEE-754.html
//
// $Id: ieee754.js,v 1.3 2012/04/02 00:26:01 anonymous Exp $
//

//object construction function
IEEE754 = function IEEE754(size) {
  var self = this;

  self.size = size;
  self.binaryPower = 0;
  self.decValue = "";
  self.dispStr = "";
  self.statCond = "normal";
  self.statCond64 = "normal";
  self.binString = "";
  // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
  self.binVal = new Array(2102);    //Binary Representation

  if (size == 32){
    self.expBias = 127;
    self.maxExp = 127;
    self.minExp = -126;
    self.minUnnormExp = -149;
    self.result = new Array(32);
  }
  else if (size == 64) {
    self.expBias = 1023;
    self.maxExp = 1023;
    self.minExp = -1022;
    self.minUnnormExp = -1074;
    self.result = new Array(64);
  }
};

//convert input to bin.
IEEE754.prototype.convert2bin = function(outstring, statstring, signBit, power, rounding) {
  var output = '';

  var binexpnt;
  var binexpnt2;
  var index1;
  var index2;
  var index3;
  var rounded;
  var lastbit;
  var moreBits;

  var cnst = 2102;   // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
  var bias = 1024;

  //init
  for (var i = 0; i < this.size; i++) {
    this.result[i] = 0;
  }

  //sign bit
  this.result[0] = signBit;

  //obtain exponent value
  index1 = 0;

  index2 = (this.size == 32) ? 9 : 12;

  if (rounding && (statstring == "normal")) {
    //find most significant bit of significand
    while ((index1 < cnst) && (this.binVal[index1] != 1)) {
      index1++;
    }

    binexpnt = bias - index1;

    //regular normalized numbers
    if (binexpnt >= this.minExp) {
      //the value is shifted until the most
      index1++;    //significant 1 is to the left of the binary
      //point and that bit is implicit in the encoding
      //if normalized numbers

      //support for zero and denormalized numbers
      //exponent underflow for this precision

    } else {
      binexpnt = this.minExp - 1;
      index1 = bias - binexpnt;
    }//if zero or denormalized (else section)

    //use round to nearest value mode

    //compute least significant (low-order) bit of significand
    lastbit = this.size - 1 - index2 + index1;

    //the bits folllowing the low-order bit have a value of (at least) 1/2
    if (this.binVal[lastbit + 1] == 1) {
      rounded = 0;

      //odd low-order bit
      if (this.binVal[lastbit] == 1) {
        //exactly 1/2 the way between odd and even rounds up to the even,
        //so the rest of the bits don't need to be checked to see if the value
        //is more than 1/2 since the round up to the even number will occur
        //anyway due to the 1/2
        rounded = 1;
        //if odd low-order bit
        //even low-order bit

      } else  { //this.binVal[lastbit] == 0
        //exactly 1/2 the way between even and odd rounds down to the even,
        //so the rest of the bits need to be checked to see if the value
        //is more than 1/2 in order to round up to the odd number
        index3 = lastbit + 2;
        while ((rounded == 0) && (index3 < cnst)) {
          rounded = this.binVal[index3];
          index3++;
        }//while checking for more than 1/2

      }//if even low-order bit (else section)

      //do rounding "additions"
      index3 = lastbit;
      while ((rounded == 1) && (index3 >= 0)) {
        // 0 + 1 -> 1 result with 0 carry
        if (this.binVal[index3] == 0) {
          // 1 result
          this.binVal[index3] = 1;

          // 0 carry
          rounded = 0;

          //if bit is a 0

          // 1 + 1 -> 0 result with 1 carry
        } else { //this.binVal[index3] == 1
          // 0 result
          this.binVal[index3] = 0;

          // 1 carry
          //          rounded = 1
        }//if bit is a 1 (else section)

        index3--;
      }//while "adding" carries from right to left in bits

    }//if at least 1/2

    //obtain exponent value
    index1 = index1 - 2;
    if (index1 < 0) {
      index1 = 0;
    }

  }//if rounding

  //find most significant bit of significand
  while ((index1 < cnst) && (this.binVal[index1] != 1)) {
    index1++;
  }

  binexpnt2 = bias - index1;

  if (statstring == "normal") {
    binexpnt = binexpnt2;

    //regular normalized numbers
    if ((binexpnt >= this.minExp) && (binexpnt <= this.maxExp)) {
      //the value is shifted until the most
      index1++;               //significant 1 is to the left of the binary
      //point and that bit is implicit in the encoding
      //if normalized numbers

      //support for zero and denormalized numbers
      //exponent underflow for this precision
    } else if (binexpnt < this.minExp) {
      if (binexpnt2 == bias - cnst)
        //value is truely zero
        this.statCond = "normal";
      else if (binexpnt2 < this.minUnnormExp)
        this.statCond = "underflow";
      else
        this.statCond = "denormalized";

      binexpnt = this.minExp - 1;
      index1 = bias - binexpnt;
    }//if zero or denormalized (else if section)

  } else { //already special values
    binexpnt = power;
    index1 = bias - binexpnt;

    if (binexpnt > this.maxExp)
      binexpnt = this.maxExp + 1;

    else if (binexpnt < this.minExp)
      binexpnt = this.minExp - 1;

  }//if already special (else section)

  //copy the result
  while ((index2 < this.size) && (index1 < cnst)) {
    this.result[index2] = this.binVal[index1];
    index2++;
    index1++;
  }//while

  //max exponent for this precision
  if ((binexpnt > this.maxExp) || (statstring != "normal")) {
    //overflow of this precision, set infinity
    if (statstring == "normal") {
      binexpnt = this.maxExp + 1;
      this.statCond = "overflow";
      this.dispStr = "Infinity";

      if (this.result[0] == 1) {
        this.dispStr = "-" + this.dispStr;
      }

      if (this.size == 32) {
        index2 = 9;
      } else {
        index2 = 12;
      }

      //zero the significand
      while (index2 < this.size) {
        this.result[index2] = 0;
        index2++;
      }//while

      //if overflowed
    } else { //already special values
      this.statCond = statstring;
      this.dispStr = outstring;
    }//if already special (else section)

  }//if max exponent

  //convert exponent value to binary representation
  index1 = (this.size == 32) ? 8 : 11;

  this.binaryPower = binexpnt;
  binexpnt += this.expBias;    //bias

  while ((binexpnt / 2) != 0) {
    this.result[index1] = binexpnt % 2;
    if (binexpnt % 2 == 0) {
      binexpnt = binexpnt / 2;
    } else {
      binexpnt = binexpnt / 2 - 0.5;
    }
    index1 -= 1;
  }

  //output binary result
  output = "";
  for (index1 = 0; index1 < this.size; index1++) {
    output = output + this.result[index1];
  }
  return output;
};

IEEE754.prototype.dec2bin = function(input) {
  var value;
  var intpart;
  var decpart;
  var binexpnt;
  var index1;
  var cnst = 2102;   // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
  var bias = 1024;

  //init
  for (index1 = 0; index1 < cnst; index1++)  {
    this.binVal[index1] = 0;
  }

  input = IEEE754.canonical(input)

  //sign bit
  if (input.charAt(0) == "-") {
    this.result[0] = 1;
  } else {
    this.result[0] = 0;
  }
  

  //if value magnitude greater than 1.7976931348623157E+308, set infinity
  input = IEEE754.ovfCheck(input);

  if (input.indexOf("Infinity") != -1) {
    binexpnt = this.maxExp + 1;
    this.statCond64 = "overflow";
    this.dispStr = input;

    //if greater than 1.7976931348623157E+308
  } else {
    //Value magnitude is not greater than 1.7976931348623157E+308

    //if value magnitude less than 2.4703282292062328E-324, set "underflow".
    this.statCond64 = IEEE754.undfCheck(input)

    if (this.statCond64 == "underflow") {
      binexpnt = this.minExp - 1;
      //if less than 2.4703282292062328E-324
    } else {
      //Value magnitude is not less than 2.4703282292062328E-324

      //convert 'input' from string to numeric
      input = input * 1.0;

      //convert and seperate input to integer and decimal parts
      value = Math.abs(input);
      intpart = Math.floor(value);
      decpart = value - intpart;

      //convert integer part
      index1 = bias;
      while (((intpart / 2) != 0) && (index1 >= 0)) {
        this.binVal[index1] = intpart % 2;
        if (intpart % 2 == 0) {
          intpart = intpart / 2;
        }  else {
          intpart = intpart / 2 - 0.5;
        }
        index1 -= 1;
      }

      //convert decimal part
      index1 = bias + 1;
      while ((decpart > 0) && (index1 < cnst)) {
        decpart *= 2;
        if (decpart >= 1) {
          this.binVal[index1] = 1;
          decpart--;
          index1++;
        } else {
          this.binVal[index1] = 0;
          index1++;
        }
      }

      //obtain exponent value
      index1 = 0;

      //find most significant bit of significand
      while ((index1 < cnst) && (this.binVal[index1] != 1)) {
        index1++;
      }

      binexpnt = bias - index1;

      //support for zero and denormalized numbers
      //exponent underflow for this precision
      if (binexpnt < this.minExp) {
        binexpnt = this.minExp - 1;

      }//if zero or denormalized

    }//if not less than 2.4703282292062328E-324 (else section)

  }//if not greater than 1.7976931348623157E+308 (else section)

  //output exponent value
  this.binaryPower = binexpnt;
};

IEEE754.prototype.hex2bin = function(input) {
  var output = '';                 //Output

  var index1;
  var nibble;
  var i;
  var s;
  var binexpnt;
  var index2;
  var zeroFirst;
  var zeroRest;
  var binexpnt2;
  var index3;

  var cnst = 2102;           // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
  var bias = 1024;

  //init
  var numerals = "0123456789ABCDEF";

  for (index1 = 0; index1 < cnst; index1++)  {
    this.binVal[index1] = 0;
  }

  for (index1 = 0; index1 < this.size; index1++) {
    this.result[index1] = 0;
  }
  input = input.trim();

  if (input.length > (this.size / 4)) {
    alert("too many hex digits");
    output = "exit";
    return output;
  }
  if (input.length < (this.size / 4)) {
    alert("too few hex digits");
    output = "exit";
    return output;
  }

  input = input.toUpperCase();

  for (index1 = 0; index1 < this.size; index1 +=4) {
    nibble = numerals.indexOf(input.charAt(index1 / 4));

    if (nibble == -1) {
      alert("invalid hex digit");
      output = "exit";
      return output;
    }

    temp = nibble / 16;

    for (i = 0; i < 4; i++) {
      temp *= 2;
      if (temp >= 1) {
        this.result[index1 + i] = 1;
        temp --;
      } else {
        this.result[index1 + i] = 0;
      }
    }
  }

  //obtain exponent value
  binexpnt = 0;

  index2 = (this.size == 32) ? 9 : 12;

  for (index1 = 1; index1 < index2; index1++) {
    binexpnt += parseInt(this.result[index1])*Math.pow(2, index2 - index1 - 1);
  }

  binexpnt -= this.expBias;            //bias
  this.binaryPower = binexpnt;

  index1 = bias - binexpnt;

  //regular normalized numbers
  if ((binexpnt >= this.minExp) && (binexpnt <= this.maxExp)) {
    //the encoding's hidden 1 is inserted
    this.binVal[index1] = 1;
    index1++;
  }//if normalized numbers

  index3 = index1;

  //copy the input
  if (this.result[index2] == 0) {
    zeroFirst = true;
  }
  this.binVal[index1] = this.result[index2];
  index2++;
  index1++;

  zeroRest = true;
  while ((index2 < this.size) && (index1 < cnst)) {
    if (this.result[index2] == 1) {
      zeroRest = false;
    }
    this.binVal[index1] = this.result[index2];
    index2++;
    index1++;
  }//while

  //find most significant bit of significand
  //for the actual denormalized exponent test for zero
  while ((index3 < cnst) && (this.binVal[index3] != 1)) {
    index3++;
  }
  binexpnt2 = bias - index3;

  //zero and denormalized numbers
  if (binexpnt < this.minExp) {
    if (binexpnt2 == bias - cnst) {
      //value is truely zero
      this.statCond = "normal";
    } else if (binexpnt2 < this.minUnnormExp) {
      this.statCond = "underflow";
    } else {
      this.statCond = "denormalized";
    }
    //if zero or denormalized
  } else if (binexpnt > this.maxExp) {
    //max exponent for this precision
    if (zeroFirst && zeroRest) {
      //Infinity
      this.statCond = "overflow";
      this.dispStr = "Infinity";
      //if Infinity
    } else if (!zeroFirst && zeroRest && (this.result[0] == 1)) {
      //Indeterminate quiet NaN
      this.statCond = "quiet";
      this.dispStr = "Indeterminate";
      //if Indeterminate quiet NaN (else if section)
    } else if (!zeroFirst) {
      //quiet NaN
      this.statCond = "quiet";
      this.dispStr = "NaN";
    //if quiet NaN (else if section)
    } else {
      //signaling NaN
      this.statCond = "signaling";
      this.dispStr = "NaN";
    }//if signaling NaN (else section)

    if ((this.result[0] == 1) && (this.dispStr != "Indeterminate")) {
      this.dispStr = "-" + this.dispStr;
    }

  }//if max exponent (else if section)

  //output binary result
  output = "";
  for (index1 = 0; index1 < this.size; index1++) {
    output = output + this.result[index1];
  }

  return output;
};

IEEE754.canonical = function(input) {
  var output = '';
  var expstr = '';
  var signstr = '';
  var expsignstr = '';
  var expstrtmp = '';

  var locE;
  var stop;
  var expnum;
  var locDPact;
  var locDP;
  var start;
  var MSDfound;
  var index;
  var expdelta;
  var expstart;
  var expprecision = 5;
  
  var numerals = "0123456789";

  input = input.toUpperCase();

  locE = input.indexOf("E");
  if (locE != -1) {
    stop = locE;
    expstr = input.substring(locE + 1, input.length);
    expnum = expstr * 1;
  } else {
    stop = input.length;
    expnum = 0;
  }

  locDPact = input.indexOf(".");
  locDP = (locDPact != -1) ? locDPact : stop;

  start = 0;
  if (input.charAt(start) == "-") {
    start++;
    signstr = "-";
  } else if (input.charAt(start) == "+") {
    start++;
    signstr = "+";
  } else {
    signstr = "+";
  }

  MSDfound = false;
  while ((start < stop) && !MSDfound) {
    index = 1;
    while (index < numerals.length) {
      if (input.charAt(start) == numerals.charAt(index)) {
        MSDfound = true;
        break;
      }
      index++;
    }
    start++;
  }
  start--;

  if (MSDfound) {
    expdelta = locDP - start;
    if (expdelta > 0) {
      expdelta = expdelta - 1;
    }

    expnum = expnum + expdelta;
  } else { //No significant digits found, value is zero
    expnum = 0;
  }

  expstrtmp = "" + expnum;

  expstart = 0;
  if (expstrtmp.charAt(expstart) == "-") {
    expstart++;
    expsignstr = "-";
  } else {
    expsignstr = "+";
  }

  expstr = "E" + expsignstr;

  index = 0;
  while (index < expprecision - expstrtmp.length + expstart) {
    expstr += "0";
    index++;
  }

  expstr += expstrtmp.substring(expstart, expstrtmp.length);

  output = signstr;

  if (locDPact == start + 1) {
    output += input.substring(start, stop);
  } else if (stop == start + 1) {
    output += input.substring(start, stop);
    output += ".";
  } else if (locDPact < start) {
    output += input.substring(start, start + 1);
    output += ".";
    output += input.substring(start + 1, stop);
  } else if (locDPact != -1) {
    output += input.substring(start, start + 1);
    output += ".";
    output += input.substring(start + 1, locDPact);
    output += input.substring(locDPact + 1, stop);
  } else {
    output += input.substring(start, stop);
    output += ".";
  }

  output += expstr;

  return output;
};

IEEE754.mostSigOrder = function(input) {
  var output = '';
  var expstr = '';

  var expprecision = 5;
  var expbias = 50000;
  var stop;
  var expnum;
  var index;
  
  stop = input.indexOf("E");

  output = input.substring(stop + 1, input.length);
  expnum = output * 1;
  expnum += expbias;

  expstr = "" + expnum;

  output = expstr;

  index = 0;
  while (index < expprecision - expstr.length) {
    output = "0" + output;
    index++;
  }

  output += input.substring(1, 2);
  output += input.substring(3, stop);

  return output;
};

IEEE754.A_gt_B = function (A, B) {
  var greater;
  var stop;
  var index;
  var Adigit;
  var Bdigit;
  
  var numerals = "0123456789";

  greater = false;

  if (A.length > B.length) {
    stop = A.length;
  } else {
    stop = B.length;
  }

  index = 0;
  while (index < stop) {
    if (index < A.length) {
      Adigit = numerals.indexOf(A.charAt(index));
    } else {
      Adigit = 0;
    }

    if (index < B.length) {
      Bdigit = numerals.indexOf(B.charAt(index));
    } else {
      Bdigit = 0;
    }

    if (Adigit < Bdigit) {
      break;
    } else if (Adigit > Bdigit) {
      greater = true;
      break;
    }

    index++;
  }//end while

  return greater;
};

IEEE754.ovfCheck = function(input) {
  var output;

  //Is value magnitude greater than +1.7976931348623157E+00308
  if (IEEE754.A_gt_B(IEEE754.mostSigOrder(input), "5030817976931348623157")) {
    output = "Infinity";
    if (input.charAt(0) == "-") {
      output = "-" + output;
    }
  } else {
    output = input;
  }

  return output;
};

IEEE754.undfCheck = function(input) {
  var output;

  //Is value magnitude less than +2.4703282292062328E-00324
  if (IEEE754.A_gt_B("4967624703282292062328", IEEE754.mostSigOrder(input))) {
    output = "underflow";
  } else {
    output = "normal";
  }

  return output;
};

String.prototype.trim = function() {
  return this.replace(/^[\s]+|[\s]+$/g, '');
};


IEEE754.prototype.convert2hex = function() {
  var temp;
  var index;
  var i;
  
  var numerals = "0123456789ABCDEF";
  var output = '';

  //convert binary result to hex and output
  for (index = 0; index < this.size; index +=4) {
    temp = 0;
    for (i = 0; i < 4; i++) {
      temp += Math.pow(2, 3 - i)*this.result[index + i];
    }

    output = output + numerals.charAt(temp);
  }
  return output;
};

IEEE754.numStrClipOff = function(input, precision) {
  var result = '';
  var tempstr = '';
  var expstr = '';
  var signstr = '';

  var locE;
  var stop;
  var expnum;
  var locDP;
  var start;
  var MSD;
  var MSDfound;
  var index;
  var expdelta;
  var digits;
  var number;

  var numerals = "0123456789";

  var tempstr = input.toUpperCase();

  locE = tempstr.indexOf("E");
  if (locE != -1) {
    stop = locE;
    expstr = input.substring(locE + 1, input.length);
    expnum = expstr * 1;
  } else {
    stop = input.length;
    expnum = 0;
  }

  if (input.indexOf(".") == -1) {
    tempstr = input.substring(0, stop);
    tempstr += ".";
    if (input.length != stop) {
      tempstr += input.substring(locE, input.length);
    }

    input = tempstr;

    locE = locE + 1;
    stop = stop + 1;
  }

  locDP = input.indexOf(".");

  start = 0;
  if (input.charAt(start) == "-") {
    start++;
    signstr = "-";
  } else {
    signstr = "";
  }

  MSD = start;
  MSDfound = false;
  while ((MSD < stop) && !MSDfound) {
    index = 1;
    while (index < numerals.length) {
      if (input.charAt(MSD) == numerals.charAt(index)) {
        MSDfound = true;
        break;
      }
      index++;
    }
    MSD++;
  }
  MSD--;

  if (MSDfound) {
    expdelta = locDP - MSD;
    if (expdelta > 0) {
      expdelta = expdelta - 1;
    }

    expnum = expnum + expdelta;

    expstr = "e" + expnum;
  } else { //No significant digits found, value is zero
    MSD = start;
  }

  digits = stop - MSD;

  tempstr = input.substring(MSD, stop);

  if (tempstr.indexOf(".") != -1) {
    digits = digits - 1;
  }

  number = digits;
  if (precision < digits) {
    number = precision;
  }

  tempstr = input.substring(MSD, MSD + number + 1);

  if ((MSD != start) || (tempstr.indexOf(".") == -1)) {
    result = signstr;
    result += input.substring(MSD, MSD + 1);
    result += ".";
    result += input.substring(MSD + 1, MSD + number);

    while (digits < precision) {
      result += "0";
      digits += 1;
    }

    result += expstr;
  } else {
    result = input.substring(0, start + number + 1);

    while (digits < precision) {
      result += "0";
      digits += 1;
    }

    if (input.length != stop) {
      result += input.substring(locE, input.length);
    }
  }

  return result;
};

IEEE754.numCutOff = function(input, precision) {
  var result = '';
  var tempstr = '';

  var temp = input;
  if(temp < 1) {
    temp += 1;
  }

  tempstr = "" + temp;

  tempstr = IEEE754.numStrClipOff(tempstr, precision);

  if(temp == input) {
    result = tempstr.substring(0, 1);
  } else {
    result = "0";
  }

  result += tempstr.substring(1, tempstr.length);

  return result;
};

IEEE754.prototype.convert2dec = function() {
  var output = '';

  var s;
  var i;
  var dp;
  var val;
  var hid;
  var temp;
  var decValue;
  var power;
  
  s = (this.size == 32) ? 9 : 12;

  if ((this.binaryPower < this.minExp) || (this.binaryPower > this.maxExp)) {
    dp = 0;
    val = 0;
  } else {
    dp = - 1;
    val = 1;
  }

  for (i = s; i < this.size; i++) {
    val += parseInt(this.result[i])*Math.pow(2, dp + s - i);
  }

  decValue = val * Math.pow(2, this.binaryPower);

  if (this.size == 32) {
    s = 8;
    if (val > 0) {
      power = Math.floor(Math.log(decValue) / Math.LN10);
      decValue += 0.5 * Math.pow(10, power - s + 1);
      val += 5E-8
    }
  } else {
    s = 17;
  }

  if (this.result[0] == 1) {
    decValue = - decValue;
  }

  //the system refuses to display negative "0"s with a minus sign
  this.decValue = "" + decValue;
  if ((this.decValue == "0") && (this.result[0] == 1)) {
    this.decValue = "-" + this.decValue;
  }

  this.decValue = IEEE754.numStrClipOff(this.decValue, s);

  output = IEEE754.numCutOff(val, s);

  return output
};


IEEE754.computeFromBinary = function(obj, rounding) {
/*
  in this javascript program, bit positions are numbered
  0 ~ 32/64 from left to right instead of right to left, the
  way the output is presented
*/
  ieee32 = new IEEE754(32);
  ieee64 = new IEEE754(64);

  var input;
  var index1;
  var cnst;
  
  input = obj.input;
  input = input.trim();

  ieee64.dec2bin(input);
  ieee64.binString =
    ieee64.convert2bin(ieee64.dispStr, ieee64.statCond64, ieee64.result[0],
                       ieee64.binaryPower, false);
  obj.bin64_0 = ieee64.binString.substring(0, 1);
  obj.bin64_1 = ieee64.binString.substring(1, 12);
  if ((ieee64.binaryPower < ieee64.minExp) ||
      (ieee64.binaryPower > ieee64.maxExp)) {
    obj.bin64_12 = "  ";
    obj.bin64_12 += ieee64.binString.substring(12, 13);
    obj.bin64_12 += ".";
    obj.bin64_12 += ieee64.binString.substring(13, 64);
  } else {
    obj.bin64_12 = "1 .";
    obj.bin64_12 += ieee64.binString.substring(12, 64);
  }
  obj.stat64 = ieee64.statCond;
  obj.binpwr64 = ieee64.binaryPower;
  obj.binpwr64f = ieee64.binaryPower + ieee64.expBias;
  obj.dec64sig = ieee64.convert2dec();
  if (ieee64.dispStr != "") {
    obj.dec64 = ieee64.dispStr;
    obj.dec64sig = "";
  } else {
    obj.dec64 = ieee64.decValue;
  }

  obj.hex64 = ieee64.convert2hex();

  var cnst = 2102;         // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
  for (index1 = 0; index1 < cnst; index1++) {
    ieee32.binVal[index1] = ieee64.binVal[index1];
  }

  ieee32.binString =
    ieee32.convert2bin(ieee64.dispStr, ieee64.statCond64, ieee64.result[0],
                       ieee64.binaryPower, rounding);
  obj.bin32_0 = ieee32.binString.substring(0, 1);
  obj.bin32_1 = ieee32.binString.substring(1, 9);

  if ((ieee32.binaryPower < ieee32.minExp) ||
      (ieee32.binaryPower > ieee32.maxExp)) {
    obj.bin32_9 = "  ";
    obj.bin32_9 += ieee32.binString.substring(9, 10);
    obj.bin32_9 += ".";
    obj.bin32_9 += ieee32.binString.substring(10, 32);
  } else {
    obj.bin32_9 = "1 .";
    obj.bin32_9 += ieee32.binString.substring(9, 32);
  }
  obj.stat32 = ieee32.statCond;
  obj.binpwr32 = ieee32.binaryPower;
  obj.binpwr32f = ieee32.binaryPower + ieee32.expBias;
  obj.dec32sig = ieee32.convert2dec();

  if (ieee32.dispStr != "") {
    obj.dec32 = ieee32.dispStr;
    obj.dec32sig = "";
  } else {
    obj.dec32 = ieee32.decValue;
  }
  obj.hex32 = ieee32.convert2hex();

  if ((ieee64.dispStr != "") && (ieee32.dispStr != "")) {
    obj.entered = ieee64.dispStr;
  } else {
    obj.entered = input * 1.0;
  }
};

IEEE754.computeFromHex = function(obj, rounding) {
/*
  in this javascript program, bit positions are numbered 
  0 ~ 32/64 from left to right instead of right to left, the
  way the output is presented
*/

  var index1;
  var cnst;

  ieee32 = new IEEE754(32)
  ieee64 = new IEEE754(64)

  ieee64.binString = ieee64.hex2bin(obj.hex64b)
  if (ieee64.binString != "exit") {
    obj.bin64_0 = ieee64.binString.substring(0, 1);
    obj.bin64_1 = ieee64.binString.substring(1, 12);

    if ((ieee64.binaryPower < ieee64.minExp) ||
        (ieee64.binaryPower > ieee64.maxExp)) {
      obj.bin64_12 = "  ";
      obj.bin64_12 += ieee64.binString.substring(12, 13);
      obj.bin64_12 += ".";
      obj.bin64_12 += ieee64.binString.substring(13, 64);
    } else {
      obj.bin64_12 = "1 .";
      obj.bin64_12 += ieee64.binString.substring(12, 64);
    }

    obj.stat64 = ieee64.statCond;
    obj.binpwr64 = ieee64.binaryPower;
    obj.binpwr64f = ieee64.binaryPower + ieee64.expBias;
    obj.dec64sig = ieee64.convert2dec();

    if (ieee64.dispStr != "") {
      obj.entered = ieee64.dispStr;
      obj.dec64 = ieee64.dispStr;
      obj.dec64sig = "";
    } else {
      obj.entered = ieee64.fullDecValue;
      obj.dec64 = ieee64.decValue;
    }
    obj.hex64 = ieee64.convert2hex();

    cnst = 2102;         // 1 (carry bit) + 1023 + 1 + 1022 + 53 + 2 (round bits)
    for (index1 = 0; index1 < cnst; index1++) {
      ieee32.binVal[index1] = ieee64.binVal[index1];
    }

    ieee32.binString =
      ieee32.convert2bin(ieee64.dispStr, ieee64.statCond, ieee64.result[0],
                         ieee64.binaryPower, rounding);
    obj.bin32_0 = ieee32.binString.substring(0, 1);
    obj.bin32_1 = ieee32.binString.substring(1, 9);

    if ((ieee32.binaryPower < ieee32.minExp) ||
        (ieee32.binaryPower > ieee32.maxExp)) {
      obj.bin32_9 = "  ";
      obj.bin32_9 += ieee32.binString.substring(9, 10);
      obj.bin32_9 += ".";
      obj.bin32_9 += ieee32.binString.substring(10, 32);
    } else {
      obj.bin32_9 = "1 .";
      obj.bin32_9 += ieee32.binString.substring(9, 32);
    }

    obj.stat32 = ieee32.statCond;
    obj.binpwr32 = ieee32.binaryPower;
    obj.binpwr32f = ieee32.binaryPower + ieee32.expBias;
    obj.dec32sig = ieee32.convert2dec();

    if (ieee32.dispStr != "") {
      obj.dec32 = ieee32.dispStr;
      obj.dec32sig = "";
    } else {
      obj.dec32 = ieee32.decValue;
    }

    obj.hex32 = ieee32.convert2hex();
  }
};

IEEE754.hexToDouble = function(hex) {
  var obj ={};
  obj.hex64b = hex.toString();

  IEEE754.computeFromHex(obj, true);
  //alert(obj.toSource().replace(/,/g, ',\r'));
  return obj.dec64 * 1.0;
};


IEEE754.doubleToHex = function(val) {
  var obj ={};
  obj.input = val.toString();

  IEEE754.computeFromBinary(obj, true);
  //alert(obj.toSource().replace(/,/g, ',\r'));
  return obj.hex64;
};
IEEE754.binToDouble = function(bin) {
  var obj ={};
  obj.hex64b = IEEE754.binToHex(bin);

  IEEE754.computeFromHex(obj, true);
  //alert(obj.toSource().replace(/,/g, ',\r'));
  return obj.dec64 * 1.0;
};
IEEE754.doubleToBin = function(val) {
  var obj ={};
  obj.input = val.toString();

  IEEE754.computeFromBinary(obj, true);
  //alert(obj.toSource().replace(/,/g, ',\r'));
  return IEEE754.hexToBin(obj.hex64);
};

IEEE754.binToHex = function(s, whitespace) {
  function hexDigit(d) {
    if (d < 10) return d.toString();
    d -= 10;
    return String.fromCharCode('A'.charCodeAt(0) + d);
  }
  var str = '';
  s = s.toString();

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
    var ch = s.charCodeAt(i);
    str += hexDigit(ch >> 4) + hexDigit(ch & 0xF);
  }
  return str;
};
IEEE754.hexToBin = function(h) {
  function binMap(n) {
    if (n.match(/[0-9]/)) return parseInt(n);
    return parseInt((n.charCodeAt(0) - 'A'.charCodeAt(0)) + 10);
  }

  h = h.toUpperCase().replace(/\s/g, '');
  var bytes = '';

  for (var i = 0; i < h.length/2; i++) {
    var hi = h.charAt(i * 2);
    var lo = h.charAt(i * 2 + 1);
    var b = (binMap(hi) << 4) + binMap(lo);
    bytes += String.fromCharCode(b);
  }
  return bytes;
};
IEEE754.hexToJS = function(h) {
  var str = '';
  var blockSize = 64;
  var blockCnt = (h.length/blockSize).toFixed();

  for (var i = 0; i < blockCnt; i++) {
    var ofs = i * blockSize;
    str += "  \"" + h.slice(ofs, ofs + blockSize) + "\" +\n";
  }

  str += "  \"" + h.slice(blockCnt * blockSize) + "\"\n";
  return str;
};

IEEE754.test = function() {
  var hex = "4036333333333333"; // 22.2 in hex
  var bin = 22.2;
  alert("The next alert should read: " + hex);
  alert(IEEE754.doubleToHex(bin));
  alert("The next alert should read: " + bin);
  alert(IEEE754.hexToDouble(hex));

  // need to add code in here for double/binary conversions
  // testing for those entry points was performed in ESTK
};

//IEEE754.test();

"ieee754.js"

// EOF
