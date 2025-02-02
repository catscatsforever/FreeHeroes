/*
 * Copyright (C) 2022 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */

#include <iostream>

#include "CoreApplication.hpp"
#include "MernelPlatform/CommandLineUtils.hpp"

#include "MapConverter.hpp"

using namespace FreeHeroes;
using namespace Mernel;

int main(int argc, char** argv)
{
    AbstractCommandLine parser({
                                   "input-fhMap",
                                   "input-fhTpl",
                                   "input-h3m",
                                   "input-h3svg",
                                   "input-diff-json",
                                   "output-fhMap",
                                   "output-fhTpl",
                                   "output-h3m",
                                   "output-h3svg",
                                   "output-diff-json",
                                   "dump-uncompressed",
                                   "dump-json",
                                   "seed",
                                   "rng-settings-file",
                                   "stopAfterStage",
                               },
                               { "tasks" });
    parser.markRequired({ "tasks" });
    if (!parser.parseArgs(std::cerr, argc, argv)) {
        std::cerr << "Map utils invocation failed, correct usage is:\n";
        std::cerr << parser.getHelp();
        return 1;
    }

    const auto        tasks            = parser.getMultiArg("tasks");
    const bool        dumpUncompressed = parser.getArg("dump-uncompressed") == "1";
    const bool        dumpJson         = parser.getArg("dump-json") == "1";
    const std::string diffJsonFile     = parser.getArg("diff-json-file");
    const std::string seedStr          = parser.getArg("seed");
    const std::string stopAfterStage   = parser.getArg("stopAfterStage");
    const std::string rngUserSettings  = parser.getArg("rng-settings-file");
    const uint64_t    seed             = std::strtoull(seedStr.c_str(), nullptr, 10);

    Core::CoreApplication fhCoreApp;
    fhCoreApp.initLogger();
    if (!fhCoreApp.load())
        return 1;

    auto makePaths = [&parser](const std::string& prefix) -> MapConverter::PathsSet {
        const std::string fhMap      = parser.getArg(prefix + "fhMap");
        const std::string fhTemplate = parser.getArg(prefix + "fhTpl");
        const std::string h3m        = parser.getArg(prefix + "h3m");
        const std::string h3sav      = parser.getArg(prefix + "h3svg");
        const std::string diffjson   = parser.getArg(prefix + "diff-json");

        const std::string h3mUncompressed = h3m.empty() ? "" : h3m + ".uncompressed";
        const std::string h3mJson         = h3m.empty() ? "" : h3m + ".json";

        const std::string h3savUncompressed = h3sav.empty() ? "" : h3sav + ".uncompressed";
        const std::string h3savJson         = h3sav.empty() ? "" : h3sav + ".json";

        MapConverter::PathsSet result;
        result.m_fhMap      = fhMap;
        result.m_fhTemplate = fhTemplate;
        result.m_h3m        = { .m_binary = h3m, .m_uncompressedBinary = h3mUncompressed, .m_json = h3mJson };
        result.m_h3svg      = { .m_binary = h3sav, .m_uncompressedBinary = h3savUncompressed, .m_json = h3savJson };
        result.m_jsonDiff   = diffjson;
        return result;
    };

    MapConverter converter(std::cerr,
                           fhCoreApp.getDatabaseContainer(),
                           fhCoreApp.getRandomGeneratorFactory(),
                           MapConverter::Settings{
                               .m_inputs                  = makePaths("input-"),
                               .m_outputs                 = makePaths("output-"),
                               .m_dumpUncompressedBuffers = dumpUncompressed,
                               .m_dumpBinaryDataJson      = dumpJson,
                               .m_seed                    = seed,
                               .m_rngUserSettings         = rngUserSettings,
                               .m_stopAfterStage          = stopAfterStage,
                           });

    for (const std::string& taskStr : tasks) {
        const MapConverter::Task task = stringToTask(taskStr);
        if (task == MapConverter::Task::Invalid) {
            std::cerr << "Unknown task: " << taskStr << "\n";
            return 1;
        }
        try {
            converter.run(task);
        }
        catch (std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return 1;
        }
    }

    return 0;
}
