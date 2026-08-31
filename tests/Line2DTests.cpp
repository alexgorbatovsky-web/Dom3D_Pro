#include "Line2D.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void RequireVar2A(const Face2D& face, const std::vector<cVec2>& cut)
{
    CellCutInfo info;
    Require(AnalyzeFaceCut(face, cut, info),
            "AnalyzeFaceCut rejected a Var-2A contour.");
    ClassifyFaceCut(face, cut, info);

    Require(info.VariantCut == 2,
            "Var-2A classification depends on the contour start or direction.");
    Require(info.cutCase == FACECUT_SPLIT_2,
            "Var-2A was not classified as a two-piece split.");
    Require(info.touchedFaceVertices.size() == 2,
            "Var-2A did not retain exactly the two splitting vertices.");

    std::vector<int> splitVertices = info.touchedFaceVertices;
    std::sort(splitVertices.begin(), splitVertices.end());
    Require(splitVertices[0] == 0 && splitVertices[1] == 2,
            "Var-2A selected the wrong diagonal vertices.");
}

void TestClosedVar2AIsIndependentOfContourStart()
{
    const Face2D face{{
        {0.0, 0.0},
        {10.0, 0.0},
        {10.0, 10.0},
        {0.0, 10.0},
    }};
    const std::vector<cVec2> uniqueContour{
        face.verts[0], face.verts[1], face.verts[2]};

    for (bool reverse : {false, true}) {
        for (size_t start = 0; start < uniqueContour.size(); ++start) {
            std::vector<cVec2> cut;
            for (size_t offset = 0; offset < uniqueContour.size(); ++offset) {
                const size_t step = reverse
                    ? uniqueContour.size() - 1 - offset
                    : offset;
                cut.push_back(uniqueContour[(start + step) % uniqueContour.size()]);
            }
            cut.push_back(cut.front());
            RequireVar2A(face, cut);
        }
    }
}

void TestOpenThreeVertexContactDoesNotInventClosingSegment()
{
    const Face2D face{{
        {0.0, 0.0},
        {10.0, 0.0},
        {10.0, 10.0},
        {0.0, 10.0},
    }};
    const std::vector<cVec2> cut{
        face.verts[0], face.verts[1], face.verts[2]};
    CellCutInfo info;
    Require(AnalyzeFaceCut(face, cut, info),
            "AnalyzeFaceCut rejected an open three-vertex contour.");
    ClassifyFaceCut(face, cut, info);
    Require(info.VariantCut == 0,
            "An open contour incorrectly acquired a synthetic closing split.");
}

void TestVar8UsesTheVertexOnTheCrossingSegment()
{
    // Exact cell from Box_Min_Box_And_Miin_Box.dom3d after the preliminary
    // trim-boundary vertex moves.  Vertices 2 and 3 both touch the contour,
    // but only vertex 3 belongs to the segment that crosses edge 1.
    const Face2D face{{
        {-17.0357723, 35.8725662},
        {-31.3713493, 35.7312508},
        {-31.7736816, 15.7494659},
        {-18.2064819, 27.9799805},
    }};
    const std::vector<cVec2> uniqueContour{
        {22.4951172, 27.9799805},
        {-18.2064819, 27.9799805},
        {-31.7736816, 27.9799805},
        {-31.7736816, 15.7494659},
        {-31.7736816, -20.9414063},
        {22.4951172, -20.9414063},
    };

    for (bool reverse : {false, true}) {
        for (size_t start = 0; start < uniqueContour.size(); ++start) {
            std::vector<cVec2> cut;
            for (size_t offset = 0; offset < uniqueContour.size(); ++offset) {
                const size_t step = reverse
                    ? uniqueContour.size() - 1 - offset
                    : offset;
                cut.push_back(
                    uniqueContour[(start + step) % uniqueContour.size()]);
            }
            cut.push_back(cut.front());

            CellCutInfo info;
            Require(AnalyzeFaceCut(face, cut, info),
                    "AnalyzeFaceCut rejected the saved Var-8 cell.");
            ClassifyFaceCut(face, cut, info);
            Require(info.VariantCut == 8,
                    "The saved Var-8 cell was not recognized.");
            Require(info.vertexToMove == 2,
                    "Var-8 selected the touched vertex from the wrong cut segment.");
            Require(info.hasMoveTarget
                        && info.moveTarget.y > 27.97
                        && info.moveTarget.y < 27.99,
                    "Var-8 calculated the wrong edge intersection.");
        }
    }
}

} // namespace

int main()
{
    TestClosedVar2AIsIndependentOfContourStart();
    TestOpenThreeVertexContactDoesNotInventClosingSegment();
    TestVar8UsesTheVertexOnTheCrossingSegment();
    return EXIT_SUCCESS;
}
