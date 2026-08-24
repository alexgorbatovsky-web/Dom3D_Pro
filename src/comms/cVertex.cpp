#include "comms.h"

namespace comms {

//-----------------------------------------------------------------------------
// cVertexType::SizeOf
//-----------------------------------------------------------------------------
int cVertexType::SizeOf(const cVertexType::Enum Type) {
	switch(Type) {
		case Float:
			return 4;
		case Byte:
			return 1;
	}
	return 0;
} // cVertexType::SizeOf

//-----------------------------------------------------------------------------
// cVertex::Attrib::IsValid
//-----------------------------------------------------------------------------
bool cVertex::Attrib::IsValid() const {
	switch(Usage) {
		case cVertexUsage::Custom:
			return (Type == cVertexType::Float && Dim >= 1 && Dim <= 4) || (Type == cVertexType::Byte && Dim == 4);
		case cVertexUsage::Position:
			return Type == cVertexType::Float && (Dim == 3 || Dim == 4);
		case cVertexUsage::Normal:
			return Type == cVertexType::Float && Dim == 3;
		case cVertexUsage::TexCoord:
			return Type == cVertexType::Float && Dim >= 2 && Dim <= 4;
		case cVertexUsage::Tangent:
			return Type == cVertexType::Float && Dim == 3;
		case cVertexUsage::BiTangent:
			return Type == cVertexType::Float && Dim == 3;
		case cVertexUsage::Indices:
			return Type == cVertexType::Float && Dim == 4;
		case cVertexUsage::Weights:
			return Type == cVertexType::Float && Dim == 4;
		default:
			return false;
	}
} // cVertex::Attrib::IsValid

//-----------------------------------------------------------------------------------------------------------------------
// cVertex::Format::Add
//-----------------------------------------------------------------------------------------------------------------------
int cVertex::Format::Add(const char *Name, const cVertexUsage::Enum Usage, const cVertexType::Enum Type, const int Dim) {
	Attrib a;
	a.Name = Name;
	a.Usage = Usage;
	a.Type = Type;
	a.Dim = Dim;
	
	cAssert(a.IsValid());
	
	// Nevertheless, we should add invalid attribute to prevent registering of vertex format in render
	
	return cList<Attrib>::Add(a);
} // cVertex::Format::Add

//-------------------------------------------------------------------------------------------
// cVertex::Format::HasUsage
//-------------------------------------------------------------------------------------------
bool cVertex::Format::HasUsage(const cVertexUsage::Enum Usage, int *Offset, int *Dim) const {
	int s = 0;
	for(int i = 0; i < Count(); i++) {
		const Attrib &a = GetAt(i);
		
		if(Usage == a.Usage) {
			if(Offset != nullptr) {
				*Offset = s;
			}
			if(Dim != nullptr) {
				*Dim = a.Dim;
			}
			return true;
		}
		s += a.Dim * cVertexType::SizeOf(a.Type);
	}
	return false;
} // cVertex::Format::HasUsage

int cVertex::PositionColored::FormatID = -1;
int cVertex::PositionColoredTextured::FormatID = -1;
int cVertex::PositionColoredTextured2::FormatID = -1;
int cVertex::Particle::FormatID = -1;
int cVertex::PositionNormal::FormatID = -1;
int cVertex::PositionNormalColored::FormatID = -1;
int cVertex::PositionNormalTextured::FormatID = -1;
int cVertex::PositionOnly::FormatID = -1;
int cVertex::PositionRhw::FormatID = -1;
int cVertex::PositionTextured::FormatID = -1;
int cVertex::Bump::FormatID = -1;
int cVertex::Anim::FormatID = -1;
int cVertex::ShadowVolume::FormatID = -1;
int cVertex::PaddingInfo::FormatID = -1;

//-----------------------------------------------------------------------------
// cVertex::Register
//-----------------------------------------------------------------------------
void cVertex::Register() {
	PositionColored::FormatID = cRender::AddVertexFormat(PositionColored::GetFormat());
	PositionColoredTextured::FormatID = cRender::AddVertexFormat(PositionColoredTextured::GetFormat());
	PositionColoredTextured2::FormatID = cRender::AddVertexFormat(PositionColoredTextured2::GetFormat());
	Particle::FormatID = cRender::AddVertexFormat(Particle::GetFormat());
	PositionNormal::FormatID = cRender::AddVertexFormat(PositionNormal::GetFormat());
	PositionNormalColored::FormatID = cRender::AddVertexFormat(PositionNormalColored::GetFormat());
	PositionNormalTextured::FormatID = cRender::AddVertexFormat(PositionNormalTextured::GetFormat());
	PositionOnly::FormatID = cRender::AddVertexFormat(PositionOnly::GetFormat());
	PositionRhw::FormatID = cRender::AddVertexFormat(PositionRhw::GetFormat());
	PositionTextured::FormatID = cRender::AddVertexFormat(PositionTextured::GetFormat());
	Bump::FormatID = cRender::AddVertexFormat(Bump::GetFormat());
	Anim::FormatID = cRender::AddVertexFormat(Anim::GetFormat());
	ShadowVolume::FormatID = cRender::AddVertexFormat(ShadowVolume::GetFormat());
	PaddingInfo::FormatID = cRender::AddVertexFormat(PaddingInfo::GetFormat());

} // cVertex::Register

} // comms
