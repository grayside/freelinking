<?php

/**
 * Define link plugins for the Freelinking module.
 * Every plugin defines elements of the Freelinking UI and syntax processing.
 *
 * @return
 *  An array of plugins, each defined as an array indexed on it's machine name.
 * @see PLUGINS.mdown
 */
function hook_freelinking_plugin_info() {
  $plugins = array();
  
  $wp = variable_get('freelinking_wikipedia', array());
  $language = empty($wp['language']) ? language_default('language') : $wp['language'];
  $plugin['wikipedia'] = array(
    'title' => t('Wikipedia'),
    'description' => t('Create links to Wikipedia articles.'),
    'indicator' => array('wp', 'wiki', 'wikipedia'),
    'process argument' => 'http://' . $language . '.wikipedia.org/wiki/%s',
    // Configure the language variable
    'settings' => array('freelinking_settings_wikipedia'),
  );
  
  // Callback-based plugin.
  $plugin['nid'] = array(
    'title' => t('NID'),
    'description' => t('Link to a Node by Node ID Number.'),
    'indicator' => 'nid',
    'callback' => 'freelinking_callback_nid',
    'syntax' => 'single_bracket',
  );
  
  return $plugins
}

/**
 * Make changes to Freelinking plugin definitions.
 *
 * This works a lot like hook_menu_alter(), allowing any aspect of a plugin to
 * be changed or swapped out. You could also add new plugins here, but they
 * would skip certain processing steps that the developer would then become
 * responsible for.
 *
 * Currently plugins are held as a static variable for performance, therefore this
 * only gets called once per page load.
 *
 * @param $plugins
 *  Array of sanitized plugins.
 */
function hook_freelinking_plugin_info_alter(&$plugins) {
  $plugin['wikipedia']['title'] = t('Encyclopedia');
}

/**
 * I
 */
function hook_freelinking_syntax_alter(&$patterns) {}

function hook_freelink_pre_render_alter(&$link, $target, $plugins) {}