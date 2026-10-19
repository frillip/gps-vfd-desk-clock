#include "tubes.h"

#include <stddef.h>
#include <string.h>
#include <xc.h>

#include "../platform/board.h"

/*
 * Board wiring (V2). The drivers' outputs are numbered as bits of one 64-bit
 * map; each tube's seven segments occupy seven consecutive outputs from its
 * offset, in the order segment_output gives; the separators have outputs of
 * their own.
 */
static const uint8_t tube_offset[TUBES_COUNT] = {33U, 20U, 13U, 0U}; // left to right

// Driver output (from the tube's offset) of segments a..g.
static const uint8_t segment_output[7] = {3U, 4U, 5U, 6U, 0U, 1U, 2U};

#define OUTPUT_START_DOT   31U
#define OUTPUT_START_LINE  32U
#define OUTPUT_MIDDLE_DOT  11U
#define OUTPUT_MIDDLE_LINE 12U

// The words go out in this order, each taken from the map at this bit. The
// overlap is how the V2 driver chain is wired: kept exactly as V2 sent it.
static const uint8_t word_shift[TUBES_WORDS] = {24U, 32U, 16U, 0U};

// SPI2: master, 16-bit, CKP idle high, SCK = FP / 8 = 5 MHz, enhanced
// (8-deep) buffers. As V2.
#define SPI2CON1_VALUE 0x0463U
#define SPI2CON2_VALUE 0x0001U
#define SPI2STAT_VALUE 0x800CU
// Polls of a busy SPI before giving up: a frame takes ~13 us.
#define SPI_POLL_LIMIT 2000U

// OC3 on BLANK: double-compare continuous pulse from FP, synchronised to
// itself, period TUBES_BRIGHTNESS_FULL ticks (10 kHz). As V2.
#define OC3CON1_VALUE 0x1C0DU
#define OC3CON2_VALUE 0x001FU
#define OC3_OCM_MASK  0x0007U

// LATCH_GPIO high for at least 1 us.
#define LATCH_PULSE_LOOPS 10U

static TubesFrame shown;
static TubesFrame loaded;
static bool load_pending; // loaded differs from shown, awaiting an epoch
static uint16_t brightness;
static TubesStats stats;

// --------------------------------------------------------------------------
// Encoding
// --------------------------------------------------------------------------

static uint64_t output_map(const TubesFrame *frame)
{
    uint64_t map = 0;
    uint8_t tube;
    uint8_t segment;

    for (tube = 0; tube < TUBES_COUNT; tube++)
    {
        for (segment = 0; segment < 7U; segment++)
        {
            if ((frame->segments[tube] & (1U << segment)) != 0U)
            {
                map |= 1ULL << (tube_offset[tube] + segment_output[segment]);
            }
        }
    }
    if ((frame->separators & TUBES_START_DOT) != 0U)
    {
        map |= 1ULL << OUTPUT_START_DOT;
    }
    if ((frame->separators & TUBES_START_LINE) != 0U)
    {
        map |= 1ULL << OUTPUT_START_LINE;
    }
    if ((frame->separators & TUBES_MIDDLE_DOT) != 0U)
    {
        map |= 1ULL << OUTPUT_MIDDLE_DOT;
    }
    if ((frame->separators & TUBES_MIDDLE_LINE) != 0U)
    {
        map |= 1ULL << OUTPUT_MIDDLE_LINE;
    }
    return map;
}


void tubes_encode(const TubesFrame *frame, uint16_t words[TUBES_WORDS])
{
    uint64_t map = output_map(frame);
    uint8_t index;

    for (index = 0; index < TUBES_WORDS; index++)
    {
        words[index] = (uint16_t)(map >> word_shift[index]);
    }
}


// --------------------------------------------------------------------------
// Hardware
// --------------------------------------------------------------------------

// Shifts a frame into the drivers and waits until the last bit is out.
static void shift(const TubesFrame *frame)
{
    uint16_t words[TUBES_WORDS];
    uint16_t polls = 0;
    uint8_t index;

    tubes_encode(frame, words);
    // The enhanced buffer holds all four words.
    for (index = 0; index < TUBES_WORDS; index++)
    {
        while (SPI2STATbits.SPITBF && polls < SPI_POLL_LIMIT)
        {
            polls++;
        }
        SPI2BUF = words[index];
    }
    while ((SPI2STATbits.SPIBEC != 0U || !SPI2STATbits.SRMPT) && polls < SPI_POLL_LIMIT)
    {
        polls++;
    }
    if (polls >= SPI_POLL_LIMIT)
    {
        stats.spi_stalls++;
    }
    // Nothing to receive: discard what came back.
    while (!SPI2STATbits.SRXMPT)
    {
        (void)SPI2BUF;
    }
    SPI2STATbits.SPIROV = 0;
}


static void latch_pulse(void)
{
    volatile uint8_t loops;

    DISPLAY_LATCH_GPIO_SET_HIGH();
    for (loops = 0; loops < LATCH_PULSE_LOOPS; loops++)
    {
    }
    DISPLAY_LATCH_GPIO_SET_LOW();
}


static void spi_init(void)
{
    SPI2STAT = 0;
    SPI2CON1 = SPI2CON1_VALUE;
    SPI2CON2 = SPI2CON2_VALUE;
    IFS2bits.SPI2IF = 0;
    IEC2bits.SPI2IE = 0;
    SPI2STAT = SPI2STAT_VALUE;
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void tubes_init(void)
{
    memset(&shown, 0, sizeof(shown));
    memset(&loaded, 0, sizeof(loaded));
    memset(&stats, 0, sizeof(stats));
    load_pending = false;

    spi_init();
    shift(&loaded);
    latch_pulse();
    tubes_brightness_set(TUBES_BRIGHTNESS_DEFAULT);
}


void tubes_load(const TubesFrame *frame)
{
    if (frame == NULL)
    {
        return;
    }
    if (tubes_latch_open())
    {
        stats.loads_too_late++;
    }
    shift(frame);
    loaded = *frame;
    load_pending = true;
    stats.loads++;
}


void tubes_show(const TubesFrame *frame)
{
    if (frame == NULL)
    {
        return;
    }
    shift(frame);
    latch_pulse();
    shown = *frame;
    stats.shows++;
    // The epoch would otherwise latch this frame again, not the one meant
    // for it. While the latch is open the outputs follow the register, so
    // the reload would show at once: then the epoch's frame wins anyway.
    if (load_pending && !tubes_latch_open())
    {
        shift(&loaded);
        stats.reloads++;
    }
    else
    {
        loaded = *frame;
        load_pending = false;
    }
}


bool tubes_latch_open(void)
{
    return DISPLAY_LATCH_OC_GET_VALUE() != 0U;
}


void tubes_epoch(void)
{
    if (load_pending)
    {
        shown = loaded;
        load_pending = false;
    }
}


void tubes_brightness_set(uint16_t level)
{
    if (level < TUBES_BRIGHTNESS_MIN)
    {
        OC3CON1 &= (uint16_t)~OC3_OCM_MASK; // output reverts to the latch:
        DISPLAY_BLANK_SET_HIGH();           // blanked
        brightness = TUBES_BRIGHTNESS_OFF;
    }
    else if (level > TUBES_BRIGHTNESS_MAX)
    {
        OC3CON1 &= (uint16_t)~OC3_OCM_MASK;
        DISPLAY_BLANK_SET_LOW(); // lit continuously
        brightness = TUBES_BRIGHTNESS_FULL;
    }
    else
    {
        // BLANK is high from OC3R to OC3RS: the rest of the period is lit.
        OC3R = level;
        OC3RS = TUBES_BRIGHTNESS_FULL - 1U;
        if ((OC3CON1 & OC3_OCM_MASK) == 0U)
        {
            OC3CON2 = OC3CON2_VALUE;
            OC3CON1 = OC3CON1_VALUE;
        }
        brightness = level;
    }
}


uint16_t tubes_brightness(void)
{
    return brightness;
}


const TubesFrame *tubes_shown(void)
{
    return &shown;
}


const TubesFrame *tubes_loaded(void)
{
    return &loaded;
}


const TubesStats *tubes_stats(void)
{
    return &stats;
}
