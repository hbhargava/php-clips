#ifndef PHP_CLIPS
#define PHP_CLIPS 1
#define PHP_CLIPS_VERSION "1.0"
#define PHP_CLIPS_EXTNAME "clips"

#include "../config.h"
#include <stdio.h>
#include "php.h"
#include "php_ini.h"
#include "utils.h"
#include "clips/clips.h"

PHP_FUNCTION(clips_init);
PHP_FUNCTION(clips_close);
PHP_FUNCTION(clips_console);
PHP_FUNCTION(clips_load);
PHP_FUNCTION(clips_exec);
PHP_FUNCTION(clips_is_command_complete);
PHP_FUNCTION(clips_query_facts);

extern zend_module_entry clips_module_entry;
#define phpext_clips_ptr &clips_module_entry
#define UNSPECIFIED -1L

void* p_clips_env;

#endif
