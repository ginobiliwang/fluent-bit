/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019      The Fluent Bit Authors
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_output.h>
#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_pack.h>
#include <fluent-bit/flb_sds.h>

#include "websocket.h"
#include "websocket_conf.h"

struct flb_out_ws *flb_ws_conf_create(struct flb_output_instance *ins,
                                          struct flb_config *config)
{
    int ret;
    int ulen;
    int io_flags = 0;
    char *uri = NULL;
    char *tmp_uri = NULL;
    const char *tmp;
    int idle_interval;
    struct flb_upstream *upstream;
    struct flb_out_ws *ctx = NULL;

    /* Allocate plugin context */
    ctx = flb_calloc(1, sizeof(struct flb_out_ws));
    if (!ctx) {
        flb_errno();
        return NULL;
    }

    /* Check if SSL/TLS is enabled */
#ifdef FLB_HAVE_TLS
    if (ins->use_tls == FLB_TRUE) {
        io_flags = FLB_IO_TLS;
    }
    else {
        io_flags = FLB_IO_TCP;
    }
#else
    io_flags = FLB_IO_TCP;
#endif

    if (ins->flags & FLB_IO_TCP_KA) {
        io_flags |= FLB_IO_TCP_KA;
    }

    upstream = flb_upstream_create(config, ins->host.name, ins->host.port, io_flags, (void *)&ins->tls);
    if (!upstream) {
        flb_free(ctx);
        return NULL;
    }

    /* Output format */
    ctx->out_format = FLB_PACK_JSON_FORMAT_NONE;
    tmp = flb_output_get_property("format", ins);
    if (tmp) {
        ret = flb_pack_to_json_format_type(tmp);
        if (ret == -1) {
            flb_error("[out_ws] unrecognized 'format' option '%s'. Using 'msgpack'", tmp);
        }
        else {
            ctx->out_format = ret;
        }
    }

    /* Date format for JSON output */
    ctx->json_date_format = FLB_PACK_JSON_DATE_DOUBLE;
    tmp = flb_output_get_property("json_date_format", ins);
    if (tmp) {
        ret = flb_pack_to_json_date_type(tmp);
        if (ret == -1) {
            flb_error("[out_ws] unrecognized 'json_date_format' option '%s'. Using 'double'", tmp);
        }
        else {
            ctx->json_date_format = ret;
        }
    }

    /* Date key for JSON output */
    tmp = flb_output_get_property("json_date_key", ins);
    if (tmp) {
        ctx->json_date_key = flb_sds_create(tmp);
    }
    else {
        ctx->json_date_key = flb_sds_create("date");
    }

    if (ins->host.uri) {
        uri = flb_strdup(ins->host.uri->full);
    }
    else {
        tmp = flb_output_get_property("uri", ins);
        if (tmp) {
            uri = flb_strdup(tmp);
        }
    }

    if (!uri) {
        uri = flb_strdup("/");
    }
    else if (uri[0] != '/') {
        ulen = strlen(uri);
        tmp_uri = flb_malloc(ulen + 2);
        tmp_uri[0] = '/';
        memcpy(tmp_uri + 1, uri, ulen);
        tmp_uri[ulen + 1] = '\0';
        flb_free(uri);
        uri = tmp_uri;
    }

    /* Idle Interval */
    tmp = flb_output_get_property("idle_interval", ins);
    if (!tmp) {
        idle_interval = WEBSOCKET_INPUT_IDLE_INTERVAL; /* 20 seconds */
    }
    else {
        /* Convert to integer */
        idle_interval  = atoi(tmp);
    }

    ctx->u = upstream;
    ctx->uri = uri;
    ctx->host = ins->host.name;
    ctx->port = ins->host.port;
    ctx->idle_interval  = idle_interval;
 
    flb_info("[out_ws] we have following parameter %s, %s, %d, %d", ctx->uri, ctx->host, ctx->port, ctx->idle_interval);
    return ctx;
}

void flb_ws_conf_destroy(struct flb_out_ws *ctx)
{
    flb_info("[out_ws] flb_ws_conf_destroy ");
    if (!ctx) {
        return;
    }

    if (ctx->u) {
        flb_upstream_destroy(ctx->u);
    }

    if (ctx->json_date_key) {
        flb_sds_destroy(ctx->json_date_key);
    }
    flb_free(ctx->uri);
    flb_free(ctx);
    ctx = NULL;
}
