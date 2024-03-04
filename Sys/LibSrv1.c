#include "../Extern.h"

static int Blobcnt, Blobx1, Blobx2, Bloby1, Bloby2, Iy1, Iy2, Iu1, Iu2, Iv1, Iv2;
static int GPSlat, GPSlon, GPSalt, GPSfix, GPSsat, GPSutc, Elcount, Ercount;
static int ScanVect[16], NNVect[NUM_OUTPUT];

ValueType IntArrayType;

void SRV1SetupFunc(State pc) {
   pc->IntArrayType = TypeGetMatching(pc, NULL, &pc->IntType, ArrayT, 16, StrEmpty, true);
   VariableDefinePlatformVar(pc, NULL, "scanvect", pc->IntArrayType, (AnyValue)&ScanVect, false);
   VariableDefinePlatformVar(pc, NULL, "neuron", pc->IntArrayType, (AnyValue)&NNVect, false);
   VariableDefinePlatformVar(pc, NULL, "blobcnt", &pc->IntType, (AnyValue)&Blobcnt, false);
   VariableDefinePlatformVar(pc, NULL, "blobx1", &pc->IntType, (AnyValue)&Blobx1, false);
   VariableDefinePlatformVar(pc, NULL, "blobx2", &pc->IntType, (AnyValue)&Blobx2, false);
   VariableDefinePlatformVar(pc, NULL, "bloby1", &pc->IntType, (AnyValue)&Bloby1, false);
   VariableDefinePlatformVar(pc, NULL, "bloby2", &pc->IntType, (AnyValue)&Bloby2, false);
   VariableDefinePlatformVar(pc, NULL, "lcount", &pc->IntType, (AnyValue)&Elcount, false);
   VariableDefinePlatformVar(pc, NULL, "rcount", &pc->IntType, (AnyValue)&Ercount, false);
   VariableDefinePlatformVar(pc, NULL, "y1", &pc->IntType, (AnyValue)&Iy1, false);
   VariableDefinePlatformVar(pc, NULL, "y2", &pc->IntType, (AnyValue)&Iy2, false);
   VariableDefinePlatformVar(pc, NULL, "u1", &pc->IntType, (AnyValue)&Iu1, false);
   VariableDefinePlatformVar(pc, NULL, "u2", &pc->IntType, (AnyValue)&Iu2, false);
   VariableDefinePlatformVar(pc, NULL, "v1", &pc->IntType, (AnyValue)&Iv1, false);
   VariableDefinePlatformVar(pc, NULL, "v2", &pc->IntType, (AnyValue)&Iv2, false);
   VariableDefinePlatformVar(pc, NULL, "gpslat", &pc->IntType, (AnyValue)&GPSlat, false);
   VariableDefinePlatformVar(pc, NULL, "gpslon", &pc->IntType, (AnyValue)&GPSlon, false);
   VariableDefinePlatformVar(pc, NULL, "gpsalt", &pc->IntType, (AnyValue)&GPSalt, false);
   VariableDefinePlatformVar(pc, NULL, "gpsfix", &pc->IntType, (AnyValue)&GPSfix, false);
   VariableDefinePlatformVar(pc, NULL, "gpssat", &pc->IntType, (AnyValue)&GPSsat, false);
   VariableDefinePlatformVar(pc, NULL, "gpsutc", &pc->IntType, (AnyValue)&GPSutc, false);
}

// Check for kbhit, return t or nil.
void Csignal(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getsignal()? 1: 0;
}

// Return 0-9 from console input.
void Cinput(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getch();
}

void Cdelay(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int del = Param[0]->Val->Integer;
   if (del < 0 || del > 1000000)
      return;
   delayMS(del);
}

void Crand(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = (int)rand()%(unsigned int)(Param[0]->Val->Integer + 1);
}

void Ctime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = (int)readRTC();
}

void Ciodir(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int dir = Param[0]->Val->Integer;
   *pPORTHIO_DIR = ((dir << 10)&0xFC00) + (*pPORTHIO_DIR&0x03FF); // H15/14/13/12/11/10 - 1=output, 0=input.
   *pPORTHIO_INEN = (((~dir) << 10)&0xFC00) + (*pPORTHIO_INEN&0x03FF); // Invert dir bits to enable inputs.
}

void Cioread(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = (*pPORTHIO >> 10)&0x003F;
}

void Ciowrite(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   *pPORTHIO = ((Param[0]->Val->Integer << 10)&0xFC00) + (*pPORTHIO&0x03FF);
}

void Cpeek(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
// x = peek(addr, size);
// Mask ptr to align with word size.
   int ptr = Param[0]->Val->Integer;
   int size = Param[1]->Val->Integer;
   switch (size) {
   // char *:
      case 1: ReturnValue->Val->Integer = (int)(unsigned int)*(unsigned char *)ptr; break;
   // short *: Align with even boundary.
      case 2: ReturnValue->Val->Integer = (int)(unsigned short)*(unsigned short *)(ptr&~1); break;
   // int *: Align with quad boundary.
      case 4: ReturnValue->Val->Integer = (int)*(unsigned int *)(ptr&~3); break;
      default: ReturnValue->Val->Integer = 0; break;
   }
}

void Cpoke(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
// x = poke(addr, size, val);
// Mask ptr to align with word size.
   int ptr = Param[0]->Val->Integer;
   int size = Param[1]->Val->Integer;
   int val = Param[2]->Val->Integer;
   switch (size) {
   // char *:
      case 1: *(unsigned char *)ptr = (unsigned char)(val&0xff); break;
   // short *:
      case 2: *(unsigned short *)(ptr&~1) = (unsigned short)(val&0xffff); break;
   // int *:
      case 4: *(unsigned int *)(ptr&~3) = val; break;
   // Don't bother with bad value.
      default: break;
   }
}

void Cencoders(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned int ix = encoders(); // Read left and right encoders; save data to C globals lcount, rcount.
   Elcount = (ix >> 16)&0x0000FFFF;
   Ercount = ix&0x0000FFFF;
}

void Cmotors(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lspeed = Param[0]->Val->Integer;
   if (lspeed < -100 || lspeed > 100)
      ProgramFail(NULL, "motors():  left motor value out of range");
   int rspeed = Param[1]->Val->Integer;
   if (rspeed < -100 || rspeed > 100)
      ProgramFail(NULL, "motors():  right motor value out of range");
   if (!pwm1_init) {
      initPWM();
      pwm1_init = 1;
      pwm1_mode = PWM_PWM;
      base_speed = 50;
   }
   setPWM(lspeed, rspeed);
}

void Cmotors2(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lspeed2 = Param[0]->Val->Integer;
   if (lspeed2 < -100 || lspeed2 > 100)
      ProgramFail(NULL, "motors2():  left motor value out of range");
   int rspeed2 = Param[1]->Val->Integer;
   if (rspeed2 < -100 || rspeed2 > 100)
      ProgramFail(NULL, "motors2():  right motor value out of range");
   if (!pwm2_init) {
      initPWM2();
      pwm2_init = 1;
      pwm2_mode = PWM_PWM;
      base_speed2 = 50;
   }
   setPWM2(lspeed2, rspeed2);
}

void Cservos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lspeed = Param[0]->Val->Integer;
   if (lspeed < 0 || lspeed > 100)
      ProgramFail(NULL, "servos():  TMR2 value out of range");
   int rspeed = Param[1]->Val->Integer;
   if (rspeed < 0 || rspeed > 100)
      ProgramFail(NULL, "servos()():  TMR3 value out of range");
   if (!pwm1_init) {
      initPPM1();
      pwm1_init = 1;
      pwm1_mode = PWM_PPM;
   }
   setPPM1(lspeed, rspeed);
}

void Cservos2(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lspeed = Param[0]->Val->Integer;
   if (lspeed < 0 || lspeed > 100)
      ProgramFail(NULL, "servos2():  TMR6 value out of range");
   int rspeed = Param[1]->Val->Integer;
   if (rspeed < 0 || rspeed > 100)
      ProgramFail(NULL, "servos2():  TMR7 value out of range");
   if (!pwm2_init) {
      initPPM2();
      pwm2_init = 1;
      pwm2_mode = PWM_PPM;
   }
   setPPM2(lspeed, rspeed);
}

// Laser(1) turns them on, laser(0) turns them off.
void Claser(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   *pPORTHIO &= ~0x0280; // Turn off both lasers.
   switch (Param[0]->Val->Integer) {
   // Turn on left laser.
      case 1: *pPORTHIO |= 0x0080; break;
   // Turn on right laser.
      case 2: *pPORTHIO |= 0x0200; break;
   // Turn on both lasers.
      case 3: *pPORTHIO |= 0x0280; break;
   }
}

// Read sonar module.
void Csonar(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned int i = Param[0]->Val->Integer;
   if (i < 1 || i > 4) {
      ProgramFail(NULL, "sonar():  1, 2, 3, 4 are only valid selections");
   }
   sonar();
   ReturnValue->Val->Integer = sonar_data[i]/100;
}

void Crange(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = laser_range(0);
}

void Cbattery(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
// 0: Low battery voltage detected, 1: Battery voltage okay.
   ReturnValue->Val->Integer = *pPORTHIO&0x0004? 0: 1;
}

// Set color bin -
//	vcolor(color, ymin, ymax, umin, umax, vmin, vmax);
void Cvcolor(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   ymin[ix] = Param[1]->Val->Integer;
   ymax[ix] = Param[2]->Val->Integer;
   umin[ix] = Param[3]->Val->Integer;
   umax[ix] = Param[4]->Val->Integer;
   vmin[ix] = Param[5]->Val->Integer;
   vmax[ix] = Param[6]->Val->Integer;
}

// Set camera functions -
//	enable/disable AGC(4) / AWB(2) / AEC(1) camera controls,
//	vcam(7) = AGC+AWB+AEC on vcam(0) = AGC+AWB+AEC off.
void Cvcam(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char cx = (unsigned char)Param[0]->Val->Integer&0x07;
   unsigned char i2c_data[2];
   i2c_data[0] = 0x13;
   i2c_data[1] = 0xC0 + cx;
   i2cwrite(0x30, (unsigned char *)i2c_data, 1, SCCB_ON); // OV9655.
   i2cwrite(0x21, (unsigned char *)i2c_data, 1, SCCB_ON); // OV7725.
}

// Set color bin -
//	vfind(color, x1, x2, y1, y2);
void Cvfind(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   int x1 = Param[1]->Val->Integer;
   int x2 = Param[2]->Val->Integer;
   int y1 = Param[3]->Val->Integer;
   int y2 = Param[4]->Val->Integer;
   ReturnValue->Val->Integer = vfind((unsigned char *)FRAME_BUF, ix, x1, x2, y1, y2);
}

void Cvcap(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   grab_frame(); // Capture frame for processing.
}

void Cvrcap(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   grab_reference_frame(); // Capture reference frame for differencing.
}

void Cvdiff(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   frame_diff_flag = Param[0]->Val->Integer; // Set/clear frame_diff_flag.
}

void Cvpix(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int x = Param[0]->Val->Integer;
   int y = Param[1]->Val->Integer;
   int ix = vpix((unsigned char *)FRAME_BUF, x, y);
   Iy1 = ((ix >> 16)&0x000000FF); // Y1.
   Iu1 = ((ix >> 24)&0x000000FF); // U.
   Iv1 = ((ix >> 8)&0x000000FF); // V.
}

void Cvscan(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int col = Param[0]->Val->Integer;
   if (col < 1 || col > 9)
      ProgramFail(NULL, "vscan():  number of columns must be between 1 and 9");
   int thresh = Param[1]->Val->Integer;
   if (thresh < 0 || thresh > 9999)
      ProgramFail(NULL, "vscan():  threshold must be between 0 and 9999");
   int ix = vscan((unsigned char *)SPI_BUFFER1, (unsigned char *)FRAME_BUF, thresh, (unsigned int)col, (unsigned int *)ScanVect);
   ReturnValue->Val->Integer = ix;
}

void Cvmean(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   vmean((unsigned char *)FRAME_BUF);
   Iy1 = mean[0];
   Iu1 = mean[1];
   Iv1 = mean[2];
}

// Search for blob by color, index; return center point X, Y and width Z.
void Cvblob(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   if (ix > MAX_COLORS)
      ProgramFail(NULL, "blob():  invalid color index");
   int iblob = Param[1]->Val->Integer;
   if (iblob > MAX_BLOBS)
      ProgramFail(NULL, "blob():  invalid blob index");
   int numblob = vblob((unsigned char *)FRAME_BUF, (unsigned char *)FRAME_BUF3, ix);
   if (blobcnt[iblob] == 0 || numblob == -1) {
      Blobcnt = 0;
   } else {
      Blobcnt = blobcnt[iblob];
      Blobx1 = blobx1[iblob];
      Blobx2 = blobx2[iblob];
      Bloby1 = bloby1[iblob];
      Bloby2 = bloby2[iblob];
   }
   ReturnValue->Val->Integer = numblob;
}

void Cvjpeg(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned int qual = Param[0]->Val->Integer;
   if (qual < 1 || qual > 8)
      ProgramFail(NULL, "vjpeg():  quality parameter out of range");
   unsigned char *output_start = (unsigned char *)JPEG_BUF;
   unsigned char *output_end = encode_image((unsigned char *)FRAME_BUF, output_start, qual, FOUR_TWO_TWO, imgWidth, imgHeight);
   ReturnValue->Val->Integer = (unsigned int)(output_end - output_start);
}

void Cvsend(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned int image_size = Param[0]->Val->Integer;
   if (image_size < 0 || image_size > 200000)
      ProgramFail(NULL, "vsend():  image size out of range");
   led1_on();
   unsigned char *cp = (unsigned char *)JPEG_BUF;
   for (unsigned int ix = 0; ix < image_size; ix++)
      putchar(*cp++);
   led0_on();
}

// Return reading from HMC6352 I2C compass.
void Ccompass(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char i2c_data[2];
   i2c_data[0] = 0x41; // Read compass twice to clear last reading.
   i2cread(0x22, (unsigned char *)i2c_data, 2, SCCB_ON);
   delayMS(20);
   i2c_data[0] = 0x41;
   i2cread(0x22, (unsigned char *)i2c_data, 2, SCCB_ON);
   ReturnValue->Val->Integer = ((unsigned int)(i2c_data[0] << 8) + i2c_data[1])/10;
}

// Return reading from HMC6352 I2C compass.
void Ctilt(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned ix = (unsigned int)Param[0]->Val->Integer;
   if (ix < 1 || ix > 3)
      ProgramFail(NULL, "tilt():  invalid channel");
   ReturnValue->Val->Integer = tilt(ix);
}

// Return reading from HMC6352 I2C compass.
void Canalog(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char mask1[] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08 };
   unsigned char mask2[] = { 0x10, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00 };
// Decide which i2c device based on channel range.
   unsigned ix = (unsigned char)Param[0]->Val->Integer;
   if (ix < 1 || ix > 28)
      ProgramFail(NULL, "analog():  invalid channel");
   unsigned char device_id = 0;
   switch (ix/10) {
   // Channels 1-8.
      case 0: device_id = 0x20; break;
   // Channels 11-18.
      case 1: device_id = 0x23; break;
   // Channels 21-28.
      case 2: device_id = 0x24; break;
   }
   unsigned channel = ix%10;
   if (channel < 1 || channel > 8)
      ProgramFail(NULL, "analog():  invalid channel");
// Set timer register 3.
   unsigned char i2c_data[3];
   i2c_data[0] = 0x03;
   i2c_data[1] = 0x01;
   i2cwrite(device_id, (unsigned char *)i2c_data, 1, SCCB_ON);
// Set analog channel.
   i2c_data[0] = 0x02;
   i2c_data[1] = mask1[channel - 1];
   i2c_data[2] = mask2[channel - 1];
   i2cwritex(device_id, (unsigned char *)i2c_data, 3, SCCB_ON);
// Small delay.
   delayUS(1000);
// Read data.
   i2c_data[0] = 0x00;
   i2cread(device_id, (unsigned char *)i2c_data, 2, SCCB_ON);
   ix = (((i2c_data[0]&0x0F) << 8) + i2c_data[1]);
   ReturnValue->Val->Integer = ix;
}

void Cgps(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   gps_parse();
   GPSlat = gps_gga.lat;
   GPSlon = gps_gga.lon;
   GPSalt = gps_gga.alt;
   GPSfix = gps_gga.fix;
   GPSsat = gps_gga.sat;
   GPSutc = gps_gga.utc;
}

// Syntax: val = readi2c(device, register);
void Creadi2c(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char i2c_device = (unsigned char)Param[0]->Val->Integer;
   unsigned char i2c_data[2];
   i2c_data[0] = (unsigned char)Param[1]->Val->Integer;
   i2cread(i2c_device, (unsigned char *)i2c_data, 1, SCCB_OFF);
   ReturnValue->Val->Integer = ((int)i2c_data[0]&0x000000FF);
}

// Syntax: two_byte_val = readi2c(device, register);
void Creadi2c2(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char i2c_device = (unsigned char)Param[0]->Val->Integer;
   unsigned char i2c_data[2];
   i2c_data[0] = (unsigned char)Param[1]->Val->Integer;
   i2cread(i2c_device, (unsigned char *)i2c_data, 2, SCCB_OFF);
   ReturnValue->Val->Integer = (((unsigned int)i2c_data[0] << 8) + i2c_data[1]);
}

// Syntax: writei2c(device, register, value);
void Cwritei2c(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char i2c_device = (unsigned char)Param[0]->Val->Integer;
   unsigned char i2c_data[2];
   i2c_data[0] = (unsigned char)Param[1]->Val->Integer;
   i2c_data[1] = (unsigned char)Param[2]->Val->Integer;
   i2cwrite(i2c_device, (unsigned char *)i2c_data, 1, SCCB_OFF);
}

// sin(angle).
void Csin(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer; // Input to function is angle in degrees.
   ReturnValue->Val->Integer = sin(ix);
}

// cos(angle).
void Ccos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer; // Input to function is angle in degrees.
   ReturnValue->Val->Integer = cos(ix);
}

// tan(angle).
void Ctan(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer; // Input to function is angle in degrees.
   ReturnValue->Val->Integer = tan(ix);
}

// asin(y, hyp).
void Casin(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int y = Param[0]->Val->Integer;
   int hyp = Param[1]->Val->Integer;
   if (y > hyp)
      ProgramFail(NULL, "asin():  opposite greater than hypotenuse");
   ReturnValue->Val->Integer = asin(y, hyp);
}

// acos(x, hyp).
void Cacos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int x = Param[0]->Val->Integer;
   int hyp = Param[1]->Val->Integer;
   if (x > hyp)
      ProgramFail(NULL, "acos():  adjacent greater than hypotenuse");
   ReturnValue->Val->Integer = acos(x, hyp);
}

// atan(y, x).
void Catan(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int y = Param[0]->Val->Integer;
   int x = Param[1]->Val->Integer;
   ReturnValue->Val->Integer = atan(y, x);
}

// gps_head(lat1, lon1, lat2, lon2).
void Cgps_head(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lat1 = Param[0]->Val->Integer;
   int lon1 = Param[1]->Val->Integer;
   int lat2 = Param[2]->Val->Integer;
   int lon2 = Param[3]->Val->Integer;
   ReturnValue->Val->Integer = gps_head(lat1, lon1, lat2, lon2);
}

// gps_dist(lat1, lon1, lat2, lon2).
void Cgps_dist(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int lat1 = Param[0]->Val->Integer;
   int lon1 = Param[1]->Val->Integer;
   int lat2 = Param[2]->Val->Integer;
   int lon2 = Param[3]->Val->Integer;
   ReturnValue->Val->Integer = gps_dist(lat1, lon1, lat2, lon2);
}

// sqrt(x).
void Csqrt(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int x = Param[0]->Val->Integer;
   ReturnValue->Val->Integer = isqrt(x);
}

void Cnnset(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   if (ix > NUM_NPATTERNS)
      ProgramFail(NULL, "nnset():  invalid index");
   for (int i1 = 0; i1 < 8; i1++)
      npattern[ix*8 + i1] = (unsigned char)Param[i1 + 1]->Val->Integer;
}

void Cnnshow(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   if (ix > NUM_NPATTERNS)
      ProgramFail(NULL, "nnshow():  invalid index");
   nndisplay(ix);
}

void Cnninit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   nninit_network();
}

void Cnntrain(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   nntrain_network(10000);
   for (int ix = 0; ix < NUM_NPATTERNS; ix++) {
      nnset_pattern(ix);
      nncalculate_network();
      for (int i1 = 0; i1 < NUM_OUTPUT; i1++)
         printf(" %3d", N_OUT(i1)/10);
      printf("\r\n");
   }
}

void Cnntest(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = 0;
   for (int i1 = 0; i1 < 8; i1++) {
      unsigned char ch = (unsigned char)Param[i1]->Val->Integer;
      for (int i2 = 0; i2 < 8; i2++) {
         N_IN(ix++) = ch&nmask[i2]? 1024: 0;
      }
   }
   nncalculate_network();
   ix = 0;
   int max = 0;
   for (int i1 = 0; i1 < NUM_OUTPUT; i1++) {
      NNVect[i1] = N_OUT(i1)/10;
      if (max < NNVect[i1]) {
         ix = i1;
         max = NNVect[i1];
      }
   }
   ReturnValue->Val->Integer = ix;
}

void Cnnmatchblob(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   if (ix > MAX_BLOBS)
      ProgramFail(NULL, "nnmatchblob():  invalid blob index");
   if (!blobcnt[ix])
      ProgramFail(NULL, "nnmatchblob():  not a valid blob");
// Use data still in blob_buf[] (FRAME_BUF3).
// Square the aspect ratio of x1, x2, y1, y2,
// then subsample blob pixels to populate N_IN(0:63) with 0:1024 values,
// then nncalculate_network() and display the N_OUT() results.
   nnscale8x8((unsigned char *)FRAME_BUF3, blobix[ix], blobx1[ix], blobx2[ix], bloby1[ix], bloby2[ix], imgWidth, imgHeight);
   nncalculate_network();
   ix = 0;
   int max = 0;
   for (int i1 = 0; i1 < NUM_OUTPUT; i1++) {
      NNVect[i1] = N_OUT(i1)/10;
      if (max < NNVect[i1]) {
         ix = i1;
         max = NNVect[i1];
      }
   }
   ReturnValue->Val->Integer = ix;
}

void Cnnlearnblob(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   if (ix > NUM_NPATTERNS)
      ProgramFail(NULL, "nnlearnblob():  invalid index");
   if (!blobcnt[0])
      ProgramFail(NULL, "nnlearnblob():  no blob to grab");
   nnscale8x8((unsigned char *)FRAME_BUF3, blobix[0], blobx1[0], blobx2[0], bloby1[0], bloby2[0], imgWidth, imgHeight);
   nnpack8x8(ix);
   nndisplay(ix);
}

void Cautorun(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ix = Param[0]->Val->Integer;
   int t0 = readRTC();
   while (readRTC() < t0 + ix*1000) { // Watch for ESC in 'ix' seconds.
      unsigned char ch;
      if (getchar(&ch)) {
         if (ch == 0x1B) { // If ESC found, exit picoC.
            printf("found ESC\r\n");
            ExitBuf[40] = 1;
            longjmp(ExitBuf, 1);
         }
      }
   }
}

void Clineno(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = Parser->Line;
}

void Cerrormsg(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   PlatformErrorPrefix(Parser);
   LibPrintf(Parser, ReturnValue, Param, NumArgs);
}

// Nothing here because we don't add any functions until srv1.h is #included.
struct LibraryFunction PlatformLibrary[] = {
   { NULL, NULL }
};

// List of all library functions included with srv1.h.
struct LibraryFunction SRV1Functions[] = {
   { Csignal, "int signal();" },
   { Cinput, "int input();" },
   { Cdelay, "void delay(int);" },
   { Crand, "int rand(int);" },
   { Ctime, "int time();" },
   { Ciodir, "void iodir(int);" },
   { Cioread, "int ioread();" },
   { Ciowrite, "void iowrite(int);" },
   { Cpeek, "int peek(int, int);" },
   { Cpoke, "void poke(int, int, int);" },
   { Cmotors, "void motors(int, int);" },
   { Cmotors2, "void motors2(int, int);" },
   { Cservos, "void servos(int, int);" },
   { Cservos2, "void servos2(int, int);" },
   { Cencoders, "void encoders();" },
   { Claser, "void laser(int);" },
   { Csonar, "int sonar(int);" },
   { Crange, "int range();" },
   { Cbattery, "int battery();" },
   { Cvcolor, "void vcolor(int, int, int, int, int, int, int);" },
   { Cvfind, "int vfind(int, int, int, int, int);" },
   { Cvcam, "void vcam(int);" },
   { Cvcap, "void vcap();" },
   { Cvrcap, "void vrcap();" },
   { Cvdiff, "void vdiff(int);" },
   { Cvpix, "void vpix(int, int);" },
   { Cvscan, "int vscan(int, int);" },
   { Cvmean, "void vmean();" },
   { Cvblob, "int vblob(int, int);" },
   { Cvjpeg, "int vjpeg(int);" },
   { Cvsend, "void vsend(int);" },
   { Ccompass, "int compass();" },
   { Canalog, "int analog(int);" },
   { Ctilt, "int tilt(int);" },
   { Cgps, "void gps();" },
   { Creadi2c, "int readi2c(int, int);" },
   { Creadi2c2, "int readi2c2(int, int);" },
   { Cwritei2c, "void writei2c(int, int, int);" },
   { Csin, "int sin(int);" },
   { Ccos, "int cos(int);" },
   { Ctan, "int tan(int);" },
   { Casin, "int asin(int, int);" },
   { Cacos, "int acos(int, int);" },
   { Catan, "int atan(int, int);" },
   { Cgps_head, "int gps_head(int, int, int, int);" },
   { Cgps_dist, "int gps_dist(int, int, int, int);" },
   { Csqrt, "int sqrt(int);" },
   { Cnnshow, "void nnshow(int);" },
   { Cnnset, "void nnset(int, int, int, int, int, int, int, int, int);" },
   { Cnninit, "void nninit();" },
   { Cnntrain, "void nntrain();" },
   { Cnntest, "int nntest(int, int, int, int, int, int, int, int);" },
   { Cnnmatchblob, "int nnmatchblob(int);" },
   { Cnnlearnblob, "void nnlearnblob(int);" },
   { Cautorun, "void autorun(int);" },
   { Clineno, "int lineno();" },
   { Cerrormsg, "void errormsg(char *);" },
   { NULL, NULL }
};

void PlatformLibraryInit(State pc) {
   IncludeRegister(pc, "srv1.h", &SRV1SetupFunc, SRV1Functions, NULL);
}
