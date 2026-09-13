#include "GltfFixtureBuilder.h"

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / "gravitas_canonical_gltf_test";
        std::filesystem::create_directories(root);
        for (const auto format : {GltfFixtureFormat::External, GltfFixtureFormat::DataUri, GltfFixtureFormat::Glb})
        {
            GltfFixtureBuilder f;
            const auto         result = importFixture(f, root, format);
            requireStaticImportSuccess(result);
            const auto& mesh      = result.asset()->meshes[0];
            const auto& primitive = mesh.primitives[0];
            require(mesh.name == "triangle" && primitive.indices == std::vector<uint32_t>({0, 1, 2}),
                    "minimal geometry");
            require(std::get<std::vector<glm::vec3>>(
                        findVertexAttribute(primitive, GtsVertexSemantic::Position)->values)[1] == glm::vec3(1, 0, 0),
                    "positions");
            require(!findVertexAttribute(primitive, GtsVertexSemantic::Normal) &&
                        !findVertexAttribute(primitive, GtsVertexSemantic::Tangent) &&
                        !findVertexAttribute(primitive, GtsVertexSemantic::Color),
                    "absent streams stay absent");
        }
        for (uint32_t component : {5121, 5123, 5125})
        {
            GltfFixtureBuilder   f;
            std::vector<uint8_t> indices;
            for (uint32_t index : {2, 1, 0})
                for (size_t byte = 0; byte < (component == 5121 ? 1 : component == 5123 ? 2 : 4); ++byte)
                    indices.push_back(uint8_t(index >> (byte * 8)));
            field(f.primitive(), "indices") = f.addAccessor(indices, "SCALAR", component);
            auto result                     = importFixture(f, root, GltfFixtureFormat::Glb);
            requireStaticImportSuccess(result);
            require(result.asset()->meshes[0].primitives[0].indices == std::vector<uint32_t>({2, 1, 0}),
                    "index encodings");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            f.addVertexStream("TANGENT", floats({1, 0, 0, 1, 1, 0, 0, -1, 1, 0, 0, 1}), "VEC4", 5126);
            f.addTexCoordStream();
            f.addTexCoordStream("TEXCOORD_1");
            f.addTexCoordStream("TEXCOORD_2");
            f.addVertexStream("COLOR_0", {255, 128, 0, 255, 0, 255, 128, 255, 0, 0, 255, 255}, "VEC4", 5121, true);
            f.addVertexStream("COLOR_1", {255, 0, 128, 0, 0, 255, 128, 0, 0, 0, 255, 0}, "VEC3", 5121, true, 4);
            for (const auto set : {"0", "1"})
            {
                f.addVertexStream(std::string("JOINTS_") + set, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, "VEC4", 5121);
                f.addVertexStream(
                    std::string("WEIGHTS_") + set, {255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0}, "VEC4", 5121, true);
            }
            const auto result = importFixture(f, root);
            requireStaticImportSuccess(result);
            const auto& p = result.asset()->meshes[0].primitives[0];
            require(std::get<std::vector<glm::vec4>>(findVertexAttribute(p, GtsVertexSemantic::Tangent)->values)[0].w ==
                        -1,
                    "tangent handedness");
            require(
                std::get<std::vector<glm::vec2>>(findVertexAttribute(p, GtsVertexSemantic::TexCoord, 1)->values)[0].y ==
                    0.75f,
                "UV1 V convention");
            require(std::get<std::vector<glm::vec4>>(findVertexAttribute(p, GtsVertexSemantic::Color)->values)[0].g ==
                        128.0f / 255,
                    "normalized color");
            require(
                std::get<std::vector<glm::vec4>>(findVertexAttribute(p, GtsVertexSemantic::Color, 1)->values)[0].a == 1,
                "RGB expands alpha");
            require(
                std::get<std::vector<glm::uvec4>>(findVertexAttribute(p, GtsVertexSemantic::Joints, 1)->values)[2].w ==
                    11,
                "numbered joint streams");
            require(
                std::get<std::vector<glm::vec4>>(findVertexAttribute(p, GtsVertexSemantic::Weights, 1)->values)[0].x ==
                    1,
                "numbered weights");
        }
        {
            GltfFixtureBuilder f;
            f.bin                     = floats({0, 0, 0, 0, 0.25f, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1});
            auto& view                = at(field(f.root, "bufferViews"), 0);
            field(view, "byteLength") = 60u;
            field(view, "byteStride") = 20u;
            Json uv                   = at(field(f.root, "accessors"), 0);
            field(uv, "type")         = "VEC2";
            field(uv, "byteOffset")   = 12u;
            std::get<Array>(field(f.root, "accessors").value).push_back(uv);
            field(f.attributes(), "TEXCOORD_0") = 1u;
            const auto result                   = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(std::get<std::vector<glm::vec2>>(
                        findVertexAttribute(result.asset()->meshes[0].primitives[0], GtsVertexSemantic::TexCoord)
                            ->values)[0]
                            .y == 0.75f,
                    "interleaved accessor");
        }
        {
            GltfFixtureBuilder f;
            f.setTexturedMaterial();
            Json second = f.primitive();
            std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(second);
            Json mesh           = at(field(f.root, "meshes"), 0);
            field(mesh, "name") = "second";
            std::get<Array>(field(f.root, "meshes").value).push_back(mesh);
            field(f.root, "nodes") = parse(
                R"([{"name":"excluded","mesh":0},{"name":"parent","children":[2],"translation":[1,2,3],"rotation":[0,0,1,0],"scale":[2,3,4]},{"name":"child","mesh":1,"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1]}])");
            field(f.root, "scenes") = parse(R"([{"nodes":[0]},{"nodes":[1]}])");
            field(f.root, "scene")  = 1u;
            auto result             = importFixture(f, root, GltfFixtureFormat::Glb);
            requireStaticImportSuccess(result);
            const auto& model = *result.asset();
            require(model.meshes.size() == 2 && model.meshes[0].primitives.size() == 2, "mesh primitive boundaries");
            require(model.nodes.size() == 2 && model.rootNodes == std::vector<uint32_t>({0}) &&
                        model.nodes[0].children == std::vector<uint32_t>({1}),
                    "selected scene remapping");
            require(model.nodes[1].meshIndex == 1 && model.nodes[1].localTransform[3] == glm::vec4(4, 5, 6, 1),
                    "matrix mesh reference");
            require(model.nodes[0].localTransform[3] == glm::vec4(1, 2, 3, 1) &&
                        model.nodes[0].localTransform[0][0] == -2,
                    "TRS order");
            const auto& m = model.materials[0];
            require(m.baseColor == glm::vec4(.2f, .4f, .6f, .8f) && m.metallic == .3f && m.roughness == .7f,
                    "PBR factors");
            require(m.normalScale == .6f && m.ambientOcclusionStrength == .4f && m.emissiveStrength == 4 &&
                        m.emissiveFactor == glm::vec3(.1f, .2f, .3f),
                    "appearance factors");
            require(m.metallicImage->image.imageIndex == m.roughnessImage->image.imageIndex &&
                        m.metallicImage->channel == GtsModelTextureChannel::Blue &&
                        m.roughnessImage->channel == GtsModelTextureChannel::Green,
                    "metal rough mapping");
            require(m.ambientOcclusionImage->channel == GtsModelTextureChannel::Red &&
                        m.baseColorImage->texCoordSet == 1,
                    "AO UV selection");
            require(m.alphaMode == GtsModelAlphaMode::Mask && m.alphaCutoff == .2f && m.doubleSided, "surface state");
            require(std::get<GtsModelEmbeddedImage>(model.images[0].source).bytes == std::vector<uint8_t>({1, 2, 3, 4}),
                    "data URI image encoded bytes");
            erase(f.root, "scene");
            result = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(result.asset()->nodes.size() == 1 && hasDiagnostic(result, "GLTF_SCENE_DEFAULT"),
                    "defaultless scene zero");
            erase(f.root, "scenes");
            result = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(result.asset()->nodes.size() == 3 && result.asset()->rootNodes.size() == 2 &&
                        hasDiagnostic(result, "GLTF_SCENE_LIBRARY"),
                    "scene-free library");
        }
        {
            GltfFixtureBuilder f;
            f.setTexturedMaterial();
            field(at(field(f.root, "images"), 0), "uri") = "external%20image.png";
            auto result                                  = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(std::get<std::filesystem::path>(result.asset()->images[0].source) == root / "external image.png" &&
                        hasDiagnostic(result, "GLTF_IMAGE_MISSING"),
                    "external image identity");
            Json image = at(field(f.root, "images"), 0);
            std::get<Array>(field(f.root, "images").value).push_back(image);
            std::get<Array>(field(f.root, "textures").value).push_back(parse(R"({"source":1,"sampler":0})"));
            field(f.root, "samplers") = parse(R"([{"wrapS":33071}])");
            result                    = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(result.asset()->images.size() == 1 && hasDiagnostic(result, "GLTF_SAMPLER_UNSUPPORTED"),
                    "dedup and sampler diagnostic");
            field(f.root, "images")                             = parse("[{}]");
            field(f.root, "textures")                           = parse(R"([{"source":0}])");
            const auto view                                     = f.addBufferView({1, 2, 3, 4});
            field(at(field(f.root, "images"), 0), "bufferView") = view;
            field(at(field(f.root, "images"), 0), "mimeType")   = "image/png";
            result                                              = importFixture(f, root, GltfFixtureFormat::Glb);
            requireStaticImportSuccess(result);
            require(std::get<GtsModelEmbeddedImage>(result.asset()->images[0].source).mimeType == "image/png",
                    "GLB image view");
        }
        {
            GltfFixtureBuilder f;
            auto               result = importFixture(f, root);
            requireStaticImportSuccess(result);
            const auto materialIndex = result.asset()->meshes[0].primitives[0].materialIndex;
            require(materialIndex && result.asset()->materials[*materialIndex].metallic == 1,
                    "implicit glTF material is resolved above the format wall");
            erase(f.root, "nodes");
            erase(f.root, "scenes");
            erase(f.root, "scene");
            result = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(result.asset()->nodes.empty() && result.asset()->meshes.size() == 1, "mesh-only library");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("TEXCOORD_0", {0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0}, "VEC2", 5123, true);
            std::vector<uint8_t> joints;
            for (int i = 0; i < 3; ++i)
                joints.insert(joints.end(), {0xfe, 0xff, 0, 0, 0, 0, 0, 0});
            f.addVertexStream("JOINTS_0", joints, "VEC4", 5123);
            f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
            const auto result = importFixture(f, root);
            requireStaticImportSuccess(result);
            const auto& primitive = result.asset()->meshes[0].primitives[0];
            require(
                std::get<std::vector<glm::uvec4>>(findVertexAttribute(primitive, GtsVertexSemantic::Joints)->values)[0]
                        .x == 65534,
                "ushort joints stay integers");
            require(
                std::get<std::vector<glm::vec2>>(findVertexAttribute(primitive, GtsVertexSemantic::TexCoord)->values)[0]
                        .y == 0,
                "normalized ushort UVs");
        }
        {
            GltfFixtureBuilder f;
            f.setTexturedMaterial();
            field(at(field(f.root, "materials"), 0), "alphaMode") = "BLEND";
            auto result                                           = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(result.asset()->materials[0].alphaMode == GtsModelAlphaMode::Blend, "blend material");
            field(at(field(f.root, "materials"), 0), "alphaMode") = "invalid";
            requireImportFailure(f, root, "GLTF_ALPHA_MODE");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "indices") = f.addAccessor({0, 1, 2}, "SCALAR", 5121, 3, true);
            requireImportFailure(f, root, "GLTF_INDEX_TYPE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "nodes"), 0), "matrix")      = parse("[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]");
            field(at(field(f.root, "nodes"), 0), "translation") = parse("[0,0,0]");
            requireImportFailure(f, root, "GLTF_NODE_TRANSFORM");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            field(at(field(f.root, "bufferViews"), 1), "byteLength") = 24u;
            requireImportFailure(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(f.attributes(), "_CUSTOM") = 0u;
            requireImportFailure(f, root, "GLTF_ATTRIBUTE_UNSUPPORTED");
        }
        {
            GltfFixtureBuilder f;
            erase(at(field(f.root, "accessors"), 0), "bufferView");
            requireImportFailure(f, root, "GLTF_ACCESSOR_SOURCE_UNSUPPORTED");
        }
        // Decoder and source-shape failures must never expose a partial canonical asset.
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "accessors"), 0), "count") = UINT32_MAX;
            requireImportFailure(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "bufferViews"), 0), "byteLength") = 35u;
            requireImportFailure(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "bufferViews"), 0), "byteLength") = UINT32_MAX;
            requireImportFailure(f, root, "GLTF_VIEW_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "accessors"), 0), "byteOffset") = UINT32_MAX;
            requireImportFailure(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "accessors"), 0), "sparse") = parse("{}");
            requireImportFailure(f, root, "GLTF_SPARSE_UNSUPPORTED");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "indices") = f.addAccessor(floats({0, 1, 2}), "SCALAR", 5126);
            requireImportFailure(f, root, "GLTF_INDEX_TYPE");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "indices") = f.addAccessor({0, 1, 1, 2, 2, 0}, "VEC2", 5121);
            requireImportFailure(f, root, "GLTF_INDEX_TYPE");
        }
        {
            GltfFixtureBuilder   f;
            std::vector<uint8_t> bytes;
            for (uint32_t i : {0u, 1u, 16777217u})
                append32(bytes, i);
            field(f.primitive(), "indices") = f.addAccessor(bytes, "SCALAR", 5125);
            requireImportFailure(f, root, "GLTF_INDEX_RANGE");
        }
        {
            GltfFixtureBuilder f;
            field(f.attributes(), "NORMAL") = 999u;
            requireImportFailure(f, root, "GLTF_ACCESSOR_REFERENCE");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("COLOR_0", floats({0, 1, 0, 1, 0, 1}), "VEC2", 5126);
            requireImportFailure(f, root, "GLTF_ATTRIBUTE_TYPE");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            field(at(field(f.root, "accessors"), 1), "count") = 2u;
            requireImportFailure(f, root, "GLTF_ATTRIBUTE_COUNT");
        }
        {
            GltfFixtureBuilder f;
            f.addTexCoordStream("TEXCOORD_1");
            requireImportFailure(f, root, "GLTF_ATTRIBUTE_SETS");
        }
        {
            GltfFixtureBuilder f;
            f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12), "VEC4", 5121);
            requireImportFailure(f, root, "GLTF_INFLUENCE_PAIR");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "mode") = 5u;
            requireImportFailure(f, root, "GLTF_TOPOLOGY_UNSUPPORTED");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "targets") = parse("[{}]");
            requireImportFailure(f, root, "GLTF_MORPH_UNSUPPORTED");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "animations") = parse("[{}]");
            requireImportFailure(f, root, "GLTF_ANIMATION_EMPTY");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "skins")                       = parse("[{}]");
            field(at(field(f.root, "nodes"), 0), "skin") = 0u;
            requireImportFailure(f, root, "GLTF_SKIN_JOINTS_EMPTY");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "nodes"), 0), "children") = parse("[4]");
            requireImportFailure(f, root, "GLTF_CHILD_REFERENCE");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "nodes") = parse(R"([{"children":[1,1]},{}])");
            requireImportFailure(f, root, "GLTF_MULTIPLE_PARENT");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "nodes") = parse(R"([{"children":[2]},{"children":[2]},{}])");
            requireImportFailure(f, root, "GLTF_MULTIPLE_PARENT");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "nodes") = parse(R"([{"children":[1]},{"children":[0]}])");
            requireImportFailure(f, root, "GLTF_NODE_CYCLE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "nodes"), 0), "mesh") = 10u;
            requireImportFailure(f, root, "GLTF_MESH_REFERENCE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "meshes"), 0), "primitives") = parse("[]");
            requireImportFailure(f, root, "GLTF_MESH_EMPTY");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "scene") = 3u;
            requireImportFailure(f, root, "GLTF_SCENE_REFERENCE");
        }
        {
            GltfFixtureBuilder f;
            field(at(field(f.root, "scenes"), 0), "nodes") = parse("[0,0]");
            requireImportFailure(f, root, "GLTF_SCENE_ROOT");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "extensionsRequired") = parse(R"(["KHR_draco_mesh_compression"])");
            requireImportFailure(f, root, "GLTF_REQUIRED_EXTENSION");
        }
        {
            GltfFixtureBuilder f;
            field(f.root, "extensionsUsed") = parse(R"(["TEST_unknown"])");
            auto result                     = importFixture(f, root);
            requireStaticImportSuccess(result);
            require(hasDiagnostic(result, "GLTF_OPTIONAL_EXTENSION"), "optional extension warning");
        }
        {
            GltfFixtureBuilder f;
            field(f.primitive(), "material") = 5u;
            auto result                      = importFixture(f, root);
            require(!result.succeeded() && !result.asset(), "canonical validation gates invalid material");
        }
        {
            GltfFixtureBuilder f;
            f.setTexturedMaterial();
            erase(f.attributes(), "TEXCOORD_1");
            auto result = importFixture(f, root);
            require(!result.succeeded(), "canonical material UV validation");
        }
        {
            for (int change = 0; change < 4; ++change)
            {
                GltfFixtureBuilder   f;
                const auto           path = f.write(root, GltfFixtureFormat::Glb);
                std::ifstream        file(path, std::ios::binary);
                std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
                file.close();
                if (change == 0)
                    bytes[8] ^= 4; // total length
                if (change == 1)
                    bytes[16] = 0; // first chunk type
                if (change == 2)
                    bytes[12] |= 1; // chunk alignment
                if (change == 3)
                {
                    bytes.push_back(0);
                    const auto size = uint32_t(bytes.size());
                    for (int i = 0; i < 4; ++i)
                        bytes[8 + i] = uint8_t(size >> (i * 8));
                }
                writeBytes(path, bytes);
                const auto result = GtsGltfModelImporter{}.importAsset({path});
                require(!result.succeeded(), "malformed GLB structure");
            }
        }
        {
            GltfFixtureBuilder f;
            f.addTexCoordStream();
            const auto a = importFixture(f, root, GltfFixtureFormat::External),
                       b = importFixture(f, root, GltfFixtureFormat::Glb);
            requireStaticImportSuccess(a);
            requireStaticImportSuccess(b);
            const auto& pa = a.asset()->meshes[0].primitives[0];
            const auto& pb = b.asset()->meshes[0].primitives[0];
            require(pa.indices == pb.indices && pa.attributes.size() == pb.attributes.size(),
                    "equivalent canonical geometry");
            for (size_t i = 0; i < pa.attributes.size(); ++i)
                require(pa.attributes[i].values == pb.attributes[i].values, "deterministic attribute values");
            require(a.asset()->rootNodes == b.asset()->rootNodes &&
                        a.asset()->nodes[0].localTransform == b.asset()->nodes[0].localTransform,
                    "deterministic nodes");
        }
        std::filesystem::remove_all(root);
        std::puts("GtsGltfModelImporterTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
