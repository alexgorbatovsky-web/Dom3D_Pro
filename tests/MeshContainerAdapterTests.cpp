#include "excomms/cMeshContainerAdapter.h"

int main() {
    comms::cMeshContainer mesh;
    mesh.GetPositions().Add(comms::cVec3(0.0f, 0.0f, 0.0f));
    mesh.GetPositions().Add(comms::cVec3(1.0f, 0.0f, 0.0f));
    mesh.GetPositions().Add(comms::cVec3(0.0f, 1.0f, 0.0f));
    mesh.GetRaw().Add(comms::cVec3i(3, 0, 0));
    mesh.GetRaw().Add(comms::cVec3i(0, -1, -1));
    mesh.GetRaw().Add(comms::cVec3i(1, -1, -1));
    mesh.GetRaw().Add(comms::cVec3i(2, -1, -1));

    mesh.TriSubd(1);

    // One subdivision point per edge and four resulting triangles.
    if (mesh.GetPositions().Count() != 6 || mesh.GetRaw().Count() != 16) {
        return 1;
    }
    if (mesh.GetEdgeFaceCount(0, 3) != 1 || mesh.GetEdgeFaceCount(3, 5) != 2) {
        return 2;
    }

    comms::cMeshContainer result;
    result.SetDefaultObjMtl();
    result.Clear();
    return result.GetPositions().Count() == 0 && result.GetRaw().Count() == 0 ? 0 : 3;
}
