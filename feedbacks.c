#include <inttypes.h>
#include <math.h>

#include "ext_include/stm32f2xx.h"
#include "feedbacks.h"
#include "gyro_xcel_compass.h"
#include "motcons.h"

#define LED_ON()  {GPIOC->BSRR = 1UL<<13;}
#define LED_OFF() {GPIOC->BSRR = 1UL<<(13+16);}

extern volatile int dbg1, dbg2, dbg3, dbg4;

extern volatile motcon_t motcons[NUM_MOTCONS];

int64_t gyro_long_integrals[3];
int64_t gyro_short_integrals[3];

int gyro_timing_issues;

int cur_angle = 0; // int32_t range --> -180..+180 deg; let it overflow freely. 1 unit = 83.81903171539 ndeg
int cur_compass_angle = 0;
int aim_angle = 0; // same

int ang_accel = 50; //100;
int ang_top_speed = 500000; //500000;
int ang_p = 700; //1000;

int speed_updated;  // For timeouting robot movements (for faulty communications)
int manual_control;
int common_speed;
int manual_ang_speed;

void zero_gyro_short_integrals()
{
	gyro_short_integrals[0] = 0;
	gyro_short_integrals[1] = 0;
	gyro_short_integrals[2] = 0;
}

void move_rel_twostep(int angle, int fwd)
{
	aim_angle += angle<<16;
	ang_top_speed = 500000;
	manual_control = 0;
	speed_updated = 100000;
}

void compass_fsm(int cmd)
{
	static int compass_x_min = 0;
	static int compass_x_max = 0;
	static int compass_y_min = 0;
	static int compass_y_max = 0;

	static int state = 0;

	if(cmd == 1)
		state = 1;

	int cx = latest_compass->x;
	int cy = latest_compass->y;

	int ang_err = cur_angle - aim_angle;
	if(state == 1)
	{
		compass_x_min = compass_x_max = cx;
		compass_y_min = compass_y_max = cy;

//		aim_angle += (65536/3)<<16; // instruct 120 deg turn
		aim_angle += 1431655765;
		ang_top_speed = 100000;
		manual_control = 0;
		speed_updated = 1000000;
		state = 2;
	}
	else if(state == 2)
	{
//		if(ang_err < (65536/6)<<16 && ang_err > (-65536/6)<<16); // when 60 deg remaining, instruct 120 deg more.
		if(ang_err < 60000000 && ang_err > -60000000)
		{
//			aim_angle += (65536/3)<<16;
			aim_angle += 1431655765;
			speed_updated = 1000000;
			state = 3;
		}
	}
	else if(state == 3)
	{
//		if(ang_err < (65536/6)<<16 && ang_err > (-65536/6)<<16); // when 60 deg remaining, instruct 120 deg more.
		if(ang_err < 60000000 && ang_err > -60000000)
		{
//			aim_angle += (65536/3)<<16;
			aim_angle += 1431655765;
			speed_updated = 1000000;
			state = 4;
		}
	}
	else if(state == 4)
	{
//		if(ang_err < (65536/6)<<16 && ang_err > (-65536/6)<<16); // when 60 deg remaining, instruct 120 deg more.
		if(ang_err < 60000000 && ang_err > -60000000)
		{
//			aim_angle += (65536/3)<<16;
			aim_angle += 1431655765;
			speed_updated = 1000000;
			state = 5;
		}
	}
	else if(state == 5)
	{
//		if(ang_err < (65536/6)<<16 && ang_err > (-65536/6)<<16); // when 60 deg remaining, instruct 120 deg more.
		if(ang_err < 60000000 && ang_err > -60000000)
		{
//			aim_angle += (65536/3)<<16;
			aim_angle += 1431655765;
			speed_updated = 1000000;
			state = 6;
		}
	}
	else if(state == 6)
	{
//		if(ang_err < (65536/6)<<16 && ang_err > (-65536/6)<<16); // when 60 deg remaining, instruct 120 deg more.
		if(ang_err < 60000000 && ang_err > -60000000)
		{
//			aim_angle += (65536/3)<<16;
			aim_angle += 1431655765;
			speed_updated = 1000000;
			state = 0;
		}
	}

	dbg4 = state;


	if(cx < compass_x_min)
		compass_x_min = cx;
	if(cy < compass_y_min)
		compass_y_min = cy;

	if(cx > compass_x_max)
		compass_x_max = cx;
	if(cy > compass_y_max)
		compass_y_max = cy;

	int dx = compass_x_max - compass_x_min;
	int dy = compass_y_max - compass_y_min;
	if(dx > 500 && dy > 500)
	{
		int dx2 = compass_x_max + compass_x_min;
		int dy2 = compass_y_max + compass_y_min;
		cx = cx - dx2/2;
		cy = cy - dy2/2;

		double heading = atan2(cx, cy);
		heading /= (2.0*M_PI);
		heading *= 65536.0*65536.0;
		cur_compass_angle = (int)heading;// + 2147483648 /*180 deg*/;
	}



}

void sync_to_compass()
{
	cur_angle = aim_angle = cur_compass_angle;
}

void move_arc_manual(int comm, int ang)
{
	common_speed = comm;
	manual_ang_speed = ang;
	speed_updated = 5000; // robot is stopped if 0.5s is elapsed between the speed commands.
	manual_control = 1;
}

// Run this at 10 kHz
void run_feedbacks(int sens_status)
{
	int i;
	static int cnt = 0;
	static int prev_gyro_cnt = 0;
	static int speeda = 0;
	static int speedb = 0;

	static int idle = 1;

	static int ang_speed = 0;
	static int ang_speed_limit = 0;

	cnt++;

	dbg1 = aim_angle;
	dbg2 = cur_angle;

	int ang_err = cur_angle - aim_angle;
	dbg3 = ang_err;
	if(!manual_control && (ang_err < -1*65536 || ang_err > 1*65536))
	{
		if(idle)
		{
			idle = 0;
			ang_speed_limit = 0;
			zero_gyro_short_integrals();
		}

		// Calculate angular speed with P loop from the gyro integral.
		// Limit the value by using acceleration ramp. P loop handles the deceleration.

		ang_speed_limit += ang_accel;
		if(ang_speed_limit > ang_top_speed) ang_speed_limit = ang_top_speed;

		int new_ang_speed = (ang_err>>20)*ang_p;
		if(new_ang_speed < 0 && new_ang_speed < -1*ang_speed_limit) new_ang_speed = -1*ang_speed_limit;
		if(new_ang_speed > 0 && new_ang_speed > ang_speed_limit) new_ang_speed = ang_speed_limit;

		ang_speed = new_ang_speed;
		common_speed = 0;

	}
	else
	{
		idle=1;
		ang_speed = 0;
	}

//	dbg4 = gyro_timing_issues;

	if(sens_status & GYRO_NEW_DATA)
	{
		int gyro_dt = cnt - prev_gyro_cnt;
		prev_gyro_cnt = cnt;
		// Gyro should give data at 200Hz, which is 50 steps at 10kHz
		if(gyro_dt < 30 || gyro_dt > 200)
		{
			// If the timing is clearly out of range, assume 200Hz data rate and log the error.
			gyro_dt = 50;
			gyro_timing_issues++;
		}

		int latest[3] = {latest_gyro->x*gyro_dt, latest_gyro->y*gyro_dt, latest_gyro->z*gyro_dt};

		for(i=0; i<3; i++)
		{
//			if(latest[i] < -GYRO_LONG_INTEGRAL_IGNORE_LEVEL || latest[i] > GYRO_LONG_INTEGRAL_IGNORE_LEVEL)
				gyro_long_integrals[i] += latest[i];

			gyro_short_integrals[i] += latest[i];
		}

		//1 gyro unit = 31.25 mdeg/s; integrated at 10kHz timesteps, 1 unit = 3.125 udeg
		// Correct ratio = (3.125*10^-6)/(360/(2^32)) = 37.28270222222358615960
		// Approximated ratio = 76355/2048 = 37.28271484375
		// Error = -0.00003385%
		cur_angle += ((int64_t)latest[2]*(int64_t)76355)>>11;
	}

	int error;
	if(manual_control)
		error = -1*latest_gyro->z/32 - manual_ang_speed;
	else
		error = -1*latest_gyro->z/32 - (ang_speed>>12);

	speeda += error>>2;
	speedb -= error>>2;

	if((cnt&0x3f) == 0x3f) // slow decay
	{
		if(speeda > 0) speeda--;
		else if(speeda < 0) speeda++;

		if(speedb > 0) speedb--;
		else if(speedb < 0) speedb++;
	}

	if(speeda > MAX_DIFFERENTIAL_SPEED*256) speeda = MAX_DIFFERENTIAL_SPEED*256;
	else if(speeda < -MAX_DIFFERENTIAL_SPEED*256) speeda = -MAX_DIFFERENTIAL_SPEED*256;
	if(speedb > MAX_DIFFERENTIAL_SPEED*256) speedb = MAX_DIFFERENTIAL_SPEED*256;
	else if(speedb < -MAX_DIFFERENTIAL_SPEED*256) speedb = -MAX_DIFFERENTIAL_SPEED*256;

	int a = common_speed + speeda/512;
	int b = common_speed + speedb/512;

	a>>=1;
	b>>=1;

	if(a > MAX_SPEED) a=MAX_SPEED;
	else if(a < -MAX_SPEED) a=-MAX_SPEED;
	if(b > MAX_SPEED) b=MAX_SPEED;
	else if(b < -MAX_SPEED) b=-MAX_SPEED;

	if(speed_updated)
	{
		LED_OFF();
		speed_updated--;
		motcons[2].cmd.speed = a;
		motcons[3].cmd.speed = -1*b;
	}
	else
	{
		LED_ON();
		motcons[2].cmd.speed = 0;
		motcons[3].cmd.speed = 0;
		speeda=0;
		speedb=0;
	}

}
