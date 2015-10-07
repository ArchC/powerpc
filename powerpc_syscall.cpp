/**
 * @file      powerpc_syscall.cpp
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

#include "powerpc_syscall.H"

using namespace powerpc_parms;

void powerpc_syscall::get_buffer(int argn, unsigned char* buf, unsigned int size)
{
  unsigned int addr = GPR.read(3+argn); 

  for (unsigned int i = 0; i<size; i++, addr++) {
    buf[i] = DATA_PORT->read_byte(addr);
  }
}

void powerpc_syscall::set_buffer(int argn, unsigned char* buf, unsigned int size)
{
  unsigned int addr = GPR.read(3+argn);

  for (unsigned int i = 0; i<size; i++, addr++) {
    DATA_PORT->write_byte(addr, buf[i]);
  }
}

void powerpc_syscall::set_buffer_noinvert(int argn, unsigned char* buf, unsigned int size)
{
  unsigned int addr = GPR.read(3+argn);

  for (unsigned int i = 0; i<size; i+=4, addr+=4) {
    DATA_PORT->write(addr, *(unsigned int *) &buf[i]);
  }
}

int powerpc_syscall::get_int(int argn)
{
  return GPR.read(3+argn);
}

void powerpc_syscall::set_int(int argn, int val)
{
  GPR.write(3+argn, val);
}

void powerpc_syscall::return_from_syscall()
{
  unsigned int oldr1;
  unsigned int oldr31;
//  oldr1=MEM.read(GPR.read(1));
//  oldr31=MEM.read(GPR.read(1)+28);
//  GPR.write(1,oldr1);
//  GPR.write(31,oldr31);
  ac_pc=LR.read();
}

void powerpc_syscall::set_prog_args(int argc, char **argv)
{
  int i, j, base;

  unsigned int ac_argv[30];
  char ac_argstr[512];

  base = AC_RAM_END - 512;
  for (i=0, j=0; i<argc; i++) {
    int len = strlen(argv[i]) + 1;
    ac_argv[i] = base + j;
    memcpy(&ac_argstr[j], argv[i], len);
    j += len;
  }

  //Write argument string
  GPR.write(3, AC_RAM_END-512);
  set_buffer(0, (unsigned char*) ac_argstr, 512);

  //Write string pointers
  GPR.write(3, AC_RAM_END-512-120);
  set_buffer_noinvert(0, (unsigned char*) ac_argv, 120);

  //Set r3 to the argument count
  GPR.write(3, argc);

  //Set r4 to the string pointers
  GPR.write(4, AC_RAM_END-512-120);
}



