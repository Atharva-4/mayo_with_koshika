#pragma once
#ifndef STL_HOLE_FILLING_H
#define STL_HOLE_FILLING_H

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <utility>
#include <cmath>

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>

namespace Mayo {
    // ---- CGAL typedefs ----
    using Kernel = CGAL::Simple_cartesian<double>;
    using Point = Kernel::Point_3;
    using SurfaceMesh = CGAL::Surface_mesh<Point>;

    // ---- Basic vector structure for STL vertices ----
    struct vect {
        float x, y, z;

        // Needed for map (edge sorting)
        bool operator<(const vect& other) const;

        // Needed for unordered_map (vertex deduplication)
        bool operator==(const vect& other) const;
    };

    // ---- Triangle structure (STL triangle) ----
    struct Triangles {
        vect normal;
        vect v1, v2, v3;
    };

    // ---- STL utilities ----
    bool isBinarySTL(const std::string& filepath);
    void readASCIISTL(const std::string& filepath, std::vector<Triangles>& triangles);
    void readBinarySTL(const std::string& filepath, std::vector<Triangles>& triangles);

    // ---- Hole detection ----
    int countBoundaryEdges(const std::vector<Triangles>& triangles);

    // ---- Conversion ----
    SurfaceMesh convertToSurfaceMesh(const std::vector<Triangles>& triangles);

    // ---- Hole filling using CGAL ----
    std::size_t countHolesCGAL(const SurfaceMesh& mesh);
    void fillHolesCGAL(SurfaceMesh& mesh);
    void fillSelectedHolesCGAL(SurfaceMesh& mesh, const std::vector<int>& selectedHoleIds);
    // ---- Output ----
    void writeSTL(const std::string& filename, const SurfaceMesh& mesh);

} // namespace Mayo

// std::hash specialization for Mayo::vect — declaration here, definition in .cpp
namespace std {
    template<>
    struct hash<Mayo::vect> {
        size_t operator()(const Mayo::vect& v) const;
    };
}

#endif // STL_HOLE_FILLING_H
