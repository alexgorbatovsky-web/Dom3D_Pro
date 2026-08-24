#pragma once

//*****************************************************************************
// cColor
//*****************************************************************************
class cColor {
public:
	float r;
	float g;
	float b;
	float a;
	
	cColor();

	cColor(const float Red, const float Green, const float Blue, const float Alpha = 1.0f);
	explicit cColor(const float Gray, const float Alpha = 1.0f);
	
	cColor(const int Red, const int Green, const int Blue, const int Alpha = 255);
	explicit cColor(const int Gray, const int Alpha = 255);

	cColor(const cColor &Base, const float Alpha);
	cColor(const cColor &Base, const int Alpha);

	void Copy(const float *Float4);
	void SetZero();
	
	void Set(const float Red, const float Green, const float Blue, const float Alpha = 1.0f);
	void Set(const float Gray, const float Alpha = 1.0f);

	void Set(const int Red, const int Green, const int Blue, const int Alpha = 255);
	void Set(const int Gray, const int Alpha = 255);

	void Set(const cColor &Base, const float Alpha);
	void Set(const cColor &Base, const int Alpha);
	
	float & operator [] (const int Index);
	float operator [] (const int Index) const;

	const cColor operator - () const; 

	void operator += (const cColor &);
	void operator -= (const cColor &);
	void operator *= (const cColor &);
	void operator /= (const cColor &);
	void operator *= (const float);
	void operator /= (const float);

	const cColor operator + (const cColor &) const;
	const cColor operator - (const cColor &) const;
	const cColor operator * (const cColor &) const;
	const cColor operator / (const cColor &) const;
	const cColor operator * (const float) const;
	friend const cColor operator * (const float, const cColor &);
	const cColor operator / (const float) const;
	
	bool operator == (const cColor &) const;
	bool operator != (const cColor &) const;
	static bool Equals(const cColor &, const cColor &, const float Eps = cMath::Epsilon);

	void Saturate();
	
	static const cColor Rand(const cColor &c0, const cColor &c1);
	static const cColor Negative(const cColor &);
	static const cColor Lerp(const cColor &, const cColor &, const float);
	static const cColor Lerp05(const cColor &, const cColor &);
	static const cColor Saturate(const cColor &);
	static float ToGray(const cColor &);
	static const cColor ToGrayScale(const cColor &);
	static const cColor AdjustSaturation(const cColor &, const float);
	static const cColor AdjustContrast(const cColor &, const float);
	static dword FloatToDword(const float);
	
	static const cColor Zero;
	static const cColor Transparent;
	
	static const cColor AliceBlue;
	static const cColor AntiqueWhite;
	static const cColor Aqua;
	static const cColor Aquamarine;
	static const cColor Azure;
	static const cColor Beige;
	static const cColor Bisque;
	static const cColor Black;
	static const cColor BlanchedAlmond;
	static const cColor Blue;
	static const cColor BlueViolet;
	static const cColor Brown;
	static const cColor BurlyWood;
	static const cColor CadetBlue;
	static const cColor Chartreuse;
	static const cColor Chocolate;
	static const cColor Coral;
	static const cColor CornflowerBlue;
	static const cColor Cornsilk;
	static const cColor Crimson;
	static const cColor Cyan;
	static const cColor DarkBlue;
	static const cColor DarkCyan;
	static const cColor DarkGoldenrod;
	static const cColor DarkGray;
	static const cColor DarkGreen;
	static const cColor DarkKhaki;
	static const cColor DarkMagenta;
	static const cColor DarkOliveGreen;
	static const cColor DarkOrange;
	static const cColor DarkOrchid;
	static const cColor DarkRed;
	static const cColor DarkSalmon;
	static const cColor DarkSeaGreen;
	static const cColor DarkSlateBlue;
	static const cColor DarkSlateGray;
	static const cColor DarkTurquoise;
	static const cColor DarkViolet;
	static const cColor DeepPink;
	static const cColor DeepSkyBlue;
	static const cColor DimGray;
	static const cColor DodgerBlue;
	static const cColor Firebrick;
	static const cColor FloralWhite;
	static const cColor ForestGreen;
	static const cColor Fuchsia;
	static const cColor Gainsboro;
	static const cColor GhostWhite;
	static const cColor Gold;
	static const cColor Goldenrod;
	static const cColor Gray;
	static const cColor Green;
	static const cColor MidGreen;
	static const cColor GreenYellow;
	static const cColor Honeydew;
	static const cColor HotPink;
	static const cColor IndianRed;
	static const cColor Indigo;
	static const cColor Ivory;
	static const cColor Khaki;
	static const cColor Lavender;
	static const cColor LavenderBlush;
	static const cColor LawnGreen;
	static const cColor LemonChiffon;
	static const cColor LightBlue;
	static const cColor LightCoral;
	static const cColor LightCyan;
	static const cColor LightGoldenrodYellow;
	static const cColor LightGray;
	static const cColor LightGreen;
	static const cColor LightPink;
	static const cColor LightSalmon;
	static const cColor LightSeaGreen;
	static const cColor LightSkyBlue;
	static const cColor LightSlateGray;
	static const cColor LightSteelBlue;
	static const cColor LightYellow;
	static const cColor Lime;
	static const cColor LimeGreen;
	static const cColor Linen;
	static const cColor Magenta;
	static const cColor Maroon;
	static const cColor MediumAquamarine;
	static const cColor MediumBlue;
	static const cColor MediumOrchid;
	static const cColor MediumPurple;
	static const cColor MediumSeaGreen;
	static const cColor MediumSlateBlue;
	static const cColor MediumSpringGreen;
	static const cColor MediumTurquoise;
	static const cColor MediumVioletRed;
	static const cColor MidnightBlue;
	static const cColor MintCream;
	static const cColor MistyRose;
	static const cColor Moccasin;
	static const cColor NavajoWhite;
	static const cColor Navy;
	static const cColor OldLace;
	static const cColor Olive;
	static const cColor OliveDrab;
	static const cColor Orange;
	static const cColor OrangeRed;
	static const cColor Orchid;
	static const cColor PaleGoldenrod;
	static const cColor PaleGreen;
	static const cColor PaleTurquoise;
	static const cColor PaleVioletRed;
	static const cColor PapayaWhip;
	static const cColor PeachPuff;
	static const cColor Peru;
	static const cColor Pink;
	static const cColor Plum;
	static const cColor PowderBlue;
	static const cColor Purple;
	static const cColor Red;
	static const cColor RosyBrown;
	static const cColor RoyalBlue;
	static const cColor SaddleBrown;
	static const cColor Salmon;
	static const cColor SandyBrown;
	static const cColor SeaGreen;
	static const cColor SeaShell;
	static const cColor Sienna;
	static const cColor Silver;
	static const cColor SkyBlue;
	static const cColor SlateBlue;
	static const cColor SlateGray;
	static const cColor Snow;
	static const cColor SpringGreen;
	static const cColor SteelBlue;
	static const cColor Tan;
	static const cColor Teal;
	static const cColor Thistle;
	static const cColor Tomato;
	static const cColor Turquoise;
	static const cColor Violet;
	static const cColor Wheat;
	static const cColor White;
	static const cColor WhiteSmoke;
	static const cColor Yellow;
	static const cColor YellowGreen;
	
	int GetDimension() const;
	
	const float * ToFloatPtr() const;
	float * ToFloatPtr();
	
	dword ToDword() const;
	static dword ToDword(const int Red, const int Green, const int Blue, const int Alpha = 255);
	static dword ToDword(const int Gray, const int Alpha = 255);
	static const cColor FromDword(const dword);
	static const cColor FromRgbe(const byte *Rgbe);
	dword ToRgbe() const;
	
	const cStr ToWebColor() const;
};

// cColor.ctor : ()
inline cColor::cColor() {
}

// cColor.ctor : (const float, const float, const float, const float)
inline cColor::cColor(const float Red, const float Green, const float Blue, const float Alpha)
: r(Red), g(Green), b(Blue), a(Alpha) {}

// cColor.ctor : (const float, const float)
inline cColor::cColor(const float Gray, const float Alpha)
: r(Gray), g(Gray), b(Gray), a(Alpha) {}

// cColor.ctor : (const int, const int, const int, const int)
inline cColor::cColor(const int Red, const int Green, const int Blue, const int Alpha) {
	Set(Red, Green, Blue, Alpha);
}

// cColor.ctor : (const int, const int)
inline cColor::cColor(const int Gray, const int Alpha) {
	Set(Gray, Alpha);
}

// cColor.ctor : (const cColor &, const float)
inline cColor::cColor(const cColor &Base, const float Alpha)
: r(Base.r), g(Base.g), b(Base.b), a(Alpha) {}

// cColor.ctor : (const cColor &, const int)
inline cColor::cColor(const cColor &Base, const int Alpha) {
	Set(Base, Alpha);
}

// cColor::Copy
inline void cColor::Copy(const float *Float4) {
	cAssert(Float4 != nullptr);
	r = Float4[0];
	g = Float4[1];
	b = Float4[2];
	a = Float4[3];
}

// cColor::SetZero
inline void cColor::SetZero() {
	r = g = b = a = 0.0f;
}

// cColor::Set : void (const float, const float, const float, const float)
inline void cColor::Set(const float Red, const float Green, const float Blue, const float Alpha) {
	r = Red;
	g = Green;
	b = Blue;
	a = Alpha;
}

// cColor::Set : void (const float, const float)
inline void cColor::Set(const float Gray, const float Alpha) {
	r = g = b = Gray;
	a = Alpha;
}

// cColor::Set : void (const int, const int, const int, const int)
inline void cColor::Set(const int Red, const int Green, const int Blue, const int Alpha) {
	const float s = 1.0f / 255.0f;
	r = s * float(Red);
	g = s * float(Green);
	b = s * float(Blue);
	a = s * float(Alpha);
}

// cColor::Set : void (const int, const int)
inline void cColor::Set(const int Gray, const int Alpha) {
	const float s = 1.0f / 255.0f;
	r = g = b = s * float(Gray);
	a = s * float(Alpha);
}

// cColor::Set : void (const cColor &, const float)
inline void cColor::Set(const cColor &Base, const float Alpha) {
	r = Base.r;
	g = Base.g;
	b = Base.b;
	a = Alpha;
}

// cColor::Set : void (const cColor &, const int)
inline void cColor::Set(const cColor &Base, const int Alpha) {
	r = Base.r;
	g = Base.g;
	b = Base.b;

	const float s = 1.0f / 255.0f;
	a = s * float(Alpha);
}

// cColor::operator [] : float & (const int)
inline float & cColor::operator [] (const int Index) {
	cAssert(Index >= 0 && Index < 4);
	return (&r)[Index];
}

// cColor::operator [] : float (const int) const
inline float cColor::operator [] (const int Index) const {
	cAssert(Index >= 0 && Index < 4);
	return (&r)[Index];
}

// cColor::operator - : const cColor () const
inline const cColor cColor::operator - () const {
	return cColor(-r, -g, -b, -a);
}

// cColor::operator +=
inline void cColor::operator += (const cColor &c) {
	r += c.r;
	g += c.g;
	b += c.b;
	a += c.a;
}

// cColor::operator -=
inline void cColor::operator -= (const cColor &c) {
	r -= c.r;
	g -= c.g;
	b -= c.b;
	a -= c.a;
}

// cColor::operator *= : void (const cColor &)
inline void cColor::operator *= (const cColor &c) {
	r *= c.r;
	g *= c.g;
	b *= c.b;
	a *= c.a;
}

// cColor::operator /= : void (const cColor &)
inline void cColor::operator /= (const cColor &c) {
	r /= c.r;
	g /= c.g;
	b /= c.b;
	a /= c.a;
}

// cColor::operator *= : void (const float)
inline void cColor::operator *= (const float s) {
	r *= s;
	g *= s;
	b *= s;
	a *= s;
}

// cColor::operator /= : void (const float)
inline void cColor::operator /= (const float s) {
	const float is = 1.0f / s;
	r *= is;
	g *= is;
	b *= is;
	a *= is;
}

// cColor::operator +
inline const cColor cColor::operator + (const cColor &c) const {
	return cColor(r + c.r, g + c.g, b + c.b, a + c.a);
}

// cColor::operator - : const cColor (const cColor &) const
inline const cColor cColor::operator - (const cColor &c) const {
	return cColor(r - c.r, g - c.g, b - c.b, a - c.a);
}

// cColor::operator * : const cColor (const cColor &) const
inline const cColor cColor::operator * (const cColor &c) const {
	return cColor(r * c.r, g * c.g, b * c.b, a * c.a);
}

// cColor::operator / : const cColor (const cColor &) const
inline const cColor cColor::operator / (const cColor &c) const {
	return cColor(r / c.r, g / c.g, b / c.b, a / c.a);
}

// cColor::operator * : const cColor (const float) const
inline const cColor cColor::operator * (const float s) const {
	return cColor(r * s, g * s, b * s, a * s);
}

// cColor::operator * : const cColor (const float, const cColor &)
inline const cColor operator * (const float s, const cColor &c) {
	return cColor(s * c.r, s * c.g, s * c.b, s * c.a);
}

// cColor::operator / : const cColor (const float) const
inline const cColor cColor::operator / (const float s) const {
	const float is = 1.0f / s;
	return cColor(r * is, g * is, b * is, a * is);
}

// cColor::operator ==
inline bool cColor::operator == (const cColor &c) const {
	return r == c.r && g == c.g && b == c.b && a == c.a;
}

// cColor::operator !=
inline bool cColor::operator != (const cColor &c) const {
	return r != c.r || g != c.g || b != c.b || a != c.a;
}

// cColor::Equals
inline bool cColor::Equals(const cColor &c, const cColor &l, const float Eps) {
	if(cMath::Abs(c.r - l.r) > Eps) {
		return false;
	}
	if(cMath::Abs(c.g - l.g) > Eps) {
		return false;
	}
	if(cMath::Abs(c.b - l.b) > Eps) {
		return false;
	}
	if(cMath::Abs(c.a - l.a) > Eps) {
		return false;
	}
    return true;
}

// cColor::Saturate
inline void cColor::Saturate() {
	r = cMath::Clamp01(r);
	g = cMath::Clamp01(g);
	b = cMath::Clamp01(b);
	a = cMath::Clamp01(a);
}

// cColor::Rand
inline const cColor cColor::Rand(const cColor &c0, const cColor &c1) {
	cColor c;
	c.r = cMath::Lerp(c0.r, c1.r, cMath::Rand01());
	c.g = cMath::Lerp(c0.g, c1.g, cMath::Rand01());
	c.b = cMath::Lerp(c0.b, c1.b, cMath::Rand01());
	c.a = cMath::Lerp(c0.a, c1.a, cMath::Rand01());
	return c;
}

// cColor::Negative
inline const cColor cColor::Negative(const cColor &c) {
	return cColor(1.0f - c.r, 1.0f - c.g, 1.0f - c.b, c.a);
}

// cColor::Lerp
inline const cColor cColor::Lerp(const cColor &c, const cColor &l, const float s) {
	cColor r;
	r.r = c.r + s * (l.r - c.r);
	r.g = c.g + s * (l.g - c.g);
	r.b = c.b + s * (l.b - c.b);
	r.a = c.a + s * (l.a - c.a);
	return r;
}

// cColor::Lerp05
inline const cColor cColor::Lerp05(const cColor &c, const cColor &l) {
	cColor r;
	r.r = 0.5f * (c.r + l.r);
	r.g = 0.5f * (c.g + l.g);
	r.b = 0.5f * (c.b + l.b);
	r.a = 0.5f * (c.a + l.a);
	return r;
}

// cColor::Saturate
inline const cColor cColor::Saturate(const cColor &c) {
	return cColor(cMath::Clamp01(c.r), cMath::Clamp01(c.g), cMath::Clamp01(c.b), cMath::Clamp01(c.a));
}

// cColor::ToGray
inline float cColor::ToGray(const cColor &c) {
	return c.r * 0.2125f + c.g * 0.7154f + c.b * 0.0721f;
}

// cColor::ToGrayScale
inline const cColor cColor::ToGrayScale(const cColor &c) {
	return cColor(ToGray(c), c.a);
}

// cColor::AdjustSaturation
inline const cColor cColor::AdjustSaturation(const cColor &c, const float s) {
	return cColor::Lerp(ToGrayScale(c), c, s);
}

// cColor::AdjustContrast
inline const cColor cColor::AdjustContrast(const cColor &c, const float s) {
	return cColor::Lerp(cColor(0.5f, c.a), c, s);
}

// cColor::FloatToDword
inline dword cColor::FloatToDword(const float f) {
	return f >= 1.0f ? 0xff : f <= 0.0f ? 0x00 : dword(f * 255.0f + 0.5f);
}

// cColor::GetDimension
inline int cColor::GetDimension() const {
	return 4;
}

// cColor::ToFloatPtr : const float * () const
inline const float * cColor::ToFloatPtr() const {
	return &r;
}

// cColor::ToFloatPtr : float * ()
inline float * cColor::ToFloatPtr() {
	return &r;
}

// cColor::ToDword : dword () const
inline dword cColor::ToDword() const {
	return FloatToDword(r) | (FloatToDword(g) << 8) | (FloatToDword(b) << 16) | (FloatToDword(a) << 24);
}

// cColor::ToDword : dword (const int, const int, const int, const int)
inline dword cColor::ToDword(const int Red, const int Green, const int Blue, const int Alpha) {
	return (dword)((Red & 0xff) | ((Green & 0xff) << 8) | ((Blue & 0xff) << 16) | ((Alpha & 0xff) << 24));
}

// cColor::ToDword : dword (const int, const int)
inline dword cColor::ToDword(const int Gray, const int Alpha) {
	return (dword)((Gray & 0xff) | ((Gray & 0xff) << 8) | ((Gray & 0xff) << 16) | ((Alpha & 0xff) << 24));
}

// cColor::FromDword
inline const cColor cColor::FromDword(const dword d) {
	cColor c;
	const float s = 1.0f / 255.0f;
	c.r = s * float(d & 0xff);
	c.g = s * float((d >> 8) & 0xff);
	c.b = s * float((d >> 16) & 0xff);
	c.a = s * float((d >> 24) & 0xff);
	return c;
}

// cColor::FromRgbe
inline const cColor cColor::FromRgbe(const byte *Rgbe) {
	cColor c(0.0f, 0.0f, 0.0f, 1.0f);
	float S;
	if(Rgbe[3] != 0) {
		S = cMath::Ldexp(1.0f, Rgbe[3] - (int)(128 + 8));
		c.r = (float)Rgbe[0] * S;
		c.g = (float)Rgbe[1] * S;
		c.b = (float)Rgbe[2] * S;
	}
	return c;
}

// cColor::ToRgbe
inline dword cColor::ToRgbe() const {
	float v = cMath::Max(r, g, b), m;
	int exp;
	dword R, G, B, E;

	if(v < 1e-32f) {
		return 0;
	} else {
		m = cMath::Frexp(v, &exp) * 256.0f / v;
		R = (dword)(m * r);
		G = (dword)(m * g);
		B = (dword)(m * b);
		E = (dword)(exp + 128);
		return R | (G << 8) | (B << 16) | (E << 24);
	}
}
