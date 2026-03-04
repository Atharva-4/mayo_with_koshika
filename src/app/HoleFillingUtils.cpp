#include "HoleFillingUtils.h"
#include "StlHoleFilling.h"

std::optional<std::size_t> detectHoleCountFromStl(const std::string& stlFilepath)
{
    try
    {
        std::vector<Mayo::Triangles> triangles;
        if (Mayo::isBinarySTL(stlFilepath))
            Mayo::readBinarySTL(stlFilepath, triangles);
        else
            Mayo::readASCIISTL(stlFilepath, triangles);

        if (triangles.empty())
            return std::nullopt;

        Mayo::SurfaceMesh stlMesh = Mayo::convertToSurfaceMesh(triangles);
        return Mayo::countHolesCGAL(stlMesh);
    }
    catch (...)
    {
        return std::nullopt;
    }
}