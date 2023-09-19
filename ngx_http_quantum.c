#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_HTTP_DELAY_BODY 1000

typedef struct {
  ngx_flag_t enable;
} ngx_http_quantum_conf_t;

typedef struct {
  ngx_event_t event;
  ngx_chain_t* out;
} ngx_http_quantum_body_ctx_t;

static char* ngx_http_quantum_set(
    ngx_conf_t* cf,
    ngx_command_t* cmd,
    void* conf);
static ngx_int_t ngx_http_quantum_init(ngx_conf_t* cf);
static ngx_int_t ngx_http_quantum_header_filter(ngx_http_request_t* r);
static ngx_int_t ngx_http_delay_body_filter(
    ngx_http_request_t *r, ngx_chain_t *in);

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
  ngx_http_quantum_init, /* postconfiguration */

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

static ngx_str_t phase_content = ngx_string("Phase-Content");
static ngx_str_t phase_preaccess = ngx_string("Phase-Preaccess");
static ngx_str_t ngx_http_json_type = ngx_string("application/json");
static ngx_str_t ngx_return_json =
  ngx_string("{\"message\": \"\xF0\x9F\x98\x8E\"}");

static ngx_str_t* lookup_headers(
    ngx_list_t* headers,
    const ngx_str_t* name) {
  ngx_list_part_t* part = &headers->part;
  ngx_table_elt_t* header = part->elts;
  for (ngx_uint_t i = 0; ; ++i) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = part->elts;
      i = 0;
    }

    if (header[i].key.len == name->len
      && ngx_strncmp(header[i].key.data, name->data, name->len) == 0) {
      return &header[i].value;
    }
  }

  return NULL;
}

static ngx_int_t ngx_http_quantum_handler_internal(
    ngx_http_request_t* r,
    const ngx_str_t* targetKey) {
  ngx_str_t* header_value = lookup_headers(&r->headers_in.headers, targetKey);
  if (header_value == NULL) {
    return NGX_DECLINED;
  }

  if (header_value->len == 3
      && ngx_strncmp(header_value->data, "foo", 3) == 0) {
    return NGX_HTTP_FORBIDDEN;
  }

  ngx_http_complex_value_t cv;
  ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));
  cv.value = ngx_return_json;

  return ngx_http_send_response(r, NGX_HTTP_OK, &ngx_http_json_type, &cv);
}

static ngx_int_t ngx_http_quantum_handler(ngx_http_request_t* r) {
  return ngx_http_quantum_handler_internal(r, &phase_content);
}

static ngx_int_t ngx_http_quantum_preaccess_handler(ngx_http_request_t* r) {
  return ngx_http_quantum_handler_internal(r, &phase_preaccess);
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

static ngx_http_request_body_filter_pt ngx_http_next_request_body_filter;
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t ngx_http_quantum_init(ngx_conf_t* cf) {
  ngx_http_core_main_conf_t* cmcf =
    ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  ngx_http_handler_pt* h =
    ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_quantum_preaccess_handler;

  // Request body filter
  ngx_http_next_request_body_filter = ngx_http_top_request_body_filter;
  ngx_http_top_request_body_filter = ngx_http_delay_body_filter;
  // Response header filter
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_quantum_header_filter;

  return NGX_OK;
}

static ngx_int_t ngx_http_quantum_header_filter(ngx_http_request_t* r) {
  if (r->headers_out.status != NGX_HTTP_FORBIDDEN) {
    return ngx_http_next_header_filter(r);
  }

  ngx_table_elt_t* h = ngx_list_push(&r->headers_out.headers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  h->hash = 1;
  ngx_str_set(&h->key, "X-Foo");
  ngx_str_set(&h->value, "foo");
  return ngx_http_next_header_filter(r);
}

static void ngx_http_delay_body_cleanup(void* data) {
  ngx_http_quantum_body_ctx_t *ctx = data;
  if (ctx->event.timer_set) {
    ngx_del_timer(&ctx->event);
  }
}

static void ngx_http_delay_body_event_handler(ngx_event_t *ev) {
  ngx_http_request_t* r = ev->data;
  ngx_connection_t* c = r->connection;
  ngx_post_event(c->read, &ngx_posted_events);
}

static ngx_int_t ngx_http_delay_body_filter(
    ngx_http_request_t *r, ngx_chain_t *in) {
  ngx_http_quantum_body_ctx_t* ctx =
    ngx_http_get_module_ctx(r, ngx_http_quantum_module);
  if (ctx == NULL) {
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_quantum_body_ctx_t));
    if (ctx == NULL) {
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_quantum_module);

    r->request_body->filter_need_buffering = 1;
  }

  if (ngx_chain_add_copy(r->pool, &ctx->out, in) != NGX_OK) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  if (!ctx->event.timedout) {
    if (!ctx->event.timer_set) {
      /* cleanup to remove the timer in case of abnormal termination */
      ngx_http_cleanup_t* cln = ngx_http_cleanup_add(r, 0);
      if (cln == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
      }

      cln->handler = ngx_http_delay_body_cleanup;
      cln->data = ctx;

      /* add timer */
      ctx->event.handler = ngx_http_delay_body_event_handler;
      ctx->event.data = r;
      ctx->event.log = r->connection->log;
      ngx_add_timer(&ctx->event, NGX_HTTP_DELAY_BODY);
    }

    return ngx_http_next_request_body_filter(r, NULL);
  }

  ngx_int_t rc = ngx_http_next_request_body_filter(r, ctx->out);
  for (ngx_chain_t* cl = ctx->out; cl; /* void */) {
    ngx_chain_t* ln = cl;
    cl = cl->next;
    ngx_free_chain(r->pool, ln);
  }

  ctx->out = NULL;
  return rc;
}
