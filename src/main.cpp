// Game 1 -- Thick Cube Demo
//
// Press ESC to exit back to the arcade menu.
// The menu launches this as a child process and waits for it to terminate.

#include "raylib.h"
#include "raymath.h"
#include "resource_dir.h"

#include "earcut.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

struct Frame { Vector3 origin, U, V, N; };

static Vector3 To3D(const Frame& f, Vector2 uv)
{
    return Vector3Add(f.origin,
           Vector3Add(Vector3Scale(f.U, uv.x), Vector3Scale(f.V, uv.y)));
}

enum class FaceId { PosX, NegX, PosY, NegY, PosZ, NegZ };

static Frame MakeCubeFaceFrame(FaceId face, float half)
{
    Frame f{};
    switch (face)
    {
        case FaceId::PosX: f.N={+1,0,0}; f.U={0,+1,0}; f.V={0,0,+1}; break;
        case FaceId::NegX: f.N={-1,0,0}; f.U={0,-1,0}; f.V={0,0,+1}; break;
        case FaceId::PosY: f.N={0,+1,0}; f.U={+1,0,0}; f.V={0,0,-1}; break;
        case FaceId::NegY: f.N={0,-1,0}; f.U={+1,0,0}; f.V={0,0,+1}; break;
        case FaceId::PosZ: f.N={0,0,+1}; f.U={+1,0,0}; f.V={0,+1,0}; break;
        case FaceId::NegZ: f.N={0,0,-1}; f.U={+1,0,0}; f.V={0,-1,0}; break;
    }
    f.origin = Vector3Scale(f.N, half);
    return f;
}

struct MeshBuilder
{
    std::vector<Vector3> positions, normals;
    std::vector<Vector2> texcoords;
    std::vector<uint16_t> indices;

    uint16_t AddVertex(Vector3 p, Vector3 n, Vector2 uv)
    {
        positions.push_back(p); normals.push_back(n); texcoords.push_back(uv);
        assert(positions.size() <= 65535);
        return static_cast<uint16_t>(positions.size() - 1);
    }

    static void AddQuad(MeshBuilder& mb, Vector3 a, Vector3 b, Vector3 c, Vector3 d)
    {
        Vector3 n0 = Vector3CrossProduct(Vector3Subtract(b,a), Vector3Subtract(c,a));
        Vector3 n1 = Vector3CrossProduct(Vector3Subtract(c,a), Vector3Subtract(d,a));
        Vector3 n  = Vector3Normalize(Vector3Add(n0, n1));
        uint16_t i0=mb.AddVertex(a,n,{0,0}), i1=mb.AddVertex(b,n,{1,0});
        uint16_t i2=mb.AddVertex(c,n,{1,1}), i3=mb.AddVertex(d,n,{0,1});
        mb.indices.push_back(i0); mb.indices.push_back(i1); mb.indices.push_back(i2);
        mb.indices.push_back(i0); mb.indices.push_back(i2); mb.indices.push_back(i3);
    }
};

using Point = std::array<double,2>;
using Ring  = std::vector<Point>;
using Poly  = std::vector<Ring>;

static Ring RingCW(float h)  { return {{-h,-h},{-h,+h},{+h,+h},{+h,-h}}; }
static Ring RingCCW(float h) { return {{-h,-h},{+h,-h},{+h,+h},{-h,+h}}; }

static void AddFaceWithHole(MeshBuilder& mb, const Frame& fr,
                             float fh, float hh, Vector3 fn, bool flip)
{
    Poly poly; poly.push_back(RingCCW(fh)); poly.push_back(RingCW(hh));
    auto triIdx = mapbox::earcut<uint32_t>(poly);

    std::vector<Vector3> pts; std::vector<Vector2> uvs;
    for (const auto& p : poly[0]) { pts.push_back(To3D(fr,{(float)p[0],(float)p[1]})); uvs.push_back({(float)p[0]/fh*.5f+.5f,(float)p[1]/fh*.5f+.5f}); }
    for (const auto& p : poly[1]) { pts.push_back(To3D(fr,{(float)p[0],(float)p[1]})); uvs.push_back({(float)p[0]/fh*.5f+.5f,(float)p[1]/fh*.5f+.5f}); }

    std::vector<uint16_t> base;
    for (size_t i=0;i<pts.size();++i) base.push_back(mb.AddVertex(pts[i],fn,uvs[i]));
    for (size_t t=0;t+2<triIdx.size();t+=3)
    {
        uint16_t a=base[triIdx[t]],b=base[triIdx[t+1]],c=base[triIdx[t+2]];
        if(flip){mb.indices.push_back(a);mb.indices.push_back(c);mb.indices.push_back(b);}
        else    {mb.indices.push_back(a);mb.indices.push_back(b);mb.indices.push_back(c);}
    }
}

static void AddTunnel(MeshBuilder& mb, const Frame& o, const Frame& in, float hh)
{
    float corners[4][2]={{-hh,-hh},{+hh,-hh},{+hh,+hh},{-hh,+hh}};
    for(int i=0;i<4;++i)
    {
        int j=(i+1)%4;
        MeshBuilder::AddQuad(mb,
            To3D(o,{corners[i][0],corners[i][1]}), To3D(o,{corners[j][0],corners[j][1]}),
            To3D(in,{corners[j][0],corners[j][1]}), To3D(in,{corners[i][0],corners[i][1]}));
    }
}

static Mesh BuildThickCubeWithHoles(float oh, float thick, float hh)
{
    float ih=oh-thick;
    MeshBuilder mb;
    for(FaceId f:{FaceId::PosX,FaceId::NegX,FaceId::PosY,FaceId::NegY,FaceId::PosZ,FaceId::NegZ})
    {
        Frame oF=MakeCubeFaceFrame(f,oh), iF=MakeCubeFaceFrame(f,ih);
        AddFaceWithHole(mb,oF,oh,hh,oF.N,false);
        AddFaceWithHole(mb,iF,ih,hh,Vector3Negate(oF.N),true);
        AddTunnel(mb,oF,iF,hh);
    }
    Mesh mesh={};
    mesh.vertexCount=(int)mb.positions.size(); mesh.triangleCount=(int)mb.indices.size()/3;
    mesh.vertices =(float*)MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.normals  =(float*)MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords=(float*)MemAlloc(mesh.vertexCount*2*sizeof(float));
    mesh.indices  =(unsigned short*)MemAlloc(mb.indices.size()*sizeof(unsigned short));
    for(int i=0;i<mesh.vertexCount;++i)
    {
        mesh.vertices[i*3+0]=mb.positions[i].x; mesh.vertices[i*3+1]=mb.positions[i].y; mesh.vertices[i*3+2]=mb.positions[i].z;
        mesh.normals[i*3+0]=mb.normals[i].x;    mesh.normals[i*3+1]=mb.normals[i].y;    mesh.normals[i*3+2]=mb.normals[i].z;
        mesh.texcoords[i*2+0]=mb.texcoords[i].x; mesh.texcoords[i*2+1]=mb.texcoords[i].y;
    }
    for(size_t i=0;i<mb.indices.size();++i) mesh.indices[i]=mb.indices[i];
    UploadMesh(&mesh,false);
    return mesh;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void)
{
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Game 1 - Thick Cube");
    SetExitKey(KEY_ESCAPE);   // ESC exits back to the arcade menu
    SetTargetFPS(60);

    SearchAndSetResourceDir("resources");

    Font dbgFont = LoadFontEx("fonts/Inter-VariableFont_opsz,wght.ttf", 22, 0, 0);

    Camera3D cam{};
    cam.position={3.5f,3.0f,3.5f}; cam.target={0,0,0}; cam.up={0,1,0};
    cam.fovy=45.0f; cam.projection=CAMERA_PERSPECTIVE;

    Mesh  mesh  = BuildThickCubeWithHoles(1.0f, 0.25f, 0.30f);
    Model model = LoadModelFromMesh(mesh);

    Shader lighting = LoadShader("shaders/glsl330/lighting.vs",
                                 "shaders/glsl330/lighting.fs");
    lighting.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lighting, "viewPos");
    int ambientLoc = GetShaderLocation(lighting, "ambient");
    float ambient[4] = {0.18f,0.18f,0.18f,1.0f};
    SetShaderValue(lighting, ambientLoc, ambient, SHADER_UNIFORM_VEC4);
    model.materials[0].shader = lighting;

    [[maybe_unused]]
    Light sun = CreateLight(LIGHT_DIRECTIONAL, (Vector3){2,4,2}, (Vector3){0,0,0}, WHITE, lighting);

    while (!WindowShouldClose())
    {
        UpdateCamera(&cam, CAMERA_FREE);
        float vp[3]={cam.position.x,cam.position.y,cam.position.z};
        SetShaderValue(lighting, lighting.locs[SHADER_LOC_VECTOR_VIEW], vp, SHADER_UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(cam);
        DrawModel(model, (Vector3){0,1,0}, 1.0f, WHITE);
        DrawModelWires(model, (Vector3){0,1,0}, 1.0f, DARKGRAY);
        DrawGrid(10, 1.0f);
        EndMode3D();
        DrawTextEx(dbgFont, "Game 1 - Thick Cube   [ESC] Back to Menu",
                   (Vector2){10,10}, 22, 1, DARKGRAY);
        EndDrawing();
    }

    UnloadShader(lighting);
    UnloadModel(model);
    UnloadFont(dbgFont);
    CloseWindow();
    return 0;
}

