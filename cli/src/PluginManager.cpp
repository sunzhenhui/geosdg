/**
 * @file PluginManager.cpp
 * @brief Implementation of plugin lifecycle management
 */

#include "PluginManager.h"
#include "Logger.h"

#include <iostream>
#include <filesystem>
#include <sstream>
#include <algorithm>

// ============================================================================
// Platform-specific dynamic library loading
// ============================================================================

#if defined(_WIN32)
    #include <windows.h>
    #define DLOPEN(name)       LoadLibraryA(name)
    #define DLCLOSE(handle)    FreeLibrary((HMODULE)(handle))
    #define DLSYM(handle, sym) GetProcAddress((HMODULE)(handle), sym)
    #define LIB_PREFIX         ""
    #define LIB_SUFFIX         ".dll"
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #define DLOPEN(name)       dlopen(name, RTLD_LAZY)
    #define DLCLOSE(handle)    dlclose(handle)
    #define DLSYM(handle, sym) dlsym(handle, sym)
    #define LIB_PREFIX         "lib"
    #define LIB_SUFFIX         ".dylib"
#else
    #include <dlfcn.h>
    #define DLOPEN(name)       dlopen(name, RTLD_LAZY)
    #define DLCLOSE(handle)    dlclose(handle)
    #define DLSYM(handle, sym) dlsym(handle, sym)
    #define LIB_PREFIX         "lib"
    #define LIB_SUFFIX         ".so"
#endif

// ============================================================================
// BuiltinIndicatorAdapter Implementation
// ============================================================================

BuiltinIndicatorAdapter::BuiltinIndicatorAdapter(const std::string& name,
                                                   const std::string& version,
                                                   const std::string& author,
                                                   ExecuteFunc execFunc)
    : name_(name), version_(version), author_(author), execFunc_(execFunc) {}

int BuiltinIndicatorAdapter::init(const std::map<std::string, std::string>& params) {
    params_ = params;
    return 0;
}

int BuiltinIndicatorAdapter::execute() {
    if (!execFunc_) {
        error_ = "No execution function set for builtin indicator: " + name_;
        return 1;
    }
    return execFunc_(params_, result_, error_);
}

std::string BuiltinIndicatorAdapter::getResult() const {
    return result_;
}

std::string BuiltinIndicatorAdapter::getError() const {
    return error_;
}

void BuiltinIndicatorAdapter::setLogger(std::function<void(const std::string&)> logger) {
    logger_ = logger;
}

std::string BuiltinIndicatorAdapter::getVersion() const {
    return version_;
}

std::string BuiltinIndicatorAdapter::getAuthor() const {
    return author_;
}

// ============================================================================
// PluginManager Implementation
// ============================================================================

PluginManager::PluginManager() : cliVersion_("1.0.0") {}

PluginManager::~PluginManager() {
    for (auto& plugin : plugins_) {
        if (plugin.status == LoadedPlugin::Status::Loaded && !plugin.isBuiltin) {
            unloadLibrary(plugin);
        }
    }
}

// ============================================================================
// Configuration
// ============================================================================

void PluginManager::addPluginDir(const std::string& dir) {
    pluginDirs_.push_back(dir);
}

void PluginManager::setCliVersion(const std::string& version) {
    cliVersion_ = version;
}

// ============================================================================
// Built-in Plugin Registration
// ============================================================================

void PluginManager::registerBuiltin(const PluginDescriptor& desc,
                                     BuiltinIndicatorAdapter::ExecuteFunc execFunc) {
    builtins_.push_back({desc, execFunc});
}

// ============================================================================
// Discovery & Loading
// ============================================================================

void PluginManager::discoverAndLoad() {
    // Register built-in plugins first
    for (const auto& pair : builtins_) {
        LoadedPlugin plugin;
        plugin.descriptor = pair.first;
        plugin.isBuiltin = true;
        plugin.status = LoadedPlugin::Status::Builtin;
        plugin.handle = nullptr;
        plugin.createFunc = nullptr;
        plugin.destroyFunc = nullptr;
        plugin.builtinExecFunc = pair.second;
        plugins_.push_back(plugin);
    }
    builtins_.clear();

    // Scan external plugin directories
    for (const auto& dir : pluginDirs_) {
        scanDirectory(dir);
    }

    // Load dynamic libraries for external plugins
    for (auto& plugin : plugins_) {
        if (plugin.status == LoadedPlugin::Status::NotLoaded) {
            loadLibrary(plugin);
        }
    }
}

void PluginManager::scanDirectory(const std::string& dir) {
    namespace fs = std::filesystem;

    if (!fs::exists(dir)) {
        return;
    }
    if (!fs::is_directory(dir)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_directory()) continue;

        fs::path pluginDir = entry.path();
        fs::path jsonPath = pluginDir / "plugin.json";

        if (!fs::exists(jsonPath)) continue;

        LoadedPlugin plugin;
        std::string error;
        if (!PluginJsonParser::parseFile(jsonPath.string(), plugin.descriptor, error)) {
            plugin.status = LoadedPlugin::Status::Error;
            plugin.errorMsg = "invalid plugin.json: " + error;
            plugin.descriptor.name = pluginDir.filename().string();
            plugins_.push_back(plugin);
            continue;
        }

        // Check for name conflicts
        bool conflict = false;
        for (const auto& existing : plugins_) {
            if (existing.descriptor.name == plugin.descriptor.name) {
                conflict = true;
                break;
            }
        }
        if (conflict) {
            plugin.status = LoadedPlugin::Status::Error;
            plugin.errorMsg = "duplicate plugin name: " + plugin.descriptor.name;
            plugins_.push_back(plugin);
            continue;
        }

        // Check min_cli_version
        if (!plugin.descriptor.minCliVersion.empty() &&
            plugin.descriptor.minCliVersion != "0.0.0") {
            if (compareVersions(plugin.descriptor.minCliVersion, cliVersion_) > 0) {
                LOG_WARN("Plugin '" + plugin.descriptor.name +
                         "' requires CLI >= " + plugin.descriptor.minCliVersion +
                         ", current is " + cliVersion_);
            }
        }

        plugin.status = LoadedPlugin::Status::NotLoaded;
        plugins_.push_back(plugin);
    }
}

void PluginManager::loadLibrary(LoadedPlugin& plugin) {
    std::string libPath = buildLibPath(plugin.descriptor.library);

    // Try to find the library in the plugin's directory
    namespace fs = std::filesystem;
    fs::path pluginDir;

    // Find the plugin directory by searching for the plugin name
    for (const auto& dir : pluginDirs_) {
        fs::path candidate = fs::path(dir) / plugin.descriptor.name;
        if (fs::exists(candidate / "plugin.json")) {
            pluginDir = candidate;
            break;
        }
    }

    // Build full library path
    std::string fullLibPath;
    if (!pluginDir.empty()) {
        fs::path libFile = pluginDir / libPath;
        fullLibPath = libFile.string();
        if (!fs::exists(fullLibPath)) {
            // Try alternative: library name as-is
            fullLibPath = (pluginDir / plugin.descriptor.library).string();
        }
    } else {
        fullLibPath = libPath;
    }

    // Attempt to load the library
    LibHandle handle = DLOPEN(fullLibPath.c_str());
    if (!handle) {
        plugin.status = LoadedPlugin::Status::Error;
#if !defined(_WIN32)
        const char* dlErr = dlerror();
        plugin.errorMsg = "library not found or cannot load: " + fullLibPath;
        if (dlErr) plugin.errorMsg += " (" + std::string(dlErr) + ")";
#else
        plugin.errorMsg = "library not found or cannot load: " + fullLibPath;
#endif
        return;
    }

    // Resolve factory functions
    auto createFunc = reinterpret_cast<CreateIndicatorFunc>(
        DLSYM(handle, "createIndicator"));
    auto destroyFunc = reinterpret_cast<DestroyIndicatorFunc>(
        DLSYM(handle, "destroyIndicator"));

    if (!createFunc || !destroyFunc) {
        DLCLOSE(handle);
        plugin.status = LoadedPlugin::Status::Error;
        plugin.errorMsg = "missing createIndicator/destroyIndicator symbol in " + fullLibPath;
        return;
    }

    plugin.handle = handle;
    plugin.createFunc = createFunc;
    plugin.destroyFunc = destroyFunc;
    plugin.status = LoadedPlugin::Status::Loaded;
}

void PluginManager::unloadLibrary(LoadedPlugin& plugin) {
    if (plugin.handle) {
        DLCLOSE(plugin.handle);
        plugin.handle = nullptr;
    }
    plugin.createFunc = nullptr;
    plugin.destroyFunc = nullptr;
    plugin.status = LoadedPlugin::Status::NotLoaded;
}

std::string PluginManager::buildLibPath(const std::string& libName) const {
    // If the name already has a prefix/suffix, use as-is
    if (libName.find(LIB_SUFFIX) != std::string::npos) {
        return libName;
    }
    // If it already has the prefix, just add suffix
    if (libName.find(LIB_PREFIX) == 0) {
        return libName + LIB_SUFFIX;
    }
    return std::string(LIB_PREFIX) + libName + LIB_SUFFIX;
}

int PluginManager::compareVersions(const std::string& a, const std::string& b) {
    // Simple semver comparison: split by '.', compare each component
    std::vector<int> va, vb;
    std::stringstream ssa(a), ssb(b);
    std::string token;
    while (std::getline(ssa, token, '.')) {
        try { va.push_back(std::stoi(token)); } catch (...) { va.push_back(0); }
    }
    while (std::getline(ssb, token, '.')) {
        try { vb.push_back(std::stoi(token)); } catch (...) { vb.push_back(0); }
    }
    size_t maxLen = std::max(va.size(), vb.size());
    for (size_t i = 0; i < maxLen; ++i) {
        int ai = (i < va.size()) ? va[i] : 0;
        int bi = (i < vb.size()) ? vb[i] : 0;
        if (ai < bi) return -1;
        if (ai > bi) return 1;
    }
    return 0;
}

// ============================================================================
// Query
// ============================================================================

const std::vector<LoadedPlugin>& PluginManager::getPlugins() const {
    return plugins_;
}

const LoadedPlugin* PluginManager::findPlugin(const std::string& name) const {
    for (const auto& plugin : plugins_) {
        if (plugin.descriptor.name == name) return &plugin;
    }
    return nullptr;
}

bool PluginManager::hasPlugin(const std::string& name) const {
    return findPlugin(name) != nullptr;
}

// ============================================================================
// Execution
// ============================================================================

int PluginManager::executePlugin(const std::string& name,
                                  const std::map<std::string, std::string>& args,
                                  std::string& result,
                                  std::string& error) {
    const LoadedPlugin* pluginPtr = findPlugin(name);
    if (!pluginPtr) {
        error = "Unknown plugin: " + name;
        return 1;
    }

    if (pluginPtr->status == LoadedPlugin::Status::Error) {
        error = "Plugin load error: " + pluginPtr->errorMsg;
        return 1;
    }

    // Validate required parameters
    std::vector<std::string> missing;
    if (!pluginPtr->descriptor.validateRequired(args, missing)) {
        std::string msg = "Missing required parameters: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) msg += ", ";
            msg += "--" + missing[i];
        }
        error = msg;
        return 1;
    }

    // Build full args map with defaults
    std::map<std::string, std::string> fullArgs = args;
    for (const auto& param : pluginPtr->descriptor.parameters) {
        if (fullArgs.find(param.name) == fullArgs.end() && !param.defaultValue.empty()) {
            fullArgs[param.name] = param.defaultValue;
        }
    }

    // Execute
    if (pluginPtr->isBuiltin) {
        // Built-in: use stored execFunc directly
        if (!pluginPtr->builtinExecFunc) {
            error = "Builtin plugin has no execution function: " + name;
            return 1;
        }
        return pluginPtr->builtinExecFunc(fullArgs, result, error);
    }

    // External plugin: use dynamic library
    if (!pluginPtr->createFunc || !pluginPtr->destroyFunc) {
        error = "Plugin factory functions not available";
        return 1;
    }

    IIndicator* indicator = pluginPtr->createFunc();
    if (!indicator) {
        error = "createIndicator() returned null";
        return 1;
    }

    // Inject logger
    indicator->setLogger([](const std::string& msg) {
        LOG_INFO(msg);
    });

    // Init
    int ret = indicator->init(fullArgs);
    if (ret != 0) {
        error = indicator->getError();
        if (error.empty()) error = "init() failed with code " + std::to_string(ret);
        pluginPtr->destroyFunc(indicator);
        return ret;
    }

    // Execute
    ret = indicator->execute();
    if (ret != 0) {
        error = indicator->getError();
        if (error.empty()) error = "execute() failed with code " + std::to_string(ret);
        pluginPtr->destroyFunc(indicator);
        return ret;
    }

    // Get result
    result = indicator->getResult();
    pluginPtr->destroyFunc(indicator);
    return 0;
}

// ============================================================================
// Listing & Info
// ============================================================================

void PluginManager::printPluginList() const {
    if (plugins_.empty()) {
        std::cout << "No plugins found." << std::endl;
        return;
    }

    for (const auto& plugin : plugins_) {
        std::string tag;
        switch (plugin.status) {
            case LoadedPlugin::Status::Builtin: tag = "[builtin]"; break;
            case LoadedPlugin::Status::Loaded:  tag = "[plugin]";  break;
            case LoadedPlugin::Status::Error:   tag = "[error]";   break;
            default:                            tag = "[unknown]"; break;
        }

        std::cout << tag << "  " << plugin.descriptor.name;
        if (!plugin.descriptor.description.empty()) {
            std::cout << "    — " << plugin.descriptor.description;
        }
        if (plugin.status == LoadedPlugin::Status::Error) {
            std::cout << "  (" << plugin.errorMsg << ")";
        }
        if (plugin.status == LoadedPlugin::Status::Loaded ||
            plugin.status == LoadedPlugin::Status::Builtin) {
            std::cout << " (v" << plugin.descriptor.version
                      << ", by " << plugin.descriptor.author << ")";
        }
        std::cout << std::endl;
    }
}

bool PluginManager::printPluginInfo(const std::string& name) const {
    const LoadedPlugin* plugin = findPlugin(name);
    if (!plugin) return false;

    std::cout << "Plugin: " << plugin->descriptor.name << std::endl;
    std::cout << "  Version:     " << plugin->descriptor.version << std::endl;
    std::cout << "  Author:      " << plugin->descriptor.author << std::endl;
    std::cout << "  Description: " << plugin->descriptor.description << std::endl;
    std::cout << "  Category:    " << plugin->descriptor.category << std::endl;
    std::cout << "  Library:     " << plugin->descriptor.library << std::endl;
    std::cout << "  Class:       " << plugin->descriptor.className << std::endl;
    std::cout << "  Min CLI:     " << plugin->descriptor.minCliVersion << std::endl;

    std::string statusStr;
    switch (plugin->status) {
        case LoadedPlugin::Status::Builtin: statusStr = "builtin"; break;
        case LoadedPlugin::Status::Loaded:  statusStr = "loaded";  break;
        case LoadedPlugin::Status::Error:   statusStr = "error: " + plugin->errorMsg; break;
        default:                            statusStr = "not loaded"; break;
    }
    std::cout << "  Status:      " << statusStr << std::endl;

    if (!plugin->descriptor.parameters.empty()) {
        std::cout << std::endl << "  Parameters:" << std::endl;
        for (const auto& param : plugin->descriptor.parameters) {
            std::cout << "    --" << param.name;
            if (!param.shortName.empty()) {
                std::cout << " / -" << param.shortName;
            }
            std::cout << "  (" << param.type;
            if (param.required) std::cout << ", required";
            else if (!param.defaultValue.empty()) std::cout << ", default=" << param.defaultValue;
            std::cout << ")";
            if (!param.description.empty()) {
                std::cout << "  — " << param.description;
            }
            std::cout << std::endl;
        }
    }

    if (!plugin->descriptor.inputs.empty()) {
        std::cout << std::endl << "  Inputs:" << std::endl;
        for (const auto& input : plugin->descriptor.inputs) {
            std::cout << "    " << input.format << " (" << input.dtype
                      << ", " << input.bands << " band(s))";
            if (!input.description.empty()) {
                std::cout << " — " << input.description;
            }
            std::cout << std::endl;
        }
    }

    if (!plugin->descriptor.outputs.empty()) {
        std::cout << std::endl << "  Outputs:" << std::endl;
        for (const auto& output : plugin->descriptor.outputs) {
            std::cout << "    " << output.format;
            if (!output.description.empty()) {
                std::cout << " — " << output.description;
            }
            std::cout << std::endl;
        }
    }

    return true;
}

std::string PluginManager::generateHelpText(const std::string& name) const {
    const LoadedPlugin* plugin = findPlugin(name);
    if (!plugin) return "Unknown plugin: " + name;

    std::ostringstream oss;
    oss << "Usage: geosdg-cli " << plugin->descriptor.name << " [options]\n\n";
    oss << plugin->descriptor.description << "\n\n";
    oss << "Options:\n";

    for (const auto& param : plugin->descriptor.parameters) {
        oss << "  --" << param.name;
        if (!param.shortName.empty()) {
            oss << ", -" << param.shortName;
        }
        oss << " <" << param.type << ">";
        if (param.required) {
            oss << " (required)";
        } else if (!param.defaultValue.empty()) {
            oss << " (default: " << param.defaultValue << ")";
        }
        if (!param.description.empty()) {
            oss << "\n        " << param.description;
        }
        oss << "\n";
    }

    return oss.str();
}
