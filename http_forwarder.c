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
  | Author: Min Huang <andhm@126.com>                                    |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "SAPI.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/url.h"
#include "php_http_forwarder.h"

#define GET "get"
#define POST "post"
#define MAX_REQ_NUM 50
#define BUFFER_SIZE 1024

#define DEFAULT_REQUEST_LIST_NAME "request_list"

ZEND_DECLARE_MODULE_GLOBALS(http_forwarder)

typedef struct _response_t {
	int http_code;
	smart_str resp_buf;
} response_t;

typedef struct _request_t {
	char *url;
	char *method;
	smart_str *body;
	struct curl_slist *headers;
	zval key;
	response_t resp;
	struct _request_t *next;
} request_t;

typedef struct _curl_plink_t {
	CURL *curl_handle;
	int in_use;
	request_t *req;
	struct _curl_plink_t *next;
} curl_plink_t;


/* True global resources - no need for thread safety here */
static int le_curl;

#define SET_ERROR(errno, errmsg) 	\
do {								\
	HTTP_FORWARDER_G(g_errno) = errno;				\
	HTTP_FORWARDER_G(g_errmsg) = errmsg;				\
} while(0);					

static inline int check_request(zval *request, request_t *out_request TSRMLS_DC);
static inline int check_url(zval *url);
static inline int check_method(zval *method);

// curl
static inline int curl_multi_do(request_t *request, int request_num TSRMLS_DC);
static inline int curl_multi_select(CURLM *curl_m TSRMLS_DC);
size_t curl_writer(void *buffer, size_t size, size_t count, void *ctx);

static void curl_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
	curl_plink_t *plink = (curl_plink_t *)rsrc->ptr;
	curl_easy_cleanup(plink->curl_handle);
	free(plink);
}

const zend_function_entry http_forwarder_functions[] = {
	PHP_FE(hf_get_last_errno, NULL)
	PHP_FE(hf_get_last_errmsg, NULL)
	PHP_FE(http_forward, NULL)
	PHP_FE_END
};

/* {{{ http_forwarder_module_entry
 */
zend_module_entry http_forwarder_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"http_forwarder",
	http_forwarder_functions,
	PHP_MINIT(http_forwarder),
	PHP_MSHUTDOWN(http_forwarder),
	PHP_RINIT(http_forwarder),
	NULL,
	PHP_MINFO(http_forwarder),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_HTTP_FORWARDER_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HTTP_FORWARDER
ZEND_GET_MODULE(http_forwarder)
#endif

ZEND_INI_MH(php_http_forwarder_modify_request_list) 
{
	if (new_value_length == 0 || new_value_length > 100) {
		return FAILURE;
	}

	HTTP_FORWARDER_G(request_list_name) = new_value;
	HTTP_FORWARDER_G(request_list_len) = new_value_length;
	return SUCCESS;
}

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("http_forwarder.request_list_name", DEFAULT_REQUEST_LIST_NAME, PHP_INI_ALL, php_http_forwarder_modify_request_list, request_list_name, zend_http_forwarder_globals, http_forwarder_globals)
    STD_PHP_INI_ENTRY("http_forwarder.timeout", "5000", PHP_INI_ALL, OnUpdateLong, timeout, zend_http_forwarder_globals, http_forwarder_globals)
PHP_INI_END()

/* }}} */

/* {{{ php_http_forwarder_init_globals
 */
static void php_http_forwarder_init_globals(zend_http_forwarder_globals *http_forwarder_globals)
{
	http_forwarder_globals->g_errno = HF_ERR_SUCC;
	http_forwarder_globals->g_errmsg = NULL;

	//http_forwarder_globals->request_list_len = strlen(DEFAULT_REQUEST_LIST_NAME);
}

/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(http_forwarder)
{
	REGISTER_INI_ENTRIES();
	le_curl = zend_register_list_destructors_ex(NULL, curl_dtor, "http_forwarder curl link", module_number);
	HTTP_FORWARDER_G(multi_handle) = curl_multi_init();
	HTTP_FORWARDER_G(request_list_len) = strlen(DEFAULT_REQUEST_LIST_NAME);
	
	REGISTER_LONG_CONSTANT("HF_VERSION", PHP_HTTP_FORWARDER_VERSION, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HF_ERR_SUCC", HF_ERR_SUCC, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HF_ERR_INPUT", HF_ERR_INPUT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HF_ERR_SYSTEM", HF_ERR_SYSTEM, CONST_CS|CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(http_forwarder)
{
	UNREGISTER_INI_ENTRIES();
	curl_multi_cleanup(HTTP_FORWARDER_G(multi_handle));
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(http_forwarder)
{
	ZEND_INIT_MODULE_GLOBALS(http_forwarder, php_http_forwarder_init_globals, NULL);
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(http_forwarder)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "http_forwarder support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */


PHP_FUNCTION(hf_get_last_errno) 
{
	RETURN_LONG(HTTP_FORWARDER_G(g_errno));
}

PHP_FUNCTION(hf_get_last_errmsg) 
{
	if (HTTP_FORWARDER_G(g_errmsg) == NULL) {
		RETURN_NULL();
	}
	RETURN_STRING(HTTP_FORWARDER_G(g_errmsg), 1);
}

PHP_FUNCTION(http_forward) 
{
	if (!SG(request_info).request_method ||
		strcasecmp(SG(request_info).request_method, POST)) {
		SET_ERROR(HF_ERR_INPUT, "http method must be "POST);
		RETURN_FALSE;
	}
	
	zval **data;
	zval **request_list;
	if (zend_hash_find(&EG(symbol_table), "_POST", sizeof("_POST"), (void **) &data) == SUCCESS &&
		Z_TYPE_PP(data) == IS_ARRAY &&
		zend_hash_find(Z_ARRVAL_PP(data), HTTP_FORWARDER_G(request_list_name), HTTP_FORWARDER_G(request_list_len)+1, (void **) &request_list) == SUCCESS &&
		Z_TYPE_PP(request_list) == IS_ARRAY) {
		int cnt = Z_TYPE_PP(request_list) = zend_hash_num_elements(Z_ARRVAL_PP(request_list));
		if (cnt == 0) {
			SET_ERROR(HF_ERR_INPUT, "request contains at least one address");
			RETURN_FALSE;
		}
	
		zval **request;
		char *key;
		int idx;
		int i;
		request_t *p_request_list = (request_t *)ecalloc(cnt, sizeof(request_t));
		if (p_request_list == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "emalloc request_t failed");
			SET_ERROR(HF_ERR_SYSTEM, "malloc for request fail");
			RETURN_FALSE;
		}
	
		request_t *p_request = p_request_list;
		for (i=0; i<cnt; ++i) {
			zend_hash_get_current_data(Z_ARRVAL_PP(request_list), (void**)&request);
			if (check_request(*request, p_request TSRMLS_CC) != SUCCESS) {
				SET_ERROR(HF_ERR_INPUT, "invalid request parameters");
				RETURN_FALSE;
			}
			
			if (zend_hash_get_current_key(Z_ARRVAL_PP(request_list), &key, &idx, 0) == HASH_KEY_IS_STRING) {
				ZVAL_STRING(&p_request->key, key, 1);
			} else {
				ZVAL_LONG(&p_request->key, idx);
			}

			if (i == cnt-1) {
				p_request->next = NULL;
			} else {
				p_request->next = p_request+1;
				++p_request;
				zend_hash_move_forward(Z_ARRVAL_PP(request_list));
			}
		}

		int ret = curl_multi_do(p_request_list, cnt TSRMLS_CC);

		zval *resp;
		if (ret == 0) {
			// http return success
			ALLOC_INIT_ZVAL(resp);
			array_init(resp);
		}

		for (p_request=p_request_list; p_request!=NULL; p_request=p_request->next) {
			if (ret == 0) {
				// http return success
				zval *http_resp;
				ALLOC_INIT_ZVAL(http_resp);
				array_init(http_resp);
				if (Z_TYPE(p_request->key) == IS_STRING) {
					add_assoc_long(http_resp, "code", p_request->resp.http_code);
					add_assoc_stringl(http_resp, "response", p_request->resp.resp_buf.c, p_request->resp.resp_buf.len, 1);
					add_assoc_zval(resp, Z_STRVAL(p_request->key), http_resp);
				} else {
					add_assoc_long(http_resp, "code", p_request->resp.http_code);
					add_assoc_stringl(http_resp, "response", p_request->resp.resp_buf.c, p_request->resp.resp_buf.len, 1);
					add_index_zval(resp, Z_LVAL(p_request->key), http_resp);
				}
			}
			smart_str_free(&p_request->resp.resp_buf);
		}

		efree(p_request_list);

		if (ret == 0) {
			// http return success
			SET_ERROR(HF_ERR_SUCC, NULL);
			RETURN_ZVAL(resp, 1, 1);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "http execution failed or timeout");
			SET_ERROR(HF_ERR_SYSTEM, "http execution failed");
			RETURN_FALSE;
		}
	}
	
	SET_ERROR(HF_ERR_INPUT, "post body must contain like \"request_list\" and type is array");
	RETURN_FALSE;
}

static inline int check_request(zval *request, request_t *out_request TSRMLS_DC) {
	size_t newlen;
	if (Z_TYPE_P(request) == IS_STRING) {
		if (check_url((zval*)request) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid request url (%s)", Z_STRVAL_P(request));
			return FAILURE;
		}
		out_request->url = Z_STRVAL_P(request);
		out_request->method = GET; // default
		out_request->body = NULL;
		out_request->next = NULL;
		out_request->headers = curl_slist_append(out_request->headers, "User-Agent: PHP HTTP FORWARDER-" PHP_HTTP_FORWARDER_VERSION);
		memset(&out_request->resp.resp_buf, 0, sizeof(smart_str));
		smart_str_alloc(&out_request->resp.resp_buf, BUFFER_SIZE, 0);

		return SUCCESS;
	} else if (Z_TYPE_P(request) == IS_ARRAY) {
		int cnt = zend_hash_num_elements(Z_ARRVAL_P(request));
		if (cnt == 0) {
			return FAILURE;
		}

		char *key = NULL;
		int idx = 0;
		zend_hash_get_current_key(Z_ARRVAL_P(request), &key, &idx, 0);

		zval **elem;
		if (zend_hash_find(Z_ARRVAL_P(request), "url", sizeof("url"), (void**)&elem) != SUCCESS ||
			check_url(*elem) != SUCCESS) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid request url (%s)", Z_STRVAL_PP(elem));
			return FAILURE;
		}
		out_request->url = Z_STRVAL_PP(elem);
		
		if (zend_hash_find(Z_ARRVAL_P(request), "method", sizeof("method"), (void**)&elem) != SUCCESS ||
			check_method(*elem) != SUCCESS) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid request method (%s)", Z_STRVAL_PP(elem));
			return FAILURE;
		}
		out_request->method = Z_STRVAL_PP(elem);

		out_request->body = NULL;
		if (zend_hash_find(Z_ARRVAL_P(request), "body", sizeof("body"), (void**)&elem) == SUCCESS) {
			if (!strcasecmp(out_request->method, POST) && Z_STRLEN_PP(elem)) {
				out_request->body = (smart_str *)emalloc(sizeof(smart_str));
				memset(out_request->body, 0, sizeof(smart_str));
				smart_str_alloc(out_request->body, BUFFER_SIZE, 0);
				smart_str_appendl(out_request->body, Z_STRVAL_PP(elem), Z_STRLEN_PP(elem));
				smart_str_0(out_request->body);
			}
		}
		
		memset(&out_request->resp.resp_buf, 0, sizeof(smart_str));
		smart_str_alloc(&out_request->resp.resp_buf, BUFFER_SIZE, 0);
		out_request->next = NULL;

		out_request->headers = curl_slist_append(out_request->headers, "User-Agent: PHP HTTP FORWARDER-" PHP_HTTP_FORWARDER_VERSION);
		
		return SUCCESS;
	}

	return FAILURE;
}

static inline int check_url(zval *url) {
	if (Z_TYPE_P(url) != IS_STRING ||
		Z_STRLEN_P(url) <= 7) {
		// begin with http:// or https://
		return FAILURE;
	}

	char *p_url = Z_STRVAL_P(url);

	/*int i = 0;
	char http[7] = "http://";
	while (i < 7) {
		if (*p_url != http[i]) {
			return FAILURE;
		}
		if (*(p_url+1) == 's') {
			++p_url;			
		}
		++i;
		++p_url;
	}*/

	php_url *p_php_url;
	if (!(p_php_url=php_url_parse(p_url)) ||
		(p_php_url->scheme != NULL && 
			strncasecmp(p_php_url->scheme, "http", 4) && 
			strncasecmp(p_php_url->scheme, "https", 5))) {

		php_url_free(p_php_url);
		return FAILURE;
	}

	php_url_free(p_php_url);

	return SUCCESS;
}

static inline int check_method(zval *method) {
	if (Z_TYPE_P(method) != IS_STRING ||
		(strcasecmp(POST, Z_STRVAL_P(method)) &&
		strcasecmp(GET, Z_STRVAL_P(method)))) {
		return FAILURE;
	}

	return SUCCESS;
}


// CURL 

static inline int curl_multi_do(request_t *request, int request_num TSRMLS_DC) {
	char *key = "http_forwarder_plink";
	zend_rsrc_list_entry *le;
	curl_plink_t *p_curl_plink = NULL;
	curl_plink_t *curr_curl = NULL;
	curl_plink_t **pp_curr_curl = NULL;
	request_t *curr_request;
	CURL *p_curl;

	if (request_num > MAX_REQ_NUM) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "request num reached limit %d", MAX_REQ_NUM);
		return -1;
	}

	if (zend_hash_find(&EG(persistent_list), key, sizeof(key) + 1, (void **) &le) == FAILURE) {
		zend_rsrc_list_entry new_le;
		for (curr_request=request; curr_request!=NULL; curr_request=curr_request->next) {		
			p_curl = curl_easy_init();
			if (!p_curl) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "start curl failed");
				return -1;
			}

			if (curr_curl == NULL) {
				curr_curl = malloc(sizeof(curl_plink_t));
			} else {
				curr_curl->next = malloc(sizeof(curl_plink_t));
				curr_curl = curr_curl->next;
			}

			if (!curr_curl) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "malloc curl_plink_t failed");
				return -1;
			}

			curr_curl->curl_handle = p_curl;
			curr_curl->in_use = 0;
			curr_curl->next = NULL;
			curr_curl->req = NULL;

			if (p_curl_plink == NULL) {
				p_curl_plink = curr_curl;
			}
		}
	
		if (p_curl_plink == NULL) {			
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid request list");
			return -1;
		}

		curr_curl = p_curl_plink;
		while (curr_curl) {
			curr_curl = curr_curl->next;
		}

		Z_TYPE(new_le) = le_curl;
		new_le.ptr = p_curl_plink;

		if (zend_hash_update(&EG(persistent_list), key, sizeof(key) + 1, (void *)&new_le, sizeof(zend_rsrc_list_entry), NULL) == SUCCESS) {
			// or do something
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "update persistent_list failed");
			return -1;
		}

	} else {
		curl_plink_t *last_curl = NULL;
		int available_curl_num = 0;
		for (curr_curl=(curl_plink_t *)le->ptr; 
			curr_curl&&request_num-available_curl_num>0; 
			last_curl=curr_curl,curr_curl=curr_curl->next) {
			if (p_curl_plink == NULL) {
				p_curl_plink = curr_curl;
			}

			curl_easy_reset(curr_curl->curl_handle);
			curr_curl->in_use = 0;
			curr_curl->req = NULL;
			++available_curl_num;
		}

		curl_plink_t *append_curl = NULL;
		while (request_num - available_curl_num > 0) {
			p_curl = curl_easy_init();
			if (!p_curl) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "start curl failed");
				return -1;
			}
			append_curl = malloc(sizeof(curl_plink_t));
			if (append_curl == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "malloc curl_plink_t failed");
				return -1;
			}
			append_curl->curl_handle = p_curl;
			append_curl->in_use = 0;
			append_curl->next = NULL;
			append_curl->req = NULL;

			if (p_curl_plink == NULL) {
				p_curl_plink = append_curl;
			} else if (curr_curl) {
				curr_curl->next = append_curl;
				curr_curl = NULL;
			} else if (last_curl) {
				last_curl->next = append_curl;
				last_curl = NULL;
			}
			append_curl = append_curl->next;

			++available_curl_num;
		}
	}

	int request_index = request_num;
	curr_curl = p_curl_plink;
	curr_request = request;
	
	while (request_index > 0) {
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_HTTPHEADER, curr_request->headers);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_URL, curr_request->url);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_WRITEFUNCTION, curl_writer);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_WRITEDATA, curr_request);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_HEADER, 0);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_FOLLOWLOCATION, 0);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(curr_curl->curl_handle, CURLOPT_TIMEOUT, (ulong)(HTTP_FORWARDER_G(timeout)/1000));

		if (!strcasecmp(curr_request->method, POST)) {
			curl_easy_setopt(curr_curl->curl_handle, CURLOPT_POST, 1);
			if (curr_request->body != NULL) {
				curl_easy_setopt(curr_curl->curl_handle, CURLOPT_POSTFIELDS, curr_request->body->c);
			}
		}

		curl_multi_add_handle(HTTP_FORWARDER_G(multi_handle), curr_curl->curl_handle);
		curr_curl->in_use = 1;
		curr_curl->req = curr_request;
		curr_curl = curr_curl->next;
		curr_request = curr_request->next;
		--request_index;
	}

	int still_running = 0;
	curl_multi_perform(HTTP_FORWARDER_G(multi_handle), &still_running);
	int ret_code = 0;
	do {
		if ((ret_code=curl_multi_select(HTTP_FORWARDER_G(multi_handle) TSRMLS_CC)) <= 0) {
			if (ret_code == 0) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "select timeout %ldms reached", HTTP_FORWARDER_G(timeout));				
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "select failed %s", strerror(errno));
			}
			// return -1;
			break;
		}
		curl_multi_perform(HTTP_FORWARDER_G(multi_handle), &still_running);
	} while(still_running);

	
	curr_curl = p_curl_plink;
	while (curr_curl) {
		if (!curr_curl->in_use) {
			// or break?
			continue;
		}

		long http_code;
		if (curl_easy_getinfo(curr_curl->curl_handle, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "curl_easy_getinfo failed");
			http_code = 502;
		}

		curr_curl->req->resp.http_code = http_code;

		curl_multi_remove_handle(HTTP_FORWARDER_G(multi_handle), curr_curl->curl_handle);
		curr_curl = curr_curl->next;
	}

	
	for (request_index = request_num, curr_request = request; 
		request_index > 0; 
		--request_index, curr_request = curr_request->next) {
		curl_slist_free_all(curr_request->headers);
		if (strcasecmp(curr_request->method, POST) || curr_request->body == NULL) {
			continue;
		}

		smart_str_free(curr_request->body);
		efree(curr_request->body);
		curr_request->body = NULL;
	}

	if (ret_code <= 0) {
		// for select error
		return -1;
	}

	return 0;

}

static inline int curl_multi_select(CURLM *curl_m TSRMLS_DC) {
	struct timeval timeout_tv;
	fd_set fd_read;
	fd_set fd_write;
	fd_set fd_except;
	int max_fd = -1;

	FD_ZERO(&fd_read);
	FD_ZERO(&fd_write);
	FD_ZERO(&fd_except);

	timeout_tv.tv_sec = (ulong)(HTTP_FORWARDER_G(timeout)/1000);
	timeout_tv.tv_usec = (ulong)((HTTP_FORWARDER_G(timeout)%1000) ? (HTTP_FORWARDER_G(timeout)&1000)*1000 : 0);
	curl_multi_fdset(curl_m, &fd_read, &fd_write, &fd_except, &max_fd);

	int ret_code = -1;
	if (max_fd == -1) {
		struct timeval wait = {0, 5*1000}; // 5ms
		ret_code = select(1, NULL, NULL, NULL, &wait);
	} else {
		ret_code = select(max_fd + 1, &fd_read, &fd_write, &fd_except, &timeout_tv);
	}

	return ret_code;
}

size_t curl_writer(void *buffer, size_t size, size_t count, void *ctx) {
	request_t *request = (request_t *)ctx;
	size_t len = size * count;
	smart_str_appendl(&request->resp.resp_buf, buffer, len);
	return len;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
