void os_sleep_ms(u32 milliseconds)
{
    Sleep(milliseconds);
    return;
}

static u32 audio_playback_thread(void* param);

s32 os_win32_audio_init(OS_Audio_Device* device, OS_Audio_Config* config)
{
    IMMDeviceEnumerator* enumerator = 0;
    IMMDevice*           dev        = 0;

    IAudioClient* audio_client = 0;
    WAVEFORMATEX* audio_format = 0;

    IAudioRenderClient* render_client = 0;

    HANDLE event_handle = 0;

    HRESULT result = -1;

    f32 duration = 1.0; // TODO[nr] @temp

    // initialize the COM library
    result = CoInitializeEx(0, COINIT_MULTITHREADED);

    // create enumerator object on the local system
    if (result >= 0)
    {
        result = CoCreateInstance(&IID_MMDeviceEnumerator, 0, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&enumerator);
    }

    // get default device for renderering audio
    if (result >= 0 && enumerator != 0)
    {
        // TODO[nr] @better: only supporting default
        result = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &dev);
        enumerator->lpVtbl->Release(enumerator); // TODO[nr] @study: should we release this or hold onto it?
    }
    
    // get audio client
    if (result >= 0 && dev)
    {
        result = dev->lpVtbl->Activate(dev, &IID_IAudioClient, CLSCTX_ALL, 0, (void**)&audio_client);
        dev->lpVtbl->Release(dev);
    }

    // initialize audio_client and get audio render client service
    if (result >= 0 && audio_client)
    {
        result = audio_client->lpVtbl->GetMixFormat(audio_client, &audio_format);

        if (result >= 0)
        {
            String fmt_tag = wav_string_from_format_tag(audio_format->wFormatTag);

            printf("\n");
            printf("Mix format:\n");
            printf("======================================\n");
            printf("  Format Tag          : %.*s\n", fmt_tag.count, fmt_tag.data);
            printf("  Channels            : %d\n",   audio_format->nChannels);
            printf("  Samples Per Second  : %lu\n",  audio_format->nSamplesPerSec);
            printf("  Bytes Per Second    : %lu\n",  audio_format->nAvgBytesPerSec);
            printf("  Block Align         : %d\n",   audio_format->nBlockAlign);
            printf("  Bits Per Sample     : %d\n",   audio_format->wBitsPerSample);
            printf("  CB Size             : %d\n",   audio_format->cbSize);

            if (audio_format->wFormatTag == Wav_Format_Tag_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* audio_format_ext = (WAVEFORMATEXTENSIBLE*)audio_format;
                String sub_fmt_tag = wav_string_from_format_tag(audio_format_ext->SubFormat.Data1 & 0xFFFF);

                printf("  Valid Bits Per Samp : %d\n",      audio_format_ext->Samples.wValidBitsPerSample);
                printf("  Channel Mask        : 0x%02lX\n", audio_format_ext->dwChannelMask);
                printf("  Sub Format Tag      : %.*s\n",    sub_fmt_tag.count, sub_fmt_tag.data);
            }
            printf("\n");

            REFERENCE_TIME buffer_duration = (REFERENCE_TIME)(duration * REF_TIME_UNITS_PER_SECOND);
            printf("Requested Buffer Duration : %f * %f = %llu\n", duration, REF_TIME_UNITS_PER_SECOND, buffer_duration);

            if (audio_format->wFormatTag == Wav_Format_Tag_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* audio_format_ext = (WAVEFORMATEXTENSIBLE*)audio_format;

                if (config->desired_format.format_tag != Wav_Format_Tag_EXTENSIBLE)
                {
                    // TODO[nr] @study: leave channel mask the same?
                    audio_format_ext->Samples.wValidBitsPerSample = config->desired_format.bits_per_sample;
                    audio_format_ext->SubFormat.Data1 = (audio_format_ext->SubFormat.Data1 & (0xFFFF0000)) | (config->desired_format.format_tag & (0x0000FFFF));
                }
                else
                {
                    audio_format_ext->Samples.wValidBitsPerSample = config->desired_format.valid_bits_per_sample;
                    audio_format_ext->dwChannelMask = config->desired_format.channel_mask;

                    u8* subformat_dest = (u8*)&audio_format_ext->SubFormat; 
                    for (u32 i = 0; i < sizeof(config->desired_format.sub_format); ++i)
                    {
                        *subformat_dest++ = config->desired_format.sub_format[i];
                    }
                }
            }
            else
            {
                audio_format->wFormatTag = config->desired_format.format_tag;
            }

            audio_format->nChannels       = config->desired_format.num_channels;
            audio_format->nSamplesPerSec  = config->desired_format.sample_rate;
            audio_format->nBlockAlign     = (config->desired_format.bits_per_sample / 8) * config->desired_format.num_channels;

            audio_format->nAvgBytesPerSec = audio_format->nSamplesPerSec * audio_format->nBlockAlign;
            audio_format->wBitsPerSample  = config->desired_format.bits_per_sample;

            // TODO[nr] @study: AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
            result = audio_client->lpVtbl->Initialize(audio_client, 
                                                      AUDCLNT_SHAREMODE_SHARED, 
                                                      AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, 
                                                      buffer_duration, 
                                                      0, 
                                                      audio_format, 
                                                      0);
        }
    }

    // set event handle
    if (result >= 0)
    {
        event_handle = CreateEvent(0, 0, 0, 0);
        result = audio_client->lpVtbl->SetEventHandle(audio_client, event_handle);
    }

    // get render client
    if (result >= 0)
    {
        result = audio_client->lpVtbl->GetService(audio_client, &IID_IAudioRenderClient, (void**)&render_client);
    }

    device->audio_client   = audio_client;
    device->audio_format   = audio_format;
    device->render_client  = render_client;
    device->event_handle   = event_handle;
    device->audio_callback = config->audio_callback;
    device->user_data      = config->user_data;

    return (s32)result;
}

s32 os_win32_audio_start(OS_Audio_Device* device)
{
    HRESULT result = -1;

    if (device != 0 && device->audio_client != 0)
    {
        result = device->audio_client->lpVtbl->Start(device->audio_client);
    }

    // start audio thread
    device->audio_thread = CreateThread(0, 
                                        0, 
                                        (LPTHREAD_START_ROUTINE)audio_playback_thread, 
                                        (LPVOID)device, 
                                        0, 
                                        0);
    if (device->audio_thread)
    {
        result = 0;
    }

    return (s32)result;
}

s32 os_win32_audio_stop(OS_Audio_Device* device)
{
    HRESULT result = -1;
    
    device->quit = true;

    if (device != 0 && device->audio_client != 0)
    {
        result = device->audio_client->lpVtbl->Stop(device->audio_client);
    }

    if (result >= 0)
    {
        result = device->audio_client->lpVtbl->Reset(device->audio_client);
    }

    return (s32)result;
}

void os_win32_audio_deinit(OS_Audio_Device* device)
{
#define WIN32_RELEASE_POINTER(p) do { if (p != 0) { (p)->lpVtbl->Release(p); (p) = 0; } } while (0)
#define WIN32_RELEASE_HANDLE(h)  do { if (h != 0) { CloseHandle(h); (h) = 0; } } while (0)
#define WIN32_FREE(p)            do { if (p != 0) { CoTaskMemFree(p); } } while (0)
    
    if (device != 0)
    {
        WIN32_RELEASE_POINTER(device->audio_client);
        WIN32_RELEASE_POINTER(device->render_client);

        WIN32_FREE(device->audio_format);

        WIN32_RELEASE_HANDLE(device->event_handle);
    }

#undef WIN32_RELEASE_POINTER
#undef WIN32_RELEASE_HANDLE
#undef WIN32_FREE

    return;
}

u32 audio_playback_thread(void* param)
{
    OS_Audio_Device* device = (OS_Audio_Device*)param;

    IAudioClient*       audio_client  = device->audio_client;
    IAudioRenderClient* render_client = device->render_client;
    HANDLE              event_handle  = device->event_handle;

    UINT32 frame_count = 0;
    audio_client->lpVtbl->GetBufferSize(audio_client, &frame_count);

    while (!device->quit)
    {
        WaitForSingleObject(event_handle, INFINITE);
        UINT32 frame_padding = 0;
        HRESULT result = audio_client->lpVtbl->GetCurrentPadding(audio_client, &frame_padding);

        if (result < 0)
        {
            fprintf(stderr, "ERROR: GetCurrentPadding failed with code: %ld\n", result);
            break;
        }

        UINT32 frames_available = frame_count - frame_padding;

        BYTE* data = 0;
        result = render_client->lpVtbl->GetBuffer(render_client, frames_available, &data);
        if (result < 0)
        {
            fprintf(stderr, "ERROR: GetBuffer failed with code: %ld\n", result);
            break;
        }

        device->audio_callback(device->user_data, frames_available, data);

        render_client->lpVtbl->ReleaseBuffer(render_client, frames_available, 0);
    }

    return 0;
}

static char* str_from_str16_in_place(WCHAR* wstr);

int wmain(int argc, WCHAR** argv)
{
    for (s32 arg_index = 0; arg_index < argc; ++arg_index)
    {
        str_from_str16_in_place(argv[arg_index]);
    }

    s32 arg_count = argc;
    char** args   = (char**)argv;

    s32 result = main2(arg_count, args);

    return result;
}

char* str_from_str16_in_place(WCHAR* wstr)
{
    char* str = (char*)wstr;

    WCHAR* src  = wstr;
    char*  dest = str;

    while (*src)
    {
        char c = (char)*src++;
        // printf("%c", c);
        *dest++ = c;
    }
    *dest = 0;

    // printf("\n");
    // printf("%s\n", str);

    return str;
}
