#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_flag_t Enabled;
  ngx_int_t IndexRequestBody;
} ngx_http_quantum_variables_conf_t;

static ngx_int_t ngx_http_quantum_add_variables(ngx_conf_t *cf);
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
  ngx_http_quantum_add_variables, /* preconfiguration */
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

static ngx_str_t kRequestBody = ngx_string("request_body");

char* ngx_http_quantum_variables_merge_conf(
    ngx_conf_t* cf, void* parent, void* child) {
  ngx_http_quantum_variables_conf_t* prev = parent;
  ngx_http_quantum_variables_conf_t* conf = child;
  ngx_conf_merge_value(conf->Enabled, prev->Enabled, 0);
  conf->IndexRequestBody = ngx_http_get_variable_index(cf, &kRequestBody);
  return NGX_CONF_OK;
}

ngx_int_t ngx_http_quantum_variables_init(ngx_conf_t* cf) {
  return NGX_OK;
}

static ngx_int_t get_quantum_variable_reqres(
   ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

static ngx_http_variable_t ngx_http_quantum_vars[] = {
  { ngx_string("quantum"), NULL, get_quantum_variable_reqres, 0, 0, 0 },

  ngx_http_null_variable
};

static ngx_int_t ngx_http_quantum_add_variables(ngx_conf_t *cf) {
  ngx_http_variable_t  *var, *v;
  for (v = ngx_http_quantum_vars; v->name.len; v++) {
    var = ngx_http_add_variable(cf, &v->name, v->flags);
    if (var == NULL) {
      return NGX_ERROR;
    }
    var->get_handler = v->get_handler;
    var->data = v->data;
  }
  return NGX_OK;
}

static ngx_int_t get_quantum_variable_reqres(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  static u_char kHyphen[] = "-";
  int rnd = rand() % 100;
  if (rnd >= 30) {
    v->len = 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = kHyphen;
    return NGX_OK;
  }

  ngx_http_quantum_variables_conf_t* conf =
      ngx_http_get_module_loc_conf(r, ngx_http_quantum_variables_module);
  ngx_http_variable_value_t* value =
    ngx_http_get_indexed_variable(r, conf->IndexRequestBody);
  if (value == NULL || value->not_found) {
    v->not_found = 1;
    v->valid = 0;
    return NGX_OK;
  }

  v->len = value->len;
  v->valid = 1;
  v->no_cacheable = 0;
  v->not_found = 0;
  v->data = value->data;
  return NGX_OK;
}
