#include "powerpc.H"


int powerpc::nRegs(void) {
  return 104;
}


ac_word powerpc::reg_read( int reg ) {
  unsigned int n;

  if ( ( reg >= 0 ) && ( reg < 32 ) )
    return GPR.read( reg );
  else {
    switch (reg) {
    
      case 96:
	n=ac_resources::ac_pc;
      break;

      /* case ??:
        n=MSR.read();
	break; */

      case 98:
        n=CR.read();
      break;

      case 99:
        n=LR.read();
      break;

      case 100:
        n=CTR.read();
      break;

      case 101:
        n=XER.read();
      break;

      default:
        return 0;
      break;

    }
    
    return(n);
    
  }
}


void powerpc::reg_write( int reg, ac_word value ) {

  /* general purpose registers */
  if ( ( reg >= 0 ) && ( reg < 32 ) )
    GPR.write(reg,value);
  else {
    switch (reg) {

    case 96:
      ac_resources::ac_pc=value;
    break;

    /*    case ??:
      MSR.write(value);
    break;
    */

    case 98:
      CR.write(value);
    break;

    case 99:
      LR.write(value);
    break;

    case 100:
      CTR.write(value);
    break;

    case 101:
      XER.write(value);
    break;

    default:
      /* No completely implemented register */
      break;

    }
  }
}


unsigned char powerpc::mem_read( unsigned int address ) {
  return ac_resources::IM->read_byte( address );
}


void powerpc::mem_write( unsigned int address, unsigned char byte ) {
  ac_resources::IM->write_byte( address, byte );
}

