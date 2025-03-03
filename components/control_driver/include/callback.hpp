#ifndef CALLBACK_HPP
#define CALLBACK_HPP

#define MAX_CALLBACKS 10

class Callback {
public:
    typedef void (*CallbackFunc)();

    Callback(const char* name) : tag(name), callback_count(0) {
        for(int i = 0; i < MAX_CALLBACKS; i++) {
            callbacks[i] = nullptr;
        }
    }

    // 注册回调
    void registerCallback(CallbackFunc callback) {
        if (!callback) {
            // ESP_LOGE(tag, "Invalid callback");
            return;
        }

        if (callback_count >= MAX_CALLBACKS) {
            // ESP_LOGW(tag, "Callback array is full");
            return;
        }

        // 检查是否已存在
        for (int i = 0; i < callback_count; i++) {
            if (callbacks[i] == callback) {
                // ESP_LOGW(tag, "Callback already registered");
                return;
            }
        }

        callbacks[callback_count++] = callback;
        // ESP_LOGI(tag, "Callback registered successfully");
    }

    // 注销回调
    void unregisterCallback(CallbackFunc callback) {
        if (!callback) {
            // ESP_LOGE(tag, "Invalid callback");
            return;
        }

        for (int i = 0; i < callback_count; i++) {
            if (callbacks[i] == callback) {
                // 移动后面的回调前移
                for (int j = i; j < callback_count - 1; j++) {
                    callbacks[j] = callbacks[j + 1];
                }
                callback_count--;
                callbacks[callback_count] = nullptr;
                // ESP_LOGI(tag, "Callback unregistered successfully");
                return;
            }
        }

        // ESP_LOGW(tag, "Callback not found");
    }

    // 触发所有回调
    void trigger() {
        for (int i = 0; i < callback_count; i++) {
            if (callbacks[i]) {
                callbacks[i]();
            }
        }
    }

private:
    const char* tag;
    CallbackFunc callbacks[MAX_CALLBACKS];
    int callback_count;
};

#endif