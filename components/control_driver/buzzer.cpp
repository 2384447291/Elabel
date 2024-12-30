#include "buzzer.hpp"

#define TAG "BUZZER"
// 开机音乐音符数组
static const Note UPON_MUSIC[] = {
    {Buzzer::NOTE_C5, 300}, {Buzzer::NOTE_D5, 300}, {Buzzer::NOTE_G5, 300},  
};
static const size_t UPON_MUSIC_LENGTH = sizeof(UPON_MUSIC) / sizeof(UPON_MUSIC[0]);

// 关机音乐音符数组
static const Note DOWN_MUSIC[] = {
    {Buzzer::NOTE_G5, 300}, {Buzzer::NOTE_D5, 300}, {Buzzer::NOTE_C5, 300},  
};
static const size_t DOWN_MUSIC_LENGTH = sizeof(DOWN_MUSIC) / sizeof(DOWN_MUSIC[0]);

// 三次蜂鸣音符数组（每次响0.5s，间隔0.2s）
static const Note TRIPLE_BEEP_NOTES[] = {
    {Buzzer::NOTE_E5, 500},  // 第一声
    {Buzzer::NOTE_C5, 200},  // 间隔
    {Buzzer::NOTE_E5, 500},  // 第二声
    {Buzzer::NOTE_C5, 200},  // 间隔
    {Buzzer::NOTE_E5, 500},  // 第三声
};
static const size_t TRIPLE_BEEP_LENGTH = sizeof(TRIPLE_BEEP_NOTES) / sizeof(TRIPLE_BEEP_NOTES[0]);

// 双次蜂鸣音符数组（每次响0.5s，间隔0.2s）
static const Note DOUBLE_BEEP_NOTES[] = {
    {Buzzer::NOTE_E5, 500},  // 第一声
    {Buzzer::NOTE_C5, 200},  // 间隔
    {Buzzer::NOTE_E5, 500},  // 第二声
};
static const size_t DOUBLE_BEEP_LENGTH = sizeof(DOUBLE_BEEP_NOTES) / sizeof(DOUBLE_BEEP_NOTES[0]);

// 长蜂鸣音符数组（1s）
static const Note LONG_BEEP_NOTES[] = {
    {Buzzer::NOTE_E5, 1000}
};
static const size_t LONG_BEEP_LENGTH = sizeof(LONG_BEEP_NOTES) / sizeof(LONG_BEEP_NOTES[0]);


// 单次超长蜂鸣音符数组（2s）
static const Note SUPER_LONG_BEEP_NOTES[] = {
    {Buzzer::NOTE_E5, 2000}
};
static const size_t SUPER_LONG_BEEP_LENGTH = sizeof(SUPER_LONG_BEEP_NOTES) / sizeof(SUPER_LONG_BEEP_NOTES[0]);

Buzzer::Buzzer() : busy(false), startTime(0), currentDuration(0), 
                   isPlayingMusic(false), currentNoteIndex(0),
                   currentMusicNotes(nullptr), currentMusicLength(0) {}

void Buzzer::init() {
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 2700,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {};  // 先全部初始化为0
    ledc_channel.gpio_num = BUZZER_GPIO;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.timer_sel = LEDC_TIMER_0;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ledc_channel.flags.output_invert = 0;
    
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", BUZZER_GPIO);
}
        
void Buzzer::start(uint32_t frequency) {
    if (frequency == 0) {
        stop();
        return;
    }
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, PWM_DUTY);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, frequency);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    busy = true;
    ESP_LOGD(TAG, "Started buzzer at %d Hz", frequency);
}

void Buzzer::stop() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    busy = false;
    isPlayingMusic = false;
    currentMusicNotes = nullptr;
    currentMusicLength = 0;
    ESP_LOGD(TAG, "Stopped buzzer");
}

void Buzzer::playNote(uint32_t frequency, uint32_t duration_ms) {
    start(frequency);
    startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    currentDuration = duration_ms;
    ESP_LOGD(TAG, "Playing note at %d Hz for %d ms", frequency, duration_ms);
}

void Buzzer::playMusic(const Note* notes, size_t length) {
    if (!isPlayingMusic && notes != nullptr && length > 0) {
        isPlayingMusic = true;
        currentNoteIndex = 0;
        currentMusicNotes = notes;
        currentMusicLength = length;
        playNextNote();
        ESP_LOGI(TAG, "Started playing music with %d notes", length);
    }
}

void Buzzer::playUponMusic() {
    playMusic(UPON_MUSIC, UPON_MUSIC_LENGTH);
}

void Buzzer::playDownMusic() {
    playMusic(DOWN_MUSIC, DOWN_MUSIC_LENGTH);
}

void Buzzer::playNextNote() {
    if (currentMusicNotes == nullptr) {
        stop();
        return;
    }

    if (currentNoteIndex < currentMusicLength) {
        playNote(currentMusicNotes[currentNoteIndex].freq, 
                currentMusicNotes[currentNoteIndex].duration);
        currentNoteIndex++;
    } else {
        stop();
    }
}

void Buzzer::handle() {
    if (!busy) return;

    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (currentTime - startTime >= currentDuration) {
        if (isPlayingMusic) {
            playNextNote();
        } else {
            stop();
        }
    }
}

void Buzzer::playSuperLongBeep() {
    playMusic(SUPER_LONG_BEEP_NOTES, SUPER_LONG_BEEP_LENGTH);
}

void Buzzer::playTripleBeep() {
    playMusic(TRIPLE_BEEP_NOTES, TRIPLE_BEEP_LENGTH);
}

void Buzzer::playLongBeep() {
    playMusic(LONG_BEEP_NOTES, LONG_BEEP_LENGTH);
} 

void Buzzer::playDoubleBeep() {
    playMusic(DOUBLE_BEEP_NOTES, DOUBLE_BEEP_LENGTH);
}
