#ifndef _LIDAR_H
#define _LIDAR_H

#include <stdint.h>


#define DEFAULT_LIDAR_RPM 300

extern volatile int lidar_rpm_setpoint_x64;

typedef struct __attribute__ ((__packed__))
{
	uint16_t flags_distance;
	uint16_t signal;
} lidar_d_t;

typedef union
{
	struct __attribute__ ((__packed__))
	{
		uint8_t start;
		uint8_t idx;
		uint16_t speed;
		lidar_d_t d[4];
		uint16_t checksum;
	};
	uint16_t u16[11];
	uint8_t u8[22];
} lidar_datum_t;

void sync_lidar();
void resync_lidar();
void init_lidar();
uint16_t lidar_calc_checksum(volatile lidar_datum_t* l);
void lidar_ctrl_loop();


#endif