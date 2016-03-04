/*
 * RIFF muxing functions
 * Copyright (c) 2000 Fabrice Bellard
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

#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bytestream.h"
#include "avformat.h"
#include "avio_internal.h"
#include "riff.h"

int64_t ff_start_tag(AVIOContext *pb, const char *tag)
{
    ffio_wfourcc(pb, tag);
    avio_wl32(pb, -1);
    return avio_tell(pb);
}

void ff_end_tag(AVIOContext *pb, int64_t start)
{
    int64_t pos;

    av_assert0((start&1) == 0);

    pos = avio_tell(pb);
    if (pos & 1)
        avio_w8(pb, 0);
    avio_seek(pb, start - 4, SEEK_SET);
    avio_wl32(pb, (uint32_t)(pos - start));
    avio_seek(pb, FFALIGN(pos, 2), SEEK_SET);
}

/* WAVEFORMATEX header */
/* returns the size or -1 on error */
<<<<<<< HEAD
int ff_put_wav_header(AVIOContext *pb, AVCodecContext *enc, int flags)
=======
int ff_put_wav_header(AVFormatContext *s, AVIOContext *pb,
                      AVCodecParameters *par)
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
{
    int bps, blkalign, bytespersec, frame_size;
    int hdrsize;
    int64_t hdrstart = avio_tell(pb);
    int waveformatextensible;
    uint8_t temp[256];
    uint8_t *riff_extradata       = temp;
    uint8_t *riff_extradata_start = temp;

    if (!par->codec_tag || par->codec_tag > 0xffff)
        return -1;

    /* We use the known constant frame size for the codec if known, otherwise
     * fall back on using AVCodecContext.frame_size, which is not as reliable
     * for indicating packet duration. */
    frame_size = av_get_audio_frame_duration2(par, par->block_align);

<<<<<<< HEAD
    waveformatextensible = (enc->channels > 2 && enc->channel_layout) ||
                           enc->sample_rate > 48000 ||
                           enc->codec_id == AV_CODEC_ID_EAC3 ||
                           av_get_bits_per_sample(enc->codec_id) > 16;
=======
    waveformatextensible = (par->channels > 2 && par->channel_layout) ||
                           par->sample_rate > 48000 ||
                           av_get_bits_per_sample(par->codec_id) > 16;
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf

    if (waveformatextensible)
        avio_wl16(pb, 0xfffe);
    else
        avio_wl16(pb, par->codec_tag);

<<<<<<< HEAD
    avio_wl16(pb, enc->channels);
    avio_wl32(pb, enc->sample_rate);
    if (enc->codec_id == AV_CODEC_ID_ATRAC3 ||
        enc->codec_id == AV_CODEC_ID_G723_1 ||
        enc->codec_id == AV_CODEC_ID_MP2    ||
        enc->codec_id == AV_CODEC_ID_MP3    ||
        enc->codec_id == AV_CODEC_ID_GSM_MS) {
=======
    avio_wl16(pb, par->channels);
    avio_wl32(pb, par->sample_rate);
    if (par->codec_id == AV_CODEC_ID_MP2 ||
        par->codec_id == AV_CODEC_ID_MP3 ||
        par->codec_id == AV_CODEC_ID_GSM_MS) {
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
        bps = 0;
    } else {
        if (!(bps = av_get_bits_per_sample(par->codec_id))) {
            if (par->bits_per_coded_sample)
                bps = par->bits_per_coded_sample;
            else
                bps = 16;  // default to 16
        }
    }
    if (bps != par->bits_per_coded_sample && par->bits_per_coded_sample) {
        av_log(s, AV_LOG_WARNING,
               "requested bits_per_coded_sample (%d) "
               "and actually stored (%d) differ\n",
               par->bits_per_coded_sample, bps);
    }

<<<<<<< HEAD
    if (enc->codec_id == AV_CODEC_ID_MP2) {
        blkalign = (144 * enc->bit_rate - 1)/enc->sample_rate + 1;
    } else if (enc->codec_id == AV_CODEC_ID_MP3) {
        blkalign = 576 * (enc->sample_rate <= (24000 + 32000)/2 ? 1 : 2);
    } else if (enc->codec_id == AV_CODEC_ID_AC3) {
        blkalign = 3840;                /* maximum bytes per frame */
    } else if (enc->codec_id == AV_CODEC_ID_AAC) {
        blkalign = 768 * enc->channels; /* maximum bytes per frame */
    } else if (enc->codec_id == AV_CODEC_ID_G723_1) {
        blkalign = 24;
    } else if (enc->block_align != 0) { /* specified by the codec */
        blkalign = enc->block_align;
    } else
        blkalign = bps * enc->channels / av_gcd(8, bps);
    if (enc->codec_id == AV_CODEC_ID_PCM_U8 ||
        enc->codec_id == AV_CODEC_ID_PCM_S24LE ||
        enc->codec_id == AV_CODEC_ID_PCM_S32LE ||
        enc->codec_id == AV_CODEC_ID_PCM_F32LE ||
        enc->codec_id == AV_CODEC_ID_PCM_F64LE ||
        enc->codec_id == AV_CODEC_ID_PCM_S16LE) {
        bytespersec = enc->sample_rate * blkalign;
    } else if (enc->codec_id == AV_CODEC_ID_G723_1) {
        bytespersec = 800;
=======
    if (par->codec_id == AV_CODEC_ID_MP2) {
        blkalign = frame_size;
    } else if (par->codec_id == AV_CODEC_ID_MP3) {
        blkalign = 576 * (par->sample_rate <= 24000 ? 1 : 2);
    } else if (par->codec_id == AV_CODEC_ID_AC3) {
        blkalign = 3840;                /* maximum bytes per frame */
    } else if (par->block_align != 0) { /* specified by the codec */
        blkalign = par->block_align;
    } else
        blkalign = bps * par->channels / av_gcd(8, bps);
    if (par->codec_id == AV_CODEC_ID_PCM_U8 ||
        par->codec_id == AV_CODEC_ID_PCM_S24LE ||
        par->codec_id == AV_CODEC_ID_PCM_S32LE ||
        par->codec_id == AV_CODEC_ID_PCM_F32LE ||
        par->codec_id == AV_CODEC_ID_PCM_F64LE ||
        par->codec_id == AV_CODEC_ID_PCM_S16LE) {
        bytespersec = par->sample_rate * blkalign;
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
    } else {
        bytespersec = par->bit_rate / 8;
    }
    avio_wl32(pb, bytespersec); /* bytes per second */
    avio_wl16(pb, blkalign);    /* block align */
    avio_wl16(pb, bps);         /* bits per sample */
<<<<<<< HEAD
    if (enc->codec_id == AV_CODEC_ID_MP3) {
=======
    if (par->codec_id == AV_CODEC_ID_MP3) {
        hdrsize += 12;
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
        bytestream_put_le16(&riff_extradata, 1);    /* wID */
        bytestream_put_le32(&riff_extradata, 2);    /* fdwFlags */
        bytestream_put_le16(&riff_extradata, 1152); /* nBlockSize */
        bytestream_put_le16(&riff_extradata, 1);    /* nFramesPerBlock */
        bytestream_put_le16(&riff_extradata, 1393); /* nCodecDelay */
<<<<<<< HEAD
    } else if (enc->codec_id == AV_CODEC_ID_MP2) {
=======
    } else if (par->codec_id == AV_CODEC_ID_MP2) {
        hdrsize += 22;
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
        /* fwHeadLayer */
        bytestream_put_le16(&riff_extradata, 2);
        /* dwHeadBitrate */
        bytestream_put_le32(&riff_extradata, par->bit_rate);
        /* fwHeadMode */
        bytestream_put_le16(&riff_extradata, par->channels == 2 ? 1 : 8);
        /* fwHeadModeExt */
        bytestream_put_le16(&riff_extradata, 0);
        /* wHeadEmphasis */
        bytestream_put_le16(&riff_extradata, 1);
        /* fwHeadFlags */
        bytestream_put_le16(&riff_extradata, 16);
        /* dwPTSLow */
        bytestream_put_le32(&riff_extradata, 0);
        /* dwPTSHigh */
        bytestream_put_le32(&riff_extradata, 0);
<<<<<<< HEAD
    } else if (enc->codec_id == AV_CODEC_ID_G723_1) {
        bytestream_put_le32(&riff_extradata, 0x9ace0002); /* extradata needed for msacm g723.1 codec */
        bytestream_put_le32(&riff_extradata, 0xaea2f732);
        bytestream_put_le16(&riff_extradata, 0xacde);
    } else if (enc->codec_id == AV_CODEC_ID_GSM_MS ||
               enc->codec_id == AV_CODEC_ID_ADPCM_IMA_WAV) {
        /* wSamplesPerBlock */
        bytestream_put_le16(&riff_extradata, frame_size);
    } else if (enc->extradata_size) {
        riff_extradata_start = enc->extradata;
        riff_extradata       = enc->extradata + enc->extradata_size;
=======
    } else if (par->codec_id == AV_CODEC_ID_GSM_MS ||
               par->codec_id == AV_CODEC_ID_ADPCM_IMA_WAV) {
        hdrsize += 2;
        /* wSamplesPerBlock */
        bytestream_put_le16(&riff_extradata, frame_size);
    } else if (par->extradata_size) {
        riff_extradata_start = par->extradata;
        riff_extradata       = par->extradata + par->extradata_size;
        hdrsize             += par->extradata_size;
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
    }
    /* write WAVEFORMATEXTENSIBLE extensions */
    if (waveformatextensible) {
        int write_channel_mask = !(flags & FF_PUT_WAV_HEADER_SKIP_CHANNELMASK) &&
                                 (enc->strict_std_compliance < FF_COMPLIANCE_NORMAL ||
                                  enc->channel_layout < 0x40000);
        /* 22 is WAVEFORMATEXTENSIBLE size */
        avio_wl16(pb, riff_extradata - riff_extradata_start + 22);
        /* ValidBitsPerSample || SamplesPerBlock || Reserved */
        avio_wl16(pb, bps);
        /* dwChannelMask */
<<<<<<< HEAD
        avio_wl32(pb, write_channel_mask ? enc->channel_layout : 0);
        /* GUID + next 3 */
        if (enc->codec_id == AV_CODEC_ID_EAC3) {
            ff_put_guid(pb, ff_get_codec_guid(enc->codec_id, ff_codec_wav_guids));
        } else {
        avio_wl32(pb, enc->codec_tag);
=======
        avio_wl32(pb, par->channel_layout);
        /* GUID + next 3 */
        avio_wl32(pb, par->codec_tag);
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
        avio_wl32(pb, 0x00100000);
        avio_wl32(pb, 0xAA000080);
        avio_wl32(pb, 0x719B3800);
        }
    } else if ((flags & FF_PUT_WAV_HEADER_FORCE_WAVEFORMATEX) ||
               enc->codec_tag != 0x0001 /* PCM */ ||
               riff_extradata - riff_extradata_start) {
        /* WAVEFORMATEX */
        avio_wl16(pb, riff_extradata - riff_extradata_start); /* cbSize */
    } /* else PCMWAVEFORMAT */
    avio_write(pb, riff_extradata_start, riff_extradata - riff_extradata_start);
    hdrsize = avio_tell(pb) - hdrstart;
    if (hdrsize & 1) {
        hdrsize++;
        avio_w8(pb, 0);
    }

    return hdrsize;
}

/* BITMAPINFOHEADER header */
<<<<<<< HEAD
void ff_put_bmp_header(AVIOContext *pb, AVCodecContext *enc,
                       const AVCodecTag *tags, int for_asf, int ignore_extradata)
=======
void ff_put_bmp_header(AVIOContext *pb, AVCodecParameters *par,
                       const AVCodecTag *tags, int for_asf)
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
{
    int keep_height = enc->extradata_size >= 9 &&
                      !memcmp(enc->extradata + enc->extradata_size - 9, "BottomUp", 9);
    int extradata_size = enc->extradata_size - 9*keep_height;
    int raw_pal_avi;

    raw_pal_avi = !for_asf && enc->codec_id == AV_CODEC_ID_RAWVIDEO &&
                  !enc->codec_tag &&
            enc->bits_per_coded_sample >= 1 && enc->bits_per_coded_sample <= 8;
    if (!enc->extradata_size && raw_pal_avi)
        extradata_size = 4 * (1 << enc->bits_per_coded_sample);

    /* size */
<<<<<<< HEAD
    avio_wl32(pb, 40 + (ignore_extradata ? 0 :extradata_size));
    avio_wl32(pb, enc->width);
    //We always store RGB TopDown
    avio_wl32(pb, enc->codec_tag || keep_height ? enc->height : -enc->height);
=======
    avio_wl32(pb, 40 + par->extradata_size);
    avio_wl32(pb, par->width);
    //We always store RGB TopDown
    avio_wl32(pb, par->codec_tag ? par->height : -par->height);
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
    /* planes */
    avio_wl16(pb, 1);
    /* depth */
    avio_wl16(pb, par->bits_per_coded_sample ? par->bits_per_coded_sample : 24);
    /* compression type */
<<<<<<< HEAD
    avio_wl32(pb, enc->codec_tag);
    avio_wl32(pb, (enc->width * enc->height * (enc->bits_per_coded_sample ? enc->bits_per_coded_sample : 24)+7) / 8);
=======
    avio_wl32(pb, par->codec_tag);
    avio_wl32(pb, par->width * par->height * 3);
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
    avio_wl32(pb, 0);
    avio_wl32(pb, 0);
    avio_wl32(pb, 0);
    avio_wl32(pb, 0);

<<<<<<< HEAD
    if (!ignore_extradata) {
        if (enc->extradata_size) {
            avio_write(pb, enc->extradata, extradata_size);
            if (!for_asf && extradata_size & 1)
                avio_w8(pb, 0);
        } else if (raw_pal_avi) {
            int i;
            enum AVPixelFormat pix_fmt = enc->pix_fmt;
            if (pix_fmt == AV_PIX_FMT_NONE && enc->bits_per_coded_sample == 1)
                pix_fmt = AV_PIX_FMT_MONOWHITE;
            for (i = 0; i < 1 << enc->bits_per_coded_sample; i++) {
                /* Initialize 1 bpp palette to black & white */
                if (i == 0 && pix_fmt == AV_PIX_FMT_MONOWHITE)
                    avio_wl32(pb, 0xffffff);
                else if (i == 1 && pix_fmt == AV_PIX_FMT_MONOBLACK)
                    avio_wl32(pb, 0xffffff);
                else
                    avio_wl32(pb, 0);
            }
        }
    }
=======
    avio_write(pb, par->extradata, par->extradata_size);

    if (!for_asf && par->extradata_size & 1)
        avio_w8(pb, 0);
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf
}

void ff_parse_specific_params(AVStream *st, int *au_rate,
                              int *au_ssize, int *au_scale)
{
    AVCodecParameters *par = st->codecpar;
    int gcd;
    int audio_frame_size;

<<<<<<< HEAD
    /* We use the known constant frame size for the codec if known, otherwise
     * fall back on using AVCodecContext.frame_size, which is not as reliable
     * for indicating packet duration. */
    audio_frame_size = av_get_audio_frame_duration(codec, 0);
    if (!audio_frame_size)
        audio_frame_size = codec->frame_size;
=======
    audio_frame_size = av_get_audio_frame_duration2(par, 0);
>>>>>>> 9200514ad8717c63f82101dc394f4378854325bf

    *au_ssize = par->block_align;
    if (audio_frame_size && par->sample_rate) {
        *au_scale = audio_frame_size;
        *au_rate  = par->sample_rate;
    } else if (par->codec_type == AVMEDIA_TYPE_VIDEO ||
               par->codec_type == AVMEDIA_TYPE_DATA ||
               par->codec_type == AVMEDIA_TYPE_SUBTITLE) {
        *au_scale = st->time_base.num;
        *au_rate  = st->time_base.den;
    } else {
        *au_scale = par->block_align ? par->block_align * 8 : 8;
        *au_rate  = par->bit_rate ? par->bit_rate :
                    8 * par->sample_rate;
    }
    gcd        = av_gcd(*au_scale, *au_rate);
    *au_scale /= gcd;
    *au_rate  /= gcd;
}

void ff_riff_write_info_tag(AVIOContext *pb, const char *tag, const char *str)
{
    size_t len = strlen(str);
    if (len > 0 && len < UINT32_MAX) {
        len++;
        ffio_wfourcc(pb, tag);
        avio_wl32(pb, len);
        avio_put_str(pb, str);
        if (len & 1)
            avio_w8(pb, 0);
    }
}

static const char riff_tags[][5] = {
    "IARL", "IART", "ICMS", "ICMT", "ICOP", "ICRD", "ICRP", "IDIM", "IDPI",
    "IENG", "IGNR", "IKEY", "ILGT", "ILNG", "IMED", "INAM", "IPLT", "IPRD",
    "IPRT", "ITRK", "ISBJ", "ISFT", "ISHP", "ISMP", "ISRC", "ISRF", "ITCH",
    { 0 }
};

static int riff_has_valid_tags(AVFormatContext *s)
{
    int i;

    for (i = 0; *riff_tags[i]; i++)
        if (av_dict_get(s->metadata, riff_tags[i], NULL, AV_DICT_MATCH_CASE))
            return 1;

    return 0;
}

void ff_riff_write_info(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    int i;
    int64_t list_pos;
    AVDictionaryEntry *t = NULL;

    ff_metadata_conv(&s->metadata, ff_riff_info_conv, NULL);

    /* writing empty LIST is not nice and may cause problems */
    if (!riff_has_valid_tags(s))
        return;

    list_pos = ff_start_tag(pb, "LIST");
    ffio_wfourcc(pb, "INFO");
    for (i = 0; *riff_tags[i]; i++)
        if ((t = av_dict_get(s->metadata, riff_tags[i],
                             NULL, AV_DICT_MATCH_CASE)))
            ff_riff_write_info_tag(s->pb, t->key, t->value);
    ff_end_tag(pb, list_pos);
}

void ff_put_guid(AVIOContext *s, const ff_asf_guid *g)
{
    av_assert0(sizeof(*g) == 16);
    avio_write(s, *g, sizeof(*g));
}

const ff_asf_guid *ff_get_codec_guid(enum AVCodecID id, const AVCodecGuid *av_guid)
{
    int i;
    for (i = 0; av_guid[i].id != AV_CODEC_ID_NONE; i++) {
        if (id == av_guid[i].id)
            return &(av_guid[i].guid);
    }
    return NULL;
}
