/*
 * A received byte stream that carries both SML frames and plain text (a PIC
 * in TEXT mode, or just reset). The SML parser finds the frames; every byte
 * it drops as noise is handed back as text, in order. Portable, for the host
 * tests.
 *
 * The parser keeps its buffer in arrival order and only ever drops bytes
 * from the front: as noise (counted in its stats) or as a returned frame.
 * The stream keeps a mirror of that buffer, so the noise bytes are the
 * oldest mirrored bytes not yet accounted for.
 */
#ifndef SML_STREAM_H
#define SML_STREAM_H

#include <stdint.h>

extern "C"
{
#include "sml/sml.h"
}

typedef void (*SmlFrameSink)(const SmlFrame *frame, void *context);
typedef void (*SmlTextSink)(const uint8_t *text, uint16_t length, void *context);

typedef struct
{
    SmlParser parser;
    uint8_t mirror[SML_MAX_FRAME_BYTES + 1];
    uint16_t count;
    uint32_t noise_seen; // parser noise bytes already handed back
    SmlFrameSink on_frame;
    SmlTextSink on_text;
    void *context;
} SmlStream;

void sml_stream_init(SmlStream *stream, SmlFrameSink on_frame, SmlTextSink on_text, void *context);
void sml_stream_push(SmlStream *stream, uint8_t byte);
// The line went quiet with a frame part-received: it was text after all.
void sml_stream_timeout(SmlStream *stream);

#endif // SML_STREAM_H
