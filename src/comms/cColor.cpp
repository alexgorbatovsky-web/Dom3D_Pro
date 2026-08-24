#include "comms.h"

namespace comms {

const cColor cColor::Zero				= cColor(0,		0,		0,		0);
const cColor cColor::Transparent		= cColor(255,	255,	255,	0);

const cColor cColor::AliceBlue			= cColor(240,	248,	255,	255);
const cColor cColor::AntiqueWhite		= cColor(250,	235,	215,	255);
const cColor cColor::Aqua				= cColor(0,		255,	255,	255);
const cColor cColor::Aquamarine			= cColor(127,	255,	212,	255);
const cColor cColor::Azure				= cColor(240,	255,	255,	255);
const cColor cColor::Beige				= cColor(245,	245,	220,	255);
const cColor cColor::Bisque				= cColor(255,	228,	196,	255);
const cColor cColor::Black				= cColor(0,		0,		0,		255);
const cColor cColor::BlanchedAlmond		= cColor(255,	235,	205,	255);
const cColor cColor::Blue				= cColor(0,		0,		255,	255);
const cColor cColor::BlueViolet			= cColor(138,	43,		226,	255);
const cColor cColor::Brown				= cColor(165,	42,		42,		255);
const cColor cColor::BurlyWood			= cColor(222,	184,	135,	255);
const cColor cColor::CadetBlue			= cColor(95,	158,	160,	255);
const cColor cColor::Chartreuse			= cColor(127,	255,	0,		255);
const cColor cColor::Chocolate			= cColor(210,	105,	30,		255);
const cColor cColor::Coral				= cColor(255,	127,	80,		255);
const cColor cColor::CornflowerBlue		= cColor(100,	149,	237,	255);
const cColor cColor::Cornsilk			= cColor(255,	248,	220,	255);
const cColor cColor::Crimson			= cColor(220,	20,		60,		255);
const cColor cColor::Cyan				= cColor(0,		255,	255,	255);
const cColor cColor::DarkBlue			= cColor(0,		0,		139,	255);
const cColor cColor::DarkCyan			= cColor(0,		139,	139,	255);
const cColor cColor::DarkGoldenrod		= cColor(184,	134,	11,		255);
const cColor cColor::DarkGray			= cColor(169,	169,	169,	255);
const cColor cColor::DarkGreen			= cColor(0,		100,	0,		255);
const cColor cColor::DarkKhaki			= cColor(189,	183,	107,	255);
const cColor cColor::DarkMagenta		= cColor(139,	0,		139,	255);
const cColor cColor::DarkOliveGreen		= cColor(85,	107,	47,		255);
const cColor cColor::DarkOrange			= cColor(255,	140,	0,		255);
const cColor cColor::DarkOrchid			= cColor(153,	50,		204,	255);
const cColor cColor::DarkRed			= cColor(139,	0,		0,		255);
const cColor cColor::DarkSalmon			= cColor(233,	150,	122,	255);
const cColor cColor::DarkSeaGreen		= cColor(143,	188,	139,	255);
const cColor cColor::DarkSlateBlue		= cColor(72,	61,		139,	255);
const cColor cColor::DarkSlateGray		= cColor(47,	79,		79,		255);
const cColor cColor::DarkTurquoise		= cColor(0,		206,	209,	255);
const cColor cColor::DarkViolet			= cColor(148,	0,		211,	255);
const cColor cColor::DeepPink			= cColor(255,	20,		147,	255);
const cColor cColor::DeepSkyBlue		= cColor(0,		191,	255,	255);
const cColor cColor::DimGray			= cColor(105,	105,	105,	255);
const cColor cColor::DodgerBlue			= cColor(30,	144,	255,	255);
const cColor cColor::Firebrick			= cColor(178,	34,		34,		255);
const cColor cColor::FloralWhite		= cColor(255,	250,	240,	255);
const cColor cColor::ForestGreen		= cColor(34,	139,	34,		255);
const cColor cColor::Fuchsia			= cColor(255,	0,		255,	255);
const cColor cColor::Gainsboro			= cColor(220,	220,	220,	255);
const cColor cColor::GhostWhite			= cColor(248,	248,	255,	255);
const cColor cColor::Gold				= cColor(255,	215,	0,		255);
const cColor cColor::Goldenrod			= cColor(218,	165,	32,		255);
const cColor cColor::Gray				= cColor(128,	128,	128,	255);
const cColor cColor::Green				= cColor(0,		255,	0,		255);
const cColor cColor::MidGreen			= cColor(0,		128,	0,		255);
const cColor cColor::GreenYellow		= cColor(173,	255,	47,		255);
const cColor cColor::Honeydew			= cColor(240,	255,	240,	255);
const cColor cColor::HotPink			= cColor(255,	105,	180,	255);
const cColor cColor::IndianRed			= cColor(205,	92,		92,		255);
const cColor cColor::Indigo				= cColor(75,	0,		130,	255);
const cColor cColor::Ivory				= cColor(255,	255,	240,	255);
const cColor cColor::Khaki				= cColor(240,	230,	140,	255);
const cColor cColor::Lavender			= cColor(230,	230,	250,	255);
const cColor cColor::LavenderBlush		= cColor(255,	240,	245,	255);
const cColor cColor::LawnGreen			= cColor(124,	252,	0,		255);
const cColor cColor::LemonChiffon		= cColor(255,	250,	205,	255);
const cColor cColor::LightBlue			= cColor(173,	216,	230,	255);
const cColor cColor::LightCoral			= cColor(240,	128,	128,	255);
const cColor cColor::LightCyan			= cColor(224,	255,	255,	255);
const cColor cColor::LightGoldenrodYellow = cColor(250,	250,	210,	255);
const cColor cColor::LightGray			= cColor(211,	211,	211,	255);
const cColor cColor::LightGreen			= cColor(144,	238,	144,	255);
const cColor cColor::LightPink			= cColor(255,	182,	193,	255);
const cColor cColor::LightSalmon		= cColor(255,	160,	122,	255);
const cColor cColor::LightSeaGreen		= cColor(32,	178,	170,	255);
const cColor cColor::LightSkyBlue		= cColor(135,	206,	250,	255);
const cColor cColor::LightSlateGray		= cColor(119,	136,	153,	255);
const cColor cColor::LightSteelBlue		= cColor(176,	196,	222,	255);
const cColor cColor::LightYellow		= cColor(255,	255,	224,	255);
const cColor cColor::Lime				= cColor(0,		255,	0,		255);
const cColor cColor::LimeGreen			= cColor(50,	205,	50,		255);
const cColor cColor::Linen				= cColor(250,	240,	230,	255);
const cColor cColor::Magenta			= cColor(255,	0,		255,	255);
const cColor cColor::Maroon				= cColor(128,	0,		0,		255);
const cColor cColor::MediumAquamarine	= cColor(102,	205,	170,	255);
const cColor cColor::MediumBlue			= cColor(0,		0,		205,	255);
const cColor cColor::MediumOrchid		= cColor(186,	85,		211,	255);
const cColor cColor::MediumPurple		= cColor(147,	112,	219,	255);
const cColor cColor::MediumSeaGreen		= cColor(60,	179,	113,	255);
const cColor cColor::MediumSlateBlue	= cColor(123,	104,	238,	255);
const cColor cColor::MediumSpringGreen	= cColor(0,		250,	154,	255);
const cColor cColor::MediumTurquoise	= cColor(72,	209,	204,	255);
const cColor cColor::MediumVioletRed	= cColor(199,	21,		133,	255);
const cColor cColor::MidnightBlue		= cColor(25,	25,		112,	255);
const cColor cColor::MintCream			= cColor(245,	255,	250,	255);
const cColor cColor::MistyRose			= cColor(255,	228,	225,	255);
const cColor cColor::Moccasin			= cColor(255,	228,	181,	255);
const cColor cColor::NavajoWhite		= cColor(255,	222,	173,	255);
const cColor cColor::Navy				= cColor(0,		0,		128,	255);
const cColor cColor::OldLace			= cColor(253,	245,	230,	255);
const cColor cColor::Olive				= cColor(128,	128,	0,		255);
const cColor cColor::OliveDrab			= cColor(107,	142,	35,		255);
const cColor cColor::Orange				= cColor(255,	165,	0,		255);
const cColor cColor::OrangeRed			= cColor(255,	69,		0,		255);
const cColor cColor::Orchid				= cColor(218,	112,	214,	255);
const cColor cColor::PaleGoldenrod		= cColor(238,	232,	170,	255);
const cColor cColor::PaleGreen			= cColor(152,	251,	152,	255);
const cColor cColor::PaleTurquoise		= cColor(175,	238,	238,	255);
const cColor cColor::PaleVioletRed		= cColor(219,	112,	147,	255);
const cColor cColor::PapayaWhip			= cColor(255,	239,	213,	255);
const cColor cColor::PeachPuff			= cColor(255,	218,	185,	255);
const cColor cColor::Peru				= cColor(205,	133,	63,		255);
const cColor cColor::Pink				= cColor(255,	192,	203,	255);
const cColor cColor::Plum				= cColor(221,	160,	221,	255);
const cColor cColor::PowderBlue			= cColor(176,	224,	230,	255);
const cColor cColor::Purple				= cColor(128,	0,		128,	255);
const cColor cColor::Red				= cColor(255,	0,		0,		255);
const cColor cColor::RosyBrown			= cColor(188,	143,	143,	255);
const cColor cColor::RoyalBlue			= cColor(65,	105,	225,	255);
const cColor cColor::SaddleBrown		= cColor(139,	69,		19,		255);
const cColor cColor::Salmon				= cColor(250,	128,	114,	255);
const cColor cColor::SandyBrown			= cColor(244,	164,	96,		255);
const cColor cColor::SeaGreen			= cColor(46,	139,	87,		255);
const cColor cColor::SeaShell			= cColor(255,	245,	238,	255);
const cColor cColor::Sienna				= cColor(160,	82,		45,		255);
const cColor cColor::Silver				= cColor(192,	192,	192,	255);
const cColor cColor::SkyBlue			= cColor(135,	206,	235,	255);
const cColor cColor::SlateBlue			= cColor(106,	90,		205,	255);
const cColor cColor::SlateGray			= cColor(112,	128,	144,	255);
const cColor cColor::Snow				= cColor(255,	250,	250,	255);
const cColor cColor::SpringGreen		= cColor(0,		255,	127,	255);
const cColor cColor::SteelBlue			= cColor(70,	130,	180,	255);
const cColor cColor::Tan				= cColor(210,	180,	140,	255);
const cColor cColor::Teal				= cColor(0,		128,	128,	255);
const cColor cColor::Thistle			= cColor(216,	191,	216,	255);
const cColor cColor::Tomato				= cColor(255,	99,		71,		255);
const cColor cColor::Turquoise			= cColor(64,	224,	208,	255);
const cColor cColor::Violet				= cColor(238,	130,	238,	255);
const cColor cColor::Wheat				= cColor(245,	222,	179,	255);
const cColor cColor::White				= cColor(255,	255,	255,	255);
const cColor cColor::WhiteSmoke			= cColor(245,	245,	245,	255);
const cColor cColor::Yellow				= cColor(255,	255,	0,		255);
const cColor cColor::YellowGreen		= cColor(154,	205,	50,		255);

//-----------------------------------------------------------------------------
// cColor::ToWebColor
//-----------------------------------------------------------------------------
const cStr cColor::ToWebColor() const {
	cStr S("#");
	int i, b;
	dword d;

	d = ToDword();
	
	for(i = 0; i < 3; i++) {
		b = (d >> 8 * i) & 0xff;
		S << cStr::Format("%x%x", b >> 4, b & 0xf);
	}
	return S;
} // cColor::ToWebColor

} // comms
