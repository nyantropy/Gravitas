#pragma once

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "AssetManifest.h"

namespace gts::tools
{
    struct AssetBrowserEntry
    {
        std::string manifestPath;
        AssetManifest manifest;
        std::string error;
        bool valid = false;
    };

    class AssetBrowserSession
    {
    public:
        void refresh(const std::vector<std::filesystem::path>& roots)
        {
            std::vector<std::filesystem::path> manifestPaths;
            std::set<std::string> seen;

            for (const std::filesystem::path& root : roots)
                scanRoot(root, manifestPaths, seen);

            std::sort(manifestPaths.begin(), manifestPaths.end());

            entries.clear();
            entries.reserve(manifestPaths.size());
            for (const std::filesystem::path& manifestPath : manifestPaths)
            {
                AssetBrowserEntry entry;
                entry.manifestPath = manifestPath.generic_string();
                entry.valid = loadAssetManifest(entry.manifestPath, entry.manifest, &entry.error);
                entries.push_back(std::move(entry));
            }

            clampSelection();
        }

        const std::vector<AssetBrowserEntry>& assets() const
        {
            return entries;
        }

        const AssetBrowserEntry* selected() const
        {
            if (entries.empty() || selectedEntry >= entries.size())
                return nullptr;
            return &entries[selectedEntry];
        }

        size_t selectedIndex() const
        {
            return selectedEntry;
        }

        const std::string& selectedManifestPath() const
        {
            return selectedPath;
        }

        void select(const std::string& manifestPath)
        {
            selectedPath = normalizedPathString(manifestPath);
            clampSelection();
        }

    private:
        std::vector<AssetBrowserEntry> entries;
        size_t selectedEntry = 0;
        std::string selectedPath;

        static void scanRoot(const std::filesystem::path& root,
                             std::vector<std::filesystem::path>& manifestPaths,
                             std::set<std::string>& seen)
        {
            std::error_code ec;
            if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().filename() == "asset.json")
                {
                    const std::filesystem::path path = stablePath(entry.path());
                    const std::string key = manifestKey(path);
                    if (seen.insert(key).second)
                        manifestPaths.push_back(path);
                }
                it.increment(ec);
            }
        }

        static std::filesystem::path stablePath(const std::filesystem::path& path)
        {
            std::error_code ec;
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
            if (!ec)
                return canonical;
            return path.lexically_normal();
        }

        static std::string normalizedPathString(const std::filesystem::path& path)
        {
            return path.lexically_normal().generic_string();
        }

        static std::string manifestKey(const std::filesystem::path& path)
        {
            std::vector<std::filesystem::path> parts;
            for (const std::filesystem::path& part : path.lexically_normal())
            {
                if (part == ".")
                    continue;
                parts.push_back(part);
            }

            for (size_t i = 0; i + 1u < parts.size(); ++i)
            {
                if (parts[i] == "resources" && parts[i + 1u] == "assets")
                {
                    std::filesystem::path relative;
                    for (size_t j = i + 2u; j < parts.size(); ++j)
                        relative /= parts[j];
                    return relative.generic_string();
                }
            }

            return normalizedPathString(path);
        }

        static bool pathMatchesSelection(const std::string& actual, const std::string& requested)
        {
            if (requested.empty())
                return false;

            const std::string actualPath = normalizedPathString(actual);
            const std::string requestedPath = normalizedPathString(requested);
            if (actualPath == requestedPath)
                return true;
            if (actualPath.size() <= requestedPath.size())
                return false;

            const size_t offset = actualPath.size() - requestedPath.size();
            if (actualPath.compare(offset, requestedPath.size(), requestedPath) != 0)
                return false;

            const char separator = actualPath[offset - 1u];
            return separator == '/' || separator == '\\';
        }

        void clampSelection()
        {
            if (entries.empty())
            {
                selectedEntry = 0;
                selectedPath.clear();
                return;
            }

            if (!selectedPath.empty())
            {
                const auto it = std::find_if(entries.begin(),
                                             entries.end(),
                                             [&](const AssetBrowserEntry& entry)
                                             {
                                                 return pathMatchesSelection(entry.manifestPath, selectedPath);
                                             });
                if (it != entries.end())
                {
                    selectedEntry = static_cast<size_t>(std::distance(entries.begin(), it));
                    return;
                }
            }

            selectedEntry = std::min(selectedEntry, entries.size() - 1u);
            selectedPath = entries[selectedEntry].manifestPath;
        }
    };
}
