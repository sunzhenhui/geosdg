/**
 * @file BuiltinPlugins.h
 * @brief Register built-in SDG indicators as plugins
 *
 * This header provides a single function to register all built-in
 * SDG indicators (CalculateSDG, ExtractPriorityAreas, etc.) as
 * plugins with the PluginManager, so they appear alongside external
 * plugins in list-plugins and can be executed through the plugin
 * dispatch path.
 */

#pragma once

#include "PluginManager.h"

// ============================================================================
// Public Interface
// ============================================================================

/**
 * @brief Register all built-in SDG indicators as plugins
 * @param mgr PluginManager to register with
 *
 * Registers the following built-in indicators:
 * - sdg-land-proportion
 * - sdg-land-conversion
 * - sdg-buffer-zone
 * - sdg-1131
 * - sdg-1322
 * - priority-loss
 * - priority-transition
 * - priority-buffer
 * - priority-emission
 * - priority-human-land
 * - priority-merge
 * - priority-stats
 *
 * Each indicator is wrapped in a BuiltinIndicatorAdapter that delegates
 * to the existing C++ calculation methods.
 */
void registerBuiltinPlugins(PluginManager& mgr);
