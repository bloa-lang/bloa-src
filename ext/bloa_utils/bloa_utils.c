/* bloa_utils extension for PHP */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_bloa_utils.h"
#include "bloa_utils_arginfo.h"
#include "ext/standard/php_string.h"

PHP_FUNCTION(shout)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_STR(php_string_toupper(str));
}

PHP_RINIT_FUNCTION(bloa_utils)
{
#if defined(ZTS) && defined(COMPILE_DL_BLOA_UTILS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

PHP_MINFO_FUNCTION(bloa_utils)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "bloa_utils support", "enabled");
	php_info_print_table_end();
}

zend_module_entry bloa_utils_module_entry = {
	STANDARD_MODULE_HEADER,
	"bloa_utils",					/* Extension name */
	ext_functions,					/* zend_function_entry */
	NULL,							/* PHP_MINIT - Module initialization */
	NULL,							/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(bloa_utils),			/* PHP_RINIT - Request initialization */
	NULL,							/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(bloa_utils),			/* PHP_MINFO - Module info */
	PHP_BLOA_UTILS_VERSION,		/* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_BLOA_UTILS
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(bloa_utils)
#endif
