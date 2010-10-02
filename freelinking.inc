<?php
// $Id$

/**
 * @file
 *  Text processing utilities for the Freelinking module.
 */

/**
 * Process linking syntax into HTML links.
 *
 * @param $patterns
 *  Collect syntax patterns used to analyze text for Freelinking syntax.
 * @param $format
 *  The currently active input format.
 *
 * @return
 *  Filter text with links in place of freelinking syntax.
 *
 * @todo
 *  Wrap freelinking_get_link() in a new freelinking_get_text(), the latter
 *  to route processing based on plugin type. This will allow support of macros
 *  like {{dp:token}} to build links to project, usage, issue queue, and commit log.
 */
function freelinking_process_text($patterns, $format) {
	$plugins = freelinking_get_plugins($format, $patterns);
	$targets = freelinking_get_link_targets($text, $plugins, $format);

  foreach ($targets as $target) {
    $rendered = freelinking_get_link($target, $format);
    if ($rendered) {
      // If the target was marked for post-processing, store it for later use.
      if ($target->access_denied == FREELINKING_ERROR_ACCESS_DENIED) {
        $target->post_process = TRUE;
        freelinking_store_post_cache_target($target);
      }

      // Replace every link with effectively identical syntax with the same link.
      // This is done here because if the $target needs to be rerun in post-process,
      // the new text will be swapped for whatever was generated in $rendered.
      foreach ($target->raw as $raw) {
        $text = str_replace($raw, $rendered, $text);
      }
    }
  }
  return $text;
}

/**
 * Collect and process text for freelinking syntax.
 *
 * The entire text is checked over for any defined Freelinking syntax, one
 * active plugin at a time. All instances of a given plugin's syntax are broken
 * down and saved to a target object, which is processed once for all effectively
 * identical syntax.
 *
 * @param $text
 *  The text submitted to the filter for processing.
 * @param $plugins
 *  The set of all plugins known to Freelinking. 
 * @param $format
 *  The currently active input format.
 *
 * @todo
 *  1. If disable_cache = TRUE, cache the targets based on $text.
 *  2. Assign plugin name to each process. In the event of a macro use the plugins structure to assign plugins.
 */
function freelinking_get_link_targets($text, $plugins, $format) {
  // Only enabled plugins are needed for syntax processing.
  freelinking_purge_disabled_plugins($plugins);
  
  $targets = array();
  foreach ($plugins as $plugin) {
    foreach ($plugin['regex'] as $regex) {
      preg_match_all($regex, ' ' . $text, $matches, PREG_SET_ORDER);

      foreach($matches as $match) {
        $target = freelinking_parse_target($match[2], $plugin);
        $tid = _freelinking_get_target_hash($target);
        // A new target is identified and stored.
        if (empty($targets[$tid])) {
          $target->tid = $tid;
          $target->raw[] = $match[0];
          $target->indicator = $match[1];
          $targets[$tid] = $target;
        }
        // An existing target is identified, raw text saved for later transformation.
        else {
          $targets[$tid]->raw[] = $match[0];
        }
      }
    }
  } 
}

/**
 * Parse target for secondary link arguments.
 *
 * There are two types of argument. The first three arguments that are not
 * explicitly assigned a key are the "destination" argument, link text, and tooltip.
 * Explicitly "keyed" values (|key=value|) are saved as such. Any additional
 * un-keyed values are dumped into an 'other' array, and are assumed to have
 * a specific order and significance understood by specific plugins.
 *
 * This is raw user input and needs to be checked by the HTML Filter.
 *
 * @param $target
 *  Target object for syntax processing.
 * @param $plugin
 *  The specific plugin for which the target is being processed.
 *
 * @return
 *  Array of arguments.
 *
 * @todo
 *  1. Be sure preg_split and str_replace strings are working properly.
 *  2. Review whether xss/etc checks need to be put in place.
 */
function freelinking_parse_target($target, $plugin = NULL) {
  $separator = '|';
  
  $args = array();

  // Allow escaped pipe's through.
  $items = preg_split("/(?<!\\)\{$separator}/S", $target);
 
  // The first three unspecified arguments will be used in a specific way.
  $index = 0;
  foreach ($items as $key => $item) {
    // un-escape seaparator
    $item = str_replace("\{$separator}", $separator, $item);

    // Get specified arguments.
    if (strpos($item, '=')) {
      $list ($key, $value) = explode('=', $item);
      $args[$key] = $value;
    }
    elseif ($index < 3) {
      switch ($index) {
        case 0:
          $args['goto'] = $item;
          break;
        case 1:
          $args['text'] = $item;
          break;
        case 2:
          $args['tooltip'] = $item;
          break;
      }
      $index++;
    }
    else {
      $args['other'][] = $item;
    }
  }

  // Convert URL-encoded text into something readable for link text & tooltip.
  $args['text'] = urldecode($args['text']);
  $args['tooltip'] = urldecode($args['tooltip']);

  // Extra parsing for goto. By separating the fragment, it's possible to have a clean
  // destination argument, and also easily slide the hash back on. Useful for internal
  // links where the path needs to be looked up.
  list ($goto, $fragment) = explode('#', $args['goto']);
  $args['goto'] = $goto;
  if (isset($fragment)) {
    $args['fragment'] = $fragment;
  }

  $target = stdClass();
  $target->args = $args;
  $target->plugin = $plugin['machine name'];

  return $target;
}

// Generate Links //

/**
 * Process the target text into a link with the specified plugin.
 *
 * @param $target
 *   The target text extracted from the freelink. Array.
 * @param $format
 *   The format id currently invoking the Freelinking filter. Might be used in
 * the future to tweak plugin behavior.
 *
 * @return
 *   A rendered link or bit of HTML markup.
 *
 * @see l
 */
function freelinking_get_link($target, $format = NULL) {
  // All plugins are needed to support processing failover to other, arbitrary plugins.
  $plugins = freelinking_get_plugins($format);

  // Apply plugin processing to the target. The initial plugin is the first in 'trace'.
  $link = _freelinking_build_link($target, $plugins);

  if (is_array($link)) {
    drupal_alter('freelink_pre_render', $link, $target, $plugins);
  }
  // If the "link" is straight text, pass it through. Allows module developers
  // to leverage the FL syntax/plugin infrastructure without link-specific processing.
  elseif (!empty($link)) {
    return $link;
  }

  if ($link['failover'] == 'error' || $link['failover'] == 'none') {
    return theme('freelink_message', $target, $link['text']);
  }

  // If a theme function was written for a specific plugin, use that instead.
  if (function_exists('theme_freelink_' . $plugin)) {
    return theme('freelink_' . $plugin, $link, $target);
  }
  return theme('freelink', $plugin, $link, $target);
}

/**
 * Construct a link out of the $target with the specified plugin.
 *
 * This function does the text processing for a specific target.
 * It works recursively to handle plugin failover.
 *
 * @param $target
 *  The link target object.
 * @param $plugins
 *  All the Freelinking plugins.
 *
 * @return
 *  One of the following:
 *   - Array defining an error response
 *   - Array defining arguments for a link generated with l()
 *   - Scalar value holding arbitrary text. (This is an intended gap in the
 *     link-specific structure to allow Freelinking's sophisticated filtration/plugin
 *     mechanism to be used for other purposes.)
 */
function _freelinking_build_link($target, $plugins) {
  // Get the current plugin.
  $current = $target->plugin;

  // Plugin not defined, fail gracefully.
  if (empty($plugins[$current])) {
    return array('failover' => 'error', 'message' => t('Missing Plugin'));
  }

  // Run the text through translation.
  if ($plugin['translate']) {
    if (empty($target->old_args['original']['goto']['original_goto']) {
    /// shouldn't this be using $target->process[]?
      $target->old_args['original']['goto'] = $target->args['goto'];
    }
    $translate = $target->old_args['original']['goto'] ? $target->old_args['original']['goto'] : $target->args['goto'];
    $target->args['goto'] = strtr($translate, $plugin['translate']);
  }

  // Record the target processing history.
  $target['trace'][] = $current;

  // Process simple replacement plugins with a standard callback.
  if (valid_url($plugin['process'], TRUE)) {
    $link = freelinking_simple_url_processor($target, $plugin)
  }
  else {
    $link = call_user_func_array($plugin['process'],
      array($target, $plugin));
  }

  // Process failover. "error" and "none" are reserved.
  if (is_array($link) && isset($link['failover']) && $link['failover'] != 'error') {
    // If failover is 'none', return cleaned text.
    if ($link['failover'] == 'none') {
      return freelinking_clean_text_processor($target, $plugin);
    }

    // Prepare for recursive failover processing.

    // Store previous target arguments and overwrite if plugin defined new ones.
    $target->old_args[$current] = $target->args
    if (!empty($link['args'])) {
      $target->args = $link['args'];
    }

    // Prevent recursion by removing previously processed plugin.
    unset($plugins[$current]);
    
    $target->plugin = $link['failover'];
    return _freelinking_build_freelink($target, $plugins);
  }

  return $link;
}

// Helper Functions //

/**
 * Get the post-cache targets.
 *
 * @return
 *  Array of targets for post-cache processing.
 */
function freelinking_get_post_process_targets() {
  return freelinking_stor_post_cache_target();
}

/**
 * Clear the post-cache targets.
 */
function freelinking_clear_post_process_targets() {
  freelinking_store_post_cache_target(NULL, TRUE)
}

/**
 * Store link targets that have an access control failure for post-filter processing.
 * 
 * Targets are stored in freelinking_process_text() after completed link markup is generated.
 * Each target is indexed on a Static ID unique to the current user and his current user roles.\
 *
 * @param $target
 *  A link target to be post-cache reprocessed.
 * @param $clear
 *  Clear the post-cache targets.
 * @return
 *  Array of targets (if any are set) for post-cacche processing.
 * @see freelinking_process_text
 */
function freelinking_store_post_process_target($target = NULL, $clear = FALSE) {
  global $user;
  static $stored_targets;
  
  if ($clear) {
    $stored_targets = NULL;
  }
  
  if (!empty($target)) {
    $sid = _freelinking_get_target_hash($target) . $user->uid . implode(':', array_keys(roles));
    $stored_targets[$sid] = $target;
  }
  
  return !empty($stored_targets) ? $stored_targets : array();
}

/**
 * Generate a unique id for a link target.
 *
 * Targets may be generated by different syntax details, but the ID generated for a processed target
 * has been normalized. This allows one processing cycle for each identical link.
 *
 * @return
 *  String target id.
 */
function _freelinking_get_target_hash($target) {
  return $target->plugin . ':' . serialize($target->args);
}
