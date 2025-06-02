#include "codec.hpp"
#include "esp_heap_caps.h"
#include <string.h>
#include <errno.h>
#include "music.hpp"
#include "espadf_init.hpp"

// 用于任务间通信的标志
static volatile bool should_stop_recording = false;
static volatile bool should_stop_playing = false;
static uint8_t buffer[READ_BLOCK_SIZE];

void play_button_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("button");
}

void play_start_task_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("starttask");


}

void play_finish_task_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("finishtask");
}

// 录音任务函数
static void mic_task_func(void *arg)
{
    audio_pipeline_run(espadf_pipline::Instance()->pipeline_record);

    should_stop_recording = false;

    memset(buffer, 0, READ_BLOCK_SIZE);
    size_t total_read = 0;
    size_t total_write = 0;
    size_t max_bytes = RecordTime * BytesPerSecond;

    // 打开文件fopen(path, "w") 在打开文件时，若文件已经存在，会先将其长度截断为 0，相当于清空了原来所有字节。
    FILE *f = fopen(FILE_PATH, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    ESP_LOGI(TAG, "Mic task started");

    while(!should_stop_recording) 
    {
        size_t bytes_read = raw_stream_read(espadf_pipline::Instance()->raw_stream_reader, (char*)buffer, READ_BLOCK_SIZE);
        size_t bytes_write = fwrite(buffer, 1, bytes_read, f);
        if (bytes_write < bytes_read) 
        {
            int e = errno;  // 注意 FatFS 对 errno 的映射
            ESP_LOGE(TAG, "fwrite 只写入了 %d/%d 字节，errno = %d", 
                    (int)bytes_write, (int)bytes_read, e);
            if (e == ENOSPC) {
                ESP_LOGE(TAG, "FAT 剩余空间不足，无法继续写入。");
            } else if (e == EIO) {
                ESP_LOGE(TAG, "I/O 错误，可能是底层 flash 出现问题。");
            } else {
                ESP_LOGE(TAG, "未知错误，errno = %d", e);
            }
            should_stop_recording = true;
            break;
        }
        total_write += bytes_write;
        total_read += bytes_read;
        
        if (total_read >= max_bytes) {
            should_stop_recording = true;
        }
    }

    audio_element_set_ringbuf_done(espadf_pipline::Instance()->raw_stream_reader);
    audio_element_finish_state(espadf_pipline::Instance()->raw_stream_reader);

    fclose(f);

    while (1) 
    {
        // ESP_LOGI(TAG, "pointer is %p", espadf_pipline::Instance()->i2s_stream_record);
        // ESP_LOGI(TAG, "pointer is %p", espadf_pipline::Instance()->fatfs_stream_writer);
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(espadf_pipline::Instance()->evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        else
        {
            // ESP_LOGI(TAG, "msg.cmd: %d", msg.cmd);
            // ESP_LOGI(TAG, "msg.source: %p", msg.source);
            // ESP_LOGI(TAG, "msg.data: %d", (int)msg.data);
        }

        if (msg.source == (void *) espadf_pipline::Instance()->raw_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) 
        {
            ESP_LOGI(TAG, "mic stop, total recorded: %d bytes ( %.1f seconds)", total_read, (float)total_read / BytesPerSecond);
            break;
        }
    }

    //重置音频pipline
    espadf_pipline::Instance()->reset(espadf_pipline::Instance()->pipeline_record);
    MCodec::Instance()->mic_task = NULL;
    vTaskDelete(NULL);   
}

// 播放任务函数
static void speaker_task_func(void *arg)
{
    //开启音频
    audio_hal_enable_pa(espadf_pipline::Instance()->board_handle->audio_hal, true);
    vTaskDelay(pdMS_TO_TICKS(250));

    audio_pipeline_run(espadf_pipline::Instance()->pipeline_play);

    ESP_LOGI(TAG, "Speaker task started");
    should_stop_playing = false;

    memset(buffer, 0, READ_BLOCK_SIZE);
    size_t total_played = 0;

    // 如果是文件播放，直接 fseek 跳过开头
    if (MCodec::Instance()->speaker_type == mic) {
        // 计算要跳过的字节数
        size_t head_bytes = (size_t)(BytesPerSecond * BeforeRecordTime);
        // 跳过开头的 head_bytes 字节
        if (fseek(MCodec::Instance()->play_file, head_bytes, SEEK_SET) != 0) {
            ESP_LOGE(TAG, "skip file head failed: errno=%d", errno);
        }
    }

    while(!should_stop_playing) 
    {
        size_t bytes_to_play = 0;
        
        if(MCodec::Instance()->speaker_type == music) 
        {
            // 从内存数组读取数据
            size_t remaining = MCodec::Instance()->play_data_size - total_played;
            if(remaining == 0) 
            {
                break; // 数据播放完毕
            }
             // 数据播放完毕
            
            bytes_to_play = (remaining > READ_BLOCK_SIZE) ? READ_BLOCK_SIZE : remaining;
            memcpy(buffer, MCodec::Instance()->play_data + total_played, bytes_to_play);
        }
        else if (MCodec::Instance()->speaker_type == mic)
        {
            // 从文件读取数据
            bytes_to_play = fread(buffer, 1, READ_BLOCK_SIZE, MCodec::Instance()->play_file);
            if (bytes_to_play == 0) break; // 文件读取完毕
        }

        // 播放数据
        raw_stream_write(espadf_pipline::Instance()->raw_stream_writer, (char*)buffer, bytes_to_play);
        total_played += bytes_to_play;
    }

    // 告知raw_stream_writer数据播放完毕
    audio_element_set_ringbuf_done(espadf_pipline::Instance()->raw_stream_writer);
    audio_element_finish_state(espadf_pipline::Instance()->raw_stream_writer);

    while (1) 
    {
        // ESP_LOGI(TAG, "pointer is %p", espadf_pipline::Instance()->raw_stream_writer);
        // ESP_LOGI(TAG, "pointer is %p", espadf_pipline::Instance()->i2s_stream_writer);
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(espadf_pipline::Instance()->evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        else
        {
            // ESP_LOGI(TAG, "msg.cmd: %d", msg.cmd);
            // ESP_LOGI(TAG, "msg.source: %p", msg.source);
            // ESP_LOGI(TAG, "msg.data: %d", (int)msg.data);
        }

        if (msg.source == (void *) espadf_pipline::Instance()->raw_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) 
        {
            break;
        }
    }

    //重置音频pipline
    espadf_pipline::Instance()->reset(espadf_pipline::Instance()->pipeline_play);
    //关闭音频
    audio_hal_enable_pa(espadf_pipline::Instance()->board_handle->audio_hal, false);

    if (MCodec::Instance()->speaker_type == mic)
    {
        fclose(MCodec::Instance()->play_file);
        MCodec::Instance()->play_file = NULL;
    }
    
    ESP_LOGI(TAG, "播放结束，总共播放: %d bytes", total_played);
    MCodec::Instance()->speaker_task = NULL;
    vTaskDelete(NULL);
}

void MCodec::init()
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");

    const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 4,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            .use_one_fat = false,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/fat", "record", &mount_config, &s_wl_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "FAT 文件系统挂载失败");
        return;
    }

    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info("/fat", &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    espadf_pipline::Instance()->init();
}

void MCodec::deinit()
{
    stop_record();
    stop_play();

    // 卸载 SPIFFS 文件系统
    esp_vfs_fat_spiflash_unmount_rw_wl("/fat", s_wl_handle);
}

void MCodec::start_record()
{
    stop_play();
    if (mic_task != NULL)
    {
        // ESP_LOGW(TAG, "mic_task already exists, No need to start recording");
        return;
    }

    ESP_LOGI(TAG, "Starting recording...");
    xTaskCreate(mic_task_func, "mic_task", 4096, NULL, CODEC_PRIORITY, &mic_task);
}

void MCodec::stop_record()
{
    if (mic_task == NULL)
    {
        // ESP_LOGW(TAG, "mic_task is NULL, No need to stop recording");
        return;
    }
    // 设置停止标志，让任务自己结束
    should_stop_recording = true;
    // 等待任务结束
    while (mic_task != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 清楚点击
    size_t half_bytes = BytesPerSecond * ShutdownTime;

    // 获取原文件大小
    struct stat st;
    if (stat(FILE_PATH, &st) != 0)
    {
        ESP_LOGE(TAG, "Failed to stat file for truncation, errno=%d", errno);
        return;
    }
    size_t original_size = st.st_size;

    // 计算截断后大小，防止 underflow
    size_t new_size = (original_size > half_bytes) ? (original_size - half_bytes) : 0;

    // 重新打开文件并截断
    FILE *f = fopen(FILE_PATH, "rb+");
    if (f)
    {
        int fd = fileno(f);
        if (ftruncate(fd, new_size) != 0)
        {
            ESP_LOGE(TAG, "Failed to truncate file: errno=%d", errno);
        }
        else
        {
            ESP_LOGI(TAG, "File truncated from %u to %u bytes", original_size, new_size);
        }
        fclose(f);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to reopen file for truncation");
    }

    ESP_LOGI(TAG, "Recording stopped");
}

void MCodec::play_mic()
{
    speaker_type = mic;
    play_record();
}

void MCodec::play_music(const char *filename)
{
    // 根据文件名获取对应的音频数组
    const uint8_t *audio_data = nullptr;
    size_t audio_size = 0;

    if (strcmp(filename, "bell") == 0)
    {
        audio_data = bell;
        audio_size = bell_size;
    }
    else if (strcmp(filename, "record") == 0)
    {
        audio_data = record20db;
        audio_size = record20db_size;
    }
    else if (strcmp(filename, "button") == 0)
    {
        audio_data = button20db;
        audio_size = button20db_size;
    }
    else if (strcmp(filename, "starttask") == 0)
    {
        audio_data = starttask;
        audio_size = starttask_size;
    }
    else if (strcmp(filename, "finishtask") == 0)
    {
        audio_data = finishtask;
        audio_size = finishtask_size;
    }
    else
    {
        ESP_LOGE(TAG, "未找到对应的音频文件: %s", filename);
        return;
    }
    speaker_type = music;
    play_record(audio_data, audio_size);
}

void MCodec::play_record(const uint8_t *data, size_t size)
{
    if (speaker_task != NULL)
    {
        // ESP_LOGE(TAG, "Speaker task already exists, stopping previous playback");
        return;
    }

    if (mic_task != NULL)
    {
        // ESP_LOGE(TAG, "Mic task running, please stop previous recording");
        return;
    }

    stop_record();

    // 任务结束自动关闭设备
    if (speaker_type == music)
    {
        // 保存播放数据的指针和大小
        play_data = data;
        play_data_size = size;

        ESP_LOGI(TAG, "Creating speaker task for %d bytes (%.1f seconds)",
                size, (float)size / BytesPerSecond);
    }
    else if (speaker_type == mic)
    {
        //只读打开一个二进制文件
        play_file = fopen(FILE_PATH, "rb");
        if (play_file == NULL)
        {
            ESP_LOGE(TAG, "无法打开文件");
            return;
        }
        fseek(play_file, 0, SEEK_END);
        size_t size = ftell(play_file);
        fseek(play_file, 0, SEEK_SET);
        ESP_LOGI(TAG, "Creating speaker task for %d bytes (%.1f seconds)",
                size, (float)size / BytesPerSecond);
    }

    xTaskCreate(speaker_task_func, "speaker_task", 4096, NULL, CODEC_PRIORITY, &speaker_task);
}

void MCodec::stop_play()
{
    if (speaker_task == NULL)
    {
        // ESP_LOGW(TAG, "speaker_task is NULL, No need to stop playback");
        return;
    }
    // 设置停止标志，让任务自己结束
    should_stop_playing = true;
    // 等待任务结束
    while (speaker_task != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "Stop play music");
}
