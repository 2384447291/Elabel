#ifndef BUZZER_HPP
#define BUZZER_HPP

#define BUZZER_GPIO GPIO_NUM_1

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 音符结构体定义
struct Note {
    uint32_t freq;
    uint32_t duration;
};

class Buzzer {
public:
    Buzzer();
    void init();
    void start(uint32_t frequency);
    void stop();
    void playNote(uint32_t frequency, uint32_t duration_ms);
    bool isBusy() const { return busy; }
    void handle();  // 处理定时任务

    // 通用音乐播放接口
    void playMusic(const Note* notes, size_t length);
    void playUponMusic();  // 播放开机音乐
    void playDownMusic();  // 播放关机音乐
    void playChargingMusic(); // 播放充电音乐
    void playTripleBeep(); // 连续响3次，间隔0.2s
    void playLongBeep();  // 响1s
    void playDoubleBeep(); // 连续响2次，间隔0.2s
    void playSuperLongBeep(); // 响2s

    // 音符频率定义
    static constexpr uint32_t NOTE_C3 = 131;  // 低音do
    static constexpr uint32_t NOTE_D3 = 147;  // 低音re
    static constexpr uint32_t NOTE_E3 = 165;  // 低音mi
    static constexpr uint32_t NOTE_F3 = 175;  // 低音fa
    static constexpr uint32_t NOTE_G3 = 196;  // 低音sol
    static constexpr uint32_t NOTE_A3 = 220;  // 低音la
    static constexpr uint32_t NOTE_B3 = 247;  // 低音si
    static constexpr uint32_t NOTE_C4 = 262;  // 中音do
    static constexpr uint32_t NOTE_D4 = 294;  // 中音re
    static constexpr uint32_t NOTE_E4 = 330;  // 中音mi
    static constexpr uint32_t NOTE_F4 = 349;  // 中音fa
    static constexpr uint32_t NOTE_G4 = 392;  // 中音sol
    static constexpr uint32_t NOTE_A4 = 440;  // 中音la
    static constexpr uint32_t NOTE_B4 = 494;  // 中音si
    static constexpr uint32_t NOTE_C5 = 523;  // 高音do
    static constexpr uint32_t NOTE_D5 = 587;  // 高音re
    static constexpr uint32_t NOTE_E5 = 659;  // 高音mi
    static constexpr uint32_t NOTE_F5 = 698;  // 高音fa
    static constexpr uint32_t NOTE_G5 = 784;  // 高音sol
    static constexpr uint32_t NOTE_A5 = 880;  // 高音la
    static constexpr uint32_t NOTE_B5 = 988;  // 高音si

private:
    bool busy;
    uint32_t startTime;
    uint32_t currentDuration;
    
    // 音乐播放控制
    bool isPlayingMusic;
    size_t currentNoteIndex;
    const Note* currentMusicNotes;  // 当前播放的音符数组
    size_t currentMusicLength;      // 当前音符数组长度
    void playNextNote();

    static constexpr uint32_t PWM_DUTY = 700;  // PWM占空比
};

#endif // BUZZER_HPP