#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
  sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
  static const char * const excnames[] = {
    "Divide error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection",
    "Page Fault",
    "(unknown trap)",
    "x87 FPU Floating-Point Error",
    "Alignment Check",
    "Machine-Check",
    "SIMD Floating-Point Exception"
  };

  if (trapno < sizeof(excnames)/sizeof(excnames[0]))
    return excnames[trapno];
  if (trapno == T_SYSCALL)
    return "System call";
  return "(unknown trap)";
}


void
trap_init(void)
{
  extern struct Segdesc gdt[];
  // LAB 3: Your code here.
  void T_DIVIDE_TRAP(); // 0  divide error
  void T_DEBUG_TRAP();  // 1  debug exception
  void T_NMI_TRAP();    // 2  non-maskable interrupt
  void T_BRKPT_TRAP();  // 3  breakpoint
  void T_OFLOW_TRAP();  // 4  overflow
  void T_BOUND_TRAP();  // 5  bounds check
  void T_ILLOP_TRAP();  // 6  illegal opcode
  void T_DEVICE_TRAP(); // 7  device not available
  void T_DBLFLT_TRAP(); // 8  double fault
  /* #define T_COPROC  9 */ // reserved (not generated by recent processors)
  void T_TSS_TRAP();    // 10 invalid task switch segment
  void T_SEGNP_TRAP();  // 11 segment not present
  void T_STACK_TRAP();  // 12 stack exception
  void T_GPFLT_TRAP();  // 13 general protection fault
  void T_PGFLT_TRAP();  // 14 page fault
  /* #define T_RES    15 */ // reserved
  void T_FPERR_TRAP();  // 16 floating point error
  void T_ALIGN_TRAP();  // 17 aligment check
  void T_MCHK_TRAP();   // 18 machine check
  void T_SIMDERR_TRAP();// 19 SIMD floating point error
  void T_SYSCALL_TRAP();// 48 system call
  
  SETGATE(idt[T_DIVIDE], 1, GD_KT, T_DIVIDE_TRAP, 0);     // 0  divide error
  SETGATE(idt[T_DEBUG], 1, GD_KT, T_DEBUG_TRAP, 0);       // 1  debug exception
  SETGATE(idt[T_NMI], 0, GD_KT, T_NMI_TRAP, 0);           // 2  non-maskable interrupt
  SETGATE(idt[T_BRKPT], 1, GD_KT, T_BRKPT_TRAP, 3);       // 3  breakpoint
  SETGATE(idt[T_OFLOW], 1, GD_KT, T_OFLOW_TRAP, 0);       // 4  overflow
  SETGATE(idt[T_BOUND], 1, GD_KT, T_BOUND_TRAP, 0);       // 5  bounds check
  SETGATE(idt[T_ILLOP], 1, GD_KT, T_ILLOP_TRAP, 0);       // 6  illegal opcode
  SETGATE(idt[T_DEVICE], 1, GD_KT, T_DEVICE_TRAP, 0);     // 7  device not available
  SETGATE(idt[T_DBLFLT], 1, GD_KT, T_DBLFLT_TRAP, 0);     // 8  double fault
  /* T_COPROC  9 */ // 9  reserved (not generated by recent processors)
  SETGATE(idt[T_TSS], 1, GD_KT, T_TSS_TRAP, 0);           // 10 invalid task switch segment
  SETGATE(idt[T_SEGNP], 1, GD_KT, T_SEGNP_TRAP, 0);       // 11 segment not present
  SETGATE(idt[T_STACK], 1, GD_KT, T_STACK_TRAP, 0);       // 12 stack exception
  SETGATE(idt[T_GPFLT], 1, GD_KT, T_GPFLT_TRAP, 0);       // 13 general protection fault
  SETGATE(idt[T_PGFLT], 1, GD_KT, T_PGFLT_TRAP, 0);       // 14 page fault
  /* T_RES    15 */ // 15 reserved
  SETGATE(idt[T_FPERR], 1, GD_KT, T_FPERR_TRAP, 0);       // 16 floating point error
  SETGATE(idt[T_ALIGN], 1, GD_KT, T_ALIGN_TRAP, 0);       // 17 aligment check
  SETGATE(idt[T_MCHK], 1, GD_KT, T_MCHK_TRAP, 0);         // 18 machine check
  SETGATE(idt[T_SIMDERR], 1, GD_KT, T_SIMDERR_TRAP, 0);   // 19 SIMD floating point error
  SETGATE(idt[T_SYSCALL], 1, GD_KT, T_SYSCALL_TRAP, 3);   // 48 system call
  
  // Per-CPU setup 
  trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
  // Setup a TSS so that we get the right stack
  // when we trap to the kernel.
  ts.ts_esp0 = KSTACKTOP;
  ts.ts_ss0 = GD_KD;

  // Initialize the TSS slot of the gdt.
  gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
          sizeof(struct Taskstate) - 1, 0);
  gdt[GD_TSS0 >> 3].sd_s = 0;

  // Load the TSS selector (like other segment selectors, the
  // bottom three bits are special; we leave them 0)
  ltr(GD_TSS0);

  // Load the IDT
  lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
  cprintf("TRAP frame at %p\n", tf);
  print_regs(&tf->tf_regs);
  cprintf("  es   0x----%04x\n", tf->tf_es);
  cprintf("  ds   0x----%04x\n", tf->tf_ds);
  cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
  // If this trap was a page fault that just happened
  // (so %cr2 is meaningful), print the faulting linear address.
  if (tf == last_tf && tf->tf_trapno == T_PGFLT)
    cprintf("  cr2  0x%08x\n", rcr2());
  cprintf("  err  0x%08x", tf->tf_err);
  // For page faults, print decoded fault error code:
  // U/K=fault occurred in user/kernel mode
  // W/R=a write/read caused the fault
  // PR=a protection violation caused the fault (NP=page not present).
  if (tf->tf_trapno == T_PGFLT)
    cprintf(" [%s, %s, %s]\n",
      tf->tf_err & 4 ? "user" : "kernel",
      tf->tf_err & 2 ? "write" : "read",
      tf->tf_err & 1 ? "protection" : "not-present");
  else
    cprintf("\n");
  cprintf("  eip  0x%08x\n", tf->tf_eip);
  cprintf("  cs   0x----%04x\n", tf->tf_cs);
  cprintf("  flag 0x%08x\n", tf->tf_eflags);
  if ((tf->tf_cs & 3) != 0) {
    cprintf("  esp  0x%08x\n", tf->tf_esp);
    cprintf("  ss   0x----%04x\n", tf->tf_ss);
  }
}

void
print_regs(struct PushRegs *regs)
{
  cprintf("  edi  0x%08x\n", regs->reg_edi);
  cprintf("  esi  0x%08x\n", regs->reg_esi);
  cprintf("  ebp  0x%08x\n", regs->reg_ebp);
  cprintf("  oesp 0x%08x\n", regs->reg_oesp);
  cprintf("  ebx  0x%08x\n", regs->reg_ebx);
  cprintf("  edx  0x%08x\n", regs->reg_edx);
  cprintf("  ecx  0x%08x\n", regs->reg_ecx);
  cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
  // Handle processor exceptions.
  // LAB 3: Your code here.
  struct PushRegs *rp;

  switch (tf->tf_trapno) {
  case T_BRKPT:
    while (1) {
      monitor(tf);
    }
    return;
  case T_PGFLT:
    page_fault_handler(tf);
    return;
  case T_SYSCALL:
    rp = &tf->tf_regs;
    rp->reg_eax = syscall(rp->reg_eax, rp->reg_edx, rp->reg_ecx, rp->reg_ebx,
      rp->reg_edi, rp->reg_esi);
    return;
  default:
    break;
  }

  // Unexpected trap: The user process or the kernel has a bug.
  print_trapframe(tf);
  if (tf->tf_cs == GD_KT)
    panic("unhandled trap in kernel");
  else {
    env_destroy(curenv);
    return;
  }
}

void
trap(struct Trapframe *tf)
{
  // The environment may have set DF and some versions
  // of GCC rely on DF being clear
  asm volatile("cld" ::: "cc");

  // Check that interrupts are disabled.  If this assertion
  // fails, DO NOT be tempted to fix it by inserting a "cli" in
  // the interrupt path.
  assert(!(read_eflags() & FL_IF));

  cprintf("Incoming TRAP frame at %p\n", tf);

  if ((tf->tf_cs & 3) == 3) {
    // Trapped from user mode.
    assert(curenv);

    // Copy trap frame (which is currently on the stack)
    // into 'curenv->env_tf', so that running the environment
    // will restart at the trap point.
    curenv->env_tf = *tf;
    // The trapframe on the stack should be ignored from here on.
    tf = &curenv->env_tf;
  }

  // Record that tf is the last real trapframe so
  // print_trapframe can print some additional information.
  last_tf = tf;

  // Dispatch based on what type of trap occurred
  trap_dispatch(tf);

  // Return to the current environment, which should be running.
  assert(curenv && curenv->env_status == ENV_RUNNING);
  env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
  uint32_t fault_va;

  // Read processor's CR2 register to find the faulting address
  fault_va = rcr2();

  // Handle kernel-mode page faults.

  // LAB 3: Your code here.
  if (tf->tf_cs == GD_KT) {
    panic("page_fault_handler: kernel-mode page fault");
  }

  // We've already handled kernel-mode exceptions, so if we get here,
  // the page fault happened in user mode.

  // Destroy the environment that caused the fault.
  cprintf("[%08x] user fault va %08x ip %08x\n",
    curenv->env_id, fault_va, tf->tf_eip);
  print_trapframe(tf);
  env_destroy(curenv);
}

