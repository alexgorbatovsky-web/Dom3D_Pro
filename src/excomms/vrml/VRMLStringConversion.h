/* 
 * File		: VRMLStringConversion.h
 * Purpouse	: Parses strings into common types interface.
 * Data		: 11/04/2013
 */

#ifndef __VRMLStringConversion_H__
#define __VRMLStringConversion_H__

#define STR_FALSE "FALSE\0"
#define STR_TRUE "TRUE\0"

#define V_CS    ' '  // space
#define V_TAB   '\t' // tab
#define V_CR    '\r' // caret
#define V_CN    '\n' // end line
#define V_COMMA ','  // comma
#define V_CZ    '\0' // zero
#define V_CAPST '"'  // apostrophe 
#define V_CBCL  '{'	 // braces left
#define V_CBCR  '}'	 // braces right   
#define V_CQR   ']'  // guard right
#define V_CQL   '['  // quard left

typedef char CH;

class VRMLStringConversion {
private: VRMLStringConversion() {}
public:
	// bool, int, float convertion
	static bool ToBoolean(const char** value);
	inline static bool ToBoolean(const char* value) { return ToBoolean(&value); }
	static float ToFloat(const CH** value);
	inline static float ToFloat(const CH* value) { return ToFloat(&value); } 
	static int32 ToInt32(const CH** value);
	inline static int32 ToInt32(const CH* value) { return ToInt32(&value); }
	static uint32 HexToUInt32(const CH** value);
	inline static uint32 HexToUInt32(const CH* value) { return HexToUInt32(&value); }
	// vector point conversion
	static sfvec2f ToVector2(const CH** value);
	inline static sfvec2f ToVector2(const CH* value) { return ToVector2(&value); } /**< See above. */
	static sfvec3f ToVector3(const CH** value);
	inline static sfvec3f ToVector3(const CH* value) { return ToVector3(&value); } 
	static sfvec4f ToVector4(const CH** value); 
	inline static sfvec4f ToVector4(const CH* value) { return ToVector4(&value); }
	// float, int list convertion 
	static void ToFloatList(const CH* value, mffloat& array);
	static void ToInt32List(const CH* value, mfint32& array);
	
	// vector float list convertion 
	static void ToVec3List(const CH* value, mfvec3f& array);
	static void ToVec2List(const CH* value, mfvec2f& array);
	static void ToVecRgbList(const CH* value, mfcolor& array);
	template <class T>
	static void ToVecList(const CH* value, mfarray<T>& _array);
	// count values
	static int CountValues(const CH* sz);
};
#endif