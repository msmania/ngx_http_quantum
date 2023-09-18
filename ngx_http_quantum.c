#include <ngx_config.h>
#include <ngx_core.h>

typedef struct {
  ngx_flag_t enable;
} ngx_http_quantum_conf_t;

static void *ngx_http_quantum_create_conf(ngx_cycle_t *cycle);
static char *ngx_http_quantum_init_conf(ngx_cycle_t *cycle, void *conf);

static char *ngx_http_quantum_enable(ngx_conf_t *cf, void *post, void *data);
static ngx_conf_post_t ngx_http_quantum_enable_post = {
  ngx_http_quantum_enable
};

static ngx_command_t ngx_http_quantum_commands[] = {
  { ngx_string("quantum_enabled"),
    NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    0,
    offsetof(ngx_http_quantum_conf_t, enable),
    &ngx_http_quantum_enable_post },

  ngx_null_command
};

static ngx_core_module_t ngx_http_quantum_module_ctx = {
  ngx_string("quantum"),
  ngx_http_quantum_create_conf,
  ngx_http_quantum_init_conf,
};

ngx_module_t ngx_http_quantum_module = {
  NGX_MODULE_V1,
  &ngx_http_quantum_module_ctx, /* module context */
  ngx_http_quantum_commands, /* module directives */
  NGX_CORE_MODULE, /* module type */
  NULL, /* init master */
  NULL, /* init module */
  NULL, /* init process */
  NULL, /* init thread */
  NULL, /* exit thread */
  NULL, /* exit process */
  NULL, /* exit master */
  NGX_MODULE_V1_PADDING
};

static void* ngx_http_quantum_create_conf(ngx_cycle_t *cycle) {
  ngx_http_quantum_conf_t *fcf =
    ngx_pcalloc(cycle->pool, sizeof(ngx_http_quantum_conf_t));
  if (fcf == NULL) {
    return NULL;
  }

  fcf->enable = NGX_CONF_UNSET;
  return fcf;
}

static char* ngx_http_quantum_init_conf(ngx_cycle_t* cycle, void* conf) {
  ngx_http_quantum_conf_t* fcf = conf;
  ngx_conf_init_value(fcf->enable, 0);
  return NGX_CONF_OK;
}

static char* ngx_http_quantum_enable(ngx_conf_t* cf, void* post, void* data) {
  ngx_flag_t* fp = data;
  if (*fp == 0) {
    return NGX_CONF_OK;
  }

  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "Quantum Module is enabled");
  return NGX_CONF_OK;
}
