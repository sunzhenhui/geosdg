/**
 * @file IIndicator.h
 * @brief Plugin SDK public interface for custom SDG indicators
 *
 * This header defines the IIndicator abstract base class that all custom
 * SDG indicator plugins must implement. Plugin developers include this
 * single header to build loadable indicator modules.
 *
 * Usage:
 *   1. Subclass IIndicator in your plugin
 *   2. Implement all pure virtual methods
 *   3. Export a C factory function: extern "C" IIndicator* createIndicator()
 *   4. Compile as shared library (.so / .dylib / .dll)
 *   5. Place library + plugin.json in the plugins directory
 */

#pragma once

#include <functional>
#include <map>
#include <string>

// ============================================================================
// IIndicator — Abstract Plugin Interface
// ============================================================================

/**
 * @class IIndicator
 * @brief Abstract base class for SDG indicator plugins
 *
 * Every custom indicator plugin must subclass IIndicator and implement
 * all pure virtual methods. The host (geosdg-cli) loads the plugin
 * dynamically, calls init() with parsed CLI arguments, then execute()
 * to run the computation, and finally getResult() to retrieve output.
 *
 * Lifecycle: create → init() → execute() → getResult()
 * On failure: getError() returns a human-readable message.
 */
class IIndicator {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IIndicator() = default;

    // ============================================================================
    // Lifecycle Methods
    // ============================================================================

    /**
     * @brief Initialize the indicator with parsed parameters
     * @param params Key-value map from CLI arguments (string representations)
     * @return 0 on success, non-zero on failure
     *
     * The host parses CLI arguments according to plugin.json and passes
     * them as string values. The plugin is responsible for type conversion
     * (e.g., std::stoi for integers, std::stod for floats).
     */
    virtual int init(const std::map<std::string, std::string>& params) = 0;

    /**
     * @brief Execute the indicator computation
     * @return 0 on success, non-zero on failure
     *
     * Called after init(). Perform all heavy computation here.
     * Results should be stored internally for getResult() to return.
     */
    virtual int execute() = 0;

    // ============================================================================
    // Result Accessors
    // ============================================================================

    /**
     * @brief Get the computation result as a string
     * @return Result string (e.g., numeric score, JSON, or formatted text)
     *
     * Only valid after execute() returns 0.
     */
    virtual std::string getResult() const = 0;

    /**
     * @brief Get the error message if init() or execute() failed
     * @return Error description string, empty if no error
     */
    virtual std::string getError() const = 0;

    // ============================================================================
    // Optional Hooks
    // ============================================================================

    /**
     * @brief Inject a logger callback from the host
     * @param logger Function that accepts a log message string
     *
     * The host injects its Logger so plugin messages appear in the
     * unified log output. If not called, plugins should fall back
     * to stderr or silent operation.
     */
    virtual void setLogger(std::function<void(const std::string&)> logger) = 0;

    /**
     * @brief Get the plugin version string
     * @return Version string (e.g., "1.0.0")
     *
     * Default implementation returns "unknown". Override to provide
     * a meaningful version that appears in list-plugins output.
     */
    virtual std::string getVersion() const { return "unknown"; }

    /**
     * @brief Get the plugin author
     * @return Author name string
     *
     * Default implementation returns "unknown". Override to provide
     * author information for list-plugins output.
     */
    virtual std::string getAuthor() const { return "unknown"; }
};

// ============================================================================
// Plugin Factory Function Type
// ============================================================================

/**
 * @typedef CreateIndicatorFunc
 * @brief Function pointer type for the plugin factory function
 *
 * Every plugin shared library must export a C-linkage function:
 * @code
 *   extern "C" IIndicator* createIndicator();
 * @endcode
 * The host calls this function to obtain a plugin instance.
 */
typedef IIndicator* (*CreateIndicatorFunc)();

/**
 * @typedef DestroyIndicatorFunc
 * @brief Function pointer type for the plugin destroy function
 *
 * Every plugin shared library must export a C-linkage function:
 * @code
 *   extern "C" void destroyIndicator(IIndicator* ptr);
 * @endcode
 * The host calls this function to properly destroy a plugin instance
 * through the plugin's own memory allocator.
 */
typedef void (*DestroyIndicatorFunc)(IIndicator*);
