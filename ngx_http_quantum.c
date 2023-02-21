#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_flag_t Enabled;
} ngx_http_quantum_variables_conf_t;

void* ngx_http_quantum_variables_create_conf(ngx_conf_t* cf);
char* ngx_http_quantum_variables_merge_conf(
    ngx_conf_t* cf, void* parent, void* child);

ngx_int_t ngx_http_quantum_variables_init(ngx_conf_t* cf);

ngx_command_t ngx_http_quantum_variables_commands[] = {
  { ngx_string("quantum_variables"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_quantum_variables_conf_t, Enabled),
    NULL },

  ngx_null_command
};

ngx_http_module_t ngx_http_quantum_variables_module_ctx = {
  NULL, /* preconfiguration */
  ngx_http_quantum_variables_init, /* postconfiguration */

  NULL, /* create main configuration */
  NULL, /* init main configuration */

  NULL, /* create server configuration */
  NULL, /* merge server configuration */

  ngx_http_quantum_variables_create_conf, /* create location configuration */
  ngx_http_quantum_variables_merge_conf   /* merge location configuration */
};

ngx_module_t ngx_http_quantum_variables_module = {
  NGX_MODULE_V1,
  &ngx_http_quantum_variables_module_ctx, /* module context */
  ngx_http_quantum_variables_commands, /* module directives */
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

void* ngx_http_quantum_variables_create_conf(ngx_conf_t* cf) {
  ngx_http_quantum_variables_conf_t* conf =
      ngx_pcalloc(cf->pool, sizeof(ngx_http_quantum_variables_conf_t));
  if (!conf) {
    return NULL;
  }

  conf->Enabled = NGX_CONF_UNSET;
  return conf;
}

char* ngx_http_quantum_variables_merge_conf(
    ngx_conf_t* cf, void* parent, void* child) {
  ngx_http_quantum_variables_conf_t* prev = parent;
  ngx_http_quantum_variables_conf_t* conf = child;
  ngx_conf_merge_value(conf->Enabled, prev->Enabled, 0);
  return NGX_CONF_OK;
}

ngx_int_t ngx_http_quantum_variables_init(ngx_conf_t* cf) {
  return NGX_OK;
}
