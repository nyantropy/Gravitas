#include "GltfSkinImporter.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "GltfSourceReader.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/skeleton/GtsSkeletonAsset.h"

namespace gts::gltf
{
    namespace
    {
        struct SourceSkin
        {
            std::string             name;
            std::vector<uint32_t>   joints;
            std::set<uint32_t>      requiredNodes;
            std::optional<uint32_t> root;
            std::vector<glm::mat4>  inverseBinds;
            bool                    used = false;
        };

        bool intersects(const std::set<uint32_t>& a, const std::set<uint32_t>& b)
        {
            for (auto value : a)
                if (b.contains(value))
                    return true;
            return false;
        }
    } // namespace

    std::vector<std::shared_ptr<const GtsSkeletonAsset>>
    importSkins(const SourceDocument&                         data,
                GtsModelAsset&                                model,
                const std::vector<GtsSkeletonLocalTransform>& localTransforms,
                const std::vector<uint32_t>&                  nodeSkins,
                const std::vector<bool>&                      active,
                std::vector<GtsModelDiagnostic>&              diagnostics)
    {
        const auto& sources = array(data.root.find("skins"), "skins");
        if (sources.empty())
            return {};
        // the caller has already checked all parent references and cycles
        std::vector<uint32_t> parents(model.nodes.size(), UINT32_MAX);
        for (uint32_t i = 0; i < model.nodes.size(); ++i)
            for (auto child : model.nodes[i].children)
                parents[child] = i;

        std::vector<SourceSkin> skins(sources.size());
        for (size_t i = 0; i < sources.size(); ++i)
        {
            const auto  location = "skins[" + std::to_string(i) + "]";
            const auto& value    = sources[i];
            object(value, location);
            auto& skin         = skins[i];
            skin.name          = stringField(value, "name", location);
            const auto& joints = array(value.find("joints"), location + ".joints");
            if (joints.empty())
                fail("GLTF_SKIN_JOINTS_EMPTY", "Skin requires a nonempty joints list", location);
            std::set<uint32_t>      unique;
            std::optional<uint32_t> treeRoot;
            for (size_t slot = 0; slot < joints.size(); ++slot)
            {
                const auto jointLocation = location + ".joints[" + std::to_string(slot) + "]";
                auto       node          = integer(joints[slot], jointLocation);
                if (node >= model.nodes.size())
                    fail("GLTF_SKIN_JOINT_REFERENCE",
                         "Joint slot references invalid source node " + std::to_string(node),
                         jointLocation);
                if (!unique.insert(node).second)
                    fail("GLTF_SKIN_JOINT_DUPLICATE", "Source skin joints must be unique", jointLocation);
                skin.joints.push_back(node);
                while (true)
                {
                    skin.requiredNodes.insert(node);
                    if (parents[node] == UINT32_MAX)
                        break;
                    node = parents[node];
                }
                if (treeRoot && *treeRoot != node)
                    fail("GLTF_SKIN_ROOT", "Skin joints must share a common source ancestor", location);
                treeRoot = node;
            }
            if (value.find("skeleton"))
            {
                skin.root = uintField(value, "skeleton", location);
                if (*skin.root >= model.nodes.size())
                    fail("GLTF_SKIN_ROOT", "Invalid skeleton root node", location + ".skeleton");
                for (auto joint : skin.joints)
                {
                    while (joint != *skin.root && parents[joint] != UINT32_MAX)
                        joint = parents[joint];
                    if (joint != *skin.root)
                        fail("GLTF_SKIN_ROOT",
                             "Declared skeleton root must be an ancestor of every joint",
                             location + ".skeleton");
                }
            }
            skin.inverseBinds.assign(skin.joints.size(), glm::mat4(1));
            if (value.find("inverseBindMatrices"))
            {
                const auto index = uintField(value, "inverseBindMatrices", location);
                const auto accessorLocation =
                    location + ".inverseBindMatrices.accessors[" + std::to_string(index) + "]";
                const auto& accessor = data.accessor(index);
                if (accessor.type != "MAT4" || accessor.componentType != 5126 || accessor.normalized ||
                    data.views[accessor.view].stride || accessor.count < skin.joints.size())
                    fail("GLTF_SKIN_INVERSE_BIND_TYPE",
                         "Inverse binds require tightly packed MAT4 FLOAT data with at least one matrix per joint",
                         accessorLocation);
                for (size_t slot = 0; slot < skin.joints.size(); ++slot)
                {
                    auto& matrix = skin.inverseBinds[slot];
                    for (size_t column = 0; column < 4; ++column)
                        for (size_t row = 0; row < 4; ++row)
                        {
                            matrix[column][row] = data.floatComponent(accessor, slot, column * 4 + row);
                            if (!std::isfinite(matrix[column][row]))
                                fail("GLTF_SKIN_INVERSE_BIND_VALUE",
                                     "Inverse bind contains nonfinite data",
                                     accessorLocation + "[" + std::to_string(slot) + "]");
                        }
                    if (matrix[0][3] != 0 || matrix[1][3] != 0 || matrix[2][3] != 0 || matrix[3][3] != 1)
                        fail("GLTF_SKIN_INVERSE_BIND_VALUE",
                             "Inverse bind must have affine bottom row [0,0,0,1]",
                             accessorLocation + "[" + std::to_string(slot) + "]");
                }
            }
        }
        for (size_t node = 0; node < nodeSkins.size(); ++node)
        {
            if (!active[node] || nodeSkins[node] == UINT32_MAX)
                continue;
            auto& skin = skins[nodeSkins[node]];
            skin.used  = true;
            for (auto required : skin.requiredNodes)
                if (!active[required])
                    fail("GLTF_SKIN_SCENE",
                         "Selected skin's joint hierarchy must belong to the selected scene; required node " +
                             std::to_string(required) + " is outside it",
                         "nodes[" + std::to_string(node) + "].skin[" + std::to_string(nodeSkins[node]) + "]");
        }

        // merge overlapping joint sets, identical closures, or explicitly shared
        // rig roots - merely sharing a scene ancestor is not evidence of one rig
        std::vector<size_t> groups(skins.size());
        std::iota(groups.begin(), groups.end(), size_t{0});
        auto group = [&](size_t i)
        {
            while (groups[i] != i)
                i = groups[i];
            return i;
        };
        for (size_t i = 0; i < skins.size(); ++i)
        {
            if (!skins[i].used)
            {
                diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                                       "GLTF_SKIN_EXCLUDED",
                                       "Unused skin is validated but not emitted for this scene",
                                       "skins[" + std::to_string(i) + "]"});
                continue;
            }
            const std::set<uint32_t> joints(skins[i].joints.begin(), skins[i].joints.end());
            for (size_t j = 0; j < i; ++j)
            {
                if (!skins[j].used)
                    continue;
                const std::set<uint32_t> other(skins[j].joints.begin(), skins[j].joints.end());
                if (intersects(joints, other) || skins[i].requiredNodes == skins[j].requiredNodes ||
                    (skins[i].root && skins[i].root == skins[j].root))
                    groups[group(i)] = group(j);
            }
        }
        std::map<size_t, std::set<uint32_t>> requiredByGroup;
        for (size_t i = 0; i < skins.size(); ++i)
            if (skins[i].used)
                requiredByGroup[group(i)].insert(skins[i].requiredNodes.begin(), skins[i].requiredNodes.end());
        for (auto a = requiredByGroup.begin(); a != requiredByGroup.end(); ++a)
            for (auto b = std::next(a); b != requiredByGroup.end(); ++b)
                if (intersects(a->second, b->second))
                    diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                                           "GLTF_SKIN_GROUP_SEPARATE",
                                           "Disjoint joint subsets with no shared declared rig root remain separate "
                                           "despite common ancestors",
                                           "skins"});

        std::vector<uint32_t>    order;
        std::vector<std::string> paths(model.nodes.size());
        std::vector<uint32_t>    roots;
        for (uint32_t i = 0; i < model.nodes.size(); ++i)
            if (parents[i] == UINT32_MAX)
            {
                paths[i] = "root/" + std::to_string(roots.size());
                roots.push_back(i);
            }
        std::vector<uint32_t> pending(roots.rbegin(), roots.rend());
        while (!pending.empty())
        {
            const auto node = pending.back();
            pending.pop_back();
            order.push_back(node);
            const auto& children = model.nodes[node].children;
            for (size_t i = children.size(); i > 0; --i)
            {
                paths[children[i - 1]] = paths[node] + "/child/" + std::to_string(i - 1);
                pending.push_back(children[i - 1]);
            }
        }
        std::vector<std::shared_ptr<const GtsSkeletonAsset>> definitions;
        std::map<size_t, uint32_t>                           groupUses;
        std::map<size_t, std::map<uint32_t, uint32_t>>       nodeMappings;
        for (const auto& [rig, required] : requiredByGroup)
        {
            GtsSkeletonAsset skeleton;
            skeleton.name               = skins[rig].name;
            auto&               mapping = nodeMappings[rig];
            GtsModelSkeletonUse use;
            for (auto sourceNode : order)
            {
                if (!required.contains(sourceNode))
                    continue;
                GtsSkeletonNode node;
                node.id                    = {paths[sourceNode]};
                node.name                  = model.nodes[sourceNode].name;
                node.defaultLocalTransform = localTransforms[sourceNode];
                if (parents[sourceNode] != UINT32_MAX)
                    node.parentIndex = mapping.at(parents[sourceNode]);
                mapping.emplace(sourceNode, static_cast<uint32_t>(skeleton.nodes.size()));
                skeleton.nodes.push_back(std::move(node));
                use.modelNodeIndices.push_back(sourceNode);
            }
            // Derivation validates quaternion/matrix forms before publishing.
            const auto compatibility = makeGtsSkeletonCompatibility(skeleton);
            if (!compatibility.succeeded())
            {
                const auto& error = compatibility.diagnostics().front();
                fail("GLTF_SKIN_SKELETON_INVALID",
                     error.code + ": " + error.message,
                     "skins[" + std::to_string(rig) + "]." + error.location);
            }
            use.skeleton = std::make_shared<const GtsSkeletonAsset>(std::move(skeleton));
            definitions.push_back(use.skeleton);
            groupUses[rig] = static_cast<uint32_t>(model.skeletonUses.size());
            model.skeletonUses.push_back(std::move(use));
        }
        std::vector<uint32_t> bindings(skins.size(), UINT32_MAX);
        for (size_t i = 0; i < skins.size(); ++i)
        {
            if (!skins[i].used)
                continue;
            const auto          rig = group(i);
            GtsModelSkinBinding association;
            association.skeletonUseIndex = groupUses.at(rig);
            association.binding.name     = skins[i].name;
            const auto compatibility =
                makeGtsSkeletonCompatibility(*model.skeletonUses[association.skeletonUseIndex].skeleton);
            association.binding.targetSkeletonCompatibility = *compatibility.compatibility();
            for (size_t slot = 0; slot < skins[i].joints.size(); ++slot)
                association.binding.joints.push_back(
                    {nodeMappings.at(rig).at(skins[i].joints[slot]), skins[i].inverseBinds[slot]});
            bindings[i] = static_cast<uint32_t>(model.skinBindings.size());
            model.skinBindings.push_back(std::move(association));
        }
        for (size_t node = 0; node < nodeSkins.size(); ++node)
            if (active[node] && nodeSkins[node] != UINT32_MAX)
                model.nodes[node].skinBindingIndex = bindings[nodeSkins[node]];
        return definitions;
    }
} // namespace gts::gltf
