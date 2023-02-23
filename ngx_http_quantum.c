#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_uint_t Throttle;
  ngx_uint_t RequestTimeThresholdMsec;
  off_t MaxOutput;
  ngx_uint_t IndexRequestBody;
} ngx_http_quantum_variables_conf_t;

typedef struct {
  int Active;
  off_t BufferSize;
  off_t Size;
  u_char* Buffer;
} ngx_http_quantum_ctx_t;

static ngx_int_t ngx_http_quantum_add_variables(ngx_conf_t *cf);
static ngx_int_t ngx_http_quantum_variables_init(ngx_conf_t* cf);
static void* ngx_http_quantum_variables_create_conf(ngx_conf_t* cf);
static char* ngx_http_quantum_variables_merge_conf(
    ngx_conf_t* cf, void* parent, void* child);

static ngx_int_t ngx_http_quantum_body_filter(
    ngx_http_request_t *r, ngx_chain_t *in);
static ngx_int_t get_quantum_variable_reqres(
   ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

static const int INIT_BUFFER_SIZE = 1024;

static ngx_http_output_body_filter_pt  ngx_http_next_body_filter;
static ngx_str_t kRequestBody = ngx_string("request_body");

static ngx_http_variable_t ngx_http_quantum_vars[] = {
  { ngx_string("quantum"), NULL, get_quantum_variable_reqres, 0, 0, 0 },

  ngx_http_null_variable
};

ngx_command_t ngx_http_quantum_variables_commands[] = {
  { ngx_string("quantum_throttle"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_num_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_quantum_variables_conf_t, Throttle),
    NULL },
  { ngx_string("quantum_request_time_threshold_ms"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_num_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_quantum_variables_conf_t, RequestTimeThresholdMsec),
    NULL },
  { ngx_string("quantum_max_output"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_off_slot ,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_quantum_variables_conf_t, MaxOutput),
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

static ngx_int_t ngx_http_quantum_variables_init(ngx_conf_t* cf) {
  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_quantum_body_filter;
  return NGX_OK;
}

static void* ngx_http_quantum_variables_create_conf(ngx_conf_t* cf) {
  ngx_http_quantum_variables_conf_t* conf =
      ngx_pcalloc(cf->pool, sizeof(ngx_http_quantum_variables_conf_t));
  if (!conf) {
    return NULL;
  }

  conf->Throttle = NGX_CONF_UNSET_UINT;
  conf->RequestTimeThresholdMsec = NGX_CONF_UNSET_UINT;
  conf->MaxOutput = NGX_CONF_UNSET;
  return conf;
}

static char* ngx_http_quantum_variables_merge_conf(
    ngx_conf_t* cf, void* parent, void* child) {
  ngx_http_quantum_variables_conf_t* prev = parent;
  ngx_http_quantum_variables_conf_t* conf = child;

  ngx_conf_merge_uint_value(conf->Throttle, prev->Throttle, 0);
  conf->Throttle = ngx_min(conf->Throttle, 100);
  ngx_conf_merge_uint_value(
      conf->RequestTimeThresholdMsec, prev->RequestTimeThresholdMsec, 0);
  ngx_conf_merge_off_value(conf->MaxOutput, prev->MaxOutput, 32);

  conf->IndexRequestBody = ngx_http_get_variable_index(cf, &kRequestBody);

  return NGX_CONF_OK;
}

static ngx_int_t ngx_http_quantum_body_filter(
    ngx_http_request_t *r, ngx_chain_t *in) {
  ngx_http_quantum_ctx_t* ctx =
    ngx_http_get_module_ctx(r, ngx_http_quantum_variables_module);
  if (ctx == NULL) {
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_quantum_ctx_t));
    if (ctx == NULL) {
      return NGX_ERROR;
    }

    ngx_http_quantum_variables_conf_t* conf =
        ngx_http_get_module_loc_conf(r, ngx_http_quantum_variables_module);

    ngx_uint_t rnd = rand() % 100;
    if (rnd < conf->Throttle) {
      ngx_time_t* tp = ngx_timeofday();
      ngx_msec_int_t request_time_ms = (tp->sec - r->start_sec) * 1000
          + (tp->msec - r->start_msec);
      request_time_ms = ngx_max(request_time_ms, 0);
      if ((ngx_uint_t)request_time_ms >= conf->RequestTimeThresholdMsec) {
        ctx->Active = 1;
      }
      // ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
      //     "quantum: r=%p ctx=%p rt=%i", r, ctx, request_time_ms);
    }

    ngx_http_set_ctx(r, ctx, ngx_http_quantum_variables_module);
  }

  if (!ctx->Active) {
    return ngx_http_next_body_filter(r, in);
  }

  if (!ctx->Buffer) {
    u_char* newbuf = ngx_pnalloc(r->pool, INIT_BUFFER_SIZE);
    if (newbuf == NULL) {
      return NGX_ERROR;
    }
    ctx->Buffer = newbuf;
    ctx->BufferSize = INIT_BUFFER_SIZE;
  }

  for (ngx_chain_t* cl = in; cl; cl = cl->next) {
    off_t size = ngx_buf_size(cl->buf);
    off_t required_size = ctx->Size + size;
    off_t bufsize_new = ctx->BufferSize;
    while (bufsize_new < required_size) {
      bufsize_new *= 2;
    }

    u_char* buffer_end = ctx->Buffer + ctx->Size;

    if (bufsize_new > ctx->BufferSize) {
      u_char* buffer_new = ngx_pnalloc(r->pool, bufsize_new);
      if (buffer_new == NULL) {
        return NGX_ERROR;
      }
      buffer_end = ngx_cpymem(buffer_new, ctx->Buffer, ctx->Size);
      ctx->Buffer = buffer_new;
      ctx->BufferSize = bufsize_new;
    }

    ngx_memcpy(buffer_end, cl->buf->pos, size);
    ctx->Size += size;
  }

  return ngx_http_next_body_filter(r, in);
}

static ngx_int_t get_quantum_variable_reqres(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  ngx_http_quantum_ctx_t* ctx =
      ngx_http_get_module_ctx(r, ngx_http_quantum_variables_module);
  if (ctx == NULL || !ctx->Active) {
    static u_char kHyphen[] = "-";
    v->len = 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = kHyphen;
    return NGX_OK;
  }

  ngx_http_quantum_variables_conf_t* conf =
      ngx_http_get_module_loc_conf(r, ngx_http_quantum_variables_module);

  ngx_str_t response_body = {
    ngx_min(ctx->Size, conf->MaxOutput),
    ctx->Buffer,
  };

  ngx_http_variable_value_t* reqbody =
    ngx_http_get_indexed_variable(r, conf->IndexRequestBody);
  ngx_str_t request_body = ngx_null_string;
  if (reqbody != NULL && !reqbody->not_found) {
    request_body.len = ngx_min(reqbody->len, conf->MaxOutput);
    request_body.data = reqbody->data;
  }

  u_char* p = ngx_pnalloc(r->pool,
      request_body.len + 4 + response_body.len);
  if (p == NULL) {
    return NGX_ERROR;
  }
  v->data = p;
  v->len = ngx_sprintf(p, "%V -> %V", &request_body, &response_body) - p;
  v->valid = 1;
  v->no_cacheable = 0;
  v->not_found = 0;
  return NGX_OK;
}
