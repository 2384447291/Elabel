// #ifndef MAC_ADDRESS_HPP
// #define MAC_ADDRESS_HPP

// #include "esp_now.h"
// #include "esp_log.h"
// #include <cstring>
// #include "esp_now_client.hpp"
// #define MAX_SLAVE_NUM 6
// struct slave_info{
//     uint8_t mac[ESP_NOW_ETH_ALEN];
//     bool is_sleep;
// };

// class MacAddress {
// public:
//     slave_info slaves[MAX_SLAVE_NUM];
//     size_t count;  // 当前存储的 MAC 地址数量

//     MacAddress() : count(0) {
//         memset(slaves, 0, sizeof(slaves));
//     }

//     // 检查 MAC 地址是否已存在
//     bool exists(const uint8_t mac[ESP_NOW_ETH_ALEN]) const {
//         for (size_t i = 0; i < count; i++) {
//             if (memcmp(slaves[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
//                 return true;
//             }
//         }
//         return false;
//     }

//     // 插入新的 MAC 地址
//     bool insert(const uint8_t mac[ESP_NOW_ETH_ALEN]) {
//         if (count >= MAX_SLAVE_NUM) {
//             ESP_LOGW(ESP_NOW, "Cannot add more slaves, reached maximum limit of %d", MAX_SLAVE_NUM);
//             return false;  // 已达到最大数量
//         }
        
//         if (exists(mac)) {
//             ESP_LOGW(ESP_NOW, "MAC address already exists");
//             return false;  // MAC 地址已存在
//         }
        
//         memcpy(slaves[count].mac, mac, ESP_NOW_ETH_ALEN);
//         slaves[count].is_sleep = false;
//         count++;
//         return true;
//     }

//     // 通过 MAC 地址删除
//     bool removeByMac(const uint8_t mac[ESP_NOW_ETH_ALEN]) {
//         for (size_t i = 0; i < count; i++) {
//             if (memcmp(slaves[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
//                 // 将后面的地址前移
//                 for (size_t j = i; j < count - 1; j++) {
//                     memcpy(slaves[j].mac, slaves[j + 1].mac, ESP_NOW_ETH_ALEN);
//                     slaves[j].is_sleep = slaves[j + 1].is_sleep;
//                 }
//                 // 清零最后一个位置
//                 memset(slaves[count - 1].mac, 0, ESP_NOW_ETH_ALEN);
//                 slaves[count - 1].is_sleep = false;
//                 count--;
//                 return true;
//             }
//         }
//         ESP_LOGW(ESP_NOW, "MAC address not found for removal");
//         return false;
//     }

//     // 清空所有 MAC 地址
//     void clear() {
//         memset(slaves, 0, sizeof(slaves));
//         count = 0;
//     }

//     void print() {
//         ESP_LOGI(ESP_NOW, "MAC addresses: %d", count);
//         for (size_t i = 0; i < count; i++) {
//             ESP_LOGI(ESP_NOW, "MAC address %d: " MACSTR, i, MAC2STR(slaves[i].mac));
//         }
//     }
//     // 重载赋值运算符
//     MacAddress& operator=(const MacAddress& other) {
//         if (this != &other) {
//             // 复制MAC地址数组
//             memcpy(slaves, other.slaves, sizeof(slaves));
//             // 复制计数
//             count = other.count;
//         }
//         return *this;
//     }

//     void set_sleep(const uint8_t mac[ESP_NOW_ETH_ALEN], bool is_sleep) {
//         for (size_t i = 0; i < count; i++) {
//             if (memcmp(slaves[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
//                 slaves[i].is_sleep = is_sleep;
//             }
//         }
//     }

//     bool is_sleep(const uint8_t mac[ESP_NOW_ETH_ALEN]) {
//         for (size_t i = 0; i < count; i++) {
//             if (memcmp(slaves[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
//                 return slaves[i].is_sleep;
//             }
//         }
//         return true;
//     }
// };

// #endif
