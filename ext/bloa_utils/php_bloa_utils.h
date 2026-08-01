/* bloa_utils extension for PHP */

#ifndef PHP_BLOA_UTILS_H
# define PHP_BLOA_UTILS_H

extern zend_module_entry bloa_utils_module_entry;
# define phpext_bloa_utils_ptr &bloa_utils_module_entry

# define PHP_BLOA_UTILS_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_BLOA_UTILS)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_BLOA_UTILS_H */
