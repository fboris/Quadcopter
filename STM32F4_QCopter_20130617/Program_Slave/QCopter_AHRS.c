/*=====================================================================================================*/
/*=====================================================================================================*/
#include "stm32f4_system.h"
#include "QCopter_AHRS.h"
#include "module_sensor.h"
#include "algorithm_mathUnit.h"
#include "algorithm_quaternion.h"
/*=====================================================================================================*/
/*=====================================================================================================*/
// #define Kp 10.0f
// #define Ki 0.008f
#define Kp 3.5f
#define Ki 0.006f
// float Kp = 10.0f;
// float Ki = 0.008f;

#define halfT 0.00125f
/*=====================================================================================================*/
/*=====================================================================================================*/
void AHRS_Init( void )
{
  NumQ.q0 = 1.0f;
  NumQ.q1 = 0.0f;
  NumQ.q2 = 0.0f;
  NumQ.q3 = 0.0f;

  AngE.Pitch = 0.0f;
  AngE.Roll  = 0.0f;
  AngE.Yaw   = 0.0f;
}
/*=====================================================================================================*/
/*=====================================================================================================*/
void AHRS_Update( void )
{
  float tempX = 0, tempY = 0;
  float Normalize;
  float gx, gy, gz;
// float hx, hy, hz;
// float wx, wy, wz;
// float bx, bz;
  float ErrX, ErrY, ErrZ;
  float AccX, AccY, AccZ;
  float GyrX, GyrY, GyrZ;
// float MegX, MegY, MegZ;
  float /*Mq11, Mq12, */Mq13,/* Mq21, Mq22, */Mq23,/* Mq31, Mq32, */Mq33;

  static float AngZ_Temp = 0.0f;
  static float exInt = 0.0f, eyInt = 0.0f, ezInt = 0.0f;

//   Mq11 = NumQ.q0*NumQ.q0 + NumQ.q1*NumQ.q1 - NumQ.q2*NumQ.q2 - NumQ.q3*NumQ.q3;
//   Mq12 = 2.0f*(NumQ.q1*NumQ.q2 + NumQ.q0*NumQ.q3);
  Mq13 = 2.0f*(NumQ.q1*NumQ.q3 - NumQ.q0*NumQ.q2);
//   Mq21 = 2.0f*(NumQ.q1*NumQ.q2 - NumQ.q0*NumQ.q3);
//   Mq22 = NumQ.q0*NumQ.q0 - NumQ.q1*NumQ.q1 + NumQ.q2*NumQ.q2 - NumQ.q3*NumQ.q3;
  Mq23 = 2.0f*(NumQ.q0*NumQ.q1 + NumQ.q2*NumQ.q3);
//   Mq31 = 2.0f*(NumQ.q0*NumQ.q2 + NumQ.q1*NumQ.q3);
//   Mq32 = 2.0f*(NumQ.q2*NumQ.q3 - NumQ.q0*NumQ.q1);
  Mq33 = NumQ.q0*NumQ.q0 - NumQ.q1*NumQ.q1 - NumQ.q2*NumQ.q2 + NumQ.q3*NumQ.q3;

  Normalize = invSqrtf(squa(Acc.TrueX) + squa(Acc.TrueY) + squa(Acc.TrueZ));      
  AccX = Acc.TrueX*Normalize;
  AccY = Acc.TrueY*Normalize;
  AccZ = Acc.TrueZ*Normalize;

// Normalize = invSqrtf(squa(Meg.TrueX) + squa(Meg.TrueY) + squa(Meg.TrueZ));
// MegX = Meg.TrueX*Normalize;
// MegY = Meg.TrueY*Normalize;
// MegZ = Meg.TrueZ*Normalize;

  gx = Mq13;
  gy = Mq23;
  gz = Mq33;

// hx = MegX*Mq11 + MegY*Mq21 + MegZ*Mq31;
// hy = MegX*Mq12 + MegY*Mq22 + MegZ*Mq32;
// hz = MegX*Mq13 + MegY*Mq23 + MegZ*Mq33;

// bx = sqrtf(squa(hx) + squa(hy));
// bz = hz;

// wx = bx*Mq11 + bz*Mq13;
// wy = bx*Mq21 + bz*Mq23;
// wz = bx*Mq31 + bz*Mq33;

  ErrX = (AccY*gz - AccZ*gy)/* + (MegY*wz - MegZ*wy)*/;
  ErrY = (AccZ*gx - AccX*gz)/* + (MegZ*wx - MegX*wz)*/;
  ErrZ = (AccX*gy - AccY*gx)/* + (MegX*wy - MegY*wx)*/;

  exInt = exInt + ErrX*Ki;
  eyInt = eyInt + ErrY*Ki;
  ezInt = ezInt + ErrZ*Ki;

  GyrX = toRad(Gyr.TrueX);
  GyrY = toRad(Gyr.TrueY);
  GyrZ = toRad(Gyr.TrueZ);

  GyrX = GyrX + Kp*ErrX + exInt;
  GyrY = GyrY + Kp*ErrY + eyInt;
  GyrZ = GyrZ + Kp*ErrZ + ezInt;

  Quaternion_RungeKutta(&NumQ, GyrX, GyrY, GyrZ, halfT);
  Quaternion_Normalize(&NumQ);
  Quaternion_ToAngE(&NumQ, &AngE);

//  AngE.Yaw = atan2f(Meg.TrueY, Meg.TrueX);
  tempX = (Meg.X*cosf(Ellipse[0])+Meg.Y*sinf(Ellipse[0]))/Ellipse[4];
  tempY = (-Meg.X*sinf(Ellipse[0])+Meg.Y*cosf(Ellipse[0]))/Ellipse[3];
  AngE.Yaw = atan2f(tempX, tempY);

	AngE.Pitch = toDeg(AngE.Pitch);
	AngE.Roll  = toDeg(AngE.Roll);
	AngE.Yaw   = toDeg(AngE.Yaw)+180.0f;

  /* �����o�i Complementary Filter */
  #define CF_A 0.985f
  #define CF_B 0.015f
  #define CF_dt 0.0025f
  AngZ_Temp = AngZ_Temp + GyrZ*CF_dt;
  AngZ_Temp = CF_A*AngZ_Temp + CF_B*AngE.Yaw;
  if(AngZ_Temp>360.0f)
    AngE.Yaw = AngZ_Temp - 360.0f;
  else if(AngZ_Temp<0.0f)
    AngE.Yaw = AngZ_Temp + 360.0f;
  else
    AngE.Yaw = AngZ_Temp;
}
/*=====================================================================================================*/
/*=====================================================================================================*/
