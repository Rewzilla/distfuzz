#ifndef _FUZZDATA_H
#define _FUZZDATA_H

#define KNOWN_INT8_LEN 4
uint8_t known_int8s[KNOWN_INT8_LEN] = {
	0x00, 0x7f, 0x80, 0xff
};

#define KNOWN_INT16_LEN 4
uint16_t known_int16s[KNOWN_INT16_LEN] = {
	0x0000, 0x7fff, 0x8000, 0xffff
};

#define KNOWN_INT32_LEN 4
uint32_t known_int32s[KNOWN_INT32_LEN] = {
	0x00000000, 0x7fffffff, 0x80000000, 0xffffffff
};

#endif