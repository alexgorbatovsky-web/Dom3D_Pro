#pragma once
class CSolid;
namespace quadro {
enum class BodyMeshAttempt { NotApplicable, Built, Failed };
BodyMeshAttempt BuildTubeCadBody(CSolid& solid,float deflection);
BodyMeshAttempt BuildStructuredCadBody(CSolid& solid,float deflection);
void BuildPeriodicBandComponents(CSolid& solid);
void BuildBoltCircleComponents(CSolid& solid);
void BuildSteppedCylinderFaces(CSolid& solid);
}
