## 2.4.0

* Revision numbers following the ArchC release
* Instructions with cycles annotations
* Two new .ac files to use with MPSoCBench (block and nonblock)
* powerpc_isa.cpp using the reserved work DATA_PORT to data request. See the [commit message](https://github.com/ArchC/powerpc/commit/18be8f932ee67c61795a331aa4682c866c864358).
* Interrupt handler support. It is inactive in standalone simulator.

[Full changelog](https://github.com/ArchC/powerpc/compare/v2.3.0...v2.4.0)

## 2.3.0
* Revision numbers following the ArchC release
+ Added id register for core identification

[Full changelog](https://github.com/ArchC/powerpc/compare/v1.0.2...v2.3.0)

## 1.0.1
* Updated syscall emulation to work with new version of libac_sysc
  (this new version of libac_sysc is used by GCC 4.8 cross compiler and above)

[Full changelog](https://github.com/ArchC/powerpc/compare/v1.0.0...v1.0.1)

## 1.0.0
* ArchC 2.2 compliant

[Full changelog](https://github.com/ArchC/powerpc/compare/v0.7.4...v1.0.0)

## 0.7.4
* Fixed gdb_funcs bug when using ArchC 2.0 or later
* ArchC 2.1 compliant

[Full changelog](https://github.com/ArchC/powerpc/compare/v0.7.3...v0.7.4)

## 0.7.3
+ Added binary utilities support files
* Adjusted mullhw syntax
* ArchC 2.0 compliant

[Full changelog](https://github.com/ArchC/powerpc/compare/v0.7.2...v0.7.3)

## 0.7.2-archc2.0beta3

+ Added license headers

[Full changelog](https://github.com/ArchC/powerpc/compare/v0.7.1-1...v0.7.2)

## 0.7.1-archc2.0beta1

* Model compliant with ArchC 2.0beta1

## 0.7.1

+ Included optional properties to optimize compiled simulation
* Changed dprintf function (conflict with native dprintf())
* Changed SC1 type (to include the LEV field)
* Changed XL2 type (to include the BH field)
+ Added assembly language information

## 0.7.0

* Model passed selected Mediabench and Mibench applications
+ Included file for GDB integration
