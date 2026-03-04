#include "HoleFilling.h"
#include "StlHoleFilling.h"

#include <unordered_map>
#include <cmath>
#include <cstdio>

SurfaceMesh mesh;
GLMesh gmesh;
Camera cam;

std::vector<std::vector<Halfedge_descriptor>> boundaries;
std::vector<std::vector<SurfaceMesh::Vertex_index>> holeLoops;
std::unordered_set<int> selectedHoles;
std::unordered_set<int> filledHoles;
std::vector<SurfaceMesh::Face_index> newFaces;
std::vector<std::vector<glm::vec3>> holeTriangleFans;

float scrollAcc = 0.0f;
GLuint pointVAO = 0;
GLuint pointVBO = 0;
GLuint progUI = 0;
bool wireframeMode = false;

bool rayIntersectTriangle(const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& tOut)
{
    const float epsilon = 1e-6f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);
    if (std::abs(a) < epsilon) return false;

    float f = 1.0f / a;
    glm::vec3 s = orig - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * glm::dot(edge2, q);
    if (t > epsilon)
    {
        tOut = t;
        return true;
    }
    return false;
}

const char* VERT_MESH = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aN;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uView;
out vec3 N;
out vec3 FragPos;
void main(){
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    N = mat3(transpose(inverse(uModel))) * aN;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

const char* FRAG_MESH = R"glsl(
#version 330 core
in vec3 N;
in vec3 FragPos;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
out vec4 FragColor;
void main(){
    vec3 norm = normalize(N);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.25);

    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 diffuse = diff * vec3(0.7);
    vec3 specular = spec * vec3(0.3);

    FragColor = vec4(diffuse + specular, 1.0);
}
)glsl";

const char* VERT_COLOR = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos; uniform mat4 uMVP;
void main(){ gl_Position=uMVP*vec4(aPos,1.0);} )glsl";

const char* FRAG_COLOR = R"glsl(
#version 330 core
uniform vec3 uColor; out vec4 FragColor;
void main(){ FragColor=vec4(uColor,1.0);} )glsl";

const char* VERT_UI = R"glsl(
#version 330 core
layout(location=0) in vec2 aPos; void main(){ gl_Position=vec4(aPos,0,1);} )glsl";

const char* FRAG_UI = R"glsl(
#version 330 core
uniform vec3 uColor; out vec4 FragColor;
void main(){ FragColor=vec4(uColor,1);} )glsl";

static GLuint compile(GLenum t, const char* s)
{
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(sh, 512, nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
    }
    return sh;
}

GLuint makeProg(const char* vs, const char* fs)
{
    GLuint p = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

void buildGLMesh(const SurfaceMesh& m, GLMesh& g)
{
    std::vector<glm::vec3> pos, nor;
    std::vector<unsigned> idx;
    pos.reserve(m.number_of_vertices());
    nor.reserve(m.number_of_vertices());

    std::unordered_map<SurfaceMesh::Vertex_index, unsigned> id;
    for (auto v : m.vertices())
    {
        Point p = m.point(v);
        id[v] = static_cast<unsigned>(pos.size());
        pos.emplace_back(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
        nor.emplace_back(0.0f);
    }

    for (auto f : m.faces())
    {
        auto h = m.halfedge(f);
        glm::vec3 v0 = pos[id[m.target(h)]];
        h = m.next(h);
        glm::vec3 v1 = pos[id[m.target(h)]];
        h = m.next(h);
        glm::vec3 v2 = pos[id[m.target(h)]];

        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        h = m.halfedge(f);
        for (int i = 0; i < 3; ++i)
        {
            nor[id[m.target(h)]] += n;
            h = m.next(h);
        }
    }

    for (auto& n : nor)
    {
        n = glm::normalize(n);
    }

    for (auto f : m.faces())
    {
        auto h = m.halfedge(f);
        for (int i = 0; i < 3; ++i)
        {
            idx.push_back(id[m.target(h)]);
            h = m.next(h);
        }
    }

    if (!g.vao) glGenVertexArrays(1, &g.vao);
    if (!g.vbo) glGenBuffers(1, &g.vbo);
    if (!g.nbo) glGenBuffers(1, &g.nbo);
    if (!g.ebo) glGenBuffers(1, &g.ebo);

    glBindVertexArray(g.vao);

    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(pos.size() * sizeof(glm::vec3)), pos.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, g.nbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(nor.size() * sizeof(glm::vec3)), nor.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned)), idx.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    g.nIdx = static_cast<GLsizei>(idx.size());
    g.cpuPos = std::move(pos);
}

void scrollCB(GLFWwindow*, double, double y)
{
    scrollAcc += static_cast<float>(y);
}

glm::vec3 screenRay(glm::vec2 m, int w, int h, const glm::mat4& P, const glm::mat4& V)
{
    float x = 2.0f * m.x / static_cast<float>(w) - 1.0f;
    float y = 1.0f - 2.0f * m.y / static_cast<float>(h);
    glm::vec4 clip{ x, y, -1.0f, 1.0f };
    glm::vec4 eye = glm::inverse(P) * clip;
    eye.z = -1.0f;
    eye.w = 0.0f;
    return glm::normalize(glm::vec3(glm::inverse(V) * eye));
}

glm::vec3 trackballProj(glm::vec2 p, int w, int h)
{
    p.x = (2.0f * p.x - static_cast<float>(w)) / static_cast<float>(w);
    p.y = -(2.0f * p.y - static_cast<float>(h)) / static_cast<float>(h);

    float d = p.x * p.x + p.y * p.y;
    float z = d < 1.0f ? std::sqrt(1.0f - d) : 0.0f;
    return glm::normalize(glm::vec3(p, z));
}

glm::mat4 viewTrackball()
{
    glm::mat4 V = glm::translate(glm::mat4(1.0f), -cam.pan);
    V *= glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -cam.dist));
    V *= glm::mat4_cast(cam.rot);
    return V;
}

void extractHoleBoundaries()
{
    boundaries.clear();

    std::unordered_set<SurfaceMesh::Halfedge_index> visited;
    for (auto h : mesh.halfedges())
    {
        if (!mesh.is_border(h) || visited.count(h))
        {
            continue;
        }

        std::vector<SurfaceMesh::Halfedge_index> loop;
        SurfaceMesh::Halfedge_index start = h;
        do
        {
            loop.push_back(h);
            visited.insert(h);
            h = mesh.next(h);
        } while (h != start);

        boundaries.push_back(loop);
    }

    holeLoops.clear();
    for (const auto& loop : boundaries)
    {
        std::vector<SurfaceMesh::Vertex_index> vloop;
        for (auto h : loop)
        {
            vloop.push_back(mesh.target(h));
        }
        holeLoops.push_back(std::move(vloop));
    }

    holeTriangleFans.clear();
    for (const auto& loop : holeLoops)
    {
        std::vector<glm::vec3> triFan;
        if (loop.size() < 3)
        {
            holeTriangleFans.push_back(std::move(triFan));
            continue;
        }

        glm::vec3 center(0.0f);
        for (auto v : loop)
        {
            Point p = mesh.point(v);
            center += glm::vec3(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
        }
        center /= static_cast<float>(loop.size());

        for (size_t i = 0; i < loop.size(); ++i)
        {
            Point p0 = mesh.point(loop[i]);
            Point p1 = mesh.point(loop[(i + 1) % loop.size()]);
            triFan.push_back(center);
            triFan.emplace_back(static_cast<float>(p0.x()), static_cast<float>(p0.y()), static_cast<float>(p0.z()));
            triFan.emplace_back(static_cast<float>(p1.x()), static_cast<float>(p1.y()), static_cast<float>(p1.z()));
        }

        holeTriangleFans.push_back(std::move(triFan));
    }

    std::cout << "Holes detected " << boundaries.size() << " boundary loops." << std::endl;
}

//std::optional<std::size_t> detectHoleCountFromStl(const std::string& stlFilepath)
//{
//    try
//    {
//        std::vector<Mayo::Triangles> triangles;
//        if (Mayo::isBinarySTL(stlFilepath))
//            Mayo::readBinarySTL(stlFilepath, triangles);
//        else
//            Mayo::readASCIISTL(stlFilepath, triangles);
//
//        if (triangles.empty())
//            return std::nullopt;
//
//        Mayo::SurfaceMesh stlMesh = Mayo::convertToSurfaceMesh(triangles);
//        return Mayo::countHolesCGAL(stlMesh);
//    }
//    catch (...)
//    {
//        return std::nullopt;
//    }
//}

void drawLoadingText()
{
    if (progUI == 0)
    {
        return;
    }

    const float overlay[] = {
        -0.95f,  0.92f,
        -0.20f,  0.92f,
        -0.20f,  0.82f,
        -0.95f,  0.92f,
        -0.20f,  0.82f,
        -0.95f,  0.82f,
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(overlay), overlay, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glUseProgram(progUI);
    glUniform3f(glGetUniformLocation(progUI, "uColor"), 1.0f, 1.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
