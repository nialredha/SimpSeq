static u32 trk_asset_add(Arena* arena, Trk_Asset_Slop* asset_slop, String filename)
{
    printf("\nAttempting to add asset %s\n", filename.data);

    // see if asset already exists and return its id if it does
    for (u32 slot_id = asset_slop->slop.first_slot; slot_id != 0; slot_id = asset_slop->slop.slots[slot_id].next)
    {
        Trk_Asset* asset = &asset_slop->assets[slot_id];

        if (str_compare(filename, asset->filename))
        {
            printf("  Already exists! ID: %u\n", slot_id);
            return slot_id;
        }
    }

    u32 slot_id = slop_slot_add(&asset_slop->slop);

    if (slot_id != 0)
    {
        Trk_Asset* asset = &asset_slop->assets[slot_id];

        asset->filename = str_copy(arena, filename);
        printf("  Success! ID: %u\n", slot_id);
    }

    return slot_id;
}

static void trk_asset_rem(Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        // TODO[nr]: do we need to clear the data?
        // clear data
        Trk_Asset* asset = &asset_slop->assets[slot_id];
        *asset = (Trk_Asset){0};

        // remove from active slot pool
        slop_slot_rem(&asset_slop->slop, slot_id);
    }

    return;
}

static Trk_Asset* trk_asset_get(Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    Trk_Asset* result = &asset_slop->assets[0];

    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        result = &asset_slop->assets[slot_id];
    }

    return result;
}

static Trk_Asset* trk_asset_find_or_load(Arena* arena, Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    Trk_Asset* result = &asset_slop->assets[0];

    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        result = &asset_slop->assets[slot_id];

        if (result->data.buffer == 0)
        {
            // load the sound!
            String file = os_read_entire_file(result->filename);

            if (file.data != 0 && file.count > 0)
            {
                Wav wav = wav_from_data(arena, file);

                Wav_Format format = wav_get_format(&wav.sub_chunks, file);

                assert(format.format_tag      == SUPPORTED_FORMAT_TAG); 
                assert(format.bits_per_sample == SUPPORTED_BIT_DEPTH);
                assert(format.num_channels    == SUPPORTED_CHANNEL_COUNT);
                assert(format.sample_rate     == SUPPORTED_SAMPLE_RATE);

                Wav_Data data = wav_get_data(arena, &wav.sub_chunks, file);

                // printf("%s samples: %u\n", result->filename.data, data.size / 4);

                result->format = format;
                result->data   = data;

                os_free_file_contents(&file);
            }
        }
    }

    return result;
}

static u32 trk_sound_add(Trk_Sound_Slop* sound_slop, u32 asset_id)
{
    u32 slot_id = slop_slot_add(&sound_slop->slop);

    if (slot_id != 0)
    {
        Trk_Sound* sound = &sound_slop->sounds[slot_id];
        sound->asset_id = asset_id;
        sound->playhead = 0;

        trk_params_init(&sound->params);

        sound->loop = false;
    }

    return slot_id;
}

static void trk_sound_rem(Trk_Sound_Slop* sound_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        // clear data
        Trk_Sound* sound = &sound_slop->sounds[slot_id];
        *sound = (Trk_Sound){0};

        // remove from active slot pool
        slop_slot_rem(&sound_slop->slop, slot_id);
    }

    return;
}

static Trk_Sound* trk_sound_get(Trk_Sound_Slop* sound_slop, u32 slot_id)
{
    Trk_Sound* result = &sound_slop->sounds[0];

    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        result = &sound_slop->sounds[slot_id];
    }

    return result;
}

static void trk_params_init(Trk_Params* params)
{
    params->volume     = 0.5f;
    params->pan        = 0.0f;
    params->pitch      = 1.0f;
    params->trim_left  = 0.0f;
    params->trim_right = 0.0f;
    params->delay      = 0.0f;

    params->use_envelope = false;
    params->envelope = (FX_Envelope){0};

    return;
}

static void trk_pattern_init(Trk_Pattern* pattern, f32 bpm, u32 num_steps)
{
    pattern->bpm = bpm;

    pattern->volume = 1.0f;
    pattern->pitch  = 1.0f;
    pattern->pan    = 0.0f;

    pattern->cell_count = num_steps;

    return;
}

static void trk_pattern_clear(Trk_Pattern* pattern)
{
    pattern->bpm = 0.0f;

    pattern->volume = 0.0;
    pattern->pitch  = 0.0;
    pattern->pan    = 0.0;

    pattern->cell_count = 0;
    pattern->row_count  = 0;

    return;
}

static u32 trk_pattern_add_row(Arena* arena, Trk_Asset_Slop* asset_slop, Trk_Pattern* pattern, String file, String sequence)
{
    u32 result = 0;

    if (pattern->row_count < TRK_MAX_ROWS)
    {
        u32 row_index = pattern->row_count;
        Trk_Row* row = &pattern->rows[row_index];

        trk_row_from_str(row, sequence);

        row->asset_id = trk_asset_add(arena, asset_slop, file);

        trk_params_init(&row->params);

        row->solo       = false;
        row->mute       = false;

        pattern->row_count += 1;

        result = row_index;
    }

    return result;
}

static void trk_row_from_str(Trk_Row* row, String str)
{
    u32 cell_index = 0; 
    while (*str.data != 0 && cell_index < TRK_MAX_CELLS)
    {
        str_eat_whitespace(&str);
        String cell_str = str_advance(&str, 1);

        switch (*cell_str.data)
        {
            case '0':
            {
                row->cells[cell_index++] = (Trk_Cell){0};
                break;
            }
            case '1':
            {
                Trk_Cell* cell = &row->cells[cell_index++];

                *cell = (Trk_Cell){0};

                cell->active = true;
                trk_params_init(&cell->params);
                cell->retrig = (Trk_Retrig){0};
                cell->swing  = 0;
                break;
            }
            default:
            {
                row = 0;
                break;
            }
       }
    }

    return;
}

static void trk_pattern_reset(Trk_Pattern* pattern)
{
    pattern->cell_count = 0;
    pattern->row_count  = 0;
    pattern->no_fx      = false;
    pattern->loop       = false;

    return;
}

static void trk_pattern_print(Trk_Pattern* pattern)
{
    printf("\n");
    printf("   |");
    for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
    {
        printf("C%.2u|", cell_index);
    }
    printf("\n");

    for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
    {
        printf("R%.2u|", row_index);
        for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
        {
            if (pattern->rows[row_index].cells[cell_index].active)
            {
                printf(" 1 |");
            }
            else
            {
                printf(" 0 |");
            }
        }
        printf("\n");
    }

    printf("   |");
    for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
    {
        printf("---|");
    }
    printf("\n");
    printf("\n");
}

static void trk_init(Trk* trk)
{
    u8* dest = (u8*)trk;
    for (u32 byte_index = 0; byte_index < sizeof(*trk); byte_index++)
    {
        *dest++ = 0;
    }

    arena_alloc(&trk->perm_arena,   16*1024*1024); // 16MB
    arena_alloc(&trk->reload_arena, 1*1024*1024);  // 1MB
    arena_alloc(&trk->update_arena, 128*1024);     // 128KB

    return;
}

static u32 trk_play_sound(Trk* trk, u32 asset_id)
{
    u32 sound_id = 0;

    Trk_Asset* asset = trk_asset_find_or_load(&trk->perm_arena, &trk->asset_slop, asset_id);

    if (asset->data.buffer != 0)
    {
        sound_id = trk_sound_add(&trk->sound_slop, asset_id);
    }

    return sound_id;
}

static void trk_stop_sound(Trk* trk, u32 sound_id)
{
    trk_sound_rem(&trk->sound_slop, sound_id);

    return;
}

static void trk_play_pattern(Trk* trk, Trk_Pattern* pattern, Ring_Buffer* out)
{
    u32 bytes_available   = (u32)os_win32_atomic_compare_exchange_s64(&out->amount_free, 0, 0);
    if (bytes_available == 0) { return; }

    if (pattern->bpm > 0)
    {
        u32 samples_per_beat = trk_samples_per_beat(pattern->bpm, (f32)SUPPORTED_SAMPLE_RATE);

        u32 samples_to_mix = 9600; // TODO[nr] @better
        samples_to_mix = samples_to_mix > samples_per_beat ? samples_per_beat : samples_to_mix; // clamp to samples per beat

        u32 total_samples_in_pattern = samples_per_beat * pattern->cell_count;

        // check whether we need to loop the pattern
        if (pattern->loop && trk->sample_playhead >= total_samples_in_pattern)
        {
            trk->beat_playhead   = 0;
            trk->sample_playhead = 0;
        }

        if (trk->sample_playhead < total_samples_in_pattern)
        {
            f32 cell_pos   = (f32)trk->sample_playhead / (f32)samples_per_beat;
            u32 cell_index = (u32)cell_pos;

            bool solo_enabled = false;
            for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
            {
                if (pattern->rows[row_index].solo)
                {
                    solo_enabled = true;
                    break;
                }
            }

            if (cell_index >= trk->beat_playhead && cell_index < TRK_MAX_CELLS)
            {
                // beat boundary!
                for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
                {
                    Trk_Row* row   = &pattern->rows[row_index];
                    Trk_Cell* cell = &row->cells[cell_index];

                    if (solo_enabled && !row->solo) { continue; }
                    if (row->mute) { continue; }

                    if (cell->active)
                    {
                        Trk_Params params; 
                        trk_params_init(&params);

                        params.volume = pattern->volume * row->params.volume * cell->params.volume;
                        if (!trk->pattern.no_fx)
                        {
                            params.pitch      = row->params.pitch * cell->params.pitch;
                            params.trim_left  = row->params.trim_left * cell->params.trim_left;
                            params.trim_right = row->params.trim_right * cell->params.trim_right;
                            params.delay      = row->params.delay * cell->params.delay;
                            
                            f32 pan           = row->params.pan + cell->params.pan;
                            params.pan = pan < -1.0f ? pan = -1.0f : pan > 1.0f ? pan = 1.0f : pan;

                            params.use_envelope = row->params.use_envelope;
                            params.envelope.att = row->params.envelope.att;
                            params.envelope.dec = row->params.envelope.dec;
                            params.envelope.sus = row->params.envelope.sus;
                            params.envelope.rel = row->params.envelope.rel;
                        }

                        u32 sound_id     = trk_play_sound(trk, row->asset_id);
                        Trk_Sound* sound = trk_sound_get(&trk->sound_slop, sound_id);
                        sound->params    = params;

                        // swing
                        u32 swing_in_samples = 0;
                        if (cell->swing > 0)
                        {
                            swing_in_samples = (u32)(cell->swing * (f32)samples_per_beat);
                            sound->playhead = -1 * swing_in_samples;
                        }

                        // retrig
                        if (cell->retrig.count > 0 && cell->retrig.division > 1)
                        {
                            assert(cell->retrig.count <= cell->retrig.division);

                            u32 samples_per_div = (u32)((f32)samples_per_beat / cell->retrig.division);

                            for (u32 div_index = 1; div_index <= cell->retrig.count; ++div_index)
                            {
                                sound_id      = trk_play_sound(trk, row->asset_id);
                                sound         = trk_sound_get(&trk->sound_slop, sound_id);
                                sound->params = params;

                                f32 a = cell->params.volume;
                                f32 b = 0.2f;
                                f32 t = cell->retrig.velocity * div_index;
                                if (cell->retrig.velocity < 0) { t *= -1;    }
                                else                           { t  = 1 - t; }

                                f32 cell_volume      = lerp(a, b, t);
                                sound->params.volume = pattern->volume * row->params.volume * cell_volume;


                                sound->playhead = -1 * (swing_in_samples + (div_index * samples_per_div));
                            }
                        }
                    }
                }

                trk->beat_playhead++;
            }
            else
            {
                // between beats!
                u32 playhead_norm = trk->sample_playhead % samples_per_beat;
                u32 samples_til_next_beat = samples_per_beat - playhead_norm;

                if (samples_til_next_beat == 0)
                {
                    printf("sample_playhead = %d\n"
                           "samples_per_beat = %d\n"
                           "samples_to_mix = %d\n"
                           "beat_playhead = %d\n"
                           "cell_index = %d\n",
                           trk->sample_playhead,
                           samples_per_beat,
                           samples_to_mix,
                           trk->beat_playhead,
                           cell_index);
                }

                assert(samples_til_next_beat != 0);

                if (samples_til_next_beat < samples_to_mix)
                {
                    samples_to_mix = samples_til_next_beat;
                }
            }
        }

        trk->sample_playhead += trk_mix(trk, samples_to_mix, out);
    }
}

static u32 trk_mix(Trk* trk, u32 samples_to_mix, Ring_Buffer* out)
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // TODO[nr] @better: this whole thing depends on the WAV being f32 48KHz stereo!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    f32* dry_mix_buffer = ARENA_PUSH_ARRAY(&trk->update_arena, f32, samples_to_mix);
    f32* wet_mix_buffer = ARENA_PUSH_ARRAY(&trk->update_arena, f32, samples_to_mix);

    Trk_Asset_Slop* assets = &trk->asset_slop;
    Trk_Sound_Slop* sounds = &trk->sound_slop;

    u32 bytes_available   = (u32)os_win32_atomic_compare_exchange_s64(&out->amount_free, 0, 0);
    if (bytes_available == 0) { return 0; }

    u32 bytes_per_sample  = sizeof(4);
    u32 samples_available = bytes_available / bytes_per_sample;

    samples_to_mix = samples_available < samples_to_mix ? samples_available : samples_to_mix;

    // clear buffer
    for (u32 mix_index = 0; mix_index < samples_to_mix; ++mix_index)
    {
        dry_mix_buffer[mix_index] = 0;
        wet_mix_buffer[mix_index] = 0;
    }

    for (u32 sound_id = sounds->slop.first_slot; sound_id != 0; )
    {
        Trk_Sound* sound = trk_sound_get(sounds, sound_id);
        Trk_Asset* asset = trk_asset_get(assets, sound->asset_id);

#if 0
        printf("Sound Params {\n"
               "  volume     = %f\n"
               "  pan        = %f\n"
               "  pitch      = %f\n"
               "  trim_left  = %f\n"
               "  trim_right = %f\n"
               "  delay      = %f\n"
               "}\n",
               sound->params.volume,
               sound->params.pan,
               sound->params.pitch,
               sound->params.trim_left,
               sound->params.trim_right,
               sound->params.delay);
#endif

        u32 samples_in_asset  = asset->data.size / bytes_per_sample;

        u32 mix_index = 0;

        while (sound->playhead < 0 && mix_index < samples_to_mix)
        {
            sound->playhead++;
            mix_index++;
        }

        u32 byte_index  = sound->playhead * bytes_per_sample;
        f32* asset_data = (f32*)&asset->data.buffer[byte_index];

        u32 trim_left_samples     = (u32)((sound->params.trim_left  * samples_in_asset) + 0.5f);
        u32 trim_right_samples    = (u32)((sound->params.trim_right * samples_in_asset) + 0.5f);
        u32 total_samples_to_trim = trim_left_samples + trim_right_samples;

        if (total_samples_to_trim <= samples_in_asset)
        {
            samples_in_asset = samples_in_asset - total_samples_to_trim;
            byte_index       = (trim_left_samples + sound->playhead) * bytes_per_sample;
            asset_data       = (f32*)&asset->data.buffer[byte_index];
        }

        // TODO[nr] @study: currently doing equal power panning, but that makes center quieter...
#if 0
        f32 pan_0 = (1 - sound->params.pan);
        f32 pan_1 = (sound->params.pan);
#else
        f32 pan_0 = sound->params.pan < 0 ? 1.0f : 1 - sound->params.pan;
        f32 pan_1 = sound->params.pan > 0 ? 1.0f : 1 + sound->params.pan;
#endif

        u32 sample_index = 0;
        f32 d_sample = sound->params.pitch;

        f32 frames_remain = ((f32)(samples_in_asset - sound->playhead)) / 2.0f;

        for (f32 frame_pos = 0; frame_pos < frames_remain; frame_pos += d_sample)
        {
            if (mix_index >= samples_to_mix) { break; }

            // u32 frame_index = (u32)(frame_pos + 0.5f);
            u32 frame_index = (u32)(frame_pos);

            f32 frac        = frame_pos - (f32)frame_index;
            sample_index    = frame_index * 2;

            f32 sample_0 = asset_data[sample_index + 0]; // 0L
            f32 sample_1 = asset_data[sample_index + 1]; // 0R

            if (sound->playhead + sample_index + 2 < samples_in_asset)
            {
                f32 sample_2 = asset_data[sample_index + 2]; // 1L
                f32 sample_3 = asset_data[sample_index + 3]; // 1R

                sample_0 = lerp(sample_0, sample_2, frac);
                sample_1 = lerp(sample_1, sample_3, frac);
            }               

            f32 mix_l = (sample_0 * sound->params.volume * pan_0);
            f32 mix_r = (sample_1 * sound->params.volume * pan_1);

            if (sound->params.use_envelope)
            {
                u32 frames_in_asset = (u32)(samples_in_asset/2);
                u32 global_frame_index = (u32)((sound->playhead + sample_index)/2);
                mix_l = fx_envelope_step(&sound->params.envelope, mix_l, frames_in_asset, global_frame_index);
                mix_r = fx_envelope_step(&sound->params.envelope, mix_r, frames_in_asset, global_frame_index);
            }

            dry_mix_buffer[mix_index]     += mix_l;
            dry_mix_buffer[mix_index + 1] += mix_r;

            wet_mix_buffer[mix_index]     += mix_l * sound->params.delay;
            wet_mix_buffer[mix_index + 1] += mix_r * sound->params.delay;

            mix_index += 2;
        }

        sound->playhead += sample_index + 2;

        u32 next_sound_id = sounds->slop.slots[sound_id].next;

        if (sound->playhead >= (s32)samples_in_asset)
        {
            if (sound->loop)
            {
                sound->playhead = 0;
            }
            else
            {
                trk_sound_rem(sounds, sound_id);
            }
        }

        sound_id = next_sound_id;

    }

    // pour wet into dry
    for (u32 i = 0; i < samples_to_mix; i+=2)
    {
        dry_mix_buffer[i]   += fx_feedback_comb_step(&trk->feedback_comb_l, wet_mix_buffer[i]);
        dry_mix_buffer[i+1] += fx_feedback_comb_step(&trk->feedback_comb_r, wet_mix_buffer[i+1]);
    }

    u32 bytes_mixed = samples_to_mix * bytes_per_sample;

    u32 bytes_written = rb_write(out, (u8*)dry_mix_buffer, bytes_mixed);

    assert(bytes_written == bytes_mixed);

    return samples_to_mix;
}

static u32 trk_samples_per_beat(f32 bpm, f32 samples_per_second)
{
    f32 beats_per_second   = bpm / 60.0f;
    u32 samples_per_beat   = (u32)(samples_per_second / beats_per_second);

    return samples_per_beat;
}
