#ifndef MAYO_STL_CUTTER_H
#define MAYO_STL_CUTTER_H

#include <string>
#include <vector>

namespace Mayo {

    // small epsilon (C++17 inline constexpr avoids multiple-definition issues)
    inline constexpr float EPS = 1e-6f;

    // 3D vector structure with basic operations
    struct Vec3 {
        float x{ 0.f }, y{ 0.f }, z{ 0.f };

        Vec3 operator-(const Vec3& b) const { return { x - b.x, y - b.y, z - b.z }; }
        Vec3 operator+(const Vec3& b) const { return { x + b.x, y + b.y, z + b.z }; }
        Vec3 operator*(float f) const { return { x * f, y * f, z * f }; }
    };

    // Facet (triangle) structure with normal
    struct Facet {
        Vec3 normal;
        Vec3 v1, v2, v3;
    };

    class STLCutter {
    public:
        STLCutter();

        // Load STL file (ASCII-stl reader)
        std::vector<Facet> loadSTL(const std::string& filename);

        // Save STL file
        void saveSTL(const std::string& filename, const std::vector<Facet>& facets);

        // Cut the mesh along a plane defined by axis and value
        void cutMesh(const std::vector<Facet>& facets, char axis, float cutValue,
            std::vector<Facet>& above, std::vector<Facet>& below);

        // Get bounding box for a specific axis
        void getBounds(const std::vector<Facet>& facets, char axis, float& minVal, float& maxVal);

    private:
        // Evaluate plane equation: Ax + By + Cz + D
        float planeValue(const Vec3& v, float A, float B, float C, float D);

        // Interpolate intersection point between two vertices
        Vec3 interpolate(const Vec3& p1, const Vec3& p2, float A, float B, float C, float D);

        // Split a single facet by the plane
        void splitFacet(const Facet& f, float A, float B, float C, float D,
            std::vector<Facet>& above, std::vector<Facet>& below);
    };

} // namespace Mayo

#endif // MAYO_STL_CUTTER_H