1.Intel 80x86 family of hardware
  (1).8088,8086:
      16-bit register
        general purpose register: AX, BX, CX, DX
        index register: SI, DI
        pointer to stack register: BP(base pointer), SP(stack pointer)
        segment register: CS(code segment), DS(data segment), SS(stack segment), ES(extra)
        instruction pointer register: IP
        store results of previous instruction: FLAGS
      
      real mode to access memory:
        a process can access any memory address of other process.
        the memory addess of process cannot be changed.
        memory size: 1M (20bits)
        use 2 16-bit registers (called selector and offset) to get a memory address:
          physical memory address = 16 * selector + offset

  (2).80286
      16-bit register
      protected mode to access memory:
        a process cannot directly access memory address of other process.
        the memory address of process can be changed (virtual memory, use descriptor table).
        segment can entirely not be in memory, but moved to disk.
          physical memory address = descriptor_table[selector].base_address + offset          
          (selector register stores the index into descriptor table, descriptor table stores base address of segment)

  (3).80386
      32-bit register
      32-bit protected mode to access memory:
        offset register is 32 bits. Each segment can have size of 4G (32 bits).
        Each segments are divided into pages (4K size per page).
        Part of segment (pages) can not be in memory, but moved to disk.

2.Physical memory, BIOS

+------------------+  <- 0xFFFFFFFF (4GB)
|      32-bit      |
|  memory mapped   |
|     devices      |
|                  |
/\/\/\/\/\/\/\/\/\/\

/\/\/\/\/\/\/\/\/\/\
|                  |
|      Unused      |
|                  |
+------------------+  <- depends on amount of RAM
|                  |
|                  |
| Extended Memory  |
|                  |
|                  |
+------------------+  <- 0x00100000 (1MB)
|     BIOS ROM     |
+------------------+  <- 0x000F0000 (960KB)
|  16-bit devices, |
|  expansion ROMs  |
+------------------+  <- 0x000C0000 (768KB)
|   VGA Display    |
+------------------+  <- 0x000A0000 (640KB)
|                  |
|    Low Memory    |
|                  |
+------------------+  <- 0x00000000

8088/8086: 1MB memory size
Memory (RAM random access memory) for user: 0x00000 - 0xA0000
BIOS (basic input output services, ROM read only memory): 0xF0000 - 0x100000
  BIOS set up interrupt vector table at 0x0 - 0x400 (0-1024 bytes), search for a bootable device, if found
  one bootable device, read data (boot loader, for disk: first boot sector 512B) from device,
  jump to boot loader to run.
    interrupt vector table: pointer(value of selector and offset registers) of interrupt handler (4 bytes)
                            of that interrupt number (0-255)


3.boot loader
  (boot/boot.S, boot/main.c)
  e.g. the first sector (512 bytes) in disk, loaded into memory by BIOS.
  (1).set up a new global descriptor table (GDT) (change from real mode to protected mode, protected mode uss GDT)
    global descriptor table: hold information (base, bound, permission) for each segment (code, data, ...).
                             each entry in table is 8 bytes, a descriptor.
                             base: 32-bit, bound: 20-bit (reason: base+bound can be used in page granularity,
                             if 1 page is 4 KB, one segment can has maximal size of 4GB)
                             permission: descriptor privilege level (DPL 2-bit): 0 kernel mode, 3 user mode
    ldgt memory-address: memory-address specifies the size and address of global descriptor table,
                         ldgt stores the size and address into global descriptor table register (GDTR)
    Figure 3-8 in https://pdos.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf
  (2).switch from real mode to protected mode
    segment registers(CS,DS,...) will contain the index of global descriptor table
  (3).load kernel into memory
    SP/ESP: the current address of stack top (stack pointer)
    BP/EBP: the beginning address of stack to store current function information (base pointer)
    +----------------------------+
    | Parameter 1 for routine 1  | \
    +----------------------------+  |
    | First callers return addr. |   >  Stack frame 1
    +----------------------------+  |
    | First callers EBP          | /   <-- Routine f1's EBP
    +----------------------------+
 +->| local data                 | 
 |  +----------------------------+
 |  | Parameter 2 for routine 2  | \
 |  +----------------------------+  |
 |  | Parameter 1 for routine 2  |  |
 |  +----------------------------+  |
 |  | Return address, routine 1  |  |
 |  +----------------------------+  |
 +--| EBP value for routine 1    |   >  Stack frame 2 <-- Routine f2's EBP
    +----------------------------+  |
    | local data                 |  |
    +----------------------------+
    main calls f1(a), f1 calls f2(a,b)
    when calling function:
      store on stack:
      (1).parameters from last to first (caller function does)
      (2).return address (call instruction does)
      (3).base pointer of caller function (callee function does)
   In x86, stack grows from high address to low address (largest address -> lowest address).

   read kernel into memory:
    format for executable and object files in Unix systems: executable and linkable format (ELF) (header + sections)
      .text: section for executable's code
      .data: section for initialized global variables
      .rodata: section for read-only data: string constants
      .bss: section for uninitialized global variables
    BIOS reads boot loader (one ELF file) into memory, boot loader reads kernel (another ELF file) into memory.
    First read header of kernel's ELF file, then read each section of ELF file.
    Use in and out instruction to read from disk.

4.kernel
  (kern/entry.S, kern/init.c)
  (1).set up paging
    The beginning virtual address of kernel: 0xf0100000 is loaded to physical address of memory: 0x00100000.
    We should set up translation between virtual address to physical address (here we use paging).
    Chapter 3.6 in https://pdos.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf
    (1).Set control register cr0 (Bit 31: enable paging and Bit 0: enable protected mode)
    (2).Store base address of page directory into cr3 (cr3 is page directory base register (PDBR)).
      x86 by default has two level page table
      (32 bits virtual address: page directory (10 bits) + page table (10 bits) + offset (12 bits))
      In this way, we change from segmentation to paging (actually segmentation and paging).
      When given a virtual address: va (determined by registers cs and eip)
      (1).first do segmentation: va' = descriptor_table[cs].base_address + eip
      (2).then do paging: consult cr3, translate va' to physical address.
  (2).set up new stack
    change esp to the bottom (largest address) of new stack.
  (3).initialize port for console (cga/vga)
    read/write from console: in/out instruction to cga/vga ports.
