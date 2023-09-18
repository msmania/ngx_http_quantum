#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static char* ngx_http_quantum_set(
    ngx_conf_t* cf,
    ngx_command_t* cmd,
    void* conf);

static ngx_command_t ngx_http_quantum_commands[] = {
  { ngx_string("foo"),
    NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
    ngx_http_quantum_set,
    0,
    0,
    NULL },

  ngx_null_command
};

static ngx_http_module_t ngx_http_quantum_module_ctx = {
  NULL, /* preconfiguration */
  NULL, /* postconfiguration */

  NULL, /* create main configuration */
  NULL, /* init main configuration */

  NULL, /* create server configuration */
  NULL, /* merge server configuration */

  NULL, /* create location configuration */
  NULL /* merge location configuration */
};

ngx_module_t ngx_http_quantum_module = {
  NGX_MODULE_V1,
  &ngx_http_quantum_module_ctx, /* module context */
  ngx_http_quantum_commands, /* module directives */
  NGX_HTTP_MODULE, /* module type */
  NULL, /* init master */
  NULL, /* init module */
  NULL, /* init process */
  NULL, /* init thread */
  NULL, /* exit thread */
  NULL, /* exit process */
  NULL, /* exit master */
  NGX_MODULE_V1_PADDING
};

static ngx_str_t ngx_http_json_type = ngx_string("application/json");
static ngx_str_t ngx_return_json =
  ngx_string("{\"message\": \"\xF0\x9F\x98\x8E\"}");

static ngx_int_t ngx_http_quantum_handler(ngx_http_request_t* r) {
  ngx_table_elt_t* ua = r->headers_in.user_agent;
  if (ua == NULL) {
    return NGX_DECLINED;
  }

  if (ua->value.len == 3 && ngx_strncmp(ua->value.data, "foo", 3) == 0) {
    ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0, "Rejecting foo");
    return NGX_HTTP_FORBIDDEN;
  }

  ngx_http_complex_value_t cv;
  ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));
  cv.value = ngx_return_json;

  return ngx_http_send_response(r, NGX_HTTP_OK, &ngx_http_json_type, &cv);
}

static char* ngx_http_quantum_set(
    ngx_conf_t *cf,
    ngx_command_t* cmd,
    void* conf) {
  ngx_http_core_loc_conf_t* clcf =
    ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_quantum_handler;

  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "Set ngx_http_quantum_handler");
  return NGX_CONF_OK;
}
