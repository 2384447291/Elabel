#include "global_tool.h"
void progress_update(size_t sent_before, size_t sent_now, size_t total)
{
    if (total == 0) return; // 防止除零

    int percent_before = (int)((sent_before * 100) / total);
    int percent_after  = (int)((sent_now * 100) / total);

    // 检查是否跨越了新的10%节点，比如从17%到24%跨过了20%
    for (int p = ((percent_before / 10) + 1) * 10; p <= percent_after && p <= 100; p += 10) {
        ESP_LOGI("UPLOAD", "已上传 %d%% (%u/%u bytes)",
                 p, (unsigned)(total * p / 100), (unsigned)total);
    }
}