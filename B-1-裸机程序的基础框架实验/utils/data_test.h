#ifndef __DATA_TEST_H
#define	__DATA_TEST_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

uint8_t get_rand_byte(void);

void fill_rand_byte_buffer(uint8_t* buffer, uint16_t size);
void fill_sequential_byte_buffer(uint8_t* buffer, uint16_t size);
void printf_byte_buffer(uint8_t* buffer, uint16_t size);

void fill_sequential_memory(uint32_t base,uint32_t* begin, uint32_t* end);
void printf_memory(uint32_t* begin, uint32_t* end);

uint8_t compare_bufferrs(uint8_t* buffer1,uint8_t* buffer2, uint16_t size);


#endif /* __DATA_TEST_H */
