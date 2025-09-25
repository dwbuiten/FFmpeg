/*
 * Copyright (c) 2017 Vittorio Giovara <vittorio.giovara@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * tonemap algorithms
 */

#include <float.h>
#include <stdio.h>

#include "libavutil/csp.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/intfloat.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/thread.h"

#include "avfilter.h"
#include "colorspace.h"
#include "filters.h"
#include "video.h"

enum TonemapAlgorithm {
    TONEMAP_NONE,
    TONEMAP_LINEAR,
    TONEMAP_GAMMA,
    TONEMAP_CLIP,
    TONEMAP_REINHARD,
    TONEMAP_HABLE,
    TONEMAP_MOBIUS,
    TONEMAP_BT2446A,
    TONEMAP_MAX,
};

typedef struct TonemapContext {
    const AVClass *class;

    enum TonemapAlgorithm tonemap;
    double param;
    double desat;
    double peak;

    const AVLumaCoefficients *coeffs;
} TonemapContext;

AVOnce bt2446a_luts = AV_ONCE_INIT;

static uint16_t bt2446a_ysdr_lut[1 << 16];
static uint16_t bt2446a_fy_lut[1 << 16];

/*
 * BT.2446 A has a lot of stuff that can simply be generated once, with
 * floats, and stashed as integers for future use in a fixed point
 * implementation.
 */
static void generate_bt2446a_luts(void)
{
    const float phdr        = 1.0 + 32.0 * powf(1000.0 / 10000.0, 1.0 / 2.4); /* Currently hardcoded to 1000 nits for HLG. */
    const float psdr        = 1.0 + 32.0 * powf(100.0 / 10000.0, 1.0 / 2.4); /* Assume SDR is 100 nits. */
    const float ilogphdr    = 1.0 / logf(phdr);
    const float ipsdrminus1 = 1.0 / (psdr - 1.0);

    for (uint32_t i = 0; i < (1 << 16); i++) {
        float luma = ((float) i) / 65535.0;
        float yp   = logf(1.0 + (phdr - 1.0) * luma) * ilogphdr;
        float yc, ysdr, fy;

        if (yp < 0.7399)
            yc = 1.077 * yp;
        else if (yp < 0.9909)
            yc = ((-1.151 * yp) + 2.7811) * yp - 0.6302;
        else
            yc = 0.5 * yp + 0.5;

        ysdr = (powf(psdr, yc) - 1.0) * ipsdrminus1;
        fy   = !i ? 0.0 : ysdr / (1.1 * luma);

        bt2446a_ysdr_lut[i] = ysdr * 65535.0;
        bt2446a_fy_lut[i]   = fy * 16383.0;
    }

    printf("init\n");
}

static av_cold int init(AVFilterContext *ctx)
{
    TonemapContext *s = ctx->priv;

    switch(s->tonemap) {
    case TONEMAP_GAMMA:
        if (isnan(s->param))
            s->param = 1.8f;
        break;
    case TONEMAP_REINHARD:
        if (!isnan(s->param))
            s->param = (1.0f - s->param) / s->param;
        break;
    case TONEMAP_MOBIUS:
        if (isnan(s->param))
            s->param = 0.3f;
        break;
    case TONEMAP_BT2446A:
        ff_thread_once(&bt2446a_luts, generate_bt2446a_luts);
        break;
    }

    if (isnan(s->param))
        s->param = 1.0f;

    return 0;
}

static float hable(float in)
{
    float a = 0.15f, b = 0.50f, c = 0.10f, d = 0.20f, e = 0.02f, f = 0.30f;
    return (in * (in * a + b * c) + d * e) / (in * (in * a + b) + d * f) - e / f;
}

static float mobius(float in, float j, double peak)
{
    float a, b;

    if (in <= j)
        return in;

    a = -j * j * (peak - 1.0f) / (j * j - 2.0f * j + peak);
    b = (j * j - 2.0f * j * peak + peak) / FFMAX(peak - 1.0f, 1e-6);

    return (b * b + 2.0f * b * j + j * j) / (b - a) * (in + a) / (in + b);
}

#define MIX(x,y,a) (x) * (1 - (a)) + (y) * (a)
static void tonemap(TonemapContext *s, AVFrame *out, const AVFrame *in,
                    const AVPixFmtDescriptor *desc, int x, int y, double peak)
{
    int map[3] = { desc->comp[0].plane, desc->comp[1].plane, desc->comp[2].plane };
    const float *r_in = (const float *)(in->data[map[0]] + x * desc->comp[map[0]].step + y * in->linesize[map[0]]);
    const float *g_in = (const float *)(in->data[map[1]] + x * desc->comp[map[1]].step + y * in->linesize[map[1]]);
    const float *b_in = (const float *)(in->data[map[2]] + x * desc->comp[map[2]].step + y * in->linesize[map[2]]);
    float *r_out = (float *)(out->data[map[0]] + x * desc->comp[map[0]].step + y * out->linesize[map[0]]);
    float *g_out = (float *)(out->data[map[1]] + x * desc->comp[map[1]].step + y * out->linesize[map[1]]);
    float *b_out = (float *)(out->data[map[2]] + x * desc->comp[map[2]].step + y * out->linesize[map[2]]);
    float sig, sig_orig;

    /* load values */
    *r_out = *r_in;
    *g_out = *g_in;
    *b_out = *b_in;

    /* desaturate to prevent unnatural colors */
    if (s->desat > 0) {
        float luma = av_q2d(s->coeffs->cr) * *r_in + av_q2d(s->coeffs->cg) * *g_in + av_q2d(s->coeffs->cb) * *b_in;
        float overbright = FFMAX(luma - s->desat, 1e-6) / FFMAX(luma, 1e-6);
        *r_out = MIX(*r_in, luma, overbright);
        *g_out = MIX(*g_in, luma, overbright);
        *b_out = MIX(*b_in, luma, overbright);
    }

    /* pick the brightest component, reducing the value range as necessary
     * to keep the entire signal in range and preventing discoloration due to
     * out-of-bounds clipping */
    sig = FFMAX(FFMAX3(*r_out, *g_out, *b_out), 1e-6);
    sig_orig = sig;

    switch(s->tonemap) {
    default:
    case TONEMAP_NONE:
        // do nothing
        break;
    case TONEMAP_LINEAR:
        sig = sig * s->param / peak;
        break;
    case TONEMAP_GAMMA:
        sig = sig > 0.05f ? pow(sig / peak, 1.0f / s->param)
                          : sig * pow(0.05f / peak, 1.0f / s->param) / 0.05f;
        break;
    case TONEMAP_CLIP:
        sig = av_clipf(sig * s->param, 0, 1.0f);
        break;
    case TONEMAP_HABLE:
        sig = hable(sig) / hable(peak);
        break;
    case TONEMAP_REINHARD:
        sig = sig / (sig + s->param) * (peak + s->param) / peak;
        break;
    case TONEMAP_MOBIUS:
        sig = mobius(sig, s->param, peak);
        break;
    }

    /* apply the computed scale factor to the color,
     * linearly to prevent discoloration */
    *r_out *= sig / sig_orig;
    *g_out *= sig / sig_orig;
    *b_out *= sig / sig_orig;
}

typedef struct ThreadData {
    AVFrame *in, *out;
    const AVPixFmtDescriptor *desc;
    double peak;
} ThreadData;

/*
 * BT.2446 (Type A) is optimized to produce as visually close to a given broadcast
 * (1000 nits) signal as possible post-tonemapping. It works in teh YCC-space for
 * perceptual color management purposes, meaning we can be sneaking and work directly
 * in the YUV444P10 integer space (by way of a fixed point implementation).
 *
 * Obviously, this is only one possible implementaiton, it is fully possible to extend
 * this implementation to other peak nit levels (see LUT generation), and pixel
 * types (float), as well as RGB. This implementation is provided as a PoC, as
 * it stands.
 *
 * Input is full range (per spec).
 *
 * Really, libavfilter ain't the right place for this sort of stuff, probably.
 *
 * Ref: https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2446-1-2021-PDF-E.pdf
 */

static void tonemap_bt2446a(TonemapContext *s, AVFrame *out, const AVFrame *in,
                            const AVPixFmtDescriptor *desc, int x, int y)
{
    int map[3] = { desc->comp[0].plane, desc->comp[1].plane, desc->comp[2].plane };
    const uint16_t *y_in = (const uint16_t *)(in->data[map[0]] + x * desc->comp[map[0]].step + y * in->linesize[map[0]]);
    const uint16_t *u_in = (const uint16_t *)(in->data[map[1]] + x * desc->comp[map[1]].step + y * in->linesize[map[1]]);
    const uint16_t *v_in = (const uint16_t *)(in->data[map[2]] + x * desc->comp[map[2]].step + y * in->linesize[map[2]]);
    uint16_t *y_out = (uint16_t *)(out->data[map[0]] + x * desc->comp[map[0]].step + y * out->linesize[map[0]]);
    uint16_t *u_out = (uint16_t *)(out->data[map[1]] + x * desc->comp[map[1]].step + y * out->linesize[map[1]]);
    uint16_t *v_out = (uint16_t *)(out->data[map[2]] + x * desc->comp[map[2]].step + y * out->linesize[map[2]]);

    /* All of these names are direct from the spec. */
    uint16_t luma   = (*y_in) << 6;
    uint16_t ysdr   = bt2446a_ysdr_lut[luma];
    uint16_t fy     = bt2446a_fy_lut[luma];
    int16_t utmo    = av_clip_int16(((int32_t) fy * ((int32_t) ((*u_in) - 512) << 6) + 32768) >> 14);
    int32_t vtmo    = ((int64_t) fy * (((int64_t) ((*v_in) - 512) << 6)) + 32768) >> 14;
    int16_t adjvtmo = ((int32_t) 6553 * ((int32_t) vtmo)) >> 16;
    uint16_t yptmo  = av_clip_uint16(((int32_t) ysdr) - FFMAX(((int32_t) adjvtmo), 0));

    vtmo = av_clip_int16(vtmo);

    *y_out = yptmo >> 6;
    *u_out = (utmo >> 6) + 512;
    *v_out = (vtmo >> 6) + 512;
}

static int tonemap_slice(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs)
{
    TonemapContext *s = ctx->priv;
    ThreadData *td = arg;
    AVFrame *in = td->in;
    AVFrame *out = td->out;
    const AVPixFmtDescriptor *desc = td->desc;
    const int slice_start = (in->height * jobnr) / nb_jobs;
    const int slice_end = (in->height * (jobnr+1)) / nb_jobs;
    double peak = td->peak;

    if (s->tonemap == TONEMAP_BT2446A) {
        for (int y = slice_start; y < slice_end; y++)
            for (int x = 0; x < out->width; x++)
                tonemap_bt2446a(s, out, in, desc, x, y);
    } else {
        for (int y = slice_start; y < slice_end; y++)
            for (int x = 0; x < out->width; x++)
                tonemap(s, out, in, desc, x, y, peak);
    }

    return 0;
}

static int filter_frame(AVFilterLink *link, AVFrame *in)
{
    AVFilterContext *ctx = link->dst;
    TonemapContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    ThreadData td;
    AVFrame *out;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(link->format);
    const AVPixFmtDescriptor *odesc = av_pix_fmt_desc_get(outlink->format);
    int ret, x, y;
    double peak = s->peak;

    if (!desc || !odesc) {
        av_frame_free(&in);
        return AVERROR_BUG;
    }

    out = ff_get_video_buffer(outlink, outlink->w, outlink->h);
    if (!out) {
        av_frame_free(&in);
        return AVERROR(ENOMEM);
    }

    ret = av_frame_copy_props(out, in);
    if (ret < 0) {
        av_frame_free(&in);
        av_frame_free(&out);
        return ret;
    }

    if (s->tonemap == TONEMAP_BT2446A && in->format != AV_PIX_FMT_YUV444P10) {
        av_log(ctx, AV_LOG_ERROR, "BT.2446 A only works with YUV444P.\n");
        return AVERROR(EINVAL);
    }

    /* NOTE/HACK: It is assumed that you have a max nits of 1000 here. */
    if (in->format == AV_PIX_FMT_YUV444P10) {
        /* Annoying, but we need to check here too. */
        if (s->tonemap != TONEMAP_BT2446A) {
            av_log(ctx, AV_LOG_ERROR, "BT.2446 A only works with YUV444P.\n");
            return AVERROR(EINVAL);
        }
        if (outlink->format != AV_PIX_FMT_YUV444P10) {
            av_log(ctx, AV_LOG_ERROR, "Only YUB444P->YUV444P is supported with BT.2446 A mode.\n");
            return AVERROR(EINVAL);
        }
        if (in->color_trc != AVCOL_TRC_BT2020_10 || in->colorspace != AVCOL_SPC_BT2020_NCL ||
            in->color_primaries != AVCOL_PRI_BT2020 || in->color_range != AVCOL_RANGE_JPEG) {
            av_log(ctx, AV_LOG_ERROR, "Only 10-bit BT.2020 is supported with BT.2446 A.\n");
            return AVERROR(EINVAL);
        }
        if (out->color_trc != AVCOL_TRC_BT2020_10 || out->colorspace != AVCOL_SPC_BT2020_NCL
            || out->color_primaries != AVCOL_PRI_BT2020 || out->color_range != AVCOL_RANGE_JPEG) {
            av_log(ctx, AV_LOG_ERROR, "Only 10-bit BT.2020 is supported with BT.2446 A.\n");
            return AVERROR(EINVAL);
        }
    } else {
    /* input and output transfer will be linear */
    if (in->color_trc == AVCOL_TRC_UNSPECIFIED) {
        av_log(ctx, AV_LOG_WARNING, "Untagged transfer, assuming linear light\n");
        out->color_trc = AVCOL_TRC_LINEAR;
    } else if (in->color_trc != AVCOL_TRC_LINEAR)
        av_log(ctx, AV_LOG_WARNING, "Tonemapping works on linear light only\n");

    /* read peak from side data if not passed in */
    if (!peak) {
        peak = ff_determine_signal_peak(in);
        av_log(ctx, AV_LOG_DEBUG, "Computed signal peak: %f\n", peak);
    }

    /* load original color space even if pixel format is RGB to compute overbrights */
    s->coeffs = av_csp_luma_coeffs_from_avcsp(in->colorspace);
    if (s->desat > 0 && (in->colorspace == AVCOL_SPC_UNSPECIFIED || !s->coeffs)) {
        if (in->colorspace == AVCOL_SPC_UNSPECIFIED)
            av_log(ctx, AV_LOG_WARNING, "Missing color space information, ");
        else if (!s->coeffs)
            av_log(ctx, AV_LOG_WARNING, "Unsupported color space '%s', ",
                   av_color_space_name(in->colorspace));
        av_log(ctx, AV_LOG_WARNING, "desaturation is disabled\n");
        s->desat = 0;
    }
    }

    /* do the tone map */
    td.out = out;
    td.in = in;
    td.desc = desc;
    td.peak = peak;
    ff_filter_execute(ctx, tonemap_slice, &td, NULL,
                      FFMIN(in->height, ff_filter_get_nb_threads(ctx)));

    /* copy/generate alpha if needed */
    if (desc->flags & AV_PIX_FMT_FLAG_ALPHA && odesc->flags & AV_PIX_FMT_FLAG_ALPHA) {
        av_image_copy_plane(out->data[3], out->linesize[3],
                            in->data[3], in->linesize[3],
                            out->linesize[3], outlink->h);
    } else if (odesc->flags & AV_PIX_FMT_FLAG_ALPHA) {
        for (y = 0; y < out->height; y++) {
            for (x = 0; x < out->width; x++) {
                AV_WN32(out->data[3] + x * odesc->comp[3].step + y * out->linesize[3],
                        av_float2int(1.0f));
            }
        }
    }

    av_frame_free(&in);

    ff_update_hdr_metadata(out, peak);

    return ff_filter_frame(outlink, out);
}

#define OFFSET(x) offsetof(TonemapContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM
static const AVOption tonemap_options[] = {
    { "tonemap",      "tonemap algorithm selection", OFFSET(tonemap), AV_OPT_TYPE_INT, {.i64 = TONEMAP_NONE}, TONEMAP_NONE, TONEMAP_MAX - 1, FLAGS, .unit = "tonemap" },
    {     "none",     0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_NONE},              0, 0, FLAGS, .unit = "tonemap" },
    {     "linear",   0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_LINEAR},            0, 0, FLAGS, .unit = "tonemap" },
    {     "gamma",    0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_GAMMA},             0, 0, FLAGS, .unit = "tonemap" },
    {     "clip",     0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_CLIP},              0, 0, FLAGS, .unit = "tonemap" },
    {     "reinhard", 0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_REINHARD},          0, 0, FLAGS, .unit = "tonemap" },
    {     "hable",    0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_HABLE},             0, 0, FLAGS, .unit = "tonemap" },
    {     "mobius",   0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_MOBIUS},            0, 0, FLAGS, .unit = "tonemap" },
    {     "bt2446a",  0, 0, AV_OPT_TYPE_CONST, {.i64 = TONEMAP_BT2446A},           0, 0, FLAGS, .unit = "tonemap" },
    { "param",        "tonemap parameter", OFFSET(param), AV_OPT_TYPE_DOUBLE, {.dbl = NAN}, DBL_MIN, DBL_MAX, FLAGS },
    { "desat",        "desaturation strength", OFFSET(desat), AV_OPT_TYPE_DOUBLE, {.dbl = 2}, 0, DBL_MAX, FLAGS },
    { "peak",         "signal peak override", OFFSET(peak), AV_OPT_TYPE_DOUBLE, {.dbl = 0}, 0, DBL_MAX, FLAGS },
    { NULL }
};

AVFILTER_DEFINE_CLASS(tonemap);

static const AVFilterPad tonemap_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
    },
};

const FFFilter ff_vf_tonemap = {
    .p.name          = "tonemap",
    .p.description   = NULL_IF_CONFIG_SMALL("Conversion to/from different dynamic ranges."),
    .p.priv_class    = &tonemap_class,
    .p.flags         = AVFILTER_FLAG_SLICE_THREADS,
    .init            = init,
    .priv_size       = sizeof(TonemapContext),
    FILTER_INPUTS(tonemap_inputs),
    FILTER_OUTPUTS(ff_video_default_filterpad),
    FILTER_PIXFMTS(AV_PIX_FMT_GBRPF32, AV_PIX_FMT_GBRAPF32, AV_PIX_FMT_YUV444P10),
};
