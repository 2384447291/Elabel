#ifndef HTTP_MUSIC_H
#define HTTP_MUSIC_H
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif
void set_success_post_music(bool _success_post_music);
void set_success_get_music(bool _success_get_music);

void start_post_music(int32_t _music_unique_id);

void start_get_music(int32_t _music_unique_id, uint32_t* _ptr_mcodec_record_message_unique_id, uint32_t success_record_message_unique_id);

#ifdef __cplusplus
}
#endif

#endif