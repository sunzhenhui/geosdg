/**
 * @file MyIndicator.h
 * @brief Example custom SDG indicator plugin
 *
 * This is a skeleton plugin that demonstrates how to implement the
 * IIndicator interface. It calculates a simple forest coverage ratio
 * from a land use/cover map.
 *
 * Build:
 *   1. Copy this directory to your plugins folder
 *   2. Build the shared library:
 *      cmake -B build && cmake --build build
 *   3. The library (.so/.dylib/.dll) should be in the same directory as plugin.json
 */

#pragma once

#include "IIndicator.h"

#include <map>
#include <string>
#include <functional>

// ============================================================================
// ForestCoverageIndicator
// ============================================================================

/**
 * @class ForestCoverageIndicator
 * @brief Example plugin: calculates forest coverage ratio
 *
 * Reads a GeoTIFF land use/cover map, counts pixels matching the
 * specified forest type codes, and returns the ratio as a score.
 */
class ForestCoverageIndicator : public IIndicator {
public:
    ForestCoverageIndicator();
    ~ForestCoverageIndicator() override = default;

    int init(const std::map<std::string, std::string>& params) override;
    int execute() override;
    std::string getResult() const override;
    std::string getError() const override;
    void setLogger(std::function<void(const std::string&)> logger) override;
    std::string getVersion() const override { return "0.1.0"; }
    std::string getAuthor() const override { return "Example Author"; }

private:
    std::string luccPath_;           ///< Path to land use/cover map
    std::vector<int> forestTypes_;   ///< Forest type codes
    double minArea_;                 ///< Minimum area threshold
    std::string result_;             ///< Computed result
    std::string error_;              ///< Error message
    std::function<void(const std::string&)> logger_; ///< Logger callback

    /**
     * @brief Parse comma-separated int list ("1,2,3" → [1,2,3])
     */
    static std::vector<int> parseIntList(const std::string& s);
};
