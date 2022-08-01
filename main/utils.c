#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include <ctype.h>

#include "common.h"
#include <string.h>
#include "md5.h"
#include "utils.h"

/**
 * @brief Print current heap ram has been use
 * 
 */
void print_heap_info(){
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
    printf("Free size: %u\n", heap_info.total_free_bytes);
    printf("Allocate size: %u\n", heap_info.total_allocated_bytes);
    printf("===========================\n");
}


UBaseType_t get_buffer_size(RingbufHandle_t* buffer)
{
    UBaseType_t uxFree;
    UBaseType_t uxRead;
    UBaseType_t uxWrite;
    UBaseType_t uxAcquire;
    UBaseType_t uxItemWaiting;
    vRingbufferGetInfo(*buffer, &uxFree, &uxRead, &uxWrite, &uxAcquire, &uxItemWaiting);
    return uxItemWaiting;
}


void free_frame_memory(struct frame_struct** f)
{
    if ((*f)->num_point_clouds > 0) free((*f)->point_clouds);
    if ((*f)->num_targets > 0) free((*f)->targets);
    if ((*f)->num_indexes > 0) free((*f)->indexes);
}


void push_to_queue(float ** q, int len, float value)
{
    memcpy(*q, *q + 1, sizeof(float) * len);
    *(*q + len - 1) = value;
}

void get_device_id(char* device_id)
{
    // uint8_t* mac_addr = (uint8_t*)malloc(sizeof(uint8_t)*6);
    // ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_addr));
    // uint8_t* hex_mac_addr = md5String((char*)mac_addr);
    // for (uint8_t i = 0; i < 16; i ++){
    //     printf("%u, ", hex_mac_addr[i]);
    // }
    // printf("\n");
    device_id[0] = 'a';
    device_id[1] = 'u';
    device_id[2] = 'r';
    device_id[3] = 'a';
    device_id[4] = '_';
    // for (uint8_t i = 0; i < 16; i++){
    //     char tmp[3];
    //     sprintf(tmp, "%02x", *(hex_mac_addr + i));
    //     if (!isdigit(tmp[0])){
    //         device_id[i*2 + 5] = tmp[0] - 32;
    //     }else if (isdigit(tmp[0])){
    //         device_id[i*2 + 5] = tmp[0];
    //     }
    //     if (!isdigit(tmp[1])){
    //         device_id[i*2 + 1 + 5] = tmp[1] - 32;
    //     }else if (isdigit(tmp[1])){
    //         device_id[i*2 + 1 + 5] = tmp[1];
    //     }
    // }
    device_id[5] = '\0';
    // free(hex_mac_addr);
    // free(mac_addr);
}