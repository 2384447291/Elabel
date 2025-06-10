#include "codec_utils.hpp"
#include "codec.hpp"
#include "esp_heap_caps.h"
#include <string.h>
#include <errno.h>
#include "music.hpp"
#include "esp_random.h"

// 用于任务间通信的标志
static volatile bool should_stop_recording = false;
static volatile bool should_stop_playing = false;
uint8_t buffer[READ_BLOCK_SIZE];

void play_button_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("button");
}

void play_charge_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("charge");
}

void play_pikachu_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("pikachu");
}

void play_finish_task_sound()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->play_music("finish");
}

void resize_file()
{
    // 清除点击
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
            // ESP_LOGI(TAG, "File truncated from %u to %u bytes", original_size, new_size);
        }
        fclose(f);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to reopen file for truncation");
    }

}

//限制幅度0---20
void amplify_db(float db_gain) 
{
    if(db_gain < 0) db_gain = 0;
    if(db_gain > 20) db_gain = 20;
    FILE *f = fopen(FILE_PATH, "rb+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for amplification");
        return;
    }

    // 计算放大倍数
    float gain = powf(10.0f, db_gain / 20.0f);

    // 分配一个 uint8_t 缓冲区，用于分块读取
    uint8_t buffer[READ_BLOCK_SIZE];
    size_t bytes_read;

    // 从文件开头开始
    fseek(f, 0, SEEK_SET);

    while ((bytes_read = fread(buffer, 1, READ_BLOCK_SIZE, f)) > 0) {
        // 读取了 bytes_read 字节，按 16-bit 样本处理
        // 一定要确保 bytes_read 是偶数（READ_BLOCK_SIZE 本身是偶数，最后一块若不足则小于它，但文件本身应保证总字节数是偶数）
        size_t sample_count = bytes_read / sizeof(int16_t);
        int16_t *samples = (int16_t *)buffer;

        for (size_t i = 0; i < sample_count; ++i) {
            float amplified = samples[i] * gain;
            // 限幅到 int16_t 范围
            if (amplified > 32767.0f) {
                amplified = 32767.0f;
            } else if (amplified < -32768.0f) {
                amplified = -32768.0f;
            }
            samples[i] = (int16_t)amplified;
        }

        // 写回：先把文件指针移回这一块起始位置
        fseek(f, -((long)bytes_read), SEEK_CUR);
        fwrite(buffer, 1, bytes_read, f);

        // 写完后，文件指针已经移到这一块末尾，继续循环即可
    }

    ESP_LOGI(TAG, "Amplification done with gain = %.2f dB", db_gain);
    fclose(f);
}

// 录音任务函数
static void mic_task_func(void *arg)
{
    MCodec *codec = MCodec::Instance();
    size_t total_bytes_write = 0;
    size_t max_bytes = RecordTime * BytesPerSecond;

    // 打开文件
    FILE *f = fopen(FILE_PATH, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    MCodec::Instance()->open_mic_dev(MIC_SAMPLE_RATE);

    // 重置录音大小
    should_stop_recording = false;

    while (!should_stop_recording)
    {
        memset(buffer, 0, READ_BLOCK_SIZE);
        // 使用esp_codec_dev_read直接从codec读取数据小丑这个读成功了就是返回0，不反悔大小
        esp_codec_dev_read(codec->codec_dev, buffer, READ_BLOCK_SIZE);
        // 复制数据到录音缓冲区
        fwrite(buffer, 1, READ_BLOCK_SIZE, f);
        total_bytes_write += READ_BLOCK_SIZE;
        // 检查是否达到缓冲区限制
        if (total_bytes_write  >= max_bytes)
        {
            should_stop_recording = true;
        }
    }
    fclose(f);

    // 放大15db
    amplify_db(15);
    // 关闭设备
    codec->close_dev();
    // 清除任务句柄
    codec->mic_task = NULL;
    // 打印录音信息
    ESP_LOGI(TAG, "Mic task ended, total recorded: %d bytes ( %.1f seconds)", total_bytes_write, (float)total_bytes_write / BytesPerSecond);
    // 生成一个随机数作为录音的唯一标识
    codec->record_message_unique_id = esp_random();
    vTaskDelete(NULL);
}

// 播放任务函数
static void speaker_task_func(void *arg)
{
    MCodec *codec = MCodec::Instance();
    should_stop_playing = false;

    size_t total_played = 0;

    // 如果是文件播放，直接 fseek 跳过开头
    if (codec->speaker_type == mic) {
        // 计算要跳过的字节数
        size_t head_bytes = (size_t)(BytesPerSecond * BeforeRecordTime);
        // 跳过开头的 head_bytes 字节
        if (fseek(codec->play_file, head_bytes, SEEK_SET) != 0) {
            ESP_LOGE(TAG, "skip file head failed: errno=%d", errno);
        }
    }

    while (!should_stop_playing)
    {
        size_t bytes_to_play = 0;
        memset(buffer, 0, READ_BLOCK_SIZE);
        // 根据播放模式获取要播放的数据
        if (codec->speaker_type == mic)
        {
            // 从文件读取数据
            bytes_to_play = fread(buffer, 1, READ_BLOCK_SIZE, codec->play_file);
            if (bytes_to_play == 0)
                break; // 文件读取完毕
        }
        else if (codec->speaker_type == music)
        {
            // 从内存数组读取数据
            size_t remaining = codec->play_data_size - total_played;
            if (remaining == 0)
                break; // 数据播放完毕

            bytes_to_play = (remaining > READ_BLOCK_SIZE) ? READ_BLOCK_SIZE : remaining;
            memcpy(buffer, codec->play_data + total_played, bytes_to_play);
        }

        // 播放数据
        esp_err_t ret = esp_codec_dev_write(codec->codec_dev, buffer, bytes_to_play);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "播放失败: %d", ret);
            break;
        }

        total_played += bytes_to_play;
    }

    // 清理工作
    if (codec->speaker_type == mic)
    {
        fclose(codec->play_file);
        codec->play_file = NULL;
    }
    // 关闭设备
    codec->close_dev();
    ESP_LOGI(TAG, "播放结束，总共播放: %d bytes", total_played);
    codec->speaker_task = NULL;
    vTaskDelete(NULL);
}

void MCodec::init()
{
    // 先调用esp_codec_init()创建codec设备
    esp_codec_init(codec_dev);
    // 确保codec_dev已经创建成功
    assert(codec_dev);
    // 关闭时反使能
    esp_codec_set_disable_when_closed(codec_dev, true);
    ESP_LOGI(TAG, "Codec initialized");

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
}

void MCodec::deinit()
{
    stop_record();
    stop_play();

    // 卸载 SPIFFS 文件系统
    esp_vfs_fat_spiflash_unmount_rw_wl("/fat", s_wl_handle);

    esp_codec_dev_close(codec_dev);
    esp_codec_deinit(codec_dev);
}

void MCodec::set_volume(uint8_t volume)
{
    if (codec_dev)
    {
        MCodec::Instance()->codec_vol = volume;
        ESP_LOGI(TAG, "Volume set to %d", volume);
    }
}

void MCodec::set_mic_gain(float gain)
{
    if (codec_dev)
    {
        MCodec::Instance()->codec_gain = gain;
        ESP_LOGI(TAG, "Mic gain set to %.1f", gain);
    }
}

void MCodec::start_record()
{
    if (mic_task != NULL)
    {
        ESP_LOGW(TAG, "mic_task already exists, No need to start recording");
        return;
    }

    ESP_LOGI(TAG, "Starting recording...");
    xTaskCreate(mic_task_func, "mic_task", 4096, NULL, 5, &mic_task);
}

void MCodec::stop_record()
{
    if (mic_task == NULL)
    {
        ESP_LOGW(TAG, "mic_task is NULL, No need to stop recording");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(DuringTime * 1000));
    // 设置停止标志，让任务自己结束
    should_stop_recording = true;
    // 等待任务结束
    while (mic_task != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    resize_file();
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
    else if (strcmp(filename, "charge") == 0)
    {
        audio_data = charge;
        audio_size = charge_size;
    }
    else if (strcmp(filename, "button") == 0)
    {
        audio_data = button;
        audio_size = button_size;
    }
    else if (strcmp(filename, "finish") == 0)
    {
        audio_data = finish;
        audio_size = finish_size;
    }
    else if (strcmp(filename, "pikachu") == 0)
    {
        audio_data = pikachu;
        audio_size = pikachu_size;
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
        ESP_LOGE(TAG, "Speaker task already exists, stopping previous playback");
        return;
    }

    if (mic_task != NULL)
    {
        ESP_LOGE(TAG, "Mic task running, please stop previous recording");
        return;
    }

    // 任务结束自动关闭设备
    if (speaker_type == music)
    {
        // 保存播放数据的指针和大小
        play_data = data;
        play_data_size = size;

        ESP_LOGI(TAG, "Creating speaker task for %d bytes (%.1f seconds)",
                size, (float)size / BytesPerSecond);
        open_speaker_dev(SPEAKER_SAMPLE_RATE);
    }
    else if (speaker_type == mic)
    {
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
        open_speaker_dev(MIC_SAMPLE_RATE);
    }

    xTaskCreate(speaker_task_func, "speaker_task", 4096, NULL, 5, &speaker_task);
}

void MCodec::stop_play()
{
    if (speaker_task == NULL)
    {
        ESP_LOGW(TAG, "speaker_task is NULL, No need to stop playback");
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
