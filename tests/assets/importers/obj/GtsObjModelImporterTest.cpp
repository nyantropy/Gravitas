#include "GtsObjModelImporter.h"
#include "GtsModelImportResult.h"
#include "GtsModelValidation.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    class Fixture
    {
        public:
        Fixture()
        {
            root = std::filesystem::temp_directory_path() /
                   ("gts_canonical_obj_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(root);
        }
        ~Fixture()
        {
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }
        std::filesystem::path write(const std::string& name, const std::string& content) const
        {
            const auto path = root / name;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream output(path);
            output << content;
            require(static_cast<bool>(output), "Could not write fixture");
            return path;
        }
        GtsModelImportResult import(const std::string& content) const
        {
            const GtsObjModelImporter importer;
            const IGtsModelImporter&  strategy = importer;
            return strategy.importAsset({write("model.obj", content)});
        }
        std::filesystem::path root;
    };

    const std::string positions    = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    const std::string uvs          = "vt 0 0\nvt 1 0.25\nvt 0.5 1\n";
    const std::string texturedFace = "f 1/1 2/2 3/3\n";

    const GtsModelAsset& succeeded(const GtsModelImportResult& result)
    {
        std::string messages;
        for (const auto& diagnostic : result.diagnostics())
            messages += diagnostic.code + ": " + diagnostic.message + "\n";
        require(result.succeeded() && result.asset(), "Import failed:\n" + messages);
        require(result.bundle() && result.bundle()->skeletons.empty(), "OBJ produces only a primary model");
        require(validateGtsModelAsset(*result.asset()).isValid(), "Success must expose a valid canonical asset");
        return *result.asset();
    }

    void
    requireDiagnostic(const GtsModelImportResult& result, const std::string& code, GtsModelDiagnosticSeverity severity)
    {
        for (const auto& diagnostic : result.diagnostics())
        {
            if (diagnostic.code == code && diagnostic.severity == severity)
            {
                require(!diagnostic.message.empty() && !diagnostic.location.empty(), "Diagnostic has source context");
                return;
            }
        }
        throw std::runtime_error("Missing diagnostic " + code);
    }

    const GtsVertexAttribute* attribute(const GtsModelPrimitive& primitive, GtsVertexSemantic semantic)
    {
        for (const auto& value : primitive.attributes)
        {
            if (value.semantic == semantic && value.setIndex == 0)
                return &value;
        }
        return nullptr;
    }

    template <class T> const std::vector<T>& values(const GtsModelPrimitive& primitive, GtsVertexSemantic semantic)
    {
        const auto* stream = attribute(primitive, semantic);
        require(stream != nullptr, "Expected semantic stream");
        return std::get<std::vector<T>>(stream->values);
    }

    void minimalAndIndexedGeometry()
    {
        Fixture     fixture;
        const auto  result = fixture.import(positions + "f 1 2 3\n");
        const auto& asset  = succeeded(result);
        require(asset.meshes.size() == 1 && asset.meshes[0].primitives.size() == 1,
                "Minimal OBJ has one mesh and primitive");
        const auto& primitive = asset.meshes[0].primitives[0];
        require(primitive.indices == std::vector<uint32_t>{0, 1, 2}, "Triangle has local indices");
        require(values<glm::vec3>(primitive, GtsVertexSemantic::Position) ==
                    std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}},
                "Positions are preserved");
        require(primitive.attributes.size() == 1, "Absent normals, UVs, colors, tangents and skinning stay absent");
        require(!primitive.materialIndex && asset.materials.empty() && asset.images.empty(),
                "Unassigned material remains absent");
        require(!result.hasWarnings(), "Entirely absent optional streams need no partial-stream warning");
        require(asset.rootNodes == std::vector<uint32_t>{0} && asset.nodes.size() == 1 &&
                    asset.nodes[0].meshIndex == 0 && asset.nodes[0].children.empty() &&
                    asset.nodes[0].localTransform == glm::mat4(1),
                "Minimal node is an identity root referencing the mesh");
        const auto  repeated = fixture.import(positions + "f 1 2 3\nf -3 -1 -2\n");
        const auto& indexed  = succeeded(repeated).meshes[0].primitives[0];
        require(indexed.indices == std::vector<uint32_t>{0, 1, 2, 0, 2, 1},
                "Repeated and relative tuples reuse vertices");
        require(values<glm::vec3>(indexed, GtsVertexSemantic::Position).size() == 3,
                "Indexed vertices are deduplicated");
        const auto quad = fixture.import(positions + "v 1 1 0\nf 1 2 4 3\n");
        require(succeeded(quad).meshes[0].primitives[0].indices.size() == 6, "TinyOBJ triangulates quads");
    }

    void streamsAndSeams()
    {
        Fixture     fixture;
        const auto  result    = fixture.import(positions + uvs + "vn 0 0 2\nf 1/1/1 2/2/1 3/3/1\n");
        const auto& primitive = succeeded(result).meshes[0].primitives[0];
        require(values<glm::vec3>(primitive, GtsVertexSemantic::Normal) ==
                    std::vector<glm::vec3>(3, glm::vec3(0, 0, 2)),
                "Authored normals are preserved without geometry repair");
        require(values<glm::vec2>(primitive, GtsVertexSemantic::TexCoord) ==
                    std::vector<glm::vec2>{{0, 1}, {1, 0.75f}, {0.5f, 0}},
                "V flip occurs during source interpretation");
        require(!attribute(primitive, GtsVertexSemantic::Tangent), "No tangents are generated");
        const auto  seams = fixture.import(positions + uvs +
                                           "vn 0 0 1\nvn 0 0 -1\n"
                                           "f 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 2/2/1\n"
                                           "f 1/1/2 2/2/2 3/3/2\nf 1/2/1 2/2/1 3/3/1\n");
        const auto& split = succeeded(seams).meshes[0].primitives[0];
        require(values<glm::vec3>(split, GtsVertexSemantic::Position).size() == 7 && split.indices.size() == 12,
                "Normal seams and UV seams split vertices while repeated tuples share them");
        require(values<glm::vec3>(split, GtsVertexSemantic::Normal)[3] == glm::vec3(0, 0, -1),
                "Hard-edge normal survives");
        require(values<glm::vec2>(split, GtsVertexSemantic::TexCoord)[6] == glm::vec2(1, 0.75f), "UV seam survives");
    }

    void partialStreamsAndShapes()
    {
        Fixture fixture;
        fixture.write("surface.mtl", "newmtl complete\nnewmtl partial\n");
        const auto  result = fixture.import("mtllib surface.mtl\n" + positions + uvs +
                                            "vn 0 0 1\no first\n"
                                            "usemtl complete\nf 1/1/1 2/2/1 3/3/1\n"
                                            "usemtl partial\nf 1/1/1 2//1 3/3\n"
                                            "usemtl complete\nf 1/1/1 2/2/1 3/3/1\n"
                                            "o second\nf 1 2 3\n");
        const auto& asset  = succeeded(result);
        require(asset.meshes.size() == 2 && asset.meshes[0].name == "first" && asset.meshes[1].name == "second",
                "Shapes retain names and independent mesh boundaries");
        const auto& runs = asset.meshes[0].primitives;
        require(runs.size() == 3 && runs[0].materialIndex == 0 && runs[1].materialIndex == 1 &&
                    runs[2].materialIndex == 0,
                "Material runs A/B/A remain distinct primitives");
        require(attribute(runs[0], GtsVertexSemantic::Normal) && attribute(runs[2], GtsVertexSemantic::TexCoord),
                "Complete streams survive in neighboring primitives");
        require(runs[1].attributes.size() == 1 && asset.meshes[1].primitives[0].attributes.size() == 1,
                "Partial and absent optional streams are omitted per primitive");
        requireDiagnostic(result, "OBJ_PARTIAL_NORMALS", GtsModelDiagnosticSeverity::Warning);
        requireDiagnostic(result, "OBJ_PARTIAL_UVS", GtsModelDiagnosticSeverity::Warning);
        for (const auto& mesh : asset.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                require(primitive.indices == std::vector<uint32_t>{0, 1, 2},
                        "Every primitive has its own local index space");
            }
        }
        require(asset.rootNodes == std::vector<uint32_t>{0, 1} && asset.nodes[1].meshIndex == 1 &&
                    asset.nodes[1].children.empty() && asset.nodes[1].localTransform == glm::mat4(1),
                "Shapes have deterministic flat roots, not an invented transform hierarchy");
        const auto  partialUV = fixture.import(positions + uvs + "vn 0 0 1\nf 1/1/1 2//1 3/3/1\n");
        const auto& noUV      = succeeded(partialUV).meshes[0].primitives[0];
        require(attribute(noUV, GtsVertexSemantic::Normal) && !attribute(noUV, GtsVertexSemantic::TexCoord),
                "Partial UVs do not remove complete normals");
        const auto  partialNormal = fixture.import(positions + uvs + "vn 0 0 1\nf 1/1/1 2/2 3/3/1\n");
        const auto& noNormal      = succeeded(partialNormal).meshes[0].primitives[0];
        require(!attribute(noNormal, GtsVertexSemantic::Normal) && attribute(noNormal, GtsVertexSemantic::TexCoord),
                "Partial normals do not remove complete UVs");
    }

    void authoredColors()
    {
        Fixture     fixture;
        const auto  result = fixture.import("v 0 0 0 1 1 1\nv 1 0 0 0 1 0\nv 0 1 0 0 0 1\n"
                                            "v 2 0 0\nv 2 1 0\nv 3 0 0\n"
                                            "g colored\nf 1 2 3\ng partial\nf 1 2 4\ng plain\nf 4 5 6\n");
        const auto& asset  = succeeded(result);
        require(asset.meshes.size() == 3, "Color fixtures retain group boundaries");
        require(values<glm::vec4>(asset.meshes[0].primitives[0], GtsVertexSemantic::Color) ==
                    std::vector<glm::vec4>{{1, 1, 1, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}},
                "Authored colors including white survive despite uncolored vertices elsewhere");
        require(!attribute(asset.meshes[1].primitives[0], GtsVertexSemantic::Color) &&
                    !attribute(asset.meshes[2].primitives[0], GtsVertexSemantic::Color),
                "No fabricated colors escape TinyOBJ fallback");
        requireDiagnostic(result, "OBJ_PARTIAL_COLORS", GtsModelDiagnosticSeverity::Warning);
    }

    void materialsAndImages()
    {
        Fixture    fixture;
        const auto shared    = fixture.write("images/shared.png", "encoded bytes are deliberately not decoded");
        const auto metallic  = fixture.write("images/metal.png", "not decoded");
        const auto roughness = fixture.write("images/rough.png", "not decoded");
        fixture.write("materials/surface.mtl",
                      "newmtl surface\nKd 0.2 0.4 0.6\nd 0.7\nPm 0.8\nPr 0.3\nKe 0.1 0.2 2\n"
                      "map_Kd ../images/shared.png\nmap_Pm ../images/metal.png\n"
                      "map_Pr -imfchan g ../images/rough.png\nnorm -bm 0.5 ../images/shared.png\n"
                      "map_Ka -imfchan b ../images/shared.png\nmap_Ke ../images/shared.png\n"
                      "newmtl packed\nTr 0.25\nmap_Kd ../images/./shared.png\n"
                      "map_Pm -imfchan m ../images/shared.png\nmap_Pr -imfchan r ../images/shared.png\n"
                      "newmtl defaults\n");
        const auto  result = fixture.import("mtllib materials/surface.mtl\n" + positions + uvs + "usemtl surface\n" +
                                            texturedFace + "usemtl packed\n" + texturedFace);
        const auto& asset  = succeeded(result);
        require(asset.materials.size() == 3 && asset.images.size() == 3,
                "Materials retained; identical image paths deduplicated across roles/materials");
        const auto& material = asset.materials[0];
        require(material.name == "surface" && material.baseColor == glm::vec4(0.2f, 0.4f, 0.6f, 0.7f),
                "MTL name, diffuse and dissolve map");
        require(material.metallic == 0.8f && material.roughness == 0.3f, "PBR factors map independently");
        require(material.emissiveFactor == glm::vec3(0.1f, 0.2f, 2) && material.emissiveStrength == 1,
                "Emission maps without HDR clamping");
        require(material.alphaMode == GtsModelAlphaMode::Blend && material.normalScale == 0.5f,
                "Opacity and normal scale map");
        require(material.baseColorImage && material.normalImage && material.ambientOcclusionImage &&
                    material.emissiveImage,
                "All supported image roles map");
        require(material.metallicImage->image.imageIndex != material.roughnessImage->image.imageIndex,
                "Independent maps are not packed");
        require(material.metallicImage->channel == GtsModelTextureChannel::Red &&
                    material.roughnessImage->channel == GtsModelTextureChannel::Green &&
                    material.ambientOcclusionImage->channel == GtsModelTextureChannel::Blue,
                "Scalar channel selection maps");
        require(std::get<std::filesystem::path>(asset.images[material.baseColorImage->imageIndex].source) == shared &&
                    std::get<std::filesystem::path>(asset.images[material.metallicImage->image.imageIndex].source) ==
                        metallic &&
                    std::get<std::filesystem::path>(asset.images[material.roughnessImage->image.imageIndex].source) ==
                        roughness,
                "External image paths resolve relative to their MTL, not the OBJ directory");
        require(material.baseColorImage->texCoordSet == 0 && material.metallicImage->image.texCoordSet == 0,
                "OBJ bindings select UV0");
        const auto& packed = asset.materials[1];
        require(packed.baseColor.a == 0.75f && packed.alphaMode == GtsModelAlphaMode::Blend, "Tr maps to opacity");
        require(packed.metallicImage->image.imageIndex == packed.roughnessImage->image.imageIndex &&
                    packed.metallicImage->channel == GtsModelTextureChannel::Alpha &&
                    packed.roughnessImage->channel == GtsModelTextureChannel::Red,
                "One source image can supply independent scalar channels");
        const auto& defaults = asset.materials[2];
        require(defaults.baseColor == glm::vec4(1) && defaults.metallic == 0 && defaults.roughness == 1 &&
                    defaults.alphaMode == GtsModelAlphaMode::Opaque && !defaults.doubleSided,
                "Unauthored appearance uses canonical defaults");
    }

    void errorsAndWarnings()
    {
        Fixture                   fixture;
        const GtsObjModelImporter importer;
        require(!importer.importAsset({fixture.root / "missing.obj"}).succeeded(), "Unreadable source fails");
        require(!importer.importAsset({}).succeeded(), "Empty source path fails");
        for (const auto& [face, code] : std::vector<std::pair<std::string, std::string>>{
                 {"f 1 2 99\n", "OBJ_POSITION_INDEX_OUT_OF_RANGE"},
                 {"f 1 2 -99\n", "OBJ_POSITION_INDEX_OUT_OF_RANGE"},
                 {"f 0 2 3\n", "OBJ_POSITION_INDEX_OUT_OF_RANGE"},
                 {"f 1 2 999999999999999999999999\n", "OBJ_POSITION_INDEX_OUT_OF_RANGE"},
                 {"f 1//2 2//1 3//1\n", "OBJ_NORMAL_INDEX_OUT_OF_RANGE"},
                 {"f 1/99 2/2 3/3\n", "OBJ_TEXCOORD_INDEX_OUT_OF_RANGE"},
                 {"f 1 2\n", "OBJ_FACE_INVALID"},
                 {"f 1 2 3 99\n", "OBJ_POSITION_INDEX_OUT_OF_RANGE"}})
        {
            const auto result = fixture.import(positions + uvs + "vn 0 0 1\n" + face);
            require(!result.succeeded() && !result.asset(), "Invalid indices never expose partial success");
            requireDiagnostic(result, code, GtsModelDiagnosticSeverity::Error);
        }
        const auto invalidNumeric = fixture.import("v nan 0 0\n");
        requireDiagnostic(invalidNumeric, "OBJ_ATTRIBUTE_INVALID", GtsModelDiagnosticSeverity::Error);
        require(!fixture.import(positions).succeeded(), "No face geometry fails explicitly");
        fixture.write("invalid.mtl", "newmtl invalid\nKd 2 0 0\n");
        const auto invalidMaterial = fixture.import("mtllib invalid.mtl\n" + positions + "usemtl invalid\nf 1 2 3\n");
        require(!invalidMaterial.succeeded() && !invalidMaterial.asset(),
                "Canonical validation gates imported appearance");
        requireDiagnostic(invalidMaterial, "MODEL_MATERIAL_RANGE", GtsModelDiagnosticSeverity::Error);

        fixture.write("mapped.mtl", "newmtl mapped\nmap_Kd missing.png\n");
        const auto missingUV = fixture.import("mtllib mapped.mtl\n" + positions + uvs + "usemtl mapped\nf 1/1 2 3/3\n");
        require(!missingUV.succeeded(),
                "Missing required UVs do not trigger fabricated data or discarded material bindings");
        requireDiagnostic(missingUV, "OBJ_PARTIAL_UVS", GtsModelDiagnosticSeverity::Warning);
        requireDiagnostic(missingUV, "MODEL_TEXCOORD_REQUIRED", GtsModelDiagnosticSeverity::Error);
        const auto missingImage =
            fixture.import("mtllib mapped.mtl\n" + positions + uvs + "usemtl mapped\n" + texturedFace);
        require(succeeded(missingImage).images.size() == 1, "Missing external image reference is retained");
        requireDiagnostic(missingImage, "OBJ_IMAGE_MISSING", GtsModelDiagnosticSeverity::Warning);
        const auto missingMtl = fixture.import("mtllib absent.mtl\n" + positions + "usemtl absent\nf 1 2 3\n");
        require(!succeeded(missingMtl).meshes[0].primitives[0].materialIndex,
                "Unresolved material safely remains unassigned");
        requireDiagnostic(missingMtl, "OBJ_PARSE_WARNING", GtsModelDiagnosticSeverity::Warning);

        fixture.write("bump.mtl", "newmtl bump\nbump height.png\nmap_d opacity.png\nmap_Pm -imfchan l scalar.png\n");
        const auto bump = fixture.import("mtllib bump.mtl\n" + positions + "usemtl bump\nf 1 2 3\n");
        require(!succeeded(bump).materials[0].normalImage && !bump.asset()->materials[0].metallicImage,
                "Unrepresentable maps are diagnosed, not mislabeled as normal or RGBA data");
        requireDiagnostic(bump, "OBJ_BUMP_MAP_UNSUPPORTED", GtsModelDiagnosticSeverity::Warning);
        requireDiagnostic(bump, "OBJ_OPACITY_MAP_UNSUPPORTED", GtsModelDiagnosticSeverity::Warning);
        requireDiagnostic(bump, "OBJ_IMAGE_CHANNEL_UNSUPPORTED", GtsModelDiagnosticSeverity::Warning);
    }
} // namespace

int main()
{
    try
    {
        minimalAndIndexedGeometry();
        streamsAndSeams();
        partialStreamsAndShapes();
        authoredColors();
        materialsAndImages();
        errorsAndWarnings();
        std::puts("GtsObjModelImporterTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
