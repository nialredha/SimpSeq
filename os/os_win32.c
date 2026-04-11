//
// General OS API
//

OS_File_Properties os_get_file_properties(String filepath)
{
    OS_File_Properties result = {0};

    WIN32_FIND_DATAA find_data = {0};
    HANDLE file_handle = FindFirstFileA(filepath.data, &find_data);

    if(file_handle != INVALID_HANDLE_VALUE)
    {
        assert(find_data.nFileSizeHigh == 0);
        result.size = (u32)find_data.nFileSizeLow;

        result.time_created  = ((u64)find_data.ftCreationTime.dwHighDateTime << 32)  | 
                               (find_data.ftCreationTime.dwLowDateTime);

        result.time_modified = ((u64)find_data.ftLastWriteTime.dwHighDateTime << 32) | 
                               (find_data.ftLastWriteTime.dwLowDateTime);
    }

    FindClose(file_handle);

    return result;
}

String os_read_entire_file(String filepath)
{
    String result = {0};

    HANDLE file_handle = CreateFileA(filepath.data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file_handle, &file_size))
        {
            result.count = (u32)(file_size.QuadPart);

            result.data = (char*)VirtualAlloc(0, result.count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            if (result.data)
            {
                DWORD bytes_read;

                if (!ReadFile(file_handle, result.data, result.count, &bytes_read, 0) || 
                    result.count != bytes_read)
                {
                    // Something went wrong
                    VirtualFree(result.data, 0, MEM_RELEASE);

                    result.data = 0;
                    result.count = 0;
                }
            }
        }

        CloseHandle(file_handle);
    }

    return result;
}

bool os_write_entire_file(String filename, String contents)
{
    bool result = true;

    // TODO[nr]: CREATE_ALWAYS instead of CREATE_NEW?
    HANDLE file_handle = CreateFileA(filename.data, GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written;

        if (!WriteFile(file_handle, contents.data, contents.count, &bytes_written, 0) || 
            contents.count != bytes_written)
        {
            result = false;
        }

        CloseHandle(file_handle);
    }
    else
    {
        result = false;
    }

    return result;
}

void os_free_file_contents(String* contents)
{
    if (contents != 0)
    {
        if (contents->data != 0)
        {
            VirtualFree(contents->data, 0, MEM_RELEASE);
            contents->data  = 0;
            contents->count = 0;
        }
    }

    return;
}

u64 os_now_us(void)
{
    LARGE_INTEGER pc_count = os_win32_get_performance_counter_count();
    LARGE_INTEGER pc_freq  = os_win32_get_performance_counter_frequency();

    f32 microseconds_per_second = 1000000;
    u64 result = (u64)(((f32)pc_count.QuadPart / (f32)pc_freq.QuadPart) * microseconds_per_second);
    return result;
}

void os_sleep_ms(u32 milliseconds)
{
    Sleep(milliseconds);
    return;
}

//
// DLL
//

static bool os_win32_reload_dll(OS_Win32_DLL* dll)
{
    bool result = false;

    // TODO[nr]: prepend the dll name to this
    char* dll_temp_name = "temp.dll";

    if (dll->handle != 0)
    {
        os_win32_unload_dll(dll);
    }

    dll->last_write_time = os_win32_get_last_write_time_of_file(dll->filename);
    result = CopyFileA(dll->filename.data, dll_temp_name, FALSE);
    dll->handle = LoadLibraryA(dll_temp_name);

    return result;
}

static void* os_win32_get_function_pointer(OS_Win32_DLL* dll, String function_name)
{
    void* result = 0;

    if (dll->handle != 0)
    {
        result = (void*)GetProcAddress(dll->handle, function_name.data);
    }

    return result;
}

static void os_win32_unload_dll(OS_Win32_DLL* dll)
{
    if (dll->handle != 0)
    {
        FreeLibrary(dll->handle);    
        dll->handle = 0;
    }

    return;
}

//
// TIME
//

FILETIME os_win32_get_last_write_time_of_file(String filename)
{
    FILETIME last_write_time = {0};

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFileA(filename.data, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE)
    {
        last_write_time = find_data.ftLastWriteTime;
        FindClose(find_handle);
    }

    return last_write_time;
}

f32 os_win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end, s64 perf_counter_freq)
{
    f32 result = (f32)(end.QuadPart - start.QuadPart) / (f32)perf_counter_freq;
    return result;
}

LARGE_INTEGER os_win32_get_performance_counter_count(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

LARGE_INTEGER os_win32_get_performance_counter_frequency(void)
{
    LARGE_INTEGER result;
    QueryPerformanceFrequency(&result);
    return result;
}

//
// WASAPI
//

#define DEFAULT_BUFFER_SIZE_IN_SECONDS (0.1f)

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

            REFERENCE_TIME buffer_duration = (REFERENCE_TIME)(DEFAULT_BUFFER_SIZE_IN_SECONDS * REF_TIME_UNITS_PER_SECOND);
            printf("Requested Buffer Duration : %f * %f = %llu\n", DEFAULT_BUFFER_SIZE_IN_SECONDS, REF_TIME_UNITS_PER_SECOND, buffer_duration);

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

        device->audio_callback((Wav_Format*)device->audio_format, device->user_data, frames_available, data);

        render_client->lpVtbl->ReleaseBuffer(render_client, frames_available, 0);
    }

    return 0;
}

#ifndef OS_WIN32_DLL
static char* str_from_str16_in_place(WCHAR* wstr);

int wmain(int argc, WCHAR** argv)
{
    for (s32 arg_index = 0; arg_index < argc; ++arg_index)
    {
        str_from_str16_in_place(argv[arg_index]);
    }

    s32 arg_count = argc;
    char** args   = (char**)argv;

    s32 result = entry_point(arg_count, args);

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

#endif
