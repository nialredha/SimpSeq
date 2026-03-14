//
// References
//   https://medium.com/@shahidahmadkhan86/sound-in-windows-the-wasapi-in-c-23024cdac7c6 
//   https://www.reddit.com/r/C_Programming/comments/1gv80uq/cannot_solve_errors_for_unresolved_external/
//   https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
//

#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "os.h"
#include "wav.h"
#include "wav_dump_utils.h"

#include "base_arena.c"
#include "base_string.c"
#include "os.c"
#include "wav.c"
#include "wav_dump_utils.c"

#include <stdio.h>

#include <windows.h>
#include <audioclient.h> // WASAPI interfaces
#include <mmdeviceapi.h> // enumerate/activate audio endpoints

#define REF_TIME_NANOSECONDS_PER_UNIT   (100.0)
#define REF_TIME_UNITS_PER_NANOSECOND   ( 1.0 / REF_TIME_NANOSECONDS_PER_UNIT)
#define REF_TIME_NANOSECONDS_PER_SECOND (1e9)
#define REF_TIME_UNITS_PER_SECOND       (REF_TIME_NANOSECONDS_PER_SECOND * REF_TIME_UNITS_PER_NANOSECOND)

typedef void (* Audio_Callback)(void* user_data, u32 num_frames_needed, void* output);

typedef struct
{
    Wav_Format     desired_format;
    Audio_Callback audio_callback;
    void*          user_data;
} Audio_Config;

char* str_from_get_default_audio_endpoint_result(HRESULT result);
char* str_from_str16_in_place(WCHAR* wstr);

// globals

static IMMDeviceEnumerator* enumerator = 0;
static IMMDevice*           device     = 0;

static IAudioClient* audio_client = 0;
static WAVEFORMATEX* audio_format = 0;

static IAudioRenderClient* render_client = 0;

static HANDLE event_handle;

static Audio_Config audio_config;

s32 os_win32_audio_init(Audio_Config* config)
{
    HRESULT result = -1;

    f32 duration = 1.0; // TODO[nr] @temp

    // initialize the COM library
    result = CoInitializeEx(0, COINIT_MULTITHREADED);

    // create enumerator object on the local system
    if (result >= 0)
    {
        result = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    }

    // get default device for renderering audio
    if (result >= 0 && enumerator != 0)
    {
        // TODO[nr] @better: only supporting default
        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        enumerator->Release(); // TODO[nr] @study: should we release this or hold onto it?
    }
    
    // get audio client
    if (result >= 0 && device)
    {
        result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&audio_client);
    }

    // initialize audio_client and get audio render client service
    if (result >= 0 && audio_client)
    {
        result = audio_client->GetMixFormat(&audio_format);

        if (result >= 0)
        {
            String fmt_tag = wav_string_from_format_tag(audio_format->wFormatTag);

            printf("\n");
            printf("Mix format:\n");
            printf("======================================\n");
            printf("  Format Tag          : %.*s\n", fmt_tag.count, fmt_tag.data);
            printf("  Channels            : %d\n",   audio_format->nChannels);
            printf("  Samples Per Second  : %d\n",   audio_format->nSamplesPerSec);
            printf("  Bytes Per Second    : %d\n",   audio_format->nAvgBytesPerSec);
            printf("  Block Align         : %d\n",   audio_format->nBlockAlign);
            printf("  Bits Per Sample     : %d\n",   audio_format->wBitsPerSample);
            printf("  CB Size             : %d\n",   audio_format->cbSize);

            if (audio_format->wFormatTag == Wav_Format_Tag_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* audio_format_ext = (WAVEFORMATEXTENSIBLE*)audio_format;
                String sub_fmt_tag = wav_string_from_format_tag(audio_format_ext->SubFormat.Data1 & 0xFFFF);

                printf("  Valid Bits Per Samp : %d\n",     audio_format_ext->Samples.wValidBitsPerSample);
                printf("  Channel Mask        : 0x%02X\n", audio_format_ext->dwChannelMask);
                printf("  Sub Format Tag      : %.*s\n",   sub_fmt_tag.count, sub_fmt_tag.data);
            }
            printf("\n");

            REFERENCE_TIME buffer_duration = (REFERENCE_TIME)(duration * REF_TIME_UNITS_PER_SECOND);
            printf("Requested Buffer Duration : %f * %f = %llu\n", duration, REF_TIME_UNITS_PER_SECOND, buffer_duration);

            if (audio_format->wFormatTag == Wav_Format_Tag_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* audio_format_ext = (WAVEFORMATEXTENSIBLE*)audio_format;

                // TODO[nr] @study: leave channel mask the same?
                audio_format_ext->Samples.wValidBitsPerSample = config->desired_format.bits_per_sample;
                audio_format_ext->SubFormat.Data1 = (audio_format_ext->SubFormat.Data1 & (0xFFFF0000)) | (config->desired_format.format_tag & (0x0000FFFF));
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
            result = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, buffer_duration, 0, audio_format, 0);
        }
    }

    // set event handle
    if (result >= 0)
    {
        event_handle = CreateEvent(0, 0, 0, 0);
        result = audio_client->SetEventHandle(event_handle);
    }

    // get render client
    if (result >= 0)
    {
        result = audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&render_client);
    }

    // save callback and user data
    if (result >= 0)
    {
        audio_config = *config;
    }

    return (s32)result;
}

s32 os_win32_audio_start(void)
{
    HRESULT result = -1;

    if (audio_client != 0)
    {
        result = audio_client->Start();
    }

    return (s32)result;
}

s32 os_win32_audio_stop(void)
{
    HRESULT result = -1;
    
    if (audio_client != 0)
    {
        result = audio_client->Stop();
    }

    if (result >= 0)
    {
        result = audio_client->Reset();
    }

    return (s32)result;
}

void os_win32_audio_deinit(void)
{
#define WIN32_RELEASE_POINTER(p) do { if (p != 0) { (p)->Release(); (p) = 0; } } while (0)
#define WIN32_RELEASE_HANDLE(h)  do { if (h != 0) { CloseHandle(h); (h) = 0; } } while (0)
#define WIN32_FREE(p)            do { if (p != 0) { CoTaskMemFree(p); } } while (0)
    
    WIN32_RELEASE_POINTER(enumerator);
    WIN32_RELEASE_POINTER(audio_client);
    WIN32_RELEASE_POINTER(render_client);

    WIN32_FREE(audio_format);

    WIN32_RELEASE_HANDLE(event_handle);

#undef WIN32_RELEASE_POINTER
#undef WIN32_RELEASE_HANDLE
#undef WIN32_FREE

    return;
}

typedef struct
{
    Wav_Format format;
    u32        playhead;
    u32        size;
    u8*        data;
} User_Data;

void audio_callback(void* user_data, u32 num_frames_needed, void* output)
{
    User_Data* ud   = (User_Data*)user_data;

    // f32* src  = (f32*)&ud->data[ud->playhead];
    // f32* dest = (f32*)output;

    u8* src  = (u8*)&ud->data[ud->playhead];
    u8* dest = (u8*)output;

    u32 num_bytes_needed = (num_frames_needed * ud->format.num_channels) * (ud->format.bits_per_sample / 8);

    for (u32 byte_index = 0; byte_index < num_bytes_needed; ++byte_index)
    // for (u32 sample_playhead = 0; sample_playhead < num_frames_needed * ud->format.num_channels; sample_playhead += 1)
    {
        if (ud->playhead + byte_index < ud->size) 
        {
            *dest++ = *src++;
            ud->playhead += sizeof(*dest);
        }
        else
        {
            *dest++ = 0;
        }
    }
}

int wmain(int argc, WCHAR** argv)
{
    if (argc != 2)
    {
        printf("%d is not enough arguments!\n Example usage: .\\%s test.wav\n", argc, str_from_str16_in_place(argv[0]));
        return 1;
    }

    // WASAPI globals
    UINT32 frame_count = 0;

    // WAV globals
    Arena  wav_arena = {0};
    arena_alloc(&wav_arena, 4096);

    char* filename_cstr8 = str_from_str16_in_place(argv[1]);
    String wav_filename = STR_C(filename_cstr8);
    String wav_file     = os_read_entire_file(wav_filename);

    Wav wav = wav_from_data(&wav_arena, wav_file);

    Wav_Sub_Chunk_Node wav_fmt_node  = wav_sub_chunk_from_id(&wav.sub_chunks, WAV_FOURCC(WAV_FORMAT_CHUNK_ID));
    Wav_Sub_Chunk_Node wav_data_node = wav_sub_chunk_from_id(&wav.sub_chunks, WAV_FOURCC(WAV_DATA_CHUNK_ID));

    Wav_Format wav_fmt  = *(Wav_Format*)&wav_file.data[wav_fmt_node.data_offset];

    String_Builder strb = {0};
    arena_alloc(&strb.arena, 1024);

    wav_dump_format(&wav_arena, &wav_fmt, 1, &strb);
    printf("Input File %s:\n%.*s\n", wav_filename.data, strb.str.count, strb.str.data);

    u32 bytes_per_sample = wav_fmt.bits_per_sample / 8;
    u32 num_samples      = wav_data_node.header.size / bytes_per_sample;
    u32 num_frames       = num_samples / wav_fmt.num_channels;

    BYTE* data = 0;

    User_Data ud = { wav_fmt, 0, wav_data_node.header.size, (u8*)&wav_file.data[wav_data_node.data_offset] };
    Audio_Config config = { wav_fmt, audio_callback, &ud };

    HRESULT result = os_win32_audio_init(&config);

    if (result >= 0)
    {
        result = os_win32_audio_start();
    }

    if (result >= 0)
    {
        audio_client->GetBufferSize(&frame_count);

        while (true)
        {
            WaitForSingleObject(event_handle, INFINITE);

            UINT32 frame_padding = 0;
            result = audio_client->GetCurrentPadding(&frame_padding);

            if (result < 0)
            {
                fprintf(stderr, "ERROR: GetCurrentPadding failed with code: %d\n", result);
                break;
            }

            UINT32 frames_available = frame_count - frame_padding;

            data = 0;
            result = render_client->GetBuffer(frames_available, &data);
            if (result < 0)
            {
                fprintf(stderr, "ERROR: GetBuffer failed with code: %d\n", result);
                break;
            }

            audio_config.audio_callback(audio_config.user_data, frames_available, data);

            render_client->ReleaseBuffer(frames_available, 0);
        }
    }

    // wait until audio is rendered
    if (result >= 0)
    {
        UINT32 current_padding = 0;
        do 
        {
            result = audio_client->GetCurrentPadding(&current_padding);
            if (result < 0)
            { 
                fprintf(stderr, "ERROR: GetBuffer failed with code: %d\n", result);
                break;
            }

            // printf("Get Current Padding Result: %d, Value: %u\n", result, current_padding);
        }
        while (current_padding);
    }

    // stop playing device
    result = os_win32_audio_stop();

    os_win32_audio_deinit();

    return result;
}

char* str_from_get_default_audio_endpoint_result(HRESULT result)
{
    switch (result)
    {
        case S_OK:
        {
            return "SUCCESS";
        }
        case E_POINTER:
        {
            return "E_POINTER";
        }
        case E_INVALIDARG:
        {
            return "E_INVALIDARG";
        }
        case E_NOTFOUND:
        {
            return "E_NOTFOUND";
        }
        case E_OUTOFMEMORY:
        {
            return "E_OUTOFMEMORY";
        }
        default:
        {
            return "UNKNOWN";
        }
    }
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
