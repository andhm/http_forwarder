/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: andhm@126.com                                                |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_FORWARDER_H
#define PHP_HTTP_FORWARDER_H

#include <curl/curl.h>

extern zend_module_entry http_forwarder_module_entry;
#define phpext_http_forwarder_ptr &http_forwarder_module_entry

#define PHP_HTTP_FORWARDER_VERSION "0.1.0"

#ifdef PHP_WIN32
#	define PHP_HTTP_FORWARDER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_HTTP_FORWARDER_API __attribute__ ((visibility("default")))
#else
#	define PHP_HTTP_FORWARDER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(http_forwarder);
PHP_MSHUTDOWN_FUNCTION(http_forwarder);
PHP_RINIT_FUNCTION(http_forwarder);
PHP_RSHUTDOWN_FUNCTION(http_forwarder);
PHP_MINFO_FUNCTION(http_forwarder);

PHP_FUNCTION(http_forward);
PHP_FUNCTION(hf_get_last_errmsg);
PHP_FUNCTION(hf_get_last_errno);

ZEND_BEGIN_MODULE_GLOBALS(http_forwarder)
	CURLM *multi_handle;
  ulong timeout;
  char *request_list_name;
  uint request_list_len;
  int g_errno;
  char *g_errmsg;
ZEND_END_MODULE_GLOBALS(http_forwarder)


#ifdef ZTS
#define HTTP_FORWARDER_G(v) TSRMG(http_forwarder_globals_id, zend_http_forwarder_globals *, v)
#else
#define HTTP_FORWARDER_G(v) (http_forwarder_globals.v)
#endif

#ifndef PHP_FE_END
#define PHP_FE_END { NULL, NULL, NULL }
#endif

#define HF_ERR_SUCC 0x0
#define HF_ERR_INPUT 0x01
#define HF_ERR_SYSTEM 0x02

#endif	/* PHP_HTTP_FORWARDER_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
