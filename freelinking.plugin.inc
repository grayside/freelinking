<?php
// $Id$

/**
 * @file
 *  Functions to prepare Freelinking module plugins for use.
 */

/**
 * Constants are used as plugin faiover return values.
 */
define('FREELINKING_ERROR_NOT_FOUND', 404);
define('FREELINKING_ERROR_ACCESS_DENIED', 403);
define('FREELINKING_ERROR_BAD_REQUEST', 400 );

/**
 * Generates an array of Freelinking plugins for link generation.
 * 
 * @param $format
 *   The Text Format ID of the currently processed piece of text. Allows
 * format-specific plugin overrides such as disabling certain plugins in a given
 * format.
 * @param $syntax_patterns
 *  Regular expressions defined as Freelinking syntax patterns.
 * Used to build syntax processing expressions for each plugin.
 *
 * @return
 *  An array of all plugins. Each plugin is itself an array.
 *
 * @see PLUGINS.mdown
 */
function freelinking_get_plugins($format = 'all', $syntax_patterns = NULL) {
  static $plugins;
  if (!empty($plugins[$format])) {
    return $plugins[$format];
  }

  $plugins[$format] = array();
  $plugins[$format] = _freelinking_get_raw_plugins();

  // Validate & standardize plugins.
  foreach($plugins[$format] as $index => $plugin) {
    $plugins[$format][$index] = _freelinking_normalize_plugin($index, $plugin, $format);
  }
  _freelinking_assign_syntax($plugins, $syntax_patterns, $format);
  drupal_alter('freelinking_plugin_info', $plugins[$format]);

  return $plugins[$format];  
}

/**
 * Standardize plugin definition.
 * @todo
 *  1. access arguments may be dropped.
 */
function _freelinking_normalize_plugin($name, $plugin, $format) {
   $plugin += array(
     'machine name' => $name,
     'title' => ucfirst(strtr($name, array('_' => ' '))),
     'process callback' => 'freelinking_external_url_processor',
     'translate' => array(' ' => '-'),
     'syntax' => 'double_bracket',
     'link options' => array('html' => TRUE), 
  );
  
  // Only internal links need the access callback. External links are allowed
  // to pass unless they have an explicit function to check with.
  if ($plugin['process callback'] != 'freelinking_external_url_processor' && empty($plugin['access callback'])) {
     $plugin['access callback'] = 'freelinking_link_access';
  }

  // Standardize various elements as arrays.
  if (!is_array($plugin['indicator'])) {
    $plugin['indicator'] = array($plugin['indicator']);
  } 
  if (isset($plugin['settings']) && !is_array($plugin['settings'])) {
    $plugin['settings callback'] = array($plugin['settings callback']);
  }
  if (isset($plugin['failover']) && !is_array($plugin['failover'])) {
    $plugin['failover'] = array($plugin['failover']);
  }
  if (isset($plugin['process arguments']) && !is_array($plugin['process arguments'])) {
    $plugin['process arguments'] = array($plugin['process arguments']);
  }
  if (isset($plugin['access arguments']) && !is_array($plugin['access arguments'])) {
    // Using access arguments in the plugin definition may be confusing. Lets see how default shapes up.
    $plugin['access arguments'] = array($plugin['access arguments']);
  }

  // Enabled unless explicitly set to FALSE
  $plugin['enabled'] = freelinking_variable_get('plugin_' . $plugin['machine name'] . '_enabled', $format);

  return $plugin;
}

/**
 * Generate and assign the regular expression associated with each plugin.
 * 
 * This includes identifying plugins configured as default for each syntax type and 
 * assigning them a special "default" expression.
 *
 * @param $plugins
 *  The set of plugins to process for defaults.
 * @param $patterns
 *  The regular expression templates.
 * @param $format
 *  The current format to check for default configuration.
 *
 * @todo
 *  1. Why is the regular expression being wrapped in an array?
 *  2. Make the separator character ':' configurable, preferably by specifying
 *      in freelinking_get_syntax_patterns().
 */
function _freelinking_assign_syntax(&$plugins, $patterns, $format) {
  $default = freelinking_get_settings('freelinking_default_plugins');

  // Purge all disabled plugins.
  foreach ($default as $syntax => $plugin) {
    if (!$plugins[$plugin]['enabled']) {
      unset($default[$syntax]);
    }
  }

  foreach ($default as $pattern => $plugin) {
    $plugins[$plugin]['default'] = $pattern;
  }

  // If plugin is enabled for direct syntax use, compute syntax pattern.
  if ($plugin['enabled']) {
    $plugin['regex'] = array();
    if ($plugin['syntax'] == FREELINKING_PLUGIN_CUSTOM_SYNTAX) {
      $plugin['regex'][] = array($plugin['indicator']);
    }
    else {
      $indicator = is_array($plugin['indicator']) ? implode('|', $plugin['indicator']) : $plugin['indicator'];
      $plugin['regex'][] = str_replace('@INDICATOR@', $indicator . '\:', $patterns[$plugin['syntax']]['value']);
    }

    // Add default regex.
    if (!empty($plugin['default'])) {
      $plugin['regex'][] = str_replace('@INDICATOR@', '', $patterns[$plugin['default']]['value']);
    }
  }
}

// Helper Functions //

/**
 * Collect all plugins defined in hook_freelinking_plugin_info().
 *
 * Module origins for each plugin are included here. This info is not available later.
 * This function is used in place of module_invoke_all specifically to get the module of origin.
 *
 * @return
 *  Array of plugins.
 */
function _freelinking_get_raw_plugins() {
  $plugins = array();
  foreach (module_implements('freelinking_plugin_info') as $module) {
    $function = $module . '_' . 'freelinking_plugin_info';
    $result = call_user_func($function);
    if (isset($result) && is_array($result)) {
      foreach ($result as $index => $plugin) {
        $result[$index]['module'] = $module;
      }
      $plugins = array_merge($plugins, $result);
    }
  }
  return $plugins;
}

/**
 * Remove all disabled plugins.
 *
 * This is intended primarily for UI construction, as disabled plugins may be
 * referred to for failover purposes by enabled plugins. Also used to identify
 * where the the text we find Freelinking syntax in need of processing.
 *
 * @param $plugins
 *  Array of all plugins known to Freelinking.
 */
function freelinking_purge_disabled_plugins(&$plugins) {
  foreach ($plugins as $index => $plugin) {
    if (!$plugin['enabled']) {
      unset($plugins[$index]);
    }
  }
}

/**
 * Plugin callback that replaces link text with simple text.
 *
 * Used to clean Freelinking syntax but leave visible text.
 *
 * @todo
 *  1. Proper comments
 *  2. Check that 'goto' is still the valid key.
 */
function freelinking_clean_text_processor($target, $plugin) {
  return array(
    'failover' => 'error',
    'message' => $target->args['text'] ? $target->args['text'] : $target->args['goto'],
  );
}

/**
 * Process for simple url replacement plugins.
 * 
 * @todo
 *  1. Proper comments
 *  2. Check that 'goto' is still the valid key. 
 *  3. Enrich links with data from external page.
 *  4. If no %s is present, append replacement text to the end of the baseurl.
 *  5. Use the first of the process arguments as base_url.
 */
function freelinking_external_url_processor($target, $plugin) {
  // %s is the token all freelinking replacement strings must include.
  $url = preg_replace('/%s/', $target->args['goto'], $plugin['process']);
  return array('', $url);
}

/**
 * Wraps access checks in a check that takes post-cache status into account.
 * Access control pre-caching should always be based on anonymous user.
 * Access control post-cache should use the currently logged in user (which in most access checks is based on a NULL account.)
 *
 * @param $path
 *  An internal Drupal path for which to check access.
 * @param $cache
 *  Specify whether the function is being called from a cached or un-cached context.
 *  If cached, the code should treat the user as anonymous. If uncached, it can do a
 *  proper access check. This information could theoretically be pulled from the general
 *  context at the time the function is called, but for now it will be pulled from the
 *  $target object being processed at the time this is called, at least as far as standard
 *  text processing by Freelinking is concerned. Defaults to cached.
 *
 * @return
 *  Boolean value to determine access.
 */
function freelinking_link_access($path, $cached = TRUE) {
  if ($cached) {
    // Ideally, anonymous user would be loaded and used as the basis for an access check.
    // Unfortunately, the menu access mechanism would require some loopedy-loops. Until
    // it is clear what the performance impact would be, that is postponed.
    return FALSE;
  }
  $item = menu_get_item($path);
  return $item['access'];
}

/**
 * API function to validate a plugin. If it is invalid it will be removed from use.
 *
 * @deprecated This function is no longer in use, and has not been updated to 
 *                     reflect the current plugin schema.
 * 
 * @param $plugin
 *  A single Freelinking plugin.
 * @param $patterns
 *  The syntax patterns available to Freelinking.
 *
 * @return
 *  Boolean. If FALSE, the plugin is invalid.
 *
 * @todo
 *  1. Build drush/admin UI that makes use of this function.
 *  2. Check machine name validity, both array key and machine_name element.
 */
function _freelinking_validate_plugin($plugin, $patterns) {
  // Explanation for why a plugin may be invalid.
  $reason = array();

  if (empty($plugin['indicator'])) {
    $reason[] = t('no indicator found');
  }

  if (empty($plugin['process'])) {
    $reason[] = t('no process found');
  }
  elseif (!valid_url($plugin['process'], TRUE) && !function_exists($plugin['process'])) {
    $reason[] = t('invalid process');
  }

  if (isset($plugin['settings']) && !function_exists($plugin['settings'])) {
    $reason[] = t('invalid settings callback');
  }

  if (isset($plugin['translate']) && !is_array($plugin['translate'])) {
    $reason[] = t('translate should be an array');
  }
  
  if(isset($plugin['syntax']) && !in_array($plugin['syntax'], array_keys($patterns))) {
    $reason[] = t('invalid syntax pattern');
  }

  if (!empty($reason)) {
    drupal_set_message(t('Freelinking plugin "!plugin" is invalid. See the !log for details.', 
      array('!plugin' => $index, '!log' => l(t('log'), 'admin/reports/dblog'))), 'warning');      
    watchdog('filter', 'Freelinking plugin "!plugin" is invalid: ' . implode(', ', $reason),
      array('!plugin' => $index), WATCHDOG_WARNING);
    return FALSE;
  }
  return TRUE;
}