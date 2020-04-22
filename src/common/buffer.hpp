/**
 * Author: Peiyao Zhao
 * E-Mail: pdszpy19930218@163.com
 *
 */

#ifndef BUFFER_HEAD
#define BUFFER_HEAD

#include "Symbols.hpp"
#include "memory_manage.h"
#include <sstream>
#include <vector>

/* boost is required for aligned memory allocation (for SIMD instructions) */
#include <boost/align/aligned_allocator.hpp>
#ifdef USE_LDPC
#include "common_typedef_sdk.h"
#else
struct complex_float {
    float re;
    float im;
};
#endif

// Event data tag for RX events
union rx_tag_t {
    struct {
        size_t tid : 8; // ID of the socket thread that received the packet
        size_t offset : 56; // Offset in the socket thread's RX buffer
    };
    size_t _tag;

    rx_tag_t(size_t tid, size_t offset)
        : tid(tid)
        , offset(offset)
    {
    }

    rx_tag_t(size_t _tag)
        : _tag(_tag)
    {
    }
};

// Event data tag for FFT task requests
using fft_req_tag_t = rx_tag_t;

// Number of bits in the generic 3D tag type
static constexpr size_t kSymbolIdBits = 14;
static constexpr size_t kBlankSymbolId = (1ull << kSymbolIdBits) - 1;
static_assert(k5GMaxSymbolsPerFrame < kBlankSymbolId, "");

static constexpr size_t kSubcarrierIdBits = 13;
static constexpr size_t kBlankSubcarrierId = (1ull << kSubcarrierIdBits) - 1;
static_assert(k5GMaxSubcarriers < kBlankSymbolId - 1, "");

static constexpr size_t kAntennaIdBits = 9;
static constexpr size_t kBlankAntennaId = (1ull << kAntennaIdBits) - 1;
static_assert(kMaxAntennas < kBlankAntennaId - 1, "");

static constexpr size_t kFrameIdBits
    = (64 - (kSymbolIdBits + kSubcarrierIdBits + kAntennaIdBits));

// A generic tag type for Millipede tasks
union gen_tag_t {
    struct {
        size_t ant_id : kAntennaIdBits;
        size_t frame_id : kFrameIdBits;
        size_t symbol_id : kSymbolIdBits;
        size_t sc_id : kSubcarrierIdBits;
    };

    size_t _tag;

    gen_tag_t(size_t ant_id, size_t frame_id, size_t symbol_id, size_t sc_id)
        : ant_id(ant_id)
        , frame_id(frame_id)
        , symbol_id(symbol_id)
        , sc_id(sc_id)
    {
    }

    gen_tag_t(size_t _tag)
        : _tag(_tag)
    {
    }

    // Generate a tag with frame ID, symbol ID, and subcarrier ID bits set and
    // other fields blank
    static gen_tag_t frm_sym_sc(size_t frame_id, size_t symbol_id, size_t sc_id)
    {
        return gen_tag_t(kBlankAntennaId, frame_id, symbol_id, sc_id);
    }

    // Generate a tag with frame ID and subcarrier ID bits set, and other fields
    // blank
    static gen_tag_t frm_sc(size_t frame_id, size_t sc_id)
    {
        return gen_tag_t(kBlankAntennaId, frame_id, kBlankSymbolId, sc_id);
    }

    // Generate a tag with frame ID and symbol ID bits set, and other fields
    // blank
    static gen_tag_t frm_sym(size_t frame_id, size_t sym_id)
    {
        return gen_tag_t(kBlankAntennaId, frame_id, sym_id, kBlankSubcarrierId);
    }
};
static_assert(sizeof(gen_tag_t) == sizeof(size_t), "");

/**
 * Millipede uses these event messages for communication between threads. Each
 * tag encodes information about a task.
 */
struct Event_data {
    static constexpr size_t kMaxTags = 7;
    EventType event_type;
    uint32_t num_tags;
    size_t tags[7];

    // Initialize and event with only the event type field set
    Event_data(EventType event_type)
        : event_type(event_type)
        , num_tags(0)
    {
    }

    // Create an event with one tag
    Event_data(EventType event_type, size_t tag)
        : event_type(event_type)
        , num_tags(1)
    {
        tags[0] = tag;
    }

    Event_data()
        : num_tags(0)
    {
    }
};
static_assert(sizeof(Event_data) == 64, "");

struct Packet {
    uint32_t frame_id;
    uint32_t symbol_id;
    uint32_t cell_id;
    uint32_t ant_id;
    uint32_t fill[12]; // Padding for 64-byte alignment needed for SIMD
    short data[]; // Elements sent by antennae are two bytes (I/Q samples)
    Packet(int f, int s, int c, int a) // TODO: Should be unsigned integers
        : frame_id(f)
        , symbol_id(s)
        , cell_id(c)
        , ant_id(a)
    {
    }

    std::string to_string()
    {
        std::ostringstream ret;
        ret << "[Frame seq num " << frame_id << ", symbol ID " << symbol_id
            << ", cell ID " << cell_id << ", antenna ID " << ant_id << "]";
        return ret.str();
    }
};

struct RX_stats {
    std::array<size_t, TASK_BUFFER_FRAME_NUM> task_count;
    std::array<size_t, TASK_BUFFER_FRAME_NUM> task_pilot_count;
    size_t max_task_count; // Max packets per frame
    size_t max_task_pilot_count; // Max pilot packets per frame
};

struct Frame_stats {
    size_t frame_count;
    std::array<size_t, TASK_BUFFER_FRAME_NUM> symbol_count;
    size_t max_symbol_count;
    bool last_symbol(int frame_id)
    {
        if (++symbol_count[frame_id] == max_symbol_count) {
            symbol_count[frame_id] = 0;
            return (true);
        }
        return (false);
    }
    void init(int _max_symbol_count)
    {
        frame_count = 0;
        symbol_count.fill(0);
        max_symbol_count = _max_symbol_count;
    }
    void update_frame_count() { frame_count++; }
};

struct ZF_stats : public Frame_stats {
    int coded_frame;
    size_t& max_task_count;
    ZF_stats(void)
        : max_task_count(max_symbol_count)
    {
    }
    void init(int max_tasks)
    {
        Frame_stats::init(max_tasks);
        coded_frame = -1;
    }
};

struct Data_stats : public Frame_stats {
    size_t* task_count[TASK_BUFFER_FRAME_NUM];
    size_t max_task_count;

    void init(int _max_task_count, int max_symbols, int max_data_symbol)
    {
        Frame_stats::init(max_symbols);
        for (size_t i = 0; i < TASK_BUFFER_FRAME_NUM; i++)
            task_count[i] = new size_t[max_data_symbol]();
        max_task_count = _max_task_count;
    }
    void fini()
    {
        for (size_t i = 0; i < TASK_BUFFER_FRAME_NUM; i++)
            delete[] task_count[i];
    }
    bool last_task(int frame_id, int data_symbol_id)
    {
        if (++task_count[frame_id][data_symbol_id] == max_task_count) {
            task_count[frame_id][data_symbol_id] = 0;
            return (true);
        }
        return (false);
    }
};

struct FFT_stats : public Data_stats {
    size_t max_symbol_data_count;
    std::array<size_t, TASK_BUFFER_FRAME_NUM> symbol_cal_count;
    size_t max_symbol_cal_count;

    // cur_frame_for_symbol[i] is the current frame for the symbol whose
    // index in the frame's uplink symbols is i
    std::vector<size_t> cur_frame_for_symbol;
};

struct RC_stats {
    size_t frame_count;
    int max_task_count;
    int last_frame;
    RC_stats(void)
        : frame_count(0)
        , max_task_count(1)
        , last_frame(-1)
    {
    }
    void update_frame_count(void) { frame_count++; }
};

/* TODO: clean up the legency code below */
struct FFTBuffer {
    // Data before IFFT
    // record TASK_BUFFER_FRAME_NUM entire frames
    Table<complex_float> FFT_inputs;
    Table<complex_float> FFT_outputs;
};

struct IFFTBuffer {
    // Data before IFFT
    // record TASK_BUFFER_FRAME_NUM entire frames
    Table<complex_float> IFFT_inputs;
    Table<complex_float> IFFT_outputs;
};

inline size_t generateOffset2d(
    size_t max_dim1, size_t max_dim2, size_t dim1_id, size_t dim2_id)
{
    dim1_id = dim1_id % max_dim1;
    return dim1_id * max_dim2 + dim2_id;
}

inline size_t generateOffset3d(size_t max_dim1, size_t max_dim2,
    size_t max_dim3, size_t dim1_id, size_t dim2_id, size_t dim3_id)
{
    dim1_id = dim1_id % max_dim1;
    size_t dim2d_id = dim1_id * max_dim2 + dim2_id;
    return dim2d_id * max_dim3 + dim3_id;
}

inline void interpretOffset2d(
    size_t max_dim2, size_t offset, size_t* dim1_id, size_t* dim2_id)
{
    *dim2_id = offset % max_dim2;
    *dim1_id = offset / max_dim2;
}

inline void interpretOffset3d(size_t max_dim2, size_t max_dim3, size_t offset,
    size_t* dim1_id, size_t* dim2d_id, size_t* dim2_id, size_t* dim3_id)
{
    *dim3_id = offset % max_dim3;
    *dim2d_id = offset / max_dim3;
    *dim2_id = (*dim2d_id) % max_dim2;
    *dim1_id = (*dim2d_id) / max_dim2;
}

#endif
