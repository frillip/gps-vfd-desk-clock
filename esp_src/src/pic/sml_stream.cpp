#include "sml_stream.h"

#include <string.h>

static void drop(SmlStream *stream, uint16_t bytes, bool text)
{
    if (bytes > stream->count)
    {
        bytes = stream->count;
    }
    if (text && bytes != 0 && stream->on_text != NULL)
    {
        stream->on_text(stream->mirror, bytes, stream->context);
    }
    stream->count = (uint16_t)(stream->count - bytes);
    memmove(stream->mirror, stream->mirror + bytes, stream->count);
}


static void account_noise(SmlStream *stream)
{
    uint32_t noise = stream->parser.stats.noise_bytes - stream->noise_seen;

    stream->noise_seen = stream->parser.stats.noise_bytes;
    drop(stream, (uint16_t)noise, true);
}


void sml_stream_init(SmlStream *stream, SmlFrameSink on_frame, SmlTextSink on_text, void *context)
{
    memset(stream, 0, sizeof(*stream));
    sml_parser_init(&stream->parser);
    stream->on_frame = on_frame;
    stream->on_text = on_text;
    stream->context = context;
}


void sml_stream_push(SmlStream *stream, uint8_t byte)
{
    SmlFrame frame;
    bool found;

    if (stream->count < sizeof(stream->mirror))
    {
        stream->mirror[stream->count++] = byte;
    }
    found = sml_parser_push(&stream->parser, byte, &frame);
    account_noise(stream);
    while (found)
    {
        drop(stream, (uint16_t)(SML_HEADER_BYTES + frame.length + SML_CRC_BYTES), false);
        if (stream->on_frame != NULL)
        {
            stream->on_frame(&frame, stream->context);
        }
        found = sml_parser_next(&stream->parser, &frame);
        account_noise(stream);
    }
}


void sml_stream_timeout(SmlStream *stream)
{
    if (sml_parser_timeout(&stream->parser))
    {
        account_noise(stream);
    }
}
