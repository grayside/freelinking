<?php
// $Id$

/**
 * @file
 *  Text processing utilities for the Freelinking module.
 */

/**
 * Constants are used as plugin faiover return values.
 */
define('FREELINKING_ERROR_NOT_FOUND', 404);
define('FREELINKING_ERROR_ACCESS_DENIED', 403);
define('FREELINKING_ERROR_BAD_REQUEST', 400 )
define('FREELINKING_ERROR', 'error');
define('FREELINKING_CLEAN_TEXT', 'none');

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
      if ($target->post_process) {
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

  // Apply plugin processing to the target.
  $target->process = array();
  $target->process[] = array('plugin' => $target->plugin);
  $link = _freelinking_build_link($target, $plugins);

  if (is_array($link)) {
    drupal_alter('freelink_pre_render', $link, $target, $plugins);
  }
  // If the "link" is straight text, pass it through. Allows module developers
  // to leverage the FL syntax/plugin infrastructure without link-specific processing.
  elseif (!empty($link)) {
    return $link;
  }

  if ($link['action'] == FREELINKING_ERROR || $link['action'] == FREELINKING_CLEAN_TEXT) {
    return theme('freelink_message', $target, $link['message']);
  }

  // If a theme function was written for a specific plugin, use that instead.
  if (function_exists('theme_freelink_' . $plugin)) {
    return theme('freelink_' . $plugin, $link['link'], $target);
  }
  return theme('freelink', $plugin, $link['link'], $target);
}

/**
 * Construct a link out of the $target with the specified plugin.
 *
 * This function does the text processing for a specific target.
 * It works recursively to handle plugin failover.
 *
 * @param $target
 *    The link target object. Referenced.
 * @param $plugins
 *    All the Freelinking plugins. When _freelinking_build_link() is invoked
 *    recursively, it removes the calling invocation's plugin. The results of a
 *    processing by a given plugin do not change, so this is done to avoid
 *    redundant processing and help break free of a runaway recursion.
 *
 * @return
 *   An associative array containing:
 *     - link: (optional) On a successfully processing run, this contains the arguments
 *       specified as parameters for l().
 *     - action: (optional) A string indicating the currently processed result. Maybe be
 *       one of the following values:
 *       - error: The syntax processing resulted in an error. Display the value
 *         of 'message' as an error.
 *       - none: No transformation required.
 *       - <plugin name>: The name of the plugin with which to retry link     
 *       processing.
 *     - text: (optional) A string explaining what happened, will be inserted into text 
 *       in place of Freelinking syntax in the event of an Error.
 *     - code: (optional) Code indicating what happened as a result of processing.
 *       If no code is present, success is assumed and freelinking will proceed
 *       to render the results. May be one of the following constants:
 *       - FREELINKING_ERROR_NOT_FOUND: Drupal wanted to link to content, but
 *         identified the link target as not found. This is usually coupled
 *         with a faiover 'action'.
 *       - FREELINKING_ERROR_ACCESS_DENIED: The target of the link is not
 *         accessible to anonymous users. This will result in the link being
 *         marked for post-cache reprocessing. This may be combined with a
 *         failover or error action.
 *       - FREELINKING_ERROR_BAD_REQUEST: Freelinking did not like what just
 *         happened. This is usually combined with an 'error' or 'none' action.
 *   The return value is inserted into the target object for the just-completed
 *   process. While all elements of the array are optional, the 'link' value must be filled
 *   unless an action is specified.
 */
function _freelinking_build_link($target, $plugins) {
  // Get the current plugin.
  $current_step = array_pop($target->process);

  // Plugin not defined, fail gracefully.
  if (empty($plugins[$current_step['plugin'])) {
    return array('action' => 'error', 'message' => t('Missing Plugin'));
  }
  $plugin = $plugins[$current_step['plugin']];

  // Run the text through translation.
  // Currently only the primary "goto" argument is translated.
  if ($plugin['translate']) {
    $translate = $target->args['goto'];
    $current_step['args']['goto'] = strtr($translate, $plugin['translate']);
  }

  // Attempt to process the target.
  $current_step['result'] = $plugin['process callback']($target, $plugin);
  // Push the current processing info back onto the target object. This is
  // always the most recent element of the target 'process' array. This must be
  // done before any recursion or the process order will get scrambled.
  $target->process[] = $current_step;
  // If the processing was rendered incomplete for access reasons, mark the
  // target for followup processing after the cache.
  if ($current_step['result']['code'] = FREELINKING_ERROR_ACCESS_DENIED) {
    $target->post_process = TRUE;
  }  
  // The just-processed plugin cannot be processed for this target again.
  unset($plugins[$plugin]['machine name']);
  
  // If the action is a plugin, failover to using that plugin.
  if (in_array($current_step['result']['action'], array_keys($plugins))) {
    $target['process'][] = array('plugin' => $current_step['result']['action']);
    return _freelinking_build_link($target, $plugins);
  }
  // The plugin requested the strange syntax be removed and the text left alone.
  elseif ($current_step['result']['action'] == FREELINKING_CLEAN_TEXT) {
     return freelinking_clean_text_processor($target, $plugin);
  }
  // The results are an error or a successful link. Return for processing.
  return $current_state['result'];
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
