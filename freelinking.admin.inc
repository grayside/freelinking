<?php
// $Id: freelinking.forms.inc,v 1.1.2.1 2010/04/18 06:45:12 grayside Exp $
/**
 * Freelinking 3
 *
 * @file
 *   Constructs freelinking & module admin forms.
 */

/**
 * Build admin/settings page.
 */
function freelinking_settings() {
  $form = array();

  $plugins = freelinking_get_plugins();

  $form = array_merge($form, _freelinking_default_plugins_form());

  $form['freelinking_enrich_links'] = array(
    '#type' => 'checkbox',
    '#default_value' => freelinking_variable_get('enrich_links'),
    '#title' => t('Enrich external links with page info'),
    '#description' => t("Plugins that link to external websites will download page info such as page title to improve the link usefulness."),
  );
  $form['freelinking_disable_cache'] = array(
    '#type' => 'checkbox',
    '#default_value' => freelinking_variable_get('disable_cache'),
    '#title' => t('Security mode'),
    '#description' => t('Impose access checks on all content at a performance price. If disabled, Freelinking will make a best effort on internal links but default to protecting content.'),
  );

  // Loop through plugin settings functions, adding a fieldset for each
  foreach ($plugins as $plugin) {
    if (empty($plugin['settings'])) {
      continue;
    }

    $form[$plugin['machine name']]  = array(
      '#title' => t('!plugin Plugin Settings', array('!plugin' => $plugin['title'])),
      '#type'  => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#tree' => TRUE,
    );
    
    foreach ($plugin['settings'] as $function) {
      $form[$plugin['machine name']] = array_merge($form[$plugin['machine name']], call_user_func($function));
    }

    // If failover is an array of greater than one option, allow user to decide.
    if (!empty($plugin['failover'])
      && !in_array('failover', $form[$plugin['machine name']])
      && empty($form[$plugin['machine name']]['failover'])) {

      $failover_options = array_values($plugin['failover']);

      $form[$plugin['machine name']]['failover'] = array(
        '#title' => t('Preferred failover'),
        '#description' => t('If the !plugin plugin fails, use the selected option as a fallback.', array('!plugin' => $plugin['title'])),
        '#type' => 'select',
        '#default_value' => $failover_options[0],
        '#options' => $failover_options,
      );
    }
  } // end foreach defining plugin configuration form

  return system_settings_form($form);
} // endfunction freelinking_settings (admin settings)

/**
 * Define default plugins for each syntax pattern.
 *
 * @param $plugins
 *  Array of plugin options.
 * @param $format
 *  The current format. 'all' is used for format-neutral configuration.
 * @return
 *  Forms API array defining configuration. 
 */
function _freelinking_default_plugins_form($format = 'all') {
  $patterns = freelinking_get_syntax_patterns();
  
  $form['freelinking_default_plugin_format_' . $format] = array(
    '#title' => t('Default Plugin by Pattern'),
    '#description' => t('Each syntax pattern may be assigned a default plugin. A default plugin does not need to have an indicator specified.'),
    '#tree' => TRUE,
  );  
  
  foreach($patterns as $name => $pattern) {
    $form['freelinking_default_plugin_format' . $format][$name] = array(
      '#title' => t('!pattern default', array('!pattern' => $pattern['title'])),
      '#type' => 'select',
      '#default_value' => freelinking_variable_get('default_plugins', $format, $name),
      '#options' => _freelinking_plugin_options('enabled', $format),
      '#multiple' => FALSE,
      '#description' => t('Use !syntax without an <em>indicator:</em> for the specified plugin.', array('!syntax' => $pattern['title'])),
    );
  }
  return $form;
}

/**
 * 
 */
function _freelnking_plugin_options($enabled = 'all', $format = 'all') {
  static $options;
  
  if (empty($options)) {
    $plugins = freelinking_get_plugins();
    $options = array();
    $options['all'] = $options[TRUE] = $options[FALSE] = array();
 
    foreach ($plugins as $plugin)  { 
      $options['all'][$plugin['machine_name']] = $plugin['title'];
      if ($plugin['enabled']) {
        $options[TRUE][$plugin['machine_name']] = $plugin['title'];
      }
      else {
        $options[FALSE][$plugin['machine_name']] = $plugin['title'];
      }
    }
  }
  return $options[$enabled];
}
// vim: tw=300 nowrap syn=php
