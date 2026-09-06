#pragma once
class CSolid;
namespace quadro {
enum class BodyMeshAttempt { NotApplicable, Built, Failed };
BodyMeshAttempt BuildStructuredCadBody(CSolid& solid,float deflection);
}
