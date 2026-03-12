// Saves result to /tmp/result.png

#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <limits>
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ======================= Math =======================
struct Vec2 { float x, y; };

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float k) const { return {x*k, y*k, z*k}; }
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
};

float dot(const Vec3& a, const Vec3& b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 cross(const Vec3& a, const Vec3& b){
    return { a.y*b.z - a.z*b.y,
             a.z*b.x - a.x*b.z,
             a.x*b.y - a.y*b.x };
}
Vec3 normalize(const Vec3& v){
    float len = std::sqrt(dot(v,v));
    if(len < 1e-6f) return {0,0,0};
    return v * (1.0f / len);
}

// ======================= Barycentric (2D) =======================
Vec3 barycentric2D(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& p){
    Vec3 s0{c.x - a.x, b.x - a.x, a.x - p.x};
    Vec3 s1{c.y - a.y, b.y - a.y, a.y - p.y};
    Vec3 u = cross(s0, s1);
    if (std::abs(u.z) < 1e-6f) return {-1,-1,-1};
    return {
        1.0f - (u.x + u.y) / u.z,
        u.y / u.z,
        u.x / u.z
    };
}

// ======================= OBJ loader =======================
struct Face { int v[3]; int n[3]; };
struct Model {
    std::vector<Vec3> verts;
    std::vector<Vec3> norms;
    std::vector<Face> faces;
};

Model loadOBJ(const std::string& path){
    Model m;
    std::ifstream file(path);
    if(!file){ std::cout << "Cannot open " << path << "\n"; return m; }

    std::string line;
    while(std::getline(file, line)){
        std::istringstream ss(line);
        std::string t; ss >> t;
        if(t == "v"){
            Vec3 v; ss >> v.x >> v.y >> v.z; m.verts.push_back(v);
        }
        else if(t == "vn"){
            Vec3 n; ss >> n.x >> n.y >> n.z; m.norms.push_back(n);
        }
        else if(t == "f"){
            Face f{};
            for(int i=0;i<3;i++){
                std::string s; ss >> s;
                int vi=0, ni=0;
                if(s.find("//") != std::string::npos)
                    sscanf(s.c_str(), "%d//%d", &vi, &ni);
                else
                    sscanf(s.c_str(), "%d/%*d/%d", &vi, &ni);
                f.v[i] = vi - 1;
                f.n[i] = ni > 0 ? ni - 1 : f.v[i];
            }
            m.faces.push_back(f);
        }
    }
    std::cout << "Loaded OBJ: " << m.verts.size() << " verts, "
              << m.faces.size() << " faces\n";
    return m;
}

// ======================= Center & scale =======================
void centerAndScale(Model& m){
    if(m.verts.empty()) return;
    Vec3 minV = m.verts[0], maxV = m.verts[0], center{0,0,0};
    for(auto& v : m.verts){
        center += v;
        minV.x = std::min(minV.x, v.x);
        minV.y = std::min(minV.y, v.y);
        minV.z = std::min(minV.z, v.z);
        maxV.x = std::max(maxV.x, v.x);
        maxV.y = std::max(maxV.y, v.y);
        maxV.z = std::max(maxV.z, v.z);
    }
    center = center * (1.0f / m.verts.size());
    float scale = std::max({maxV.x-minV.x, maxV.y-minV.y, maxV.z-minV.z});
    for(auto& v : m.verts)
        v = (v - center) * (2.0f / scale);
}

// ======================= Draw Cube =======================
void drawCube(std::vector<unsigned char>& image, std::vector<float>& zbuf, int W, int H) {
    // Вершины куба (единичный куб от -0.95 до 0.95)
    Vec3 cubeVerts[8] = {
        {-0.95, -0.95, -0.95}, {0.95, -0.95, -0.95}, {0.95, -0.95, 0.95}, {-0.95, -0.95, 0.95},  // нижняя грань
        {-0.95, 0.95, -0.95}, {0.95, 0.95, -0.95}, {0.95, 0.95, 0.95}, {-0.95, 0.95, 0.95}       // верхняя грань
    };
    
    // Индексы для 12 треугольников (6 граней * 2 треугольника)
    int indices[12][3] = {
        {0, 1, 2}, {0, 2, 3},
        {4, 6, 5}, {4, 7, 6},
        {3, 2, 6}, {3, 6, 7},
        {0, 5, 1}, {0, 4, 5},
        {0, 3, 7}, {0, 7, 4},
        {1, 6, 2}, {1, 5, 6}
    };

    // Цвет куба
    unsigned char cubeColor[3] = {100, 150, 255};

    for (int tri = 0; tri < 12; tri++) {
        Vec3 v[3];
        Vec2 p[3];
        
        for (int i = 0; i < 3; i++) {
            v[i] = cubeVerts[indices[tri][i]];
            p[i].x = (v[i].x + 1.0f) * 0.5f * W;
            p[i].y = (v[i].y + 1.0f) * 0.5f * H;
        }

        // Bounding box треугольника
        int minX = std::max(0, (int)std::floor(std::min({p[0].x, p[1].x, p[2].x})));
        int maxX = std::min(W-1, (int)std::ceil(std::max({p[0].x, p[1].x, p[2].x})));
        int minY = std::max(0, (int)std::floor(std::min({p[0].y, p[1].y, p[2].y})));
        int maxY = std::min(H-1, (int)std::ceil(std::max({p[0].y, p[1].y, p[2].y})));

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                Vec3 bc = barycentric2D(p[0], p[1], p[2], {(float)x + 0.5f, (float)y + 0.5f});
                if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

                float z = v[0].z * bc.x + v[1].z * bc.y + v[2].z * bc.z;
                int idx = x + y * W;
                
                if (z <= zbuf[idx]) continue;
                
                float oldZ = zbuf[idx];
                zbuf[idx] = z;

                // Смешиваем с существующим цветом (альфа-смешивание 50%)
                unsigned char bgColor[3] = {
                    image[idx*3+0],
                    image[idx*3+1], 
                    image[idx*3+2]
                };
                
                // Полупрозрачное смешивание (50% куб, 50% фон)
                image[idx*3+0] = (unsigned char)(cubeColor[0] * 0.5f + bgColor[0] * 0.5f);
                image[idx*3+1] = (unsigned char)(cubeColor[1] * 0.5f + bgColor[1] * 0.5f);
                image[idx*3+2] = (unsigned char)(cubeColor[2] * 0.5f + bgColor[2] * 0.5f);
                
                zbuf[idx] = oldZ;
            }
        }
    }
}

// ======================= MAIN =======================
int main(){
    const int W = 512, H = 512;
    std::vector<unsigned char> image(W*H*3, 30);
    std::vector<float> zbuf(W*H, -std::numeric_limits<float>::infinity());

    Model model = loadOBJ("C:/Users/User/Downloads/gf/face.obj");
    
    centerAndScale(model);

    if(model.faces.empty()){
        std::cout << "Model is empty!" << std::endl;
        return 0;
    }

    Vec3 lightDir = normalize({1,1,1});

    for(const Face& f : model.faces){
        Vec3 v[3], n[3];
        Vec2 p[3];
        for(int i=0;i<3;i++){
            v[i] = model.verts[f.v[i]];
            n[i] = model.norms.empty() ? Vec3{0,0,1} : model.norms[f.n[i]];
            p[i].x = (v[i].x + 1.0f) * 0.5f * W;
            p[i].y = (v[i].y + 1.0f) * 0.5f * H;
        }

        int minX = std::max(0, (int)std::floor(std::min({p[0].x,p[1].x,p[2].x})));
        int maxX = std::min(W-1, (int)std::ceil (std::max({p[0].x,p[1].x,p[2].x})));
        int minY = std::max(0, (int)std::floor(std::min({p[0].y,p[1].y,p[2].y})));
        int maxY = std::min(H-1, (int)std::ceil (std::max({p[0].y,p[1].y,p[2].y})));

        for(int y=minY; y<=maxY; y++){
            for(int x=minX; x<=maxX; x++){
                Vec3 bc = barycentric2D(p[0], p[1], p[2], {(float)x+0.5f,(float)y+0.5f});
                if(bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
                // Поворот UV координат на 180 градусов
                float u = bc.x;
                float v = bc.y;
                // Поворот на 180°: (u,v) -> (1-u, 1-v)
                float rotatedU = 1.0f - u;
                float rotatedV = 1.0f - v;
                float z = v[0].z*bc.x + v[1].z*bc.y + v[2].z*bc.z;
                int idx = x + y*W;
                if(z <= zbuf[idx]) continue;
                zbuf[idx] = z;

                Vec3 normal = normalize(n[0]*bc.x + n[1]*bc.y + n[2]*bc.z);
                float diff = std::clamp(dot(normal, lightDir), 0.0f, 1.0f);

                image[idx*3+0] = (unsigned char)(diff * 255);
                image[idx*3+1] = (unsigned char)(diff * 220);
                image[idx*3+2] = (unsigned char)(diff * 180);
            }
        }
    }

    drawCube(image, zbuf, W, H);
                    
    int ok = stbi_write_png("/tmp/result.png", W, H, 3, image.data(), W*3);
    std::cout << "stbi_write_png returned " << ok << std::endl;
    std::cout << "Open /tmp/result.png" << std::endl;
    return 0;
}
