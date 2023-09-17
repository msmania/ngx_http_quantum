#include <ngx_config.h>
#include <ngx_core.h>

static ngx_core_module_t ngx_http_quantum_module_ctx = {
  ngx_string("quantum"),
  NULL,
  NULL,
};

ngx_module_t ngx_http_quantum_module = {
  NGX_MODULE_V1,
  &ngx_http_quantum_module_ctx, /* module context */
  NULL, /* module directives */
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
