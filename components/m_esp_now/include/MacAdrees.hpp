#ifndef MAC_ADDRESS_HPP
#define MAC_ADDRESS_HPP

#include "esp_now.h"
#include <cstring>
#define MAX_SLAVE_NUM 6

class MacAddress {
public:
    uint8_t bytes[ESP_NOW_ETH_ALEN][MAX_SLAVE_NUM];
    size_t count;  // 当前存储的 MAC 地址数量

    MacAddress() : count(0) {
        memset(bytes, 0, ESP_NOW_ETH_ALEN * MAX_SLAVE_NUM);
    }

    // 检查 MAC 地址是否已存在
    bool exists(const uint8_t mac[ESP_NOW_ETH_ALEN]) const {
        for (size_t i = 0; i < count; i++) {
            if (memcmp(bytes[i], mac, ESP_NOW_ETH_ALEN) == 0) {
                return true;
            }
        }
        return false;
    }

    // 插入新的 MAC 地址
    bool insert(const uint8_t mac[ESP_NOW_ETH_ALEN]) {
        if (count >= MAX_SLAVE_NUM) {
            ESP_LOGW(ESP_NOW, "Cannot add more slaves, reached maximum limit of %d", MAX_SLAVE_NUM);
            return false;  // 已达到最大数量
        }
        
        if (exists(mac)) {
            ESP_LOGW(ESP_NOW, "MAC address already exists");
            return false;  // MAC 地址已存在
        }
        
        memcpy(bytes[count], mac, ESP_NOW_ETH_ALEN);
        count++;
        return true;
    }

    // 通过 MAC 地址删除
    bool removeByMac(const uint8_t mac[ESP_NOW_ETH_ALEN]) {
        for (size_t i = 0; i < count; i++) {
            if (memcmp(bytes[i], mac, ESP_NOW_ETH_ALEN) == 0) {
                // 将后面的地址前移
                for (size_t j = i; j < count - 1; j++) {
                    memcpy(bytes[j], bytes[j + 1], ESP_NOW_ETH_ALEN);
                }
                // 清零最后一个位置
                memset(bytes[count - 1], 0, ESP_NOW_ETH_ALEN);
                count--;
                // ESP_LOGI(ESP_NOW, "Removed slave " MACSTR ". Current count: %d", MAC2STR(mac), count);
                return true;
            }
        }
        ESP_LOGW(ESP_NOW, "MAC address not found for removal");
        return false;
    }

    // 清空所有 MAC 地址
    void clear() {
        memset(bytes, 0, ESP_NOW_ETH_ALEN * MAX_SLAVE_NUM);
        count = 0;
    }

    void print() {
        ESP_LOGI(ESP_NOW, "MAC addresses: %d", count);
        for (size_t i = 0; i < count; i++) {
            ESP_LOGI(ESP_NOW, "MAC address %d: " MACSTR, i, MAC2STR(bytes[i]));
        }
    }
    // 重载赋值运算符
    MacAddress& operator=(const MacAddress& other) {
        if (this != &other) {
            // 复制MAC地址数组
            memcpy(bytes, other.bytes, ESP_NOW_ETH_ALEN * MAX_SLAVE_NUM);
            // 复制计数
            count = other.count;
            ESP_LOGI(ESP_NOW, "Copied %d MAC addresses", count);
        }
        return *this;
    }
};

#endif
