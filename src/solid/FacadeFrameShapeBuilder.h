#pragma once

class TopoDS_Shape;
class TopoDS_Wire;

TopoDS_Shape BuildFastFacadeFrameShapeFromSection(
    const TopoDS_Wire& section,
    double x,
    double y,
    double z,
    double width,
    double height);

TopoDS_Shape BuildFastFacadeFrameShape(double x,
                                       double y,
                                       double z,
                                       double width,
                                       double thickness,
                                       double height,
                                       double frame_width);
