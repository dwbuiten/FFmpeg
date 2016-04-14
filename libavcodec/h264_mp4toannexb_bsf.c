/*
 * H.264 MP4 to Annex B byte stream format filter
 * Copyright (c) 2007 Benoit Fouet <benoit.fouet@free.fr>
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

#include <string.h>

#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"

#include "avcodec.h"
#include "bsf.h"

typedef struct H264BSFContext {
    int32_t  sps_offset;
    int32_t  pps_offset;
    uint8_t  length_size;
    uint8_t  new_idr;
    uint8_t  idr_sps_seen;
    uint8_t  idr_pps_seen;
    int      extradata_parsed;

    /* When private_spspps is zero then spspps_buf points to global extradata
       and bsf does replace a global extradata to own-allocated version (default
       behaviour).
       When private_spspps is non-zero the bsf uses a private version of spspps buf.
       This mode necessary when bsf uses in decoder, else bsf has issues after
       decoder re-initialization. Use the "private_spspps_buf" argument to
       activate this mode.
     */
    int      private_spspps;
    uint8_t *spspps_buf;
    uint32_t spspps_size;
} H264BSFContext;

static int alloc_and_copy(AVPacket *out,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *in, uint32_t in_size)
{
    uint32_t offset         = out->size;
    uint8_t nal_header_size = offset ? 3 : 4;
    int err;

    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
    if (err < 0)
        return err;

    if (sps_pps)
        memcpy(out->data + offset, sps_pps, sps_pps_size);
    memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);
    if (!offset) {
        AV_WB32(out->data + sps_pps_size, 1);
    } else {
        (out->data + offset + sps_pps_size)[0] =
        (out->data + offset + sps_pps_size)[1] = 0;
        (out->data + offset + sps_pps_size)[2] = 1;
    }

    return 0;
}

<<<<<<< HEAD
static int h264_extradata_to_annexb(H264BSFContext *ctx, AVCodecContext *avctx, const int padding)
=======
static int h264_extradata_to_annexb(AVBSFContext *ctx, const int padding)
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab
{
    uint16_t unit_size;
    uint64_t total_size                 = 0;
    uint8_t *out                        = NULL, unit_nb, sps_done = 0,
             sps_seen                   = 0, pps_seen = 0;
    const uint8_t *extradata            = ctx->par_in->extradata + 4;
    static const uint8_t nalu_header[4] = { 0, 0, 0, 1 };
    int length_size = (*extradata++ & 0x3) + 1; // retrieve length coded size

    ctx->sps_offset = ctx->pps_offset = -1;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extradata++ & 0x1f; /* number of sps unit(s) */
    if (!unit_nb) {
        goto pps;
    } else {
        ctx->sps_offset = 0;
        sps_seen = 1;
    }

    while (unit_nb--) {
        int err;

        unit_size   = AV_RB16(extradata);
        total_size += unit_size + 4;
<<<<<<< HEAD
        if (total_size > INT_MAX - padding) {
            av_log(avctx, AV_LOG_ERROR,
                   "Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(out);
            return AVERROR(EINVAL);
        }
        if (extradata + 2 + unit_size > avctx->extradata + avctx->extradata_size) {
            av_log(avctx, AV_LOG_ERROR, "Packet header is not contained in global extradata, "
                   "corrupted stream or invalid MP4/AVCC bitstream\n");
=======
        if (total_size > INT_MAX - padding ||
            extradata + 2 + unit_size > ctx->par_in->extradata +
            ctx->par_in->extradata_size) {
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab
            av_free(out);
            return AVERROR(EINVAL);
        }
        if ((err = av_reallocp(&out, total_size + padding)) < 0)
            return err;
        memcpy(out + total_size - unit_size - 4, nalu_header, 4);
        memcpy(out + total_size - unit_size, extradata + 2, unit_size);
        extradata += 2 + unit_size;
pps:
        if (!unit_nb && !sps_done++) {
            unit_nb = *extradata++; /* number of pps unit(s) */
            if (unit_nb) {
                ctx->pps_offset = total_size;
                pps_seen = 1;
            }
        }
    }

    if (out)
        memset(out + total_size, 0, padding);

    if (!sps_seen)
        av_log(ctx, AV_LOG_WARNING,
               "Warning: SPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    if (!pps_seen)
        av_log(ctx, AV_LOG_WARNING,
               "Warning: PPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

<<<<<<< HEAD
    if (!ctx->private_spspps) {
        av_free(avctx->extradata);
        avctx->extradata      = out;
        avctx->extradata_size = total_size;
    }
    ctx->spspps_buf  = out;
    ctx->spspps_size = total_size;
=======
    av_freep(&ctx->par_out->extradata);
    ctx->par_out->extradata      = out;
    ctx->par_out->extradata_size = total_size;
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab

    return length_size;
}

static int h264_mp4toannexb_init(AVBSFContext *ctx)
{
<<<<<<< HEAD
    H264BSFContext *ctx = bsfc->priv_data;
    int i;
=======
    H264BSFContext *s = ctx->priv_data;
    int ret;

    /* retrieve sps and pps NAL units from extradata */
    if (ctx->par_in->extradata_size >= 6) {
        ret = h264_extradata_to_annexb(ctx, AV_INPUT_BUFFER_PADDING_SIZE);
        if (ret < 0)
            return ret;

        s->length_size      = ret;
        s->first_idr        = 1;
        s->extradata_parsed = 1;
    }

    return 0;
}

static int h264_mp4toannexb_filter(AVBSFContext *ctx, AVPacket *out)
{
    H264BSFContext *s = ctx->priv_data;

    AVPacket *in;
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size    = 0;
    const uint8_t *buf;
    const uint8_t *buf_end;
    int            buf_size;
    int ret = 0;

    ret = ff_bsf_get_packet(ctx, &in);
    if (ret < 0)
        return ret;

    /* nothing to filter */
    if (!s->extradata_parsed) {
        av_packet_move_ref(out, in);
        av_packet_free(&in);
        return 0;
    }

<<<<<<< HEAD
    /* retrieve sps and pps NAL units from extradata */
    if (!ctx->extradata_parsed) {
        if (args && strstr(args, "private_spspps_buf"))
            ctx->private_spspps = 1;

        ret = h264_extradata_to_annexb(ctx, avctx, AV_INPUT_BUFFER_PADDING_SIZE);
        if (ret < 0)
            return ret;
        ctx->length_size      = ret;
        ctx->new_idr          = 1;
        ctx->idr_sps_seen     = 0;
        ctx->idr_pps_seen     = 0;
        ctx->extradata_parsed = 1;
    }
=======
    buf      = in->data;
    buf_size = in->size;
    buf_end  = in->data + in->size;
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab

    do {
<<<<<<< HEAD
        ret= AVERROR(EINVAL);
        if (buf + ctx->length_size > buf_end)
            goto fail;

        for (nal_size = 0, i = 0; i<ctx->length_size; i++)
            nal_size = (nal_size << 8) | buf[i];
=======
        if (buf + s->length_size > buf_end)
            goto fail;

        if (s->length_size == 1) {
            nal_size = buf[0];
        } else if (s->length_size == 2) {
            nal_size = AV_RB16(buf);
        } else
            nal_size = AV_RB32(buf);
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab

        buf += s->length_size;
        unit_type = *buf & 0x1f;

        if (nal_size > buf_end - buf || nal_size < 0)
            goto fail;

<<<<<<< HEAD
        if (unit_type == 7)
            ctx->idr_sps_seen = ctx->new_idr = 1;
        else if (unit_type == 8) {
            ctx->idr_pps_seen = ctx->new_idr = 1;
            /* if SPS has not been seen yet, prepend the AVCC one to PPS */
            if (!ctx->idr_sps_seen) {
                if (ctx->sps_offset == -1)
                    av_log(avctx, AV_LOG_WARNING, "SPS not present in the stream, nor in AVCC, stream may be unreadable\n");
                else {
                    if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                         ctx->spspps_buf + ctx->sps_offset,
                                         ctx->pps_offset != -1 ? ctx->pps_offset : ctx->spspps_size - ctx->sps_offset,
                                         buf, nal_size)) < 0)
                        goto fail;
                    ctx->idr_sps_seen = 1;
                    goto next_nal;
                }
            }
        }

        /* if this is a new IDR picture following an IDR picture, reset the idr flag.
         * Just check first_mb_in_slice to be 0 as this is the simplest solution.
         * This could be checking idr_pic_id instead, but would complexify the parsing. */
        if (!ctx->new_idr && unit_type == 5 && (buf[1] & 0x80))
            ctx->new_idr = 1;

        /* prepend only to the first type 5 NAL unit of an IDR picture, if no sps/pps are already present */
        if (ctx->new_idr && unit_type == 5 && !ctx->idr_sps_seen && !ctx->idr_pps_seen) {
            if ((ret=alloc_and_copy(poutbuf, poutbuf_size,
                               ctx->spspps_buf, ctx->spspps_size,
                               buf, nal_size)) < 0)
                goto fail;
            ctx->new_idr = 0;
        /* if only SPS has been seen, also insert PPS */
        } else if (ctx->new_idr && unit_type == 5 && ctx->idr_sps_seen && !ctx->idr_pps_seen) {
            if (ctx->pps_offset == -1) {
                av_log(avctx, AV_LOG_WARNING, "PPS not present in the stream, nor in AVCC, stream may be unreadable\n");
                if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                     NULL, 0, buf, nal_size)) < 0)
                    goto fail;
            } else if ((ret = alloc_and_copy(poutbuf, poutbuf_size,
                                        ctx->spspps_buf + ctx->pps_offset, ctx->spspps_size - ctx->pps_offset,
                                        buf, nal_size)) < 0)
                goto fail;
        } else {
            if ((ret=alloc_and_copy(poutbuf, poutbuf_size,
                               NULL, 0, buf, nal_size)) < 0)
                goto fail;
            if (!ctx->new_idr && unit_type == 1) {
                ctx->new_idr = 1;
                ctx->idr_sps_seen = 0;
                ctx->idr_pps_seen = 0;
            }
=======
        /* prepend only to the first type 5 NAL unit of an IDR picture */
        if (s->first_idr && unit_type == 5) {
            if (alloc_and_copy(out,
                               ctx->par_out->extradata, ctx->par_out->extradata_size,
                               buf, nal_size) < 0)
                goto fail;
            s->first_idr = 0;
        } else {
            if (alloc_and_copy(out,
                               NULL, 0, buf, nal_size) < 0)
                goto fail;
            if (!s->first_idr && unit_type == 1)
                s->first_idr = 1;
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab
        }

next_nal:
        buf        += nal_size;
        cumul_size += nal_size + s->length_size;
    } while (cumul_size < buf_size);

    ret = av_packet_copy_props(out, in);
    if (ret < 0)
        goto fail;

fail:
<<<<<<< HEAD
    av_freep(poutbuf);
    *poutbuf_size = 0;
    return ret;
}

static void h264_mp4toannexb_filter_close(AVBitStreamFilterContext *bsfc)
{
    H264BSFContext *ctx = bsfc->priv_data;
    if (ctx->private_spspps)
        av_freep(&ctx->spspps_buf);
}

AVBitStreamFilter ff_h264_mp4toannexb_bsf = {
    .name           = "h264_mp4toannexb",
    .priv_data_size = sizeof(H264BSFContext),
    .filter         = h264_mp4toannexb_filter,
    .close          = h264_mp4toannexb_filter_close,
=======
    if (ret < 0)
        av_packet_unref(out);
    av_packet_free(&in);

    return ret;
}

static const enum AVCodecID codec_ids[] = {
    AV_CODEC_ID_H264, AV_CODEC_ID_NONE,
};

const AVBitStreamFilter ff_h264_mp4toannexb_bsf = {
    .name           = "h264_mp4toannexb",
    .priv_data_size = sizeof(H264BSFContext),
    .init           = h264_mp4toannexb_init,
    .filter         = h264_mp4toannexb_filter,
    .codec_ids      = codec_ids,
>>>>>>> 33d18982fa03feb061c8f744a4f0a9175c1f63ab
};
