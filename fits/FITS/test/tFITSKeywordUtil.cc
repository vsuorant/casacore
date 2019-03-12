//# tFITSKeywordUtil.cc: Test program for FITSKeywordUtil
//# Copyright (C) 2002
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify it
//# under the terms of the GNU General Public License as published by the Free
//# Software Foundation; either version 2 of the License, or (at your option)
//# any later version.
//#
//# This program is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//# more details.
//#
//# You should have received a copy of the GNU General Public License along
//# with this program; if not, write to the Free Software Foundation, Inc.,
//# 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA
//#
//# $Id$

//# Includes
#include <casacore/fits/FITS/FITSKeywordUtil.h>

#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/Cube.h>
#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Exceptions/Error.h>
#include <casacore/fits/FITS/fits.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/Utilities/Regex.h>

#include <casacore/casa/namespace.h>
int main()
{
    try {
	// Create a Record that will contain the value to be put into FITS keywords
	Record myKeywords;
	myKeywords.define("hello", 6.5);
	myKeywords.define("world", True);
	// long keywords are truncated to 8 characters
	myKeywords.define("alongname", Short(-1));
	// other scalar types to round out the testing of the code
	myKeywords.define("a", uInt(10));
	myKeywords.define("b", Int(-10));
	myKeywords.define("c", Float(10.0));
	myKeywords.define("d","I like dogs");
	// Array types for testing
	Vector<Bool> flags(2); flags(0) = False; flags(1) = True;
	myKeywords.define("flags", flags);
	// NAXIS generates the NAXIS keyword as well as NAXIS1 .. NAXISn
	Vector<Int> naxis(5); naxis(0) = 128; naxis(1) = 64; naxis(2) = 32; 
	naxis(3) = 16; naxis(4) = 8;
	myKeywords.define("naxis", naxis);
	Vector<Float> tarray(3); tarray(0) = 1; tarray(1) = 2; tarray(2) = 3;
	myKeywords.define("tarray", tarray);
	// make one a Matrix
	Matrix<Double> mat(3,3);
	mat(0,0) = 1;
	mat(1,0) = 2;
	mat(2,0) = 3;
	mat(0,1) = 4;
	mat(1,1) = 5;
	mat(2,1) = 6;
	mat(0,2) = 7;
	mat(1,2) = 8;
	mat(2,2) = 9;
	myKeywords.define("ma", mat);
	Vector<String> sarray(2);
	sarray(0) = "Hello";
	sarray(1) = "World";
	myKeywords.define("sarray", sarray);
	
	// scalar fields can have comments
	myKeywords.setComment("hello", "A comment for HELLO");
	myKeywords.setComment("alongname", "This is truncated from ALONGNAME");

	// the CD-matrix keywords are special
	myKeywords.define("cd1_1", 1.0);
	myKeywords.define("cd1_2", 0.0);
	myKeywords.define("cd2_1", 0.0);
	myKeywords.define("cd2_2", 1.0);
	
	// Add a couple of comments. Note that addComment appends numbers to each 
	// comment to keep them unique
	// Record::define could also be used - so long as the name of the comment 
	// was unique e.g. "comment1" and "comment2"
	FITSKeywordUtil::addComment(myKeywords, "Comment created by testKeywordUtil.");
	FITSKeywordUtil::addComment(myKeywords, "Boring content, I agree!");
	// Multi-line comments can also be added - the line is split at \n
	// There is no check on the length of the comment
	FITSKeywordUtil::addComment(myKeywords, 
             "This comment will extend across\nmultiple lines.\nThree in this case.");
	
	// And add a HISTORY card - generally FITSHistoryUtil should be used 
	FITSKeywordUtil::addHistory(myKeywords, "tFITSUtil");    
	
	// Create an empty FitsKeywordList, containing only "SIMPLE=T", which is
	// necessary for all valid FITS files.
	FitsKeywordList nativeList = FITSKeywordUtil::makeKeywordList();
	
	// OK, now lets add the keywords to the native list
	AlwaysAssertExit(FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	
	// Fetch the keywords into another Record, suppressing "simple", since it 
	// is generated by makeKeywordList and also suppress "world".
	Record myNewKeywords;
	Vector<String> ignore(2); ignore(0) = "world"; ignore(1) = "simple";
	// can't pass a FitsKeywordList as second arg of getKeywords because 
	// it isn't const.
	ConstFitsKeywordList nativeListRO(nativeList);
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	// compare contents of myNewKeywords with myKeywords
	// Every field in myKeywords should be in myNewKeywords EXCEPT
	//   world, because we ignored it
	//   comment* fields
	//   history* fields
	//   And alongname will have been truncated to alongnam
	// also check comments of each field
	for (uInt i=0;i<myKeywords.nfields();i++) {
	    String inName = myKeywords.name(i);
	    String outName = inName;
	    if (inName == "alongname") outName = "alongnam";
	    Int outField = myNewKeywords.fieldNumber(outName);
	    AlwaysAssertExit(outField>=0 || inName == "world" || 
			     inName.contains(Regex("^comment")) ||
			     inName.contains(Regex("^history")));
	    // check types
	    
	    if (outField >= 0) {
		// short -> int
		// uInt -> int
		// Array<Float> -> Array<Double>
		DataType inType = myKeywords.dataType(i);
		DataType outType = myNewKeywords.dataType(outField);
		AlwaysAssertExit(inType == outType ||
				 (inType == TpShort && outType == TpInt) ||
				 (inType == TpUInt && outType == TpInt) ||
				 (inType == TpArrayFloat && outType == TpArrayDouble));
		// check shapes
		AlwaysAssertExit(myKeywords.shape(i) == myNewKeywords.shape(outField));
		// check comments
		AlwaysAssertExit(myKeywords.comment(i) == 
				 myNewKeywords.comment(outField));
		// finally, check values
		switch(myKeywords.dataType(i)) {
		case TpBool:
		    AlwaysAssertExit(myKeywords.asBool(i) == 
				     myNewKeywords.asBool(outField));
		    break;
		case TpUInt:
		    AlwaysAssertExit(Int(myKeywords.asuInt(i)) == 
				     myNewKeywords.asInt(outField));
		    break;
		case TpInt:
		    AlwaysAssertExit(myKeywords.asInt(i) == 
				     myNewKeywords.asInt(outField));
		    break;
		case TpShort:
		    AlwaysAssertExit(Int(myKeywords.asShort(i)) == 
				     myNewKeywords.asInt(outField));
		    break;
		case TpFloat:
		    AlwaysAssertExit(myKeywords.asFloat(i) == 
				     myNewKeywords.asFloat(outField));
		    break;
		case TpDouble:
		    AlwaysAssertExit(myKeywords.asDouble(i) == 
				     myNewKeywords.asDouble(outField));
		    break;
		case TpString:
		    AlwaysAssertExit(myKeywords.asString(i) == 
				     myNewKeywords.asString(outField));
		    break;
		case TpArrayBool:
		    AlwaysAssertExit(allEQ(myKeywords.asArrayBool(i),
					   myNewKeywords.asArrayBool(outField)));
		    break;
		case TpArrayInt:
		    AlwaysAssertExit(allEQ(myKeywords.asArrayInt(i),
					   myNewKeywords.asArrayInt(outField)));
		    break;
		case TpArrayFloat:
		    {
			Array<Double> inArr(myKeywords.shape(i));
			convertArray(inArr, myKeywords.asArrayFloat(i));
			AlwaysAssertExit(allEQ(inArr,
					       myNewKeywords.asArrayDouble(outField)));
		    }
		    break;
		case TpArrayDouble:
		    AlwaysAssertExit(allEQ(myKeywords.asArrayDouble(i),
					   myNewKeywords.asArrayDouble(outField)));
		    break;
		case TpArrayString:
		    AlwaysAssertExit(allEQ(myKeywords.asArrayString(i),
					   myNewKeywords.asArrayString(outField)));
		    break;
		default:
		    throw(AipsError("Unrecognized field data type"));
		}
	    }
	}
	
	// the COMMENT keywords can't come out easily, so don't test for them.
	// test HISTORY elsewhere
	
	// remove the tarray keywords
	ignore(0) = "tarray.*";
	FITSKeywordUtil::removeKeywords(myNewKeywords, ignore);
	// verify that myNewKeywords doesn't contain any tarray keywords
	Regex rx("tarray.*");
	for (uInt i=0;i<myNewKeywords.nfields();i++) {
	    AlwaysAssertExit(!myNewKeywords.name(i).contains(rx));
	}
	
	// test the TDIM manipulation functions
	IPosition ipos;
	String tdim("(128,4,2)");
	AlwaysAssertExit(FITSKeywordUtil::fromTDIM(ipos, tdim));
	AlwaysAssertExit(ipos == IPosition(3,128,4,2));
	
	ipos.resize(0);
	ipos = IPosition(4,512,128,4,2);
	AlwaysAssertExit(FITSKeywordUtil::toTDIM(tdim, ipos));
	AlwaysAssertExit(tdim=="(512,128,4,2)");
	
	// Test for expected failures.
	// do these one at a time to ensure that each is tested separately
	Record empty;
	
	// String keyword value longer than 68 characters - truncated with SEVERE error
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("longstr",
	  "This is a long string keyword value that will be truncated at 68 characters in length");
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	AlwaysAssertExit(myNewKeywords.asString("longstr").length()==68);
	
	// illegal FITS type - ignored
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("cplxkey",Complex(1.0));
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	AlwaysAssertExit(myNewKeywords.fieldNumber("cplxkey")<0);
	
	// too many dimensions
	Cube<Int> c(1,2,3); c = 1;
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("c", c);
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	AlwaysAssertExit(myNewKeywords.shape("c") == IPosition(1,6));
	
	// name too long for matrix name - max of 2 chars
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("mat", mat);
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	// all fields should not contain "mat" but should contain "ma"
	for (uInt i=0;i<myNewKeywords.nfields();i++) {
	    String name = myNewKeywords.name(i);
	    AlwaysAssertExit(!name.contains(Regex("^mat")) && 
			     name.contains(Regex("^ma")));
	}
	
	// name too long for array field - max of 8 chars including number of elements
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("sarrayname", sarray);
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	// should have sarrayn array
	AlwaysAssertExit(myNewKeywords.nfields()==1 && 
			 myNewKeywords.fieldNumber("sarrayn")>=0);
	
	// illegal array type
	Vector<Complex> cvec(1); cvec = 1.0;
	myKeywords = myNewKeywords = empty;
	nativeList = FITSKeywordUtil::makeKeywordList();
	myKeywords.define("cvec", cvec);
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	AlwaysAssertExit(myNewKeywords.nfields()==0);
	
	// illegal type (not a scalar or an array - i.e. another record);
	myKeywords = myNewKeywords = empty;
	myKeywords.defineRecord("rec",Record());
	nativeList = FITSKeywordUtil::makeKeywordList();
	AlwaysAssertExit(!FITSKeywordUtil::addKeywords(nativeList, myKeywords));
	AlwaysAssertExit(FITSKeywordUtil::getKeywords(myNewKeywords, nativeListRO, 
						      ignore));
	AlwaysAssertExit(myNewKeywords.nfields()==0);

    } catch (AipsError& x) {
	cerr << "Caught an exception: " << x.getMesg() << endl;
	return 1;
    }
    return 0;
}
