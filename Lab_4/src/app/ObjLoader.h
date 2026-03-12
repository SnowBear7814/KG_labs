#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <DirectXMath.h>
#include <algorithm>
#include <map>

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
};

struct ObjMesh
{
    std::vector<Vertex> Vertices;
    std::vector<std::uint16_t> Indices;
    std::string Name;

    // Для хранения подобластей (если нужно)
    struct Subset
    {
        std::string MaterialName;
        uint32_t StartIndex;
        uint32_t IndexCount;
    };
    std::vector<Subset> Subsets;
};

class ObjLoader
{
public:
    static bool Load(const std::string& filename, ObjMesh& mesh)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            return false;

        std::vector<XMFLOAT3> positions;
        std::vector<XMFLOAT3> normals;
        std::vector<XMFLOAT2> texCoords;

        std::vector<XMFLOAT3> tempPositions;
        std::vector<XMFLOAT3> tempNormals;
        std::vector<XMFLOAT2> tempTexCoords;

        std::vector<unsigned int> posIndices, normIndices, texIndices;

        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v")
            {
                XMFLOAT3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vn")
            {
                XMFLOAT3 norm;
                iss >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            }
            else if (prefix == "vt")
            {
                XMFLOAT2 tex;
                iss >> tex.x >> tex.y;
                texCoords.push_back(tex);
            }
            else if (prefix == "f")
            {
                std::string vertexData;
                while (iss >> vertexData)
                {
                    std::replace(vertexData.begin(), vertexData.end(), '/', ' ');
                    std::istringstream viss(vertexData);

                    unsigned int pIdx = 0, tIdx = 0, nIdx = 0;
                    viss >> pIdx;
                    if (viss.peek() == ' ') viss.get();
                    if (viss >> tIdx)
                    {
                        if (viss.peek() == ' ') viss.get();
                        viss >> nIdx;
                    }
                    else
                    {
                        viss.clear();
                        viss >> nIdx;
                    }

                    posIndices.push_back(pIdx);
                    normIndices.push_back(nIdx);
                    texIndices.push_back(tIdx);
                }
            }
        }

        // Создаем уникальные вершины
        mesh.Vertices.clear();
        mesh.Indices.clear();

        std::map<std::tuple<unsigned int, unsigned int, unsigned int>, unsigned int> uniqueVertices;

        for (size_t i = 0; i < posIndices.size(); i++)
        {
            auto key = std::make_tuple(
                posIndices[i],
                normIndices.empty() ? 0 : normIndices[i],
                texIndices.empty() ? 0 : texIndices[i]
            );

            if (uniqueVertices.find(key) == uniqueVertices.end())
            {
                Vertex v;
                v.Position = positions[posIndices[i] - 1];
                v.Normal = normals[normIndices[i] - 1];
                v.Color = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f); // Серый цвет по умолчанию

                uniqueVertices[key] = (unsigned int)mesh.Vertices.size();
                mesh.Vertices.push_back(v);
            }

            mesh.Indices.push_back(uniqueVertices[key]);
        }

        return true;
    }
};