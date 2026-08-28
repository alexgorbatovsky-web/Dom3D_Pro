/*
 * File		: VRMLStringConversion.hpp
 * Purpouse	: Parses strings into common types.
 * Data		: 11/04/2013
 */

#include "VRMLStringConversion.h"

 bool VRMLStringConversion::ToBoolean(const CH** value)
 {
	bool resval=true;
	if(!*value) return 0;
	
	const CH *s= *value;
	CH c;
	
	while( (c = *s) != 0 && 	
		   (c == ' ' || c == '\t' || c == '\r' || c == '\n') && 
		   (c != 'F' && c != 'f' && c != 'T' && c != 't'))  ++s; 
	
	if(c == 'F' || c == 'f') {
		const char *sf = STR_FALSE;
		while( (c = *s) != 0 && 
			   (c == (*sf) && *sf != V_CZ)) {
			++s; 
			++sf; 
		} 
		if(*sf == '\0') resval = false;
		else resval = true;
	}
	while ((c = *s) != '\0' && (c != ' ' && c != '\t' && c != '\r' && c != '\n')) s++;
	*value = s;
	return resval;
}


int32 VRMLStringConversion::ToInt32(const CH** value)
{
	if (!*value) return 0;
	const CH* s = *value;
	CH c;
	while ((c = *s) != 0 && 
		   (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',')) 
	{ ++s; }

	int32 val = 0;
	int32 sign = 1;
	if (*s == '-') { ++s; sign = -1; }

	while ((c = *s) != 0)
	{
		if (c >= '0' && c <= '9') val = val * 10 + c - '0';
		else break;
		++s;
	}
	val *= sign;
	while ( (c = *s) != '\0' && 
			(c != ' '  &&  c != '\t' && c != '\n' &&  c != ',')) 
	{ s++; }
	while ( (c = *s) != '\0' && 
			(c == ' '  || c == '\t' || c == '\n' || c == ',')) 
	{ s++; }
	*value = s;
	return val;
}

uint32 VRMLStringConversion::HexToUInt32(const CH** value)
{
	if (value == NULL || *value == NULL || **value == 0) return 0;

	const CH* s = *value;
	CH c;
	while ((c = *s) != 0 && 
		   (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',')) 
	{ ++s; }

	// Skip any '0x' prefix.
	if (*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) s += 2;

	uint32 val = 0;
	while ((c = *s) != 0)
	{
		if (c >= '0' && c <= '9') val = val * 16 + c - '0';
		else if (c >= 'A' && c <= 'F') val = val * 16 + c + 10 - 'A';
		else if (c >= 'a' && c <= 'f') val = val * 16 + c + 10 - 'a';
		else break;
		++s;
	}
	while ( (c = *s) != '\0' && 
		    (c != ' '  &&  c != '\t' && c != '\n' && c != ',')) 
	{ s++; }
	while ( (c = *s) != '\0' && 
		    (c == ' ' || c == '\t' ||  c == '\n' || c == ',')) 
	{ s++; }
	
	*value = s;
	return val;
}

float VRMLStringConversion::ToFloat(const CH** value)
{
	const CH* s = *value;
	if (s == NULL || *s == 0) return 0.0f;
	CH c;
	while ((c = *s) != 0 && 
		   (c == ' '  || 
		    c == '\t' || 
			c == '\r' || 
			c == '\n' || 
			c == ',')) 
	{ ++s; }

	double val = 0.0;
	float sign = 1.0;
	if (*s == '-') { ++s; sign = -1.0; }
	float decimals = 0.0;
	int32 exponent = 0;
	bool nonValidFound = false;
	while ((c = *s) != 0 && !nonValidFound)
	{
		switch (c)
		{
		case '.': decimals = 1; break;
		case '0': val *= 10.0; decimals *= 10.0; break;
		case '1': val = val * 10.0 + 1.0; decimals *= 10.0; break;
		case '2': val = val * 10.0 + 2.0; decimals *= 10.0; break;
		case '3': val = val * 10.0 + 3.0; decimals *= 10.0; break;
		case '4': val = val * 10.0 + 4.0; decimals *= 10.0; break;
		case '5': val = val * 10.0 + 5.0; decimals *= 10.0; break;
		case '6': val = val * 10.0 + 6.0; decimals *= 10.0; break;
		case '7': val = val * 10.0 + 7.0; decimals *= 10.0; break;
		case '8': val = val * 10.0 + 8.0; decimals *= 10.0; break;
		case '9': val = val * 10.0 + 9.0; decimals *= 10.0; break;
		case 'e':
		case 'E': ++s; 
				  exponent = ToInt32(&s); 
				  s -= 2; 
				  nonValidFound = true; 
				  break;
		default: nonValidFound = true; --s; break;
		}
		++s;
	}
	float resval = 0.0f;
	
	// Generate the value
	if (decimals == 0.0) decimals = 1.0;
	resval = (float) (val * sign / decimals);
	if (exponent != 0) resval *= powf(10.0f, (float) exponent);
	// Skip beg and end whitespaces
	while ((c = *s) != 0 && 
			c != ' '  && 
			c != '\t' && 
			c != '\r' && 
			c != '\n' && 
			c != ',')   
	{ ++s; }
	while ((c = *s) != 0 && 
		   (c == ' '  || 
		    c == '\t' || 
			c == '\r' || 
			c == '\n' || 
			c == ',')) 
	{ ++s; }
	*value = s;
	return resval;
}

sfvec2f VRMLStringConversion::ToVector2(const CH** value)
{
	sfvec2f p;
 	if (value != NULL && *value != NULL && **value != 0)
	{
		p.x = ToFloat(value);
		p.y = ToFloat(value);
	}
	return p;
}

sfvec3f VRMLStringConversion::ToVector3(const CH** value)
{
	sfvec3f p;
 	if (value != NULL && *value != NULL && **value != 0)
	{
		p.x = ToFloat(value);
		p.y = ToFloat(value);
		p.z = ToFloat(value);
	}
	return p;
}

sfvec4f VRMLStringConversion::ToVector4(const CH** value)
{
	sfvec4f p;
 	if (value != NULL && *value != NULL && **value != 0)
	{
		p.x = ToFloat(value);
		p.y = ToFloat(value);
		p.z = ToFloat(value);
		p.w = ToFloat(value);
	}
	return p;
}

int VRMLStringConversion::CountValues(const CH* sz)
{
	int count = 0;
	if (sz != NULL && *sz > 0)
	{
		while (*sz != 0 || *sz != ']')
		{
			// Clear all whitespace
			while (*sz != 0 && *sz != ']' &&
				  (*sz == ' '  || 
				   *sz == '\t' || 
				   *sz == '\r' || 
				   *sz == '\n' ||
				   *sz == ',')) ++sz;
			if (*sz == 0 || *sz == ']') break;
			// New value found.
			++count;
			while (*sz != 0    && 
				   *sz != ']'  &&
				   *sz != ' '  && 
				   *sz != '\t' && 
				   *sz != '\r' && 
				   *sz != '\n' &&
				   *sz != ',') ++sz;
		}
	}
	return count;
}

// float, int list convertion 

void VRMLStringConversion::ToInt32List(const CH* value, mfint32& _array)
{
	if (value != NULL && *value != 0)
	{ 
		_array.Clear();
		int  count = CountValues(value);
		if (count > 0) { 
			_array.SetCount(count);
			int ii = 0;
			while (*value != 0 && ii < count) { 
				_array[ii]=ToInt32(&value); 
				++ii; 
			}
			if( ii < count) _array.RemoveAt(ii,count-ii);
		}
	}
}

void VRMLStringConversion::ToFloatList(const CH* value, mffloat& _array)
{
	if (value != NULL && *value != 0)
	{ 
		_array.Clear();
		int  count = CountValues(value);
		if (count > 0) { 
			_array.SetCount(count);
			int ii = 0;
			while (*value != 0 && ii < count) { 
				_array[ii]=ToFloat(&value); 
				++ii; 
			}
			if( ii < count) _array.RemoveAt(ii,count-ii);
		}
	}
}

// vector float list convertion 
void VRMLStringConversion::ToVec2List(const CH* value, mfvec2f& _array)
{
	if (value != NULL && *value != 0)
	{ 
		_array.Clear();
		int count = CountValues(value) / 2;
		if (count > 0) { 
			_array.SetCount(count);
			int ii = 0;
			while(*value != 0 && ii < count) { 
				_array[ii].x=ToFloat(&value); 
				_array[ii].y=ToFloat(&value); 
				++ii; 
			}
			if( ii < count) _array.RemoveAt(ii,count-ii);
		}
	}
}

void VRMLStringConversion::ToVec3List(const CH* value, mfvec3f& _array)
{
	if (value != NULL && *value != 0)
	{ 
		_array.Clear();
		int count = CountValues(value) / 3;
		if (count > 0) { 
			_array.SetCount(count);
			int ii = 0;
			while(*value != 0 && ii < count) { 
				_array[ii].x=ToFloat(&value); 
				_array[ii].y=ToFloat(&value);
				_array[ii].z=ToFloat(&value);
				++ii; 
			}
			if( ii < count) _array.RemoveAt(ii,count-ii);
		}
	}
}

void VRMLStringConversion::ToVecRgbList(const CH* value, mfcolor& _array)
{
	if (value != NULL && *value != 0)
	{ 
		_array.Clear();
		int count = CountValues(value) / 3;
		if (count > 0) { 
			_array.SetCount(count);
			int ii = 0;
			while(*value != 0 && ii < count) { 
				_array[ii].r=ToFloat(&value); 
				_array[ii].g=ToFloat(&value);
				_array[ii].b=ToFloat(&value);
				++ii; 
			}
			if( ii < count) _array.RemoveAt(ii,count-ii);
		}
	}
}

template <>
void VRMLStringConversion::ToVecList<cVec3>(const CH* value, mfarray<cVec3>& _array) {
	ToVec3List(value,_array); 
}

template <>
void VRMLStringConversion::ToVecList<cVec2>(const CH* value, mfarray<cVec2>& _array) {
	ToVec2List(value,_array); 

}
template <>
void VRMLStringConversion::ToVecList<rgb>(const CH* value, mfarray<rgb>& _array) {
	ToVecRgbList(value,_array); 
}

