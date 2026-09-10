/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "BundledScenarios.h"

#include <filesystem>
#include <openrct2/Diagnostic.h>
#include <openrct2/PlatformEnvironment.h>

namespace OpenRCT2::Ui::Accessibility
{
    void InstallBundledScenarios(IPlatformEnvironment& env)
    {
        try
        {
            // DirBase::openrct2 resolves to the data root - where g2.dat, language\ and object\ live -
            // so do NOT append DirId::data, which would add a second "data" segment. The build stages
            // the maps into data\scenarios (see CopyAccessMaps in openrct2.targets).
            const auto source = std::filesystem::u8path(env.GetDirectoryPath(DirBase::openrct2)) / "scenarios";
            if (!std::filesystem::is_directory(source))
                return; // nothing bundled with this build

            const auto destination = std::filesystem::u8path(env.GetDirectoryPath(DirBase::user, DirId::scenarios));
            std::filesystem::create_directories(destination);

            for (const auto& entry : std::filesystem::directory_iterator(source))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".park")
                    continue;

                const auto target = destination / entry.path().filename();
                if (std::filesystem::exists(target))
                    continue; // already installed, or the player has their own version of it

                std::filesystem::copy_file(entry.path(), target);
                LOG_INFO("Accessibility: installed bundled map '%s'", entry.path().filename().u8string().c_str());
            }
        }
        catch (const std::exception& e)
        {
            LOG_WARNING("Accessibility: could not install the bundled maps: %s", e.what());
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
