#pragma once

#include <TopoDS_Shape.hxx>

#include <string>

class CSmartLine;

bool BuildSolidBetweenSketches(const CSmartLine& first,
                               const CSmartLine& second,
                               TopoDS_Shape& result,
                               std::string* error = nullptr);
