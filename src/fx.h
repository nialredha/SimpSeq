#ifndef FX_H
#define FX_H

//
// delay
//

typedef struct
{
    f32* base;
    u32  capacity;
    u32  used;
} FX_Delay;

static FX_Delay fx_delay_init(Arena* arena, u32 capacity);
static f32      fx_delay_step(FX_Delay* delay, f32 value);
static f32      fx_delay_peek(FX_Delay* delay);
static f32      fx_delay_clear(FX_Delay* delay);

//
// feedforward comb filter
//

typedef struct
{
    FX_Delay delay;

    f32 input_path_coeff;
    f32 delay_path_coeff;
} FX_Feedforward_Comb;

static FX_Feedforward_Comb fx_feedforward_comb_init   (Arena* arena, u32 sample_rate, u32 targ_freq, bool destruct_at_targ_freq);
static FX_Feedforward_Comb fx_feedforward_comb_init_ll(Arena* arena, u32 capacity, f32 input_path_coeff, f32 delay_path_coeff);
static f32                 fx_feedforward_comb_step   (FX_Feedforward_Comb* ffc, f32 value);

//
// feedback comb filter
//

typedef struct
{
    FX_Delay delay;

    f32 feedforward_coeff;
    f32 feedback_coeff;
} FX_Feedback_Comb;

static FX_Feedback_Comb fx_feedback_comb_init(Arena* arena, u32 capacity, f32 feedforward_coeff, f32 feedback_coeff);
static f32              fx_feedback_comb_step(FX_Feedback_Comb* fbc, f32 value);

typedef struct
{
    f32 att;
    f32 dec;
    f32 sus;
    f32 rel;
} FX_Envelope;

static f32 fx_envelope_step(FX_Envelope* env, f32 value, u32 sample_len, u32 sample_pos);

#endif // FX_H
