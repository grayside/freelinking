<?php
// $Id$

/**
 * @file
 *  Define standard plugins that ship with Freelinking.
 */

/**
 * Implementation of hook_freelinking().
 * Include plugins/*.inc plugins
 */
function freelinking_freelinking_alter($plugins = array()) {
  $plugins['nodetitle'] = array(
    'name' => t('Node Title'),
    'indicator' => array(
      'nodetitle',
      'title',
      'nt',
    ),
    'callback' => 'freelinking_nodetitle_callback',
    'settings' => 'freelinking_settings_callback',
    'description' => t('Link to a local node by title.'),
  );

  $plugins['nid'] = array(
    'name' => t('NID'),
    'indicator' => '/(n(id|ode)?)$/A',
    'callback' => 'freelinking_nid_callback',
    'description' => t('Link to a local node by nid'),
  );

  $plugins['path'] = array(
    'name' => t('Path'),
    'indicator' => array(
      'path',
      'alias',
    ),
    'callback' => 'freelinking_path_callback',
    'settings' => 'freelinking_path_settings',
    'description' => t('Link to a page by internal URL.'),
    'failover' => 'nodetitle',
  );

  $plugins['user'] = array(
    'name' => t('User'),
    'indicator' => array(
      'u',
      'user',
      'username',
      'uid',
      'userid',
    ),
    'callback' => 'freelinking_user_callback',
    'description' => t('Link to a user profile by name or uid.'),
    'failover' => array('search'),
  );

  $plugins['file'] = array(
    'name' => t('File'),
    'indicator' => array(
      'file',
    );
    'callback' => 'freelinking_file_file_callback',
    'settings' => 'freelinking_file_file_settings',
    'description' => t('Link to an uploaded file.'),
  );

  $plugins['image'] = array(
    'name' => t('Image'),
    'indicator' => array(
      'image',
    );
    'callback' => 'freelinking_file_file_callback',
    'settings' => 'freelinking_file_file_settings',
    'description' => t('Link to an uploaded image.'),
  );

  return $plugins;
}
