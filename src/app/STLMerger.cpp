#include "STLMerger.h"
#include <fstream>
#include <iostream>
#include <cmath>
namespace Mayo {

    STLMerger::STLMerger() {}

    Vec3 STLMerger::computeNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 u = { b.x - a.x, b.y - a.y, b.z - a.z };
        Vec3 v = { c.x - a.x, c.y - a.y, c.z - a.z };

        Vec3 normal = {
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };

        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > EPS) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
        return normal;
    }

    bool STLMerger::parseFacet(std::ifstream& in, Facet& f) {
        std::string line;
        if (!std::getline(in, line)) return false;
        if (line.find("facet") == std::string::npos) return false;

        std::getline(in, line); // outer loop
        std::getline(in, line); sscanf_s(line.c_str(), "      vertex %f %f %f", &f.v1.x, &f.v1.y, &f.v1.z);
        std::getline(in, line); sscanf_s(line.c_str(), "      vertex %f %f %f", &f.v2.x, &f.v2.y, &f.v2.z);
        std::getline(in, line); sscanf_s(line.c_str(), "      vertex %f %f %f", &f.v3.x, &f.v3.y, &f.v3.z);
        std::getline(in, line); // endloop
        std::getline(in, line); // endfacet

        f.normal = computeNormal(f.v1, f.v2, f.v3);
        return true;
    }

    bool STLMerger::loadSTL(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in) {
            std::cerr << "Cannot open " << filePath << "\n";
            return false;
        }

        std::string line;
        std::getline(in, line); // solid ...
        Facet f;
        while (parseFacet(in, f)) {
            m_allFacets.push_back(f);
        }

        return true;
    }

    void STLMerger::merge() {
        // Merge all facets
        m_mergedFacets = m_allFacets;

        // STEP 1: Compute center of merged mesh
        Vec3 center = { 0, 0, 0 };
        int count = 0;
        for (const auto& f : m_mergedFacets) {
            center.x += f.v1.x + f.v2.x + f.v3.x;
            center.y += f.v1.y + f.v2.y + f.v3.y;
            center.z += f.v1.z + f.v2.z + f.v3.z;
            count += 3;
        }
        center.x /= count;
        center.y /= count;
        center.z /= count;

        // STEP 2: Fix triangle normals to point outward from center
        auto dot = [](const Vec3& a, const Vec3& b) {
            return a.x * b.x + a.y * b.y + a.z * b.z;
            };

        for (auto& f : m_mergedFacets) {
            Vec3 faceCenter = {
                (f.v1.x + f.v2.x + f.v3.x) / 3.0f,
                (f.v1.y + f.v2.y + f.v3.y) / 3.0f,
                (f.v1.z + f.v2.z + f.v3.z) / 3.0f
            };

            Vec3 toCenter = {
                faceCenter.x - center.x,
                faceCenter.y - center.y,
                faceCenter.z - center.z
            };

            Vec3 normal = computeNormal(f.v1, f.v2, f.v3);
            if (dot(normal, toCenter) > 0) {
                std::swap(f.v2, f.v3); // flip triangle
                normal = computeNormal(f.v1, f.v2, f.v3);
            }

            f.normal = normal;
        }
    }

    bool STLMerger::saveMerged(const std::string& outputPath) {
        std::ofstream out(outputPath);
        if (!out.is_open()) {
            return false;
        }

        out << "solid merged\n";
        for (const auto& f : m_mergedFacets) {
            out << "  facet normal " << f.normal.x << " " << f.normal.y << " " << f.normal.z << "\n";
            out << "    outer loop\n";
            out << "      vertex " << f.v1.x << " " << f.v1.y << " " << f.v1.z << "\n";
            out << "      vertex " << f.v2.x << " " << f.v2.y << " " << f.v2.z << "\n";
            out << "      vertex " << f.v3.x << " " << f.v3.y << " " << f.v3.z << "\n";
            out << "    endloop\n";
            out << "  endfacet\n";
        }
        out << "endsolid merged\n";

        return true;
    }
}