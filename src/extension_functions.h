#ifndef PHP_CLIPS_EXTENSION_FUNCTIONS
#define PHP_CLIPS_EXTENSION_FUNCTIONS

#include <php.h>
#include "clips/clips.h"

void php_call(void* env, DATA_OBJECT_PTR p_return_val);

void convert_do2php(void* p_clips_env, DATA_OBJECT data, zval* pzv_val);

#endif
