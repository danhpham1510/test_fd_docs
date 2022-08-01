
#include "freertos/ringbuf.h"


void print_heap_info();

UBaseType_t get_buffer_size(RingbufHandle_t* buffer);

void free_frame_memory(struct frame_struct** f);

void push_to_queue(float ** q, int len, float value);

/*
 *  Function get mac address from esp and use md5 to encrypt
 *  to be a unique serial number for device
 *  
 *  @param: device_id   a buffer to contain serial number
*/
void get_device_id(char* device_id);