/**
 * @file PluginJsonParser.h
 * @brief Parser for plugin.json descriptor files
 *
 * Reads and validates plugin.json files, providing structured access
 * to plugin metadata, parameters, inputs, and outputs.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @struct PluginParameter
 * @brief Describes a single CLI parameter for a plugin
 */
struct PluginParameter {
    std::string name;         ///< Long name (e.g., "init-lucc")
    std::string shortName;    ///< Short name (e.g., "i"), may be empty
    std::string type;         ///< Type: "file", "float", "int", "string", "int_list"
    bool        required;     ///< Whether the parameter is mandatory
    std::string defaultValue; ///< Default value (string representation), empty if none
    std::string description;  ///< Human-readable description
};

/**
 * @struct PluginInput
 * @brief Describes an expected input data format
 */
struct PluginInput {
    std::string format;      ///< e.g., "GeoTIFF"
    std::string dtype;       ///< e.g., "GDT_Byte"
    int         bands;       ///< Number of bands expected
    std::string description; ///< Human-readable description
};

/**
 * @struct PluginOutput
 * @brief Describes an output data format
 */
struct PluginOutput {
    std::string format;      ///< e.g., "text", "GeoTIFF"
    std::string description; ///< Human-readable description
};

/**
 * @struct PluginDescriptor
 * @brief Complete parsed representation of a plugin.json file
 */
struct PluginDescriptor {
    std::string name;            ///< Plugin subcommand name (e.g., "sdg-custom-forest")
    std::string version;         ///< Plugin version string
    std::string author;          ///< Plugin author
    std::string description;     ///< Human-readable description
    std::string category;        ///< Category (e.g., "sdg-calc", "priority-area")
    std::string library;         ///< Library name without prefix/suffix (e.g., "libsdg_custom_forest")
    std::string className;       ///< C++ class name to instantiate
    std::string minCliVersion;   ///< Minimum CLI version required
    std::vector<PluginParameter> parameters; ///< CLI parameters
    std::vector<PluginInput>     inputs;     ///< Expected inputs
    std::vector<PluginOutput>    outputs;    ///< Expected outputs

    /**
     * @brief Find a parameter by long name
     * @param name Parameter long name
     * @return Pointer to parameter, or nullptr if not found
     */
    const PluginParameter* findParameter(const std::string& name) const;

    /**
     * @brief Check if all required parameters are present in the given args map
     * @param args Parsed CLI arguments
     * @param missing Output: names of missing required parameters
     * @return true if all required parameters are present
     */
    bool validateRequired(const std::map<std::string, std::string>& args,
                          std::vector<std::string>& missing) const;
};

// ============================================================================
// PluginJsonParser
// ============================================================================

/**
 * @class PluginJsonParser
 * @brief Parses plugin.json files into PluginDescriptor structures
 *
 * Uses a minimal hand-written JSON parser (no external dependencies)
 * to avoid adding nlohmann/json or similar to the CLI build.
 */
class PluginJsonParser {
public:
    /**
     * @brief Parse a plugin.json file from disk
     * @param filePath Path to the plugin.json file
     * @param desc Output descriptor (filled on success)
     * @param error Output: error message on failure
     * @return true on success, false on failure (error message in @p error)
     */
    static bool parseFile(const std::string& filePath,
                          PluginDescriptor& desc,
                          std::string& error);

    /**
     * @brief Parse a plugin.json string
     * @param jsonStr JSON content string
     * @param desc Output descriptor (filled on success)
     * @param error Output: error message on failure
     * @return true on success, false on failure
     */
    static bool parseString(const std::string& jsonStr,
                            PluginDescriptor& desc,
                            std::string& error);
};
