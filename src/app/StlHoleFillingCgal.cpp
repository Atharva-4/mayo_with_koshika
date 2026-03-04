#include "StlHoleFilling.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iterator> // <-- added
#include <unordered_set>

#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/iterator.h>
#include <CGAL/boost/graph/iterator.h>

namespace PMP = CGAL::Polygon_mesh_processing;

static constexpr float VERT_EPS = 1e-6f;

namespace Mayo {

    // vect operators
    bool vect::operator<(const vect& other) const {
        if (std::fabs(x - other.x) > VERT_EPS) return x < other.x;
        if (std::fabs(y - other.y) > VERT_EPS) return y < other.y;
        return (z + VERT_EPS) < other.z;
    }

    bool vect::operator==(const vect& other) const {
        return std::fabs(x - other.x) <= VERT_EPS &&
            std::fabs(y - other.y) <= VERT_EPS &&
            std::fabs(z - other.z) <= VERT_EPS;
    }

    // Check whether an STL is binary reliably: read header, triangle count and compare to file size.
    bool isBinarySTL(const std::string& filepath) {
        std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
        if (!ifs) return false;
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        if (size < 84) return false;

        char header[80];
        ifs.read(header, 80);
        uint32_t triCount = 0;
        ifs.read(reinterpret_cast<char*>(&triCount), sizeof(triCount));

        std::uint64_t expected = 84 + static_cast<std::uint64_t>(triCount) * 50ULL;
        if (expected == static_cast<std::uint64_t>(size)) return true;

        std::string hdr80(header, header + 80);
        if (hdr80.rfind("solid", 0) != 0) return true;

        return false;
    }

    // Read ASCII STL
    void readASCIISTL(const std::string& filepath, std::vector<Triangles>& triangles) {
        triangles.clear();
        std::ifstream ifs(filepath);
        if (!ifs) return;

        std::string token;
        Triangles tri;
        while (ifs >> token) {
            if (token == "facet") {
                ifs >> token; // "normal"
                ifs >> tri.normal.x >> tri.normal.y >> tri.normal.z;
                std::string line;
                std::getline(ifs, line); // remainder of line
                for (int i = 0; i < 3; ++i) {
                    while (ifs >> token) {
                        if (token == "vertex") {
                            if (i == 0) ifs >> tri.v1.x >> tri.v1.y >> tri.v1.z;
                            else if (i == 1) ifs >> tri.v2.x >> tri.v2.y >> tri.v2.z;
                            else if (i == 2) ifs >> tri.v3.x >> tri.v3.y >> tri.v3.z;
                            break;
                        }
                    }
                }
                triangles.push_back(tri);
            }
        }
    }

    // Read binary STL (little-endian floats assumed)
    void readBinarySTL(const std::string& filepath, std::vector<Triangles>& triangles) {
        triangles.clear();
        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) return;

        char header[80];
        ifs.read(header, 80);
        uint32_t triCount = 0;
        ifs.read(reinterpret_cast<char*>(&triCount), sizeof(triCount));

        triangles.reserve(triCount);
        for (uint32_t i = 0; i < triCount; ++i) {
            Triangles t;
            float vals[12];
            ifs.read(reinterpret_cast<char*>(vals), sizeof(vals));
            t.normal.x = vals[0]; t.normal.y = vals[1]; t.normal.z = vals[2];
            t.v1.x = vals[3]; t.v1.y = vals[4]; t.v1.z = vals[5];
            t.v2.x = vals[6]; t.v2.y = vals[7]; t.v2.z = vals[8];
            t.v3.x = vals[9]; t.v3.y = vals[10]; t.v3.z = vals[11];
            uint16_t attr;
            ifs.read(reinterpret_cast<char*>(&attr), sizeof(attr));
            triangles.push_back(t);
        }
    }

    // Count boundary edges by edge frequency in triangle list
    int countBoundaryEdges(const std::vector<Triangles>& triangles) {
        using Edge = std::pair<vect, vect>;
        struct EdgeCmp {
            bool operator()(const Edge& a, const Edge& b) const {
                if (a.first < b.first) return true;
                if (b.first < a.first) return false;
                return a.second < b.second;
            }
        };

        std::map<Edge, int, EdgeCmp> edgeCount;
        auto addEdge = [&](const vect& a, const vect& b) {
            if (b < a) edgeCount[{b, a}]++;
            else edgeCount[{a, b}]++;
            };

        for (const auto& t : triangles) {
            addEdge(t.v1, t.v2);
            addEdge(t.v2, t.v3);
            addEdge(t.v3, t.v1);
        }

        int boundaryEdges = 0;
        for (const auto& kv : edgeCount) {
            if (kv.second == 1) boundaryEdges++;
        }
        return boundaryEdges;
    }

    // convert triangles to CGAL Surface_mesh with vertex deduplication
    SurfaceMesh convertToSurfaceMesh(const std::vector<Triangles>& triangles) {
        SurfaceMesh mesh;
        std::unordered_map<vect, SurfaceMesh::Vertex_index> vmap;
        vmap.reserve(triangles.size() * 3);

        auto get_vid = [&](const vect& v) -> SurfaceMesh::Vertex_index {
            auto it = vmap.find(v);
            if (it != vmap.end()) return it->second;
            Point p(static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
            auto vi = mesh.add_vertex(p);
            vmap.emplace(v, vi);
            return vi;
            };

        for (const auto& t : triangles) {
            SurfaceMesh::Vertex_index a = get_vid(t.v1);
            SurfaceMesh::Vertex_index b = get_vid(t.v2);
            SurfaceMesh::Vertex_index c = get_vid(t.v3);
            mesh.add_face(a, b, c);
        }
        return mesh;
    }

    // Fill holes using CGAL Polygon Mesh Processing

    std::size_t countHolesCGAL(const SurfaceMesh& mesh) {
        std::vector<SurfaceMesh::Halfedge_index> boundaries;
        PMP::extract_boundary_cycles(mesh, std::back_inserter(boundaries));
        return boundaries.size();
    }

    void fillHolesCGAL(SurfaceMesh& mesh) {
        std::vector<SurfaceMesh::Halfedge_index> boundaries;

        PMP::extract_boundary_cycles(mesh, std::back_inserter(boundaries));

        for (SurfaceMesh::Halfedge_index h : boundaries) {
            if (h == SurfaceMesh::null_halfedge())
                continue;

            std::vector<SurfaceMesh::Face_index> patch;

            PMP::triangulate_hole(
                mesh,
                h,
                std::back_inserter(patch),
                CGAL::parameters::use_delaunay_triangulation(true)
            );
        }
    }

    void fillSelectedHolesCGAL(SurfaceMesh& mesh, const std::vector<int>& selectedHoleIds) {
        std::vector<SurfaceMesh::Halfedge_index> boundaries;
        PMP::extract_boundary_cycles(mesh, std::back_inserter(boundaries));

        std::unordered_set<int> selectedSet(selectedHoleIds.begin(), selectedHoleIds.end());
        for (int idx = 0; idx < static_cast<int>(boundaries.size()); ++idx) {
            if (!selectedSet.count(idx))
                continue;

            SurfaceMesh::Halfedge_index h = boundaries[idx];
            if (h == SurfaceMesh::null_halfedge())
                continue;

            std::vector<SurfaceMesh::Face_index> patch;
            PMP::triangulate_hole(
                mesh,
                h,
                std::back_inserter(patch),
                CGAL::parameters::use_delaunay_triangulation(true)
            );
        }
    }


    // Write ASCII STL; compute face normal by cross product
    void writeSTL(const std::string& filename, const SurfaceMesh& mesh) {
        std::ofstream ofs(filename);
        if (!ofs) return;
        ofs << "solid mesh\n";
        for (auto f : mesh.faces()) {
            std::vector<Point> pts;
            for (auto v : CGAL::vertices_around_face(mesh.halfedge(f), mesh)) {
                pts.push_back(mesh.point(v));
            }
            if (pts.size() < 3) continue;
            Kernel::Vector_3 u = pts[1] - pts[0];
            Kernel::Vector_3 v = pts[2] - pts[0];
            Kernel::Vector_3 n = CGAL::cross_product(u, v);
            double nx = n.x(), ny = n.y(), nz = n.z();
            double norm = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (norm > std::numeric_limits<double>::epsilon()) {
                nx /= norm; ny /= norm; nz /= norm;
            }
            else { nx = ny = nz = 0.0; }
            ofs << "  facet normal " << nx << " " << ny << " " << nz << "\n";
            ofs << "    outer loop\n";
            for (const auto& p : pts) {
                ofs << "      vertex " << p.x() << " " << p.y() << " " << p.z() << "\n";
            }
            ofs << "    endloop\n";
            ofs << "  endfacet\n";
        }
        ofs << "endsolid mesh\n";
    }

} // namespace Mayo

// std::hash<Mayo::vect> definition (outside Mayo)
namespace std {
    size_t hash<Mayo::vect>::operator()(const Mayo::vect& v) const {
        uint32_t xi, yi, zi;
        static_assert(sizeof(uint32_t) == sizeof(float), "float must be 32-bit");
        std::memcpy(&xi, &v.x, sizeof(float));
        std::memcpy(&yi, &v.y, sizeof(float));
        std::memcpy(&zi, &v.z, sizeof(float));
        size_t h = xi;
        h ^= (static_cast<size_t>(yi) << 1);
        h = h * 0x9e3779b97f4a7c15ULL + (std::hash<size_t>{}(h) << 6) + (std::hash<size_t>{}(h) >> 2);
        h ^= (static_cast<size_t>(zi) << 1);
        return h;
    }
}