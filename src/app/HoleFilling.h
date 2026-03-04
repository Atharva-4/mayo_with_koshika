#pragma once

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <optional>
#include <fstream>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <mutex>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/quaternion.hpp>

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/IO/STL.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

namespace PMP = CGAL::Polygon_mesh_processing;
typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
using Point = Kernel::Point_3;
using SurfaceMesh = CGAL::Surface_mesh<Point>;
using Halfedge_descriptor = boost::graph_traits<SurfaceMesh>::halfedge_descriptor;

struct GLMesh {
    GLuint vao = 0, vbo = 0, nbo = 0, ebo = 0;
    GLsizei nIdx = 0;
    std::vector<glm::vec3> cpuPos;
};

struct Camera {
    float dist = 3.0f;
    glm::quat rot = glm::quat(1,0,0,0);
    glm::vec3 pan = glm::vec3(0);
    bool dragging = false;
    glm::vec2 last{};
};

/* -------- GLOBAL VARIABLES (extern) -------- */

extern SurfaceMesh mesh;
extern GLMesh gmesh;
extern Camera cam;

extern std::vector<std::vector<Halfedge_descriptor>> boundaries;
extern std::vector<std::vector<SurfaceMesh::Vertex_index>> holeLoops;

extern std::unordered_set<int> selectedHoles;
extern std::unordered_set<int> filledHoles;
extern std::vector<SurfaceMesh::Face_index> newFaces;
extern std::vector<std::vector<glm::vec3>> holeTriangleFans;

extern float scrollAcc;
extern GLuint pointVAO;
extern GLuint pointVBO;
extern GLuint progUI;
extern bool wireframeMode;

extern const char* VERT_MESH;
extern const char* FRAG_MESH;
extern const char* VERT_COLOR;
extern const char* FRAG_COLOR;
extern const char* VERT_UI;
extern const char* FRAG_UI;

/* -------- FUNCTION DECLARATIONS -------- */

GLuint makeProg(const char* vs, const char* fs);
void scrollCB(GLFWwindow*, double, double y);
glm::vec3 screenRay(glm::vec2 m, int w, int h, const glm::mat4& P, const glm::mat4& V);
glm::vec3 trackballProj(glm::vec2 p, int w, int h);
glm::mat4 viewTrackball();
bool rayIntersectTriangle(const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& tOut);
void extractHoleBoundaries();
void buildGLMesh(const SurfaceMesh& m, GLMesh& g);

void drawLoadingText();
std::optional<std::size_t> detectHoleCountFromStl(const std::string& stlFilepath);