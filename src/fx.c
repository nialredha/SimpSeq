//
// delay
//

static FX_Delay fx_delay_init(Arena* arena, u32 capacity)
{
    FX_Delay result = {0};

    result.base = ARENA_PUSH_ARRAY(arena, f32, capacity);
    result.capacity = capacity;

    // clear delay line to 0
    for (u32 i = 0; i < result.capacity; ++i)
    {
        result.base[i] = 0;
    }

    return result;
}

static f32 fx_delay_step(FX_Delay* delay, f32 value)
{
    f32* pos = delay->base + delay->used;

    f32 result = *pos;
    *pos       = value;

    if (++delay->used >= delay->capacity)
    {
        delay->used = 0;
    }

    return result;
}

static f32 fx_delay_peek(FX_Delay* delay)
{
    return *(delay->base + delay->used);
}

//
// feedforward comb filter
//

static FX_Feedforward_Comb fx_feedforward_comb_init(Arena* arena, u32 sample_rate, u32 targ_freq, bool destruct_at_targ_freq)
{
    assert(targ_freq > 0);

    u32 capacity         = (u32)((f32)sample_rate / (2.0f * (f32)targ_freq));
    f32 input_path_coeff = 1.0f;
    f32 delay_path_coeff = -1.0f;

    if (destruct_at_targ_freq)
    {
        delay_path_coeff = 1.0f;
    }

    return fx_feedforward_comb_init_ll(arena, capacity, input_path_coeff, delay_path_coeff);
}

static FX_Feedforward_Comb fx_feedforward_comb_init_ll(Arena* arena, u32 capacity, f32 input_path_coeff, f32 delay_path_coeff)
{
    FX_Feedforward_Comb result = {0};

    result.delay            = fx_delay_init(arena, capacity);
    result.input_path_coeff = input_path_coeff;
    result.delay_path_coeff = delay_path_coeff;

    return result;
}

static f32 fx_feedforward_comb_step(FX_Feedforward_Comb* ffc, f32 value)
{
    f32 delay_value = fx_delay_step(&ffc->delay, value);

    f32 result = (ffc->input_path_coeff * value) + (ffc->delay_path_coeff * delay_value);

    return result;
}

//
// feedback comb filter
//

static FX_Feedback_Comb fx_feedback_comb_init(Arena* arena, u32 capacity, f32 feedforward_coeff, f32 feedback_coeff)
{
    FX_Feedback_Comb result = {0};

    result.delay             = fx_delay_init(arena, capacity);
    result.feedforward_coeff = feedforward_coeff;
    result.feedback_coeff    = feedback_coeff;

    return result;
}

static f32 fx_feedback_comb_step(FX_Feedback_Comb* fbc, f32 value)
{
    f32 feedforward_value = fbc->feedforward_coeff * value;
    f32 feedback_value    = fbc->feedback_coeff * fx_delay_peek(&fbc->delay);

    f32 result = feedforward_value + feedback_value;

    fx_delay_step(&fbc->delay, result);

    return result;
}

//
// envelope
//

static f32 fx_envelope_step(FX_Envelope* env, f32 value, u32 sample_len, u32 sample_pos)
{
    f32 sample_frac = (f32)sample_pos / (f32)sample_len;

    assert(sample_frac >= 0 && sample_frac <= 1.0f);

    f32 t = 0.0f;
    f32 volume = 0.0f;

    if (sample_frac < env->att)
    {
        // attack
        t = sample_frac / env->att;
        volume = lerp(0.0f, 1.0f, t);

    }
    else if (sample_frac < (env->att + env->dec))
    {
        // decay
        f32 frac_norm = sample_frac - env->att;
        t = frac_norm / env->dec;
        volume = lerp(1.0f, env->sus, t);
    }
    else if (sample_frac < (1 - env->rel))
    {
        // sustain
        volume = env->sus;
    }
    else if (sample_frac > (1 - env->rel))
    {
        // release
        f32 frac_norm = sample_frac - (1 - env->rel);
        t = frac_norm / env->rel;
        volume = lerp(env->sus, 0.0f, t);
    }
    else
    {
        return value;
    }

    f32 result = value * volume;
    return result;
}
