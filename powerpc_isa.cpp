/**
 * @file      powerpc_isa.cpp
 * @author    Bruno Corsi dos Santos
 *
 *            The ArchC Team
 *            http://www.archc.org/
 *
 *            Computer Systems Laboratory (LSC)
 *            IC-UNICAMP
 *            http://www.lsc.ic.unicamp.br
 *
 * @version   1.0
 * @date      Mon, 19 Jun 2006 15:50:48 -0300
 * 
 * @brief     The ArchC POWERPC functional model.
 * 
 * @attention Copyright (C) 2002-2006 --- The ArchC Team
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

//IMPLEMENTATION NOTES:
// PowerPC 32 bits family.
// The PowerPC 405 instruction family are not implemented.
// Based on IBM and Xilinx manuals of PowerPC 405.
// mtspr and mfspr instructions not completely implemented.
// sc instruction not completely implemented and never used.

#include  "powerpc_isa.H"
#include  "powerpc_isa_init.cpp"
#include  "powerpc_bhv_macros.H"

//If you want debug information for this model, uncomment next line
//#define DEBUG_MODEL
#include  "ac_debug_model.H"

#define DEFAULT_STACK_SIZE (512 * 1024)
static int processors_started = 0;

using namespace powerpc_parms;

#ifdef SLEEP_AWAKE_MODE
/*********************************************************************************/
/* SLEEP / AWAKE mode control                                                    */
/* INTR_REG may store 1 (AWAKE MODE) or 0 (SLEEP MODE)                           */
/* if intr_reg == 0, the simulator will be suspended until it receives a         */   
/* interruption 1                                                                */    
/*********************************************************************************/
#define test_sleep() { if (intr_reg.read() == 0) ac_wait();  }
#else
#define test_sleep() {}
#endif

//Compute CR0 fields LT, GT, EQ, SO
//XER.SO must be updated by instruction before the use of this routine!
//Arguments:
//int result -> The result register
inline void CR0_update(ac_reg<ac_word> &CR, ac_reg<ac_word> &XER, unsigned int result) {

  /* LT field */
  if((result & 0x80000000) >> 31)
    CR.write(CR.read() | 0x80000000); /* 1 */
  else
    CR.write(CR.read() & 0x7FFFFFFF); /* 0 */

  /* GT field */
  if(((~result & 0x80000000) >> 31) && (result!=0))
    CR.write(CR.read() | 0x40000000); /* 1 */
  else
    CR.write(CR.read() & 0xBFFFFFFF); /* 0 */

  /* EQ field */
  if(result==0)
    CR.write(CR.read() | 0x20000000); /* 1 */
  else
    CR.write(CR.read() & 0xDFFFFFFF); /* 0 */

  /* SO field */
  if(XER.read() & 0x80000000)
    CR.write(CR.read() | 0x10000000); /* 1 */
  else
    CR.write(CR.read() & 0xEFFFFFFF); /* 0 */
}


//Compute XER overflow fields SO, OV
//Arguments:
//int result -> The result register
//int s1 -> Source 1
//int s2 -> Source 2
//int s3 -> Source 3 (if only two sources, use 0)
inline void add_XER_OV_SO_update(ac_reg<ac_word> &XER, int result,int s1,int s2,int s3) {

  long long int longresult =
    (long long int)(int)s1 + (long long int)(int)s2 + (long long int)(int)s3;

  if(longresult != (long long int)(int)result) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else {
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
  }
}


//Compute XER carry field CA
//Arguments:
//int result -> The result register
//int s1 -> Source 1
//int s2 -> Source 2
//int s3 -> Source 3 (if only two sources, use 0)
inline void add_XER_CA_update(ac_reg<ac_word> &XER, int result,int s1,int s2,int s3) {

  unsigned long long int longresult =
    (unsigned long long int)(unsigned int)s1 + 
    (unsigned long long int)(unsigned int)s2 + 
    (unsigned long long int)(unsigned int)s3;
    
  if(longresult > 0xFFFFFFFF)
    XER.write(XER.read() | 0x20000000); /* Write 1 to bit 2 CA */
  else 
    XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */
  
} 

//Compute XER overflow fields SO, OV
//Arguments:
//int result -> The result register
//int s1 -> Source 1
//int s2 -> Source 2
inline void divws_XER_OV_SO_update(ac_reg<ac_word> &XER, int result,int s1,int s2) {

  long long int longresult =
    (long long int)(int)s1 / (long long int)(int)s2;

  if(longresult != (long long int)(int)result) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */    
 
}


//Compute XER overflow fields SO, OV
//Arguments:
//int result -> The result register
//int s1 -> Source 1
//int s2 -> Source 2
inline void divwu_XER_OV_SO_update(ac_reg<ac_word> &XER, int result,int s1,int s2) {

  unsigned long long int longresult =
    (unsigned long long int)(unsigned int)s1 / 
    (unsigned long long int)(unsigned int)s2;

  if(longresult != (unsigned long long int)(unsigned int)result) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
 
}


//Function to do_branch
inline void do_Branch(ac_reg<ac_word> &ac_pc, ac_reg<ac_word> &LR, signed int ili,unsigned int iaa,unsigned int ilk) {
  
  int displacement;
  unsigned int nia;

  ac_pc-=4; /* Because pre-increment */

  if(iaa==1) {
    displacement=ili<<2;
    nia=displacement;
  }
  else { /* iaa=0 */
    displacement=ili<<2;
    nia=ac_pc+displacement;
  }

  if(ilk==1)
    LR.write(ac_pc+4);

  ac_pc=nia;
  
}

//Function to do conditional branch
inline void do_Branch_Cond(ac_reg<ac_word> &ac_pc, ac_reg<ac_word> &LR, ac_reg<ac_word> &CR, ac_reg<ac_word> &CTR,  unsigned int ibo,unsigned int ibi,
		    signed int ibd,unsigned int iaa,
		    unsigned int ilk) {
  
  int displacement;
  unsigned int nia;
  unsigned int masc;

  masc=0x80000000;
  masc=masc>>ibi;

  ac_pc-=4; /* Because pre-increment */

  if((ibo & 0x04) == 0x00) {
    CTR.write(CTR.read()-1);
  }

  if(((ibo & 0x04) || /* Branch */
      ((CTR.read()==0) && (ibo & 0x02)) || 
      (!(CTR.read()==0) && !(ibo & 0x02)))
     && 
     ((ibo & 0x10) ||
      (((CR.read() & masc) && (ibo & 0x08)) ||
       (!(CR.read() & masc) && !(ibo & 0x08))))) {

    if(iaa == 1) {
      displacement=ibd<<2;
      nia=displacement;
    }
    else {
      displacement=ibd<<2;
      nia=ac_pc+displacement;
    }
  }
  else { /* No branch */
    nia=ac_pc+4;
  }

  if(ilk==1)
    LR.write(ac_pc+4);
  
  ac_pc=nia;
  
}

//Function to do conditional branch to count register
inline void do_Branch_Cond_Count_Reg(ac_reg<ac_word> &ac_pc, ac_reg<ac_word> &LR, ac_reg<ac_word> &CR, ac_reg<ac_word> &CTR, unsigned int ibo, unsigned int ibi,
			      unsigned int ilk) {

  unsigned int nia;
  unsigned int masc;

  masc=0x80000000;
  masc=masc>>ibi;

  ac_pc-=4; /* Because pre-increment */
  
  if((ibo & 0x04) == 0x00)
    CTR.write(CTR.read()-1);
  
  if(((ibo & 0x04) || /* Branch */
      ((CTR.read()==0) && (ibo & 0x02)) || 
      (!(CTR.read()==0) && !(ibo & 0x02)))
     && 
     ((ibo & 0x10) ||
      (((CR.read() & masc) && (ibo & 0x08)) ||
       (!(CR.read() & masc) && !(ibo & 0x08))))) {
    
    nia=CTR.read() & 0xFFFFFFFC;

  }
  else { /* No Branch */
    nia=ac_pc+4;
  }

  if(ilk==1)
    LR.write(ac_pc+4);
  
  ac_pc=nia;

}

//Function to do conditional branch to link register
inline void do_Branch_Cond_Link_Reg(ac_reg<ac_word> &ac_pc, ac_reg<ac_word> &LR, ac_reg<ac_word> &CR, ac_reg<ac_word> &CTR,unsigned int ibo,unsigned int ibi,
			     unsigned int ilk) {
  
  unsigned int nia;
  unsigned int masc;

  masc=0x80000000;
  masc=masc>>ibi;

  ac_pc-=4; /* Because pre-increment */
  
  if((ibo & 0x04) == 0x00)
    CTR.write(CTR.read()-1);
  
  if(((ibo & 0x04) || /* Branch */
      ((CTR.read()==0) && (ibo & 0x02)) || 
      (!(CTR.read()==0) && !(ibo & 0x02)))
     && 
     ((ibo & 0x10) ||
      (((CR.read() & masc) && (ibo & 0x08)) ||
       (!(CR.read() & masc) && !(ibo & 0x08))))) {
    
    nia=LR.read() & 0xFFFFFFFC;

  }
  else { /* No Branch */
    nia=ac_pc+4;
  }

  if(ilk==1)
    LR.write(ac_pc+4);
  
  ac_pc=nia;

}

#ifdef AC_COMPSIM
//Function to test if conditional branch is taken
int test_Branch_Cond(ac_reg<ac_word> &CR, ac_reg<ac_word> &CTR, unsigned int ibo, unsigned int ibi) {

  unsigned int masc;
  unsigned int test;
 
  masc=0x80000000;
  masc=masc>>ibi;

  if((ibo & 0x04) == 0x00)
    CTR.write(CTR.read()-1);
  
  if(((ibo & 0x04) || /* Branch */
      ((CTR.read()==0) && (ibo & 0x02)) || 
     (!(CTR.read()==0) && !(ibo & 0x02)))
     &&   
     ((ibo & 0x10) ||
      (((CR.read() & masc) && (ibo & 0x08)) ||
       (!(CR.read() & masc) && !(ibo & 0x08)))))
    return 1;

  else  /* No Branch */
    return 0;
}
#endif

//Ceil function
inline int ceil(int value, int divisor) {
  int res;
  if ((value % divisor)!=0)
   res=int(value/divisor)+1;
  else res=value/divisor;
  return res;
}

//Rotl function
inline unsigned int rotl(unsigned int reg,unsigned int n) {
  unsigned int tmp1=reg;
  unsigned int tmp2=reg;
  unsigned int rotated=(tmp1 << n) | (tmp2 >> 32-n);
 
  return(rotated);
}

//Mask32rlw function
inline unsigned int mask32rlw(unsigned int i,unsigned int f) {

  unsigned int maski,maskf;

  if(i<=f) {
    maski=(0xFFFFFFFF>>i);
    maskf=(0xFFFFFFFF<<(31-f));
    return(maski & maskf);
  }
  else {
    maski=(0xFFFFFFFF>>f);
    maski=maski>>1;
    maskf=(0xFFFFFFFF<<(31-i));
    maskf=maskf<<1;
    return(~(maski & maskf));
  }
  
}


//Function dump General Purpose Registers
inline void dumpGPR(ac_regbank<32, ac_word, ac_Dword> &GPR) {
  int i;
  for(i=0 ; i<32 ; i++)
    dbg_printf("r%d -> %#x \n",i,GPR.read(i));
}

//Function that returns the value of XER TBC
inline unsigned int XER_TBC_read(ac_reg<ac_word> &XER) {
  return(XER.read() & 0x0000007F);
}


//Function that returns the value of XER OV
inline unsigned int XER_OV_read(ac_reg<ac_word> &XER) {
  if(XER.read() & 0x40000000)
    return 1;
  else
    return 0;
}

//Function that returns the value of XER SO
inline unsigned int XER_SO_read(ac_reg<ac_word> &XER) {
  if(XER.read() & 0x80000000)
    return 1;
  else
    return 0;
}

//Function that returns the value of XER CA
inline unsigned int XER_CA_read(ac_reg<ac_word> &XER) {
  if(XER.read() & 0x20000000)
    return 1;
  else
    return 0;
}


//Function dump various registers
inline void dumpREG(ac_reg<ac_word> &XER, ac_reg<ac_word> &CR, ac_reg<ac_word> &LR, ac_reg<ac_word> &CTR) {
  dbg_printf("XER.OV = %d\n",XER_OV_read(XER));
  dbg_printf("XER.SO = %d\n",XER_SO_read(XER));
  dbg_printf("XER.CA = %d\n",XER_CA_read(XER));
  dbg_printf("CR = %#x\n",CR.read());
  dbg_printf("LR = %#x\n",LR.read());
  dbg_printf("CTR = %#x\n",CTR.read());
}



//!Generic instruction behavior method.
void ac_behavior( instruction )
{

  test_sleep();

  dbg_printf("\n program counter=%#x\n",(int)ac_pc);
  ac_pc+=4;
  //dumpGPR();
  //dumpREG();
}

//!Generic begin behavior method.
void ac_behavior( begin )
{
  dbg_printf("Starting simulator...\n");
  
  /* Here the stack is started in a */
  // GPR.write(1,AC_RAM_END - 1024);
  GPR.write(1, AC_RAM_END - 1024 - processors_started++ * DEFAULT_STACK_SIZE);
  /* Make a jump out of DC_portory if it doesn't have an abi */
  LR.write(0xFFFFFFFF);
  
}

void ac_behavior(end)
{
  dbg_printf("@@@ end behavior @@@\n");
}

//! Instruction Format behavior methods.
void ac_behavior( I1 ){}
void ac_behavior( B1 ){}
void ac_behavior( SC1 ){}
void ac_behavior( D1 ){}
void ac_behavior( D2 ){}
void ac_behavior( D3 ){}
void ac_behavior( D4 ){}
void ac_behavior( D5 ){}
void ac_behavior( D6 ){}
void ac_behavior( D7 ){}
void ac_behavior( X1 ){}
void ac_behavior( X2 ){}
void ac_behavior( X3 ){}
void ac_behavior( X4 ){}
void ac_behavior( X5 ){}
void ac_behavior( X6 ){}
void ac_behavior( X7 ){}
void ac_behavior( X8 ){}
void ac_behavior( X9 ){}
void ac_behavior( X10 ){}
void ac_behavior( X11 ){}
void ac_behavior( X12 ){}
void ac_behavior( X13 ){}
void ac_behavior( X14 ){}
void ac_behavior( X15 ){}
void ac_behavior( X16 ){}
void ac_behavior( X17 ){}
void ac_behavior( X18 ){}
void ac_behavior( X19 ){}
void ac_behavior( X20 ){}
void ac_behavior( X21 ){}
void ac_behavior( X22 ){}
void ac_behavior( X23 ){}
void ac_behavior( X24 ){}
void ac_behavior( X25 ){}
void ac_behavior( XL1 ){}
void ac_behavior( XL2 ){}
void ac_behavior( XL3 ){}
void ac_behavior( XL4 ){}
void ac_behavior( XFX1 ){}
void ac_behavior( XFX2 ){}
void ac_behavior( XFX3 ){}
void ac_behavior( XFX4 ){}
void ac_behavior( XFX5 ){}
void ac_behavior( XO1 ){}
void ac_behavior( XO2 ){}
void ac_behavior( XO3 ){}
void ac_behavior( M1 ){}
void ac_behavior( M2 ){}


//!Instruction add behavior method.
void ac_behavior( add )
{
  dbg_printf(" add r%d, r%d, r%d\n\n",rt,ra,rb);
  GPR.write(rt,GPR.read(ra) + GPR.read(rb));

};

//!Instruction add_ behavior method.
void ac_behavior( add_ )
{
  dbg_printf(" add. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);
  
  CR0_update(CR, XER, result);
  GPR.write(rt,result);

};

//!Instruction addo behavior method.
void ac_behavior( addo )
{
  dbg_printf(" addo r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);

  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),0);

  GPR.write(rt,result);

};

//!Instruction addo_ behavior method.
void ac_behavior( addo_ )
{
  dbg_printf(" addo. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),0);
  CR0_update(CR, XER, result);
  GPR.write(rt,result);

};

//!Instruction addc behavior method.
void ac_behavior( addc )
{
  dbg_printf(" addc r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);

  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),0);
  GPR.write(rt,result);

};

//!Instruction addc_ behavior method.
void ac_behavior( addc_ )
{
  dbg_printf(" addc. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);
  
  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),0);

  CR0_update(CR, XER, result);

};

//!Instruction addco behavior method.
void ac_behavior( addco )
{
  dbg_printf(" addco r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);

  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),0);

  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),0);

  GPR.write(rt,result);

};

//!Instruction addco_ behavior method.
void ac_behavior( addco_ )
{
  dbg_printf(" addco. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=GPR.read(ra) + GPR.read(rb);

  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),0);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),0);
  CR0_update(CR, XER, result);
  
  GPR.write(rt,result);

};

//!Instruction adde behavior method.
void ac_behavior( adde )
{
  dbg_printf(" adde r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);

  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  GPR.write(rt,result);

};

//!Instruction adde_ behavior method.
void ac_behavior( adde_ )
{
  dbg_printf(" adde. r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addeo behavior method.
void ac_behavior( addeo )
{
  dbg_printf(" addeo r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
 
  add_XER_CA_update(XER, result,GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  GPR.write(rt,result);
};

//!Instruction addeo_ behavior method.
void ac_behavior( addeo_ )
{
  dbg_printf(" addeo. r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
  
  add_XER_CA_update(XER, GPR.read(rt),GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb),XER_CA_read(XER));
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addi behavior method.
void ac_behavior( addi )
{
  dbg_printf(" addi r%d, r%d, %d\n\n",rt,ra,d);

  int ime32=d;
  
  if(ra == 0)
    GPR.write(rt,ime32);
  else
    GPR.write(rt,GPR.read(ra)+ime32);
  
};

//!Instruction addic behavior method.
void ac_behavior( addic )
{
  dbg_printf(" addic r%d, r%d, %d\n\n",rt,ra,d);
  int ime32=d;
  int result=GPR.read(ra)+ime32;

  add_XER_CA_update(XER, result,GPR.read(ra),ime32,0);

  GPR.write(rt,result);
};

//!Instruction addic_ behavior method.
void ac_behavior( addic_ )
{
  dbg_printf(" addic. r%d, r%d, %d\n\n",rt,ra,d);
  /* Do not have rc field and update CR0 */
  int ime32=d;
  int result=GPR.read(ra)+ime32;

  add_XER_CA_update(XER, result,GPR.read(ra),ime32,0);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addis behavior method.
void ac_behavior( addis )
{
  dbg_printf(" addis r%d, r%d, %d\n\n",rt,ra,d);
  int ime32=d;
  ime32=ime32<<16;

  if(ra == 0) 
    GPR.write(rt,ime32);
  else
    GPR.write(rt,GPR.read(ra)+ime32);
    
};

//!Instruction addme behavior method.
void ac_behavior( addme )
{
  dbg_printf(" addme r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);

  GPR.write(rt,result);
};

//!Instruction addme_ behavior method.
void ac_behavior( addme_ )
{
  dbg_printf(" addme. r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addmeo behavior method.
void ac_behavior( addmeo )
{
  dbg_printf(" addmeo r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);

  add_XER_OV_SO_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);
 
  GPR.write(rt,result);
};

//!Instruction addmeo_ behavior method.
void ac_behavior( addmeo_ )
{
  dbg_printf(" addmeo. r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,GPR.read(ra),XER_CA_read(XER),-1);
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addze behavior method.
void ac_behavior( addze )
{
  dbg_printf(" addze r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);

  GPR.write(rt,result);
};

//!Instruction addze_ behavior method.
void ac_behavior( addze_ )
{
  dbg_printf(" addze. %d, %d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction addzeo behavior method.
void ac_behavior( addzeo )
{
  dbg_printf(" addzeo r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);

  add_XER_OV_SO_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);  

  GPR.write(rt,result);
};

//!Instruction addzeo_ behavior method.
void ac_behavior( addzeo_ )
{
  dbg_printf(" addzeo. r%d, r%d\n\n",rt,ra);
  int result=GPR.read(ra)+XER_CA_read(XER);
    
  add_XER_CA_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,GPR.read(ra),XER_CA_read(XER),0);  
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction ande behavior method.
void ac_behavior( ande )
{
  dbg_printf(" and r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,GPR.read(rs) & GPR.read(rb));

};

//!Instruction ande_ behavior method.
void ac_behavior( ande_ )
{
  dbg_printf(" and. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=GPR.read(rs) & GPR.read(rb);

  CR0_update(CR, XER, result);

  GPR.write(ra,result);
};

//!Instruction andc behavior method.
void ac_behavior( andc )
{
  dbg_printf(" andc r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,GPR.read(rs) & ~GPR.read(rb));

};

//!Instruction ande_ behavior method.
void ac_behavior( andc_ )
{
  dbg_printf(" andc. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=GPR.read(rs) & ~GPR.read(rb);

  CR0_update(CR, XER, result);

  GPR.write(ra,result);
};

//!Instruction andi_ behavior method.
void ac_behavior( andi_ )
{
  dbg_printf(" andi. r%d, r%d, %d\n\n",ra,rs,ui);
  unsigned int ime32=(unsigned short int)ui;
  int result=GPR.read(rs) & ime32;

  CR0_update(CR, XER, result);

  GPR.write(ra,result);
};

//!Instruction andis_ behavior method.
void ac_behavior( andis_ )
{
  dbg_printf(" andis. r%d, r%d, %d\n\n",ra,rs,ui);
  unsigned int ime32=(unsigned short int)ui;

  ime32=ime32<<16;
  int result=GPR.read(rs) & ime32;

  CR0_update(CR, XER, result);

  GPR.write(ra,result);
};

//!Instruction b behavior method.
void ac_behavior( b )
{
  dbg_printf(" b %d\n\n",li);
  do_Branch(ac_pc, LR, li,aa,lk);

};

//!Instruction ba behavior method.
void ac_behavior( ba )
{
  dbg_printf(" ba %d\n\n",li);
  do_Branch(ac_pc, LR, li,aa,lk);

};

//!Instruction bl behavior method.
void ac_behavior( bl )
{
  dbg_printf(" bl %d\n\n",li);
  do_Branch(ac_pc, LR, li,aa,lk);
  
};

//!Instruction bla behavior method.
void ac_behavior( bla )
{
  dbg_printf(" bla %d\n\n",li);
  do_Branch(ac_pc, LR, li,aa,lk);

};

//!Instruction bc behavior method.
void ac_behavior( bc )
{
  dbg_printf(" bc %d, %d, %d\n\n",bo,bi,bd);
  do_Branch_Cond(ac_pc, LR, CR, CTR, bo,bi,bd,aa,lk);

};

//!Instruction bca behavior method.
void ac_behavior( bca )
{
  dbg_printf(" bca %d, %d, %d\n\n",bo,bi,bd);
  do_Branch_Cond(ac_pc, LR, CR, CTR, bo,bi,bd,aa,lk);

};

//!Instruction bcl behavior method.
void ac_behavior( bcl )
{
  dbg_printf(" bcl %d, %d, %d\n\n",bo,bi,bd);
  do_Branch_Cond(ac_pc, LR, CR, CTR, bo,bi,bd,aa,lk);
  
};

//!Instruction bcla behavior method.
void ac_behavior( bcla )
{
  dbg_printf(" bcla %d, %d, %d\n\n",bo,bi,bd);
  do_Branch_Cond(ac_pc, LR, CR, CTR, bo,bi,bd,aa,lk);

};

//!Instruction bcctr behavior method.
void ac_behavior( bcctr )
{
  dbg_printf(" bcctr %d, %d\n\n",bo,bi);
  do_Branch_Cond_Count_Reg(ac_pc, LR, CR, CTR,bo,bi,lk);

};

//!Instruction bcctrl behavior method.
void ac_behavior( bcctrl )
{
  dbg_printf(" bcctrl %d, %d\n\n",bo,bi);
  do_Branch_Cond_Count_Reg(ac_pc, LR, CR, CTR,bo,bi,lk);

};

//!Instruction bclr behavior method.
void ac_behavior( bclr )
{
  dbg_printf(" bclr %d, %d\n\n",bo,bi);
  do_Branch_Cond_Link_Reg(ac_pc, LR, CR, CTR,bo,bi,lk);

};

//!Instruction bclrl behavior method.
void ac_behavior( bclrl )
{
  dbg_printf(" bclrl %d, %d\n\n",bo,bi);
  do_Branch_Cond_Link_Reg(ac_pc, LR, CR, CTR,bo,bi,lk);

};

//!Instruction cmp behavior method.
void ac_behavior( cmp )
{
  dbg_printf(" cmp crf%d, 0, r%d, r%d\n\n",bf,ra,rb);
  unsigned int c=0x00;
  unsigned int n=bf;
  unsigned int masc=0xF0000000;
  
  if((int)GPR.read(ra) < (int)GPR.read(rb)) 
    c = c | 0x80000000;
  if((int)GPR.read(ra) > (int)GPR.read(rb))
    c = c | 0x40000000;
  if((int)GPR.read(ra) == (int)GPR.read(rb))
    c = c | 0x20000000;
  if(XER_SO_read(XER)==1)
    c = c | 0x10000000;
   
  c = c >> (n*4);
  masc = ~(masc >> (n*4));
    
  CR.write((CR.read() & masc) | c);
  
};

//!Instruction cmpi behavior method.
void ac_behavior( cmpi )
{
  dbg_printf(" cmpi crf%d, 0, r%d, %d\n\n",bf,ra,si);
  unsigned int c=0x00;
  unsigned int n=bf;
  unsigned int masc=0xF0000000;

  int ime32=(short int)(si);
 
  if((int)GPR.read(ra) < ime32) 
    c = c | 0x80000000;
  if((int)GPR.read(ra) > ime32)
    c = c | 0x40000000;
  if((int)GPR.read(ra) == ime32)
    c = c | 0x20000000;
  if(XER_SO_read(XER)==1)
    c = c | 0x10000000;
   
  c = c >> (n*4);
  masc = ~(masc >> (n*4));
    
  CR.write((CR.read() & masc) | c);

};

//!Instruction cmpl behavior method.
void ac_behavior( cmpl )
{
  dbg_printf(" cmpl crf%d, 0, r%d, r%d\n\n",bf,ra,rb);
  unsigned int c=0x00;
  unsigned int n=bf;
  unsigned int masc=0xF0000000;

  unsigned int uintra=GPR.read(ra);
  unsigned int uintrb=GPR.read(rb);
  
  if(uintra < uintrb) 
    c = c | 0x80000000;
  if(uintra > uintrb)
    c = c | 0x40000000;
  if(uintra == uintrb)
    c = c | 0x20000000;
  if(XER_SO_read(XER)==1)
    c = c | 0x10000000;
   
  c = c >> (n*4);
  masc = ~(masc >> (n*4));
    
  CR.write((CR.read() & masc) | c);

};

//!Instruction cmpli behavior method.
void ac_behavior( cmpli )
{
  dbg_printf(" cmpli crf%d, 0, r%d, %d\n\n",bf,ra,ui);
  unsigned int c=0x00;
  unsigned int n=bf;
  unsigned int masc=0xF0000000;

  unsigned int ime32=(unsigned short int)(ui);
 
  if(GPR.read(ra) < ime32) 
    c = c | 0x80000000;
  if(GPR.read(ra) > ime32)
    c = c | 0x40000000;
  if(GPR.read(ra) == ime32)
    c = c | 0x20000000;
  if(XER_SO_read(XER)==1)
    c = c | 0x10000000;
   
  c = c >> (n*4);
  masc = ~(masc >> (n*4));
    
  CR.write((CR.read() & masc) | c);

};

//!Instruction cntlzw behavior method.
void ac_behavior( cntlzw )
{
  dbg_printf(" cntlzw r%d, r%d\n\n",ra,rs);
  
  unsigned int urs=GPR.read(rs);
  unsigned int masc=0x80000000;
  unsigned int n=0;

  while(n < 32) {
    if(urs & masc)
      break;
    n++;
    masc=masc>>1;
  }
  
  GPR.write(ra,n);

};

//!Instruction cntlzw_ behavior method.
void ac_behavior( cntlzw_ )
{
  dbg_printf(" cntlzw. r%d, r%d\n\n",ra,rs);

  unsigned int urs=GPR.read(rs);
  unsigned int masc=0x80000000;
  unsigned int n=0;

  while(n < 32) {
    if(urs & masc)
      break;
    n++;
    masc=masc>>1;
  }
  
  GPR.write(ra,n);
  CR0_update(CR, XER, n); 

};

//!Instruction crand behavior method.
void ac_behavior( crand )
{
  dbg_printf(" crand %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=(CRba & CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction crandc behavior method.
void ac_behavior( crandc )
{
  dbg_printf(" crandc %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=(CRba & ~CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction creqv behavior method.
void ac_behavior( creqv )
{
  dbg_printf(" creqv %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=~(CRba ^ CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction crnand behavior method.
void ac_behavior( crnand )
{
  dbg_printf(" crnand %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=~(CRba & CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction crnor behavior method.
void ac_behavior( crnor )
{
  dbg_printf(" crnor %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=~(CRba | CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction cror behavior method.
void ac_behavior( cror )
{
  dbg_printf(" cror %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=(CRba | CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction crorc behavior method.
void ac_behavior( crorc )
{
  dbg_printf(" crorc %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=(CRba | ~CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction crxor behavior method.
void ac_behavior( crxor )
{
  dbg_printf(" crxor %d, %d, %d\n\n",bt,ba,bb);
  
  unsigned int CRbt;
  unsigned int CRba;
  unsigned int CRbb;

  CRba=CR.read();
  CRbb=CR.read();

  /* Shift source bit to first position and zero another */
  CRba=(CRba<<ba)&0x80000000;
  CRbb=(CRbb<<bb)&0x80000000;

  CRbt=(CRba ^ CRbb) & 0x80000000;
  CRbt=CRbt>>bt;

  if(CRbt)
    CR.write(CR.read() | CRbt);
  else
    CR.write(CR.read() & ~(0x80000000 >> bt));    

};

//!Instruction divw behavior method.
void ac_behavior( divw )
{
  dbg_printf(" divw r%d, r%d, r%d\n\n",rt,ra,rb);

  GPR.write(rt,(int)GPR.read(ra)/(int)GPR.read(rb));

};

//!Instruction divw_ behavior method.
void ac_behavior( divw_ )
{
  dbg_printf(" divw. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=(int)GPR.read(ra)/(int)GPR.read(rb);
  
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction divwo behavior method.
void ac_behavior( divwo )
{
  dbg_printf(" divwo r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=(int)GPR.read(ra)/(int)GPR.read(rb);  

  divws_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb));
  
  GPR.write(rt,result);
};

//!Instruction divwo_ behavior method.
void ac_behavior( divwo_ )
{
  dbg_printf(" divwo_ r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=(int)GPR.read(ra)/(int)GPR.read(rb);  
  
  /* Note: XER_OV_SO_update before CR0_update */
  divws_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb));
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction divw behavior method.
void ac_behavior( divwu )
{
  dbg_printf(" divwu r%d, r%d, r%d\n\n",rt,ra,rb);

  GPR.write(rt,(unsigned int)GPR.read(ra)/(unsigned int)GPR.read(rb));

};

//!Instruction divwu_ behavior method.
void ac_behavior( divwu_ )
{
  dbg_printf(" divwu. r%d, r%d, r%d\n\n",rt,ra,rb);
  
  unsigned int result=(unsigned int)GPR.read(ra)/(unsigned int)GPR.read(rb);
  
  CR0_update(CR, XER, result);
  
  GPR.write(rt,result);
};

//!Instruction divwou behavior method.
void ac_behavior( divwou )
{
  dbg_printf(" divwou r%d, r%d, r%d\n\n",rt,ra,rb);
  
  unsigned int result=(unsigned int)GPR.read(ra)/(unsigned int)GPR.read(rb);  
  
  divwu_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb));

  GPR.write(rt,result);
};

//!Instruction divwou_ behavior method.
void ac_behavior( divwou_ )
{
  dbg_printf(" divwou_ r%d, r%d, r%d\n\n",rt,ra,rb);

  unsigned int result=(unsigned int)GPR.read(ra)/(unsigned int)GPR.read(rb);  

  /* Note: XER_OV_SO_update before CR0_update */
  divwu_XER_OV_SO_update(XER, result,GPR.read(ra),GPR.read(rb));
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction eqv behavior method.
void ac_behavior( eqv )
{
  dbg_printf(" eqv r%d, r%d, r%d\n\n",ra,rs,rb);

  GPR.write(ra,~(GPR.read(rs)^GPR.read(rb)));
};

//!Instruction eqv_ behavior method.
void ac_behavior( eqv_ )
{
  dbg_printf(" eqv. r%d, r%d, r%d\n\n",ra,rs,rb);

  GPR.write(ra,~(GPR.read(rs)^GPR.read(rb)));

  CR0_update(CR, XER, GPR.read(ra));
};

//!Instruction extsb behavior method.
void ac_behavior( extsb )
{
  dbg_printf(" extsb r%d, r%d\n\n",ra,rs);

  GPR.write(ra,(char)(GPR.read(rs)));

};

//!Instruction extsb_ behavior method.
void ac_behavior( extsb_ )
{
  dbg_printf(" extsb. r%d, r%d\n\n",ra,rs);

  GPR.write(ra,(char)(GPR.read(rs)));

  CR0_update(CR, XER, GPR.read(ra));
};

//!Instruction extsh behavior method.
void ac_behavior( extsh )
{
  dbg_printf(" extsh r%d, r%d\n\n",ra,rs);

  GPR.write(ra,(short int)(GPR.read(rs)));

};

//!Instruction extsh_ behavior method.
void ac_behavior( extsh_ )
{
  dbg_printf(" extsh. r%d, r%d\n\n",ra,rs);

  GPR.write(ra,(short int)(GPR.read(rs)));

  CR0_update(CR, XER, GPR.read(ra));

};


//!Instruction lbz behavior method.
void ac_behavior( lbz )
{
  dbg_printf(" lbz r%d, %d(r%d)\n\n",rt,d,ra);

  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;
 
  GPR.write(rt,(unsigned int)DATA_PORT->read_byte(ea));
  
};

//!Instruction lbzu behavior method.
void ac_behavior( lbzu )
{
  dbg_printf(" lbzu r%d, %d(r%d)\n\n",rt,d,ra);

  int ea;
  
  ea=GPR.read(ra)+(short int)d;
  
  GPR.write(ra,ea);
  GPR.write(rt,(unsigned int)DATA_PORT->read_byte(ea));
  
};

//!Instruction lbzux behavior method.
void ac_behavior( lbzux )
{
  dbg_printf(" lbzux r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;
  
  ea=GPR.read(ra)+GPR.read(rb);
  
  GPR.write(ra,ea);
  GPR.write(rt,(unsigned int)DATA_PORT->read_byte(ea));
  
};

//!Instruction lbzx behavior method.
void ac_behavior( lbzx )
{
  dbg_printf(" lbzx r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,(unsigned int)DATA_PORT->read_byte(ea));
  
};

//!Instruction lha behavior method.
void ac_behavior( lha )
{
  dbg_printf(" lha r%d, %d(r%d)\n\n",rt,d,ra);

  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;
 
  GPR.write(rt,(short int)DATA_PORT->read_half(ea));
  
};

//!Instruction lhau behavior method.
void ac_behavior( lhau )
{
  dbg_printf(" lhau r%d, %d(r%d)\n\n",rt,d,ra);

  int ea=GPR.read(ra)+(short int)d;

  GPR.write(ra,ea);
  GPR.write(rt,(short int)DATA_PORT->read_half(ea));
  
};

//!Instruction lhaux behavior method.
void ac_behavior( lhaux )
{
  dbg_printf(" lhaux r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea=GPR.read(ra)+GPR.read(rb);

  GPR.write(ra,ea);
  GPR.write(rt,(short int)DATA_PORT->read_half(ea));
  
};

//!Instruction lhax behavior method.
void ac_behavior( lhax )
{
  dbg_printf(" lhax r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,(short int)DATA_PORT->read_half(ea));
  
};

//!Instruction lhbrx behavior method.
void ac_behavior( lhbrx )
{
  dbg_printf(" lhbrx r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,(((int)(DATA_PORT->read_byte(ea+1)) & 0x000000FF)<<8) | ((int)(DATA_PORT->read_byte(ea)) & 0x000000FF));

};

//!Instruction lhz behavior method.
void ac_behavior( lhz )
{
  dbg_printf(" lhz r%d, %d(r%d)\n\n",rt,d,ra);
  
  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;

  GPR.write(rt,(unsigned short int)DATA_PORT->read_half(ea));

};

//!Instruction lhzu behavior method.
void ac_behavior( lhzu )
{
  dbg_printf(" lhzu r%d, %d(%d)\n\n",rt,d,ra);
  
  int ea=GPR.read(ra)+(short int)d;
  
  GPR.write(ra,ea);
  GPR.write(rt,(unsigned short int)DATA_PORT->read_half(ea));

};

//!Instruction lhzux behavior method.
void ac_behavior( lhzux )
{
  dbg_printf(" lhzux r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int ea=GPR.read(ra)+GPR.read(rb);

  GPR.write(ra,ea);
  GPR.write(rt,(unsigned short int)DATA_PORT->read_half(ea));

};

//!Instruction lhzx behavior method.
void ac_behavior( lhzx )
{
  dbg_printf(" lhzx r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,(unsigned short int)DATA_PORT->read_half(ea));
  
};

//!Instruction lmw behavior method.
void ac_behavior( lmw )
{
  dbg_printf(" lmw r%d, %d(r%d)\n\n",rt,d,ra);

  int ea;
  unsigned int r;

  if(ra !=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;

  r=rt;

  while(r<=31) {
    if((r!=ra)||(r==31))
      GPR.write(r,DATA_PORT->read(ea));
    r=r+1;
    ea=ea+4;
  }

};

//!Instruction lswi behavior method.
void ac_behavior( lswi )
{
  dbg_printf(" lswi r%d, r%d, %d\n\n",rt,ra,nb);

  int ea;
  unsigned int cnt,n;
  unsigned int rfinal,r;
  unsigned int i,masc;

  if(ra!=0)
    ea=GPR.read(ra);
  else
    ea=0;
  
  if(nb==0)
    cnt=32;
  else
    cnt=nb;

  n=cnt;
  rfinal=((rt + ceil(cnt,4) - 1) % 32);
  r=rt-1;
  i=0;

  while(n>0) {
    if(i==0) {
      r=r+1;
      if(r==32)
	r=0;
      if((r!=ra)||(r==rfinal))
	GPR.write(r,0);
    }
    if((r!=ra)||(r==rfinal)) {
      masc=0xFF000000>>i;
      masc=~masc;
      GPR.write(r,(GPR.read(r) & masc));
      GPR.write(r,(((unsigned int)DATA_PORT->read_byte(ea)) << (24-i)) | GPR.read(r));
    }
    i=i+8;
    if(i==32)
      i=0;
    ea=ea+1;
    n=n-1;
  }
  
};

//!Instruction lswx behavior method.
void ac_behavior( lswx )
{
  dbg_printf(" lswx r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;
  unsigned int cnt,n;
  unsigned int rfinal,r;
  unsigned int i,masc;

  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  cnt=XER_TBC_read(XER);
  n=cnt;
  rfinal=((rt + ceil(cnt,4) - 1) % 32);
  r=rt-1;
  i=0;

  while(n>0) {
    if(i==0) {
      r=r+1;
      if(r==32)
	r=0;
      if(((r!=ra) && (r!=rb)) || (r==rfinal))
	GPR.write(r,0);
    }
    if(((r!=ra) && (r!=rb)) || (r==rfinal)) {
      masc=0xFF000000>>i;
      masc=~masc;
      GPR.write(r,(GPR.read(r) & masc));
      GPR.write(r,(((unsigned int)DATA_PORT->read_byte(ea)) << (24-i)) | GPR.read(r));
    }
    i=i+8;
    if(i==32)
      i=0;
    ea=ea+1;
    n=n-1;    
  }

};


//!Instruction lwbrx behavior method.
void ac_behavior( lwbrx )
{
  dbg_printf(" lwbrx r%d, r%d, r%d\n\n",rt,ra,rb);

  int ea;

  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,(((unsigned int)DATA_PORT->read_byte(ea+3) & 0x000000FF) << 24) | 
	    (((unsigned int)DATA_PORT->read_byte(ea+2) & 0x000000FF) << 16) | 
	    (((unsigned int)DATA_PORT->read_byte(ea+1) & 0x000000FF) << 8) | 
	    ((unsigned int)DATA_PORT->read_byte(ea) & 0x000000FF));

};

//!Instruction lwz behavior method.
void ac_behavior( lwz )
{
  dbg_printf(" lwz r%d, %d(r%d)\n\n",rt,d,ra);
  
  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;

  GPR.write(rt,DATA_PORT->read(ea));

};

//!Instruction lwzu behavior method.
void ac_behavior( lwzu )
{
  dbg_printf(" lwzu r%d, %d(r%d)\n\n",rt,d,ra);
  
  int ea=GPR.read(ra)+(short int)d;
 
  GPR.write(ra,ea);
  GPR.write(rt,DATA_PORT->read(ea));

};

//!Instruction lwzux behavior method.
void ac_behavior( lwzux )
{
  dbg_printf(" lwzux r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int ea=GPR.read(ra)+GPR.read(rb);

  GPR.write(ra,ea);
  GPR.write(rt,DATA_PORT->read(ea));

};

//!Instruction lwzx behavior method.
void ac_behavior( lwzx )
{
  dbg_printf(" lwzx %d, %d, %d\n\n",rt,ra,rb);

  int ea;
  
  if(ra !=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  GPR.write(rt,DATA_PORT->read(ea));
  
};

//!Instruction mcrf behavior method.
void ac_behavior( mcrf )
{
  dbg_printf(" mcrf %d, %d\n\n",bf,bfa);

  unsigned int m=bfa;
  unsigned int n=bf;
  unsigned int tmp,i;
  unsigned int masc;
  /* n <- m */

  /* Clean all bits except m-bits in tmp */
  masc=0xF0000000;
  for(i=0 ; i<m ; i++)
    masc=masc>>4;
  tmp=CR.read();
  tmp=tmp & masc;
  
  /* Put m-bits in n position */
  if(n<m)
    for(i=0 ; i < m-n ; i++)
      tmp=tmp<<4;
  else 
    if(n>m) /* Else nothing */
      for(i=0 ; i < n-m ; i++)
	tmp=tmp>>4;
  
  /* Clean all bits of n-bits and make an or with tmp */
  masc=0xF0000000;
  for(i=0 ; i<n ; i++)
    masc=masc>>4;
  masc=~masc;
  CR.write((CR.read() & masc)|tmp);

};

//!Instruction mcrxr behavior method.
void ac_behavior( mcrxr )
{
  dbg_printf(" mcrxr %d\n\n",bf);

  unsigned int n=bf;
  unsigned int i;
  unsigned int tmp=0x00;
  
  /* Calculate tmp bits by XER */
  if(XER_SO_read(XER))
    tmp=tmp | 0x80000000;
  if(XER_OV_read(XER))
    tmp=tmp | 0x40000000;
  if(XER_CA_read(XER))
    tmp=tmp | 0x20000000;

  /* Move altered bits to correct CR field */
  for(i=0 ; i<n ; i++)
    tmp=tmp>>4;
  CR.write(tmp);

  /* Clean XER bits */
  XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
  XER.write(XER.read() & 0x7FFFFFFF); /* Write 0 to bit 0 SO */
  XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */
};

//!Instruction mfcr behavior method.
void ac_behavior( mfcr )
{
  dbg_printf(" mfcr r%d\n\n",rt);
  GPR.write(rt,CR.read());
  
};

//!Instruction mfspr behavior method.
void ac_behavior( mfspr )
{
  /* This instruction is a fix, other implementations can be better */
  dbg_printf(" mfspr r%d,%d\n\n",rt,sprf);
  unsigned int spvalue=sprf;
  spvalue=((spvalue>>5) & 0x0000001f ) |
    ((spvalue<<5) & 0x000003e0 );

  
  switch(spvalue) {

    /* LR */
    case 0x008:
      GPR.write(rt,LR.read());
    break;
      
    /* CTR */
    case 0x009:
      GPR.write(rt,CTR.read());
    break;

    /* USPRG0 */
    case 0x100:
      GPR.write(rt,USPRG0.read());
    break;

    /* SPRG4 */
    case 0x104:
      GPR.write(rt,SPRG4.read());
    break;

    /* SPRG5 */
    case 0x105:
      GPR.write(rt,SPRG5.read());
    break;

    /* SPRG6 */
    case 0x106:
      GPR.write(rt,SPRG6.read());
    break;

    /* SPRG7 */
    case 0x107:
      GPR.write(rt,SPRG7.read());
    break;



    /* Not implemented yet! */
   default:
      dbg_printf("\nERROR!\n");
      exit(-1);
   break;


  }
};


//!Instruction mtcrf behavior method.
void ac_behavior( mtcrf ) {
  dbg_printf(" mtcrf %d, r%d\n\n",xfm,rs);

  unsigned int tmpop,tmpmask;
  unsigned int mask;
  unsigned int i;
  unsigned int crm=xfm;

  tmpmask=0xF0000000;
  tmpop=0x80;
  mask=0;
  for(i=0;i<8;i++) {
    if(crm & tmpop) 
      mask=mask | tmpmask;
    tmpmask=tmpmask>>4;
    tmpop=tmpop>>1;
  }

  CR.write((GPR.read(rs) & mask) | (CR.read() & ~mask));

};



//!Instruction mtspr behavior method.
void ac_behavior( mtspr )
{
  /* This instruction is a fix, other implementations can be better */
  dbg_printf(" mtspr %d,r%d\n\n",sprf,rs);
  unsigned int spvalue=sprf;
  spvalue=((spvalue>>5) & 0x0000001f ) |
    ((spvalue<<5) & 0x000003e0 );

  switch(spvalue) {

    /* LR */
    case 0x008:
      LR.write(GPR.read(rs));
    break;

    /* CTR */
    case 0x009:
      CTR.write(GPR.read(rs));
    break;

    /* USPRG0 */
    case 0x100:
      USPRG0.write(GPR.read(rs));
    break;

    /* SPRG4 */
    case 0x104:
      SPRG4.write(GPR.read(rs));
    break;

    /* SPRG5 */
    case 0x105:
      SPRG5.write(GPR.read(rs));
    break;

    /* SPRG6 */
    case 0x106:
      SPRG6.write(GPR.read(rs));
    break;

    /* SPRG7 */
    case 0x107:
      SPRG7.write(GPR.read(rs));
    break;


    /* Not implemented yet! */
    default:
      dbg_printf("\nERROR!\n");
      exit(-1);
    break;
      
  }


};


//!Instruction mulhw behavior method.
void ac_behavior( mulhw )
{
  dbg_printf(" mulhw r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int high;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  unsigned long long int shprod=prod;
  shprod=shprod>>32;
  high=shprod;

  GPR.write(rt,high);
  
};


//!Instruction mulhw_ behavior method.
void ac_behavior( mulhw_ )
{
  dbg_printf(" mulhw. r%d, r%d, r%d\n\n",rt,ra,rb);

  int high;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  unsigned long long int shprod=prod;
  shprod=shprod>>32;
  high=shprod;
  
  GPR.write(rt,high);
  CR0_update(CR, XER, high); 
};

//!Instruction mulhwu behavior method.
void ac_behavior( mulhwu )
{
  dbg_printf(" mulhwu r%d, r%d, r%d\n\n",rt,ra,rb);
  
  unsigned int high;
  unsigned long long int prod = 
    (unsigned long long int)(unsigned int)GPR.read(ra)*
  (unsigned long long int)(unsigned int)GPR.read(rb);

  prod=prod>>32;
  high=prod;
  
  GPR.write(rt,high);
  
};

//!Instruction mulhwu_ behavior method.
void ac_behavior( mulhwu_ )
{
  dbg_printf(" mulhwu. r%d, r%d, r%d\n\n",rt,ra,rb);

  unsigned int high;
  unsigned long long int prod = 
    (unsigned long long int)(unsigned int)GPR.read(ra)*
    (unsigned long long int)(unsigned int)GPR.read(rb);

  prod=prod>>32;
  high=prod;
  
  GPR.write(rt,high);
  CR0_update(CR, XER, high); 
};

//!Instruction mullhw behavior method.
void ac_behavior( mullhw )
{
  dbg_printf(" mullhw r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int prod = 
    (int)(short int)(GPR.read(ra) & 0x0000FFFF)*
    (int)(short int)(GPR.read(rb) & 0x0000FFFF);

  GPR.write(rt,prod);
  
};

//!Instruction mullhw_ behavior method.
void ac_behavior( mullhw_ )
{
  dbg_printf(" mullhw. r%d, r%d, r%d\n\n",rt,ra,rb);

  int prod = 
    (int)(short int)(GPR.read(ra) & 0x0000FFFF)*
    (int)(short int)(GPR.read(rb) & 0x0000FFFF);
  
  GPR.write(rt,prod);
  CR0_update(CR, XER, prod); 
};

//!Instruction mullhwu behavior method.
void ac_behavior( mullhwu )
{
  dbg_printf(" mullhwu r%d, r%d, r%d\n\n",rt,ra,rb);
  
  unsigned int prod = 
    (unsigned int)(unsigned short int)(GPR.read(ra) & 0x0000FFFF)*
    (unsigned int)(unsigned short int)(GPR.read(rb) & 0x0000FFFF);

  GPR.write(rt,prod);
  
};

//!Instruction mullhwu_ behavior method.
void ac_behavior( mullhwu_ )
{
  dbg_printf(" mullhwu. r%d, r%d, r%d\n\n",rt,ra,rb);

  unsigned int prod = 
    (unsigned int)(unsigned short int)(GPR.read(ra) & 0x0000FFFF)*
    (unsigned int)(unsigned short int)(GPR.read(rb) & 0x0000FFFF);
  
  GPR.write(rt,prod);
  CR0_update(CR, XER, prod); 
};

//!Instruction mulli behavior method.
void ac_behavior( mulli )
{
  dbg_printf(" mulli r%d, r%d, %d\n\n",rt,ra,d);

  long long int prod = 
    (long long int)(int)GPR.read(ra) * (long long int)(short int)(d);
  
  int low;
  low=prod;

  GPR.write(rt,low);
   
};

//!Instruction mullw behavior method.
void ac_behavior( mullw )
{
  dbg_printf(" mullw r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int low;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  low=prod;

  GPR.write(rt,low);

};

//!Instruction mullw_ behavior method.
void ac_behavior( mullw_ )
{
  dbg_printf(" mullw. r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int low;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  low=prod;

  GPR.write(rt,low);
  CR0_update(CR, XER, low);

};

//!Instruction mullwo behavior method.
void ac_behavior( mullwo )
{
  dbg_printf(" mullwo r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int low;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  low=prod;

  if(prod != (long long int)(int)low) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */


  GPR.write(rt,low);

};

//!Instruction mullw behavior method.
void ac_behavior( mullwo_ )
{
  dbg_printf(" mullwo_ r%d, r%d, r%d\n\n",rt,ra,rb);
  
  int low;
  long long int prod = 
    (long long int)(int)GPR.read(ra)*(long long int)(int)GPR.read(rb);

  low=prod;

  GPR.write(rt,low);

  if(prod != (long long int)(int)low) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
  
  CR0_update(CR, XER, low);
};

//!Instruction nand behavior method.
void ac_behavior( nand )
{
  dbg_printf(" nand r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,~(GPR.read(rs) & GPR.read(rb)));

};

//!Instruction nand_ behavior method.
void ac_behavior( nand_ )
{
  dbg_printf(" nand. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=~(GPR.read(rs) & GPR.read(rb));

  GPR.write(ra,result);
  CR0_update(CR, XER, result);

};

//!Instruction neg behavior method.
void ac_behavior( neg )
{
  dbg_printf(" neg r%d, r%d\n\n",rt,ra);

  GPR.write(rt,~(GPR.read(ra))+1);
};

//!Instruction neg_ behavior method.
void ac_behavior( neg_ )
{
  dbg_printf(" neg. r%d, r%d\n\n",rt,ra);
  
  int result=~(GPR.read(ra))+1;
  GPR.write(rt,result);
  CR0_update(CR, XER, result);
};

//!Instruction nego behavior method.
void ac_behavior( nego )
{
  dbg_printf(" nego r%d, r%d\n\n",rt,ra);
  
  long long int longresult=~((unsigned int)GPR.read(ra))+1;
  int result=~(GPR.read(ra))+1;

  if(longresult != (long long int)result) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
  
  GPR.write(rt,result);
};

//!Instruction nego_ behavior method.
void ac_behavior( nego_ )
{
  dbg_printf(" nego. r%d, r%d\n\n",rt,ra);
  
  long long int longresult=~((unsigned int)GPR.read(ra))+1;
  int result=~(GPR.read(ra))+1;

  if(longresult != (long long int)result) {
    XER.write(XER.read() | 0x40000000); /* Write 1 to bit 1 OV */
    XER.write(XER.read() | 0x80000000); /* Write 1 to bit 0 SO */
  }
  else
    XER.write(XER.read() & 0xBFFFFFFF); /* Write 0 to bit 1 OV */
  
  GPR.write(rt,result);
  CR0_update(CR, XER, result);

};

//!Instruction nor behavior method.
void ac_behavior( nor )
{
  dbg_printf(" nor r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,~(GPR.read(rs) | GPR.read(rb)));

};

//!Instruction nor_ behavior method.
void ac_behavior( nor_ )
{
  dbg_printf(" nor. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=~(GPR.read(rs) | GPR.read(rb));

  GPR.write(ra,result);
  CR0_update(CR, XER, result);

};

//!Instruction ore behavior method.
void ac_behavior( ore )
{
  dbg_printf(" or r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,GPR.read(rs) | GPR.read(rb));

};

//!Instruction ore_ behavior method.
void ac_behavior( ore_ )
{
  dbg_printf(" or. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=GPR.read(rs) | GPR.read(rb);

  GPR.write(ra,result);
  CR0_update(CR, XER, result);

};

//!Instruction orc behavior method.
void ac_behavior( orc )
{
  dbg_printf(" orc r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,GPR.read(rs) | ~GPR.read(rb));

};

//!Instruction orc_ behavior method.
void ac_behavior( orc_ )
{
  dbg_printf(" orc. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=GPR.read(rs) | ~GPR.read(rb);

  GPR.write(ra,result);
  CR0_update(CR, XER, result);

};

//!Instruction ori behavior method.
void ac_behavior( ori )
{
  dbg_printf(" ori r%d, r%d, %d\n\n",ra,rs,ui);

  GPR.write(ra,GPR.read(rs) | (int)((unsigned short int)ui));
};

//!Instruction oris behavior method.
void ac_behavior( oris )
{
  dbg_printf(" oris r%d, r%d, r%d\n\n",ra,rs,ui);

  GPR.write(ra,GPR.read(rs) | (((int)((unsigned short int)ui)) << 16));
};

//!Instruction rlwimi behavior method.
void ac_behavior( rlwimi )
{
  dbg_printf(" rlwimi r%d, r%d, %d, %d, %d\n\n",ra,rs,sh,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),sh);
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m) | (GPR.read(ra) & ~m));

};

//!Instruction rlwimi_ behavior method.
void ac_behavior( rlwimi_ )
{
  dbg_printf(" rlwimi. r%d, r%d, %d, %d, %d\n\n",ra,rs,sh,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),sh);
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m) | (GPR.read(ra) & ~m));

  CR0_update(CR, XER, GPR.read(ra));  

};

//!Instruction rlwinm behavior method.
void ac_behavior( rlwinm )
{
  dbg_printf(" rlwinm r%d, r%d, %d, %d, %d\n\n",ra,rs,sh,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),sh);
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m));

};

//!Instruction rlwinm_ behavior method.
void ac_behavior( rlwinm_ )
{
  dbg_printf(" rlwinm. r%d, r%d, %d, %d, %d\n\n",ra,rs,sh,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),sh);
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m));

  CR0_update(CR, XER, GPR.read(ra));  

};

//!Instruction rlwnm behavior method.
void ac_behavior( rlwnm )
{
  dbg_printf(" rlwnm r%d, r%d, %d, %d, %d\n\n",ra,rs,rb,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),(GPR.read(rb) | 0x0000001F));
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m));

};

//!Instruction rlwnm_ behavior method.
void ac_behavior( rlwnm_ )
{
  dbg_printf(" rlwnm. r%d, r%d, %d, %d, %d\n\n",ra,rs,rb,mb,me);
  
  unsigned int r=rotl(GPR.read(rs),(GPR.read(rb) | 0x0000001F));
  unsigned int m=mask32rlw(mb,me);

  GPR.write(ra,(r & m));

  CR0_update(CR, XER, GPR.read(ra));  

};

//!Instruction sc behavior method.
/* The registers used in this intruction may be defined better */
void ac_behavior( sc )
{
  dbg_printf(" sc\n\n");

  SRR1.write(MSR.read()); /* It uses a function to join MSR fields */

  /* Write WE, EE, PR, DR and IR as 0 in MSR */
  MSR.write(MSR.read() | 0xFFFB3FCF);

  SRR0.write((ac_pc-4)+4); /* Only to understand pre-increment */

  ac_pc=((EVPR.read() & 0xFFFF0000) | 0x00000C00);

};

//!Instruction slw behavior method.
void ac_behavior( slw )
{
  dbg_printf(" slw r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),n);
  unsigned int m;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(0,31-n);
  else
    m=0;
  
  GPR.write(ra,r & m);

};

//!Instruction slw_ behavior method.
void ac_behavior( slw_ )
{
  dbg_printf(" slw. r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),n);
  unsigned int m;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(0,31-n);
  else
    m=0;
  
  int result=r & m;
  GPR.write(ra,result);

  CR0_update(CR, XER, result);

};

//!Instruction sraw behavior method.
void ac_behavior( sraw )
{
  dbg_printf(" sraw r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),32-n);
  unsigned int m;
  unsigned int s;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(n,31);
  else
    m=0;
  
  s=(GPR.read(rs) & 0x80000000);
  if(s!=0)
    s=0xFFFFFFFF;
  
  int result=(r & m) | (s & ~m);

  /* Write ra */
  GPR.write(ra,result);

  /* Update XER CA */
  if(s && ((r & ~m)!=0))
    XER.write(XER.read() | 0x20000000); /* Write 1 to bit 2 CA */
  else
    XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */
  
};

//!Instruction sraw_ behavior method.
void ac_behavior( sraw_ )
{
  dbg_printf(" sraw. r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),32-n);
  unsigned int m;
  unsigned int s;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(n,31);
  else
    m=0;
  
  s=(GPR.read(rs) & 0x80000000);
  if(s!=0)
    s=0xFFFFFFFF;
  
  /* Write ra */
  int result=(r & m) | (s & ~m);
  
  GPR.write(ra,result);

  /* Update XER CA */
  if(s && ((r & ~m)!=0))
    XER.write(XER.read() | 0x20000000); /* Write 1 to bit 2 CA */
  else
    XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */
  
  /* Update CR register */
  CR0_update(CR, XER, result);

};

//!Instruction srawi behavior method.
void ac_behavior( srawi )
{
  dbg_printf(" srawi r%d, r%d, %d\n\n",ra,rs,sh);
  
  unsigned int n=sh;
  unsigned int r=rotl(GPR.read(rs),32-n);
  unsigned int m=mask32rlw(n,31);
  unsigned int s=(GPR.read(rs) & 0x80000000);

  if(s!=0)
    s=0xFFFFFFFF;
  
  int result=(r & m) | (s & ~m);

  GPR.write(ra,result);
  
  /* Update XER CA */
  if(s && ((r & ~m)!=0))
    XER.write(XER.read() | 0x20000000); /* Write 1 to bit 2 CA */
  else
    XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */
  
};

//!Instruction srawi_ behavior method.
void ac_behavior( srawi_ )
{
  dbg_printf(" srawi. r%d, r%d, %d\n\n",ra,rs,sh);
 
  unsigned int n=sh;
  unsigned int r=rotl(GPR.read(rs),32-n);;
  unsigned int m=mask32rlw(n,31);
  unsigned int s=(GPR.read(rs) & 0x80000000);
  if(s!=0)
    s=0xFFFFFFFF;
  
  /* Write ra */
  int result=(r & m) | (s & ~m);
  GPR.write(ra,result);
  
  /* Update XER CA */
  if(s && ((r & ~m)!=0))
    XER.write(XER.read() | 0x20000000); /* Write 1 to bit 2 CA */
  else
    XER.write(XER.read() & 0xDFFFFFFF); /* Write 0 to bit 2 CA */

  /* Update CR register */
  CR0_update(CR, XER, result);
};

//!Instruction srw behavior method.
void ac_behavior( srw )
{
  dbg_printf(" srw r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),32-n);
  unsigned int m;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(n,31);
  else
    m=0;
  
  GPR.write(ra,r & m);

};

//!Instruction srw_ behavior method.
void ac_behavior( srw_ )
{
  dbg_printf(" srw. r%d, r%d, r%d\n\n",ra,rs,rb);
  
  unsigned int n=(GPR.read(rb) & 0x0000001F);
  unsigned int r=rotl(GPR.read(rs),32-n);
  unsigned int m;

  if((GPR.read(rb) & 0x00000020)==0x00) /* Check bit 26 */
    m=mask32rlw(n,31);
  else
    m=0;
  
  int result=r & m;

  GPR.write(ra,result);

  CR0_update(CR, XER, result);

};

//!Instruction stb behavior method.
void ac_behavior( stb )
{
  dbg_printf(" stb r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;
 
  DATA_PORT->write_byte(ea,(unsigned char)GPR.read(rs));
    
};

//!Instruction stbu behavior method.
void ac_behavior( stbu )
{
  dbg_printf(" stbu r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea=GPR.read(ra)+(short int)d;
 
  DATA_PORT->write_byte(ea,(unsigned char)GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction stbux behavior method.
void ac_behavior( stbux )
{
  dbg_printf(" stbux r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea=GPR.read(ra)+GPR.read(rb);
 
  DATA_PORT->write_byte(ea,(unsigned char)GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction stbx behavior method.
void ac_behavior( stbx )
{
  dbg_printf(" stbx r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  DATA_PORT->write_byte(ea,(unsigned char)GPR.read(rs));
   
};

//!Instruction sth behavior method.
void ac_behavior( sth )
{
  dbg_printf(" sth r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;
 
  DATA_PORT->write_half(ea,(unsigned short int)GPR.read(rs));
    
};

//!Instruction sthbrx behavior method.
void ac_behavior( sthbrx )
{
  dbg_printf(" sthbrx r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  DATA_PORT->write_half(ea,(unsigned short int)
		 ( ((GPR.read(rs) & 0x000000FF) << 8) | 
		   ((GPR.read(rs) & 0x0000FF00) >> 8) ));
   
}

//!Instruction sthu behavior method.
void ac_behavior( sthu )
{
  dbg_printf(" sthu r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea=GPR.read(ra)+(short int)d;
 
  DATA_PORT->write_half(ea,(unsigned short int)GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction sthux behavior method.
void ac_behavior( sthux )
{
  dbg_printf("sthux r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea=GPR.read(ra)+GPR.read(rb);
 
  DATA_PORT->write_half(ea,(unsigned short int)GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction sthx behavior method.
void ac_behavior( sthx )
{
  dbg_printf(" shhx r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  DATA_PORT->write_half(ea,(unsigned short int)GPR.read(rs));
    
};

//!Instruction stmw behavior method.
void ac_behavior( stmw )
{
  dbg_printf(" stmw r%d, %d(r%d)\n\n",rs,d,ra);

  int ea;
  unsigned int r;

  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;

  r=rs;
  
  while(r<=31) {
    DATA_PORT->write(ea,GPR.read(r));
    r+=1;
    ea+=4;
  }
    
};
  
//!Instruction stswi behavior method.
void ac_behavior( stswi )
{
  dbg_printf(" stswi r%d, r%d, %d\n\n",rs,ra,nb);

  int ea;
  unsigned int n;
  unsigned int r;
  unsigned int i,masc;

  if(ra!=0)
    ea=GPR.read(ra);
  else
    ea=0;
  
  if(nb==0)
    n=32;
  else
    n=nb;

  r=rs-1;
  i=0;

  while(n>0) {
    if(i==0) 
      r=r+1;
    if(r==32)
      r=0; 
    masc=mask32rlw(i,i+7);
    DATA_PORT->write_byte(ea,(unsigned char)((GPR.read(r) & masc) >> (24-i)));
    i=i+8;
    if(i==32)
      i=0;
    ea=ea+1;
    n=n-1;
  }
  
};

//!Instruction stswx behavior method.
void ac_behavior( stswx )
{
  dbg_printf(" stswx r%d, r%d, r%d\n\n",rs,ra,rb);

  int ea;
  unsigned int n;
  unsigned int r;
  unsigned int i,masc;

  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  n=XER_TBC_read(XER);

  r=rs-1;
  i=0;

  while(n>0) {
    if(i==0) 
      r=r+1;
    if(r==32)
      r=0; 
    masc=mask32rlw(i,i+7);
    DATA_PORT->write_byte(ea,(unsigned char)((GPR.read(r) & masc) >> (24-i)));
    i=i+8;
    if(i==32)
      i=0;
    ea=ea+1;
    n=n-1;
  }
  
};

//!Instruction stw behavior method.
void ac_behavior( stw )
{
  dbg_printf(" stw r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+(short int)d;
  else
    ea=(short int)d;

  DATA_PORT->write(ea,(unsigned int)GPR.read(rs));
    
};

//!Instruction stwbrx behavior method.
void ac_behavior( stwbrx )
{
  dbg_printf(" stwbrx r%d, r%d, r%d\n\n",rs,ra,rb);

  int ea;

  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);

  DATA_PORT->write(ea,(((GPR.read(rs) & 0x000000FF) << 24)  |
		((GPR.read(rs) & 0x0000FF00) << 16 ) |
		((GPR.read(rs) & 0x00FF0000) << 8 ) |
		(GPR.read(rs) & 0xFF000000)));

};


//!Instruction stwu behavior method.
void ac_behavior( stwu )
{
  dbg_printf(" stwu r%d, %d(r%d)\n\n",rs,d,ra);
  
  int ea=GPR.read(ra)+(short int)d;

  DATA_PORT->write(ea,(unsigned int)GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction stwux behavior method.
void ac_behavior( stwux )
{
  dbg_printf("stwux r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea=GPR.read(ra)+GPR.read(rb);
 
  DATA_PORT->write(ea,GPR.read(rs));
  GPR.write(ra,ea);
    
};

//!Instruction stwx behavior method.
void ac_behavior( stwx )
{
  dbg_printf(" stwx r%d, r%d, r%d\n\n",rs,ra,rb);
  
  int ea;
  
  if(ra!=0)
    ea=GPR.read(ra)+GPR.read(rb);
  else
    ea=GPR.read(rb);
  
  DATA_PORT->write(ea,(unsigned int)GPR.read(rs));
   
};







//!Instruction subf behavior method.
void ac_behavior( subf )
{
  dbg_printf(" subf r%d, r%d, r%d\n\n",rt,ra,rb);

  GPR.write(rt,~GPR.read(ra) + GPR.read(rb) + 1);

};

//!Instruction subf_ behavior method.
void ac_behavior( subf_ )
{
  dbg_printf(" subf. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;
  
  GPR.write(rt,result);
  CR0_update(CR, XER, result);

};

//!Instruction subfo behavior method.
void ac_behavior( subfo )
{
  dbg_printf(" subfo r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;

  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),1);
  GPR.write(rt,result);
};

//!Instruction subfo_ behavior method.
void ac_behavior( subfo_ )
{
  dbg_printf(" subfo. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),1);
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction subfc behavior method.
void ac_behavior( subfc )
{
  dbg_printf(" subfc r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;

  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),1);

  GPR.write(rt,result);
};

//!Instruction subfc_ behavior method.
void ac_behavior( subfc_ )
{
  dbg_printf(" subfc. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;
  
  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),1);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction subfco behavior method.
void ac_behavior( subfco )
{
  dbg_printf(" subfco r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;

  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),1);

  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),1);

  GPR.write(rt,result);
};

//!Instruction subfco_ behavior method.
void ac_behavior( subfco_ )
{
  dbg_printf(" subfco. r%d, r%d, r%d\n\n",rt,ra,rb);
  int result=~GPR.read(ra) + GPR.read(rb) + 1;

  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),1);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),1);
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction subfe behavior method.
void ac_behavior( subfe )
{
  dbg_printf(" subfe r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=~GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);

  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  GPR.write(rt,result);
};

//!Instruction subfe_ behavior method.
void ac_behavior( subfe_ )
{
  dbg_printf(" subfe. r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=~GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  CR0_update(CR, XER, result);
  
  GPR.write(rt,result);
};

//!Instruction subfeo behavior method.
void ac_behavior( subfeo )
{
  dbg_printf(" subfeo r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=~GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
 
  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  GPR.write(rt,result);
};

//!Instruction subfeo_ behavior method.
void ac_behavior( subfeo_ )
{
  dbg_printf(" subfeo. r%d, r%d, r%d\n\n",rt,ra,rb);

  int result=~GPR.read(ra) + GPR.read(rb) + XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,~GPR.read(ra),GPR.read(rb),XER_CA_read(XER));
  CR0_update(CR, XER, result);
  
  GPR.write(rt,result);
};

//!Instruction subfic behavior method.
void ac_behavior( subfic )
{
  dbg_printf(" subfic r%d, r%d, %d\n\n",rt,ra,d);
  int ime32=d;
  int result=~GPR.read(ra)+ime32+1;

  add_XER_CA_update(XER, result,~GPR.read(ra),ime32,1);

  GPR.write(rt,result);
};

//!Instruction subfme behavior method.
void ac_behavior( subfme )
{
  dbg_printf(" subfme r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);

  GPR.write(rt,result);
};

//!Instruction subfme_ behavior method.
void ac_behavior( subfme_ )
{
  dbg_printf(" subfme. r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction subfmeo behavior method.
void ac_behavior( subfmeo )
{
  dbg_printf(" subfmeo r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);

  add_XER_OV_SO_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);
 
  GPR.write(rt,result);
};

//!Instruction subfmeo_ behavior method.
void ac_behavior( subfmeo_ )
{
  dbg_printf(" subfmeo. r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER)+(-1);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,~GPR.read(ra),XER_CA_read(XER),-1);
  CR0_update(CR, XER, result);

  GPR.write(rt,result);  
};

//!Instruction subfze behavior method.
void ac_behavior( subfze )
{
  dbg_printf(" subfze r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);

  GPR.write(rt,result);
};

//!Instruction subfze_ behavior method.
void ac_behavior( subfze_ )
{
  dbg_printf(" subfze. r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);

  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction subfzeo behavior method.
void ac_behavior( subfzeo )
{
  dbg_printf(" subfzeo r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER);
  
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);

  add_XER_OV_SO_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);

  GPR.write(rt,result);
};

//!Instruction subfzeo_ behavior method.
void ac_behavior( subfzeo_ )
{
  dbg_printf(" subfzeo. r%d, r%d\n\n",rt,ra);
  int result=~GPR.read(ra)+XER_CA_read(XER);
    
  add_XER_CA_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);

  /* Note: XER_OV_SO_update before CR0_update */
  add_XER_OV_SO_update(XER, result,~GPR.read(ra),XER_CA_read(XER),0);  
  CR0_update(CR, XER, result);

  GPR.write(rt,result);
};

//!Instruction xor behavior method.
void ac_behavior( xxor )
{
  dbg_printf(" xor r%d, r%d, r%d\n\n",ra,rs,rb);
  GPR.write(ra,GPR.read(rs) ^ GPR.read(rb));

};

//!Instruction xor_ behavior method.
void ac_behavior( xxor_ )
{
  dbg_printf(" xor. r%d, r%d, r%d\n\n",ra,rs,rb);
  int result=GPR.read(rs) ^ GPR.read(rb);

  CR0_update(CR, XER, result);

  GPR.write(ra,result);
};

//!Instruction xori behavior method.
void ac_behavior( xori )
{
  dbg_printf(" xori r%d, r%d, %d\n\n",ra,rs,ui);

  GPR.write(ra,GPR.read(rs) ^ (int)((unsigned short int)ui));
};

//!Instruction xoris behavior method.
void ac_behavior( xoris )
{
  dbg_printf(" xoris r%d, r%d, %d\n\n",ra,rs,ui);

  GPR.write(ra,GPR.read(rs) ^ (((int)((unsigned short int)ui)) << 16));
};
