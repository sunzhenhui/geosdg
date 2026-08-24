/**
 * @file PluginManager.h
 * @brief Plugin lifecycle management: scan, load, execute, unload
 *
 * PluginManager discovers plugin.json files in configured directories,
 * loads the corresponding dynamic libraries, and provides a unified
 * interface for listing and running plugins.
 */

#pragma once

#include "PluginJsonParser.h"
#include "IIndicator.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Platform-specific dynamic library handle
// ============================================================================

#if defined(_WIN32)
    using LibHandle = void*; // HMODULE
#else
    using LibHandle = void*; // dlopen handle
#endif

// ============================================================================
// BuiltinIndicatorAdapter (must be defined before LoadedPlugin)
// ============================================================================

/**
 * @class BuiltinIndicatorAdapter
 * @brief Adapter that wraps built-in SDG calculations as IIndicator instances
 *
 * Built-in indicators (CalculateSDG, ExtractPriorityAreas, etc.) are not
 * dynamically loaded but are wrapped to conform to the IIndicator interface
 * so that PluginManager can treat them uniformly.
 */
class BuiltinIndicatorAdapter : public IIndicator {
public:
    /**
     * @typedef ExecuteFunc
     * @brief Function type for built-in indicator execution
     * @param params Parsed CLI parameters
     * @param result Output result string
     * @param error Output error message
     * @return 0 on success, non-zero on failure
     */
    typedef std::function<int(const std::map<std::string, std::string>& params,
                               std::string& result, std::string& error)> ExecuteFunc;

    /**
     * @brief Constructor
     * @param name Indicator name
     * @param version Version string
     * @param author Author string
     * @param execFunc Execution function
     */
    BuiltinIndicatorAdapter(const std::string& name,
                            const std::string& version,
                            const std::string& author,
                            ExecuteFunc execFunc);

    int init(const std::map<std::string, std::string>& params) override;
    int execute() override;
    std::string getResult() const override;
    std::string getError() const override;
    void setLogger(std::function<void(const std::string&)> logger) override;
    std::string getVersion() const override;
    std::string getAuthor() const override;

private:
    std::string name_;
    std::string version_;
    std::string author_;
    ExecuteFunc execFunc_;
    std::map<std::string, std::string> params_;
    std::string result_;
    std::string error_;
    std::function<void(const std::string&)> logger_;
};

// ============================================================================
// LoadedPlugin
// ============================================================================

/**
 * @struct LoadedPlugin
 * @brief Represents a loaded (or failed-to-load) plugin
 */
struct LoadedPlugin {
    enum class Status {
        NotLoaded,   ///< Descriptor parsed but library not yet loaded
        Loaded,      ///< Successfully loaded and ready
        Error,       ///< Failed to load (error in errorMsg)
        Builtin      ///< Built-in plugin (no dynamic library)
    };

    PluginDescriptor descriptor;  ///< Parsed plugin.json descriptor
    Status           status;      ///< Current load status
    std::string      errorMsg;    ///< Error message if status == Error
    LibHandle        handle;      ///< Dynamic library handle (nullptr if builtin/error)
    CreateIndicatorFunc   createFunc;   ///< Factory function (nullptr if builtin/error)
    DestroyIndicatorFunc  destroyFunc;  ///< Destroy function (nullptr if builtin/error)
    bool              isBuiltin;  ///< Whether this is a built-in plugin
    BuiltinIndicatorAdapter::ExecuteFunc builtinExecFunc; ///< Exec function for builtins

    /**
     * @brief Default constructor
     */
    LoadedPlugin() : status(Status::NotLoaded), handle(nullptr),
                     createFunc(nullptr), destroyFunc(nullptr), isBuiltin(false) {}
};

// ============================================================================
// PluginManager
// ============================================================================

/**
 * @class PluginManager
 * @brief Manages plugin discovery, loading, and execution
 *
 * PluginManager scans one or more directories for plugin.json files,
 * loads the corresponding dynamic libraries, and provides methods to
 * list plugins, get plugin info, and execute plugin commands.
 */
class PluginManager {
public:
    /**
     * @brief Default constructor
     */
    PluginManager();

    /**
     * @brief Destructor — unloads all loaded plugins
     */
    ~PluginManager();

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * @brief Add a plugin search directory
     * @param dir Path to directory containing plugin subdirectories
     */
    void addPluginDir(const std::string& dir);

    /**
     * @brief Set the current CLI version (for min_cli_version checking)
     * @param version CLI version string (e.g., "1.0.0")
     */
    void setCliVersion(const std::string& version);

    // ============================================================================
    // Built-in Plugin Registration
    // ============================================================================

    /**
     * @brief Register a built-in indicator as a plugin
     * @param desc Plugin descriptor (with isBuiltin=true semantics)
     * @param execFunc Execution function for this indicator
     */
    void registerBuiltin(const PluginDescriptor& desc,
                         BuiltinIndicatorAdapter::ExecuteFunc execFunc);

    // ============================================================================
    // Discovery & Loading
    // ============================================================================

    /**
     * @brief Scan all configured plugin directories and load plugins
     *
     * Scans each directory for subdirectories containing plugin.json,
     * parses the descriptor, and loads the dynamic library.
     * Failed plugins are recorded with Error status.
     */
    void discoverAndLoad();

    // ============================================================================
    // Query
    // ============================================================================

    /**
     * @brief Get all loaded plugins (including builtins and errors)
     * @return Const reference to the plugin list
     */
    const std::vector<LoadedPlugin>& getPlugins() const;

    /**
     * @brief Find a plugin by subcommand name
     * @param name Plugin subcommand name
     * @return Pointer to LoadedPlugin, or nullptr if not found
     */
    const LoadedPlugin* findPlugin(const std::string& name) const;

    /**
     * @brief Check if a command name corresponds to a loaded plugin
     * @param name Command name
     * @return true if a plugin with this name exists
     */
    bool hasPlugin(const std::string& name) const;

    // ============================================================================
    // Execution
    // ============================================================================

    /**
     * @brief Execute a plugin by name
     * @param name Plugin subcommand name
     * @param args Parsed CLI arguments
     * @param result Output: result string from plugin
     * @param error Output: error message on failure
     * @return 0 on success, non-zero on failure
     */
    int executePlugin(const std::string& name,
                      const std::map<std::string, std::string>& args,
                      std::string& result,
                      std::string& error);

    // ============================================================================
    // Listing & Info
    // ============================================================================

    /**
     * @brief Print a formatted list of all plugins to stdout
     */
    void printPluginList() const;

    /**
     * @brief Print detailed info about a specific plugin
     * @param name Plugin name
     * @return true if plugin found, false otherwise
     */
    bool printPluginInfo(const std::string& name) const;

    /**
     * @brief Generate help text for a plugin's CLI parameters
     * @param name Plugin name
     * @return Help text string
     */
    std::string generateHelpText(const std::string& name) const;

private:
    // ============================================================================
    // Internal Helpers
    // ============================================================================

    /**
     * @brief Scan a single directory for plugins
     * @param dir Directory path
     */
    void scanDirectory(const std::string& dir);

    /**
     * @brief Load a plugin's dynamic library
     * @param plugin LoadedPlugin to populate
     */
    void loadLibrary(LoadedPlugin& plugin);

    /**
     * @brief Unload a plugin's dynamic library
     * @param plugin LoadedPlugin to clean up
     */
    void unloadLibrary(LoadedPlugin& plugin);

    /**
     * @brief Build the platform-specific library file path
     * @param libName Library name (without prefix/suffix)
     * @return Full file path
     */
    std::string buildLibPath(const std::string& libName) const;

    /**
     * @brief Compare two version strings (semver-like)
     * @return -1 if a < b, 0 if equal, 1 if a > b
     */
    static int compareVersions(const std::string& a, const std::string& b);

    // ============================================================================
    // Data Members
    // ============================================================================

    std::vector<std::string> pluginDirs_;     ///< Plugin search directories
    std::string              cliVersion_;     ///< Current CLI version
    std::vector<LoadedPlugin> plugins_;       ///< All discovered/loaded plugins
    std::vector<std::pair<PluginDescriptor, BuiltinIndicatorAdapter::ExecuteFunc>> builtins_; ///< Pending builtin registrations
};
