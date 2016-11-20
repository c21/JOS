#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

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
  if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
    return "Hardware Interrupt";
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
  // Hardware IRQ numbers.
  void IRQ_TIMER_TRAP();    // 0
  void IRQ_KBD_TRAP();      // 1
  void IRQ_SERIAL_TRAP();   // 4
  void IRQ_SPURIOUS_TRAP(); // 7
  void IRQ_IDE_TRAP();      // 14
  void IRQ_ERROR_TRAP();    // 19
  
  SETGATE(idt[T_DIVIDE], 1, GD_KT, T_DIVIDE_TRAP, 0);     // 0  divide error
  SETGATE(idt[T_DEBUG], 1, GD_KT, T_DEBUG_TRAP, 0);       // 1  debug exception
  SETGATE(idt[T_NMI], 0, GD_KT, T_NMI_TRAP, 0);           // 2  non-maskable interrupt
  SETGATE(idt[T_BRKPT], 0, GD_KT, T_BRKPT_TRAP, 3);       // 3  breakpoint
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
  SETGATE(idt[T_PGFLT], 0, GD_KT, T_PGFLT_TRAP, 0);       // 14 page fault
  /* T_RES    15 */ // 15 reserved
  SETGATE(idt[T_FPERR], 1, GD_KT, T_FPERR_TRAP, 0);       // 16 floating point error
  SETGATE(idt[T_ALIGN], 1, GD_KT, T_ALIGN_TRAP, 0);       // 17 aligment check
  SETGATE(idt[T_MCHK], 1, GD_KT, T_MCHK_TRAP, 0);         // 18 machine check
  SETGATE(idt[T_SIMDERR], 1, GD_KT, T_SIMDERR_TRAP, 0);   // 19 SIMD floating point error
  SETGATE(idt[T_SYSCALL], 0, GD_KT, T_SYSCALL_TRAP, 3);   // 48 system call
  // Hardware IRQ numbers.
  SETGATE(idt[IRQ_OFFSET + IRQ_TIMER], 0, GD_KT, IRQ_TIMER_TRAP, 3);        // 0
  SETGATE(idt[IRQ_OFFSET + IRQ_KBD], 0, GD_KT, IRQ_KBD_TRAP, 3);            // 1
  SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL], 0, GD_KT, IRQ_SERIAL_TRAP, 3);      // 4
  SETGATE(idt[IRQ_OFFSET + IRQ_SPURIOUS], 0, GD_KT, IRQ_SPURIOUS_TRAP, 3);  // 7
  SETGATE(idt[IRQ_OFFSET + IRQ_IDE], 0, GD_KT, IRQ_IDE_TRAP, 3);            // 14
  SETGATE(idt[IRQ_OFFSET + IRQ_ERROR], 0, GD_KT, IRQ_ERROR_TRAP, 3);        // 19
  
  // Per-CPU setup 
  trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
  // The example code here sets up the Task State Segment (TSS) and
  // the TSS descriptor for CPU 0. But it is incorrect if we are
  // running on other CPUs because each CPU has its own kernel stack.
  // Fix the code so that it works for all CPUs.
  //
  // Hints:
  //   - The macro "thiscpu" always refers to the current CPU's
  //     struct CpuInfo;
  //   - The ID of the current CPU is given by cpunum() or
  //     thiscpu->cpu_id;
  //   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
  //     rather than the global "ts" variable;
  //   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
  //   - You mapped the per-CPU kernel stacks in mem_init_mp()
  //
  // ltr sets a 'busy' flag in the TSS selector, so if you
  // accidentally load the same TSS on more than one CPU, you'll
  // get a triple fault.  If you set up an individual CPU's TSS
  // wrong, you may not get a fault until you try to return from
  // user space on that CPU.
  //
  // LAB 4: Your code here:
  size_t i = thiscpu->cpu_id;
  struct Taskstate *ts = &thiscpu->cpu_ts;
 
  // Setup a TSS so that we get the right stack
  // when we trap to the kernel.
  ts->ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
  ts->ts_ss0 = GD_KD;

  // Initialize the TSS slot of the gdt.
  gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t) (ts),
          sizeof(struct Taskstate) - 1, 0);
  gdt[(GD_TSS0 >> 3) + i].sd_s = 0;

  // Load the TSS selector (like other segment selectors, the
  // bottom three bits are special; we leave them 0)
  ltr(GD_TSS0 + (i << 3));

  // Load the IDT
  lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
  cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
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

  // Handle spurious interrupts
  // The hardware sometimes raises these because of noise on the
  // IRQ line or other reasons. We don't care.
  if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
    cprintf("Spurious interrupt on irq 7\n");
    print_trapframe(tf);
    return;
  }

  // Handle clock interrupts. Don't forget to acknowledge the
  // interrupt using lapic_eoi() before calling the scheduler!
  // LAB 4: Your code here.
  if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
    lapic_eoi();
    sched_yield(); 
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

  // Halt the CPU if some other CPU has called panic()
  extern char *panicstr;
  if (panicstr)
    asm volatile("hlt");

  // Re-acqurie the big kernel lock if we were halted in
  // sched_yield()
  if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
    lock_kernel();
  // Check that interrupts are disabled.  If this assertion
  // fails, DO NOT be tempted to fix it by inserting a "cli" in
  // the interrupt path.
  assert(!(read_eflags() & FL_IF));

  if ((tf->tf_cs & 3) == 3) {
    // Trapped from user mode.
    // Acquire the big kernel lock before doing any
    // serious kernel work.
    // LAB 4: Your code here.
    lock_kernel();

    assert(curenv);

    // Garbage collect if current enviroment is a zombie
    if (curenv->env_status == ENV_DYING) {
      env_free(curenv);
      curenv = NULL;
      sched_yield();
    }

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

  // If we made it to this point, then no other environment was
  // scheduled, so we should return to the current environment
  // if doing so makes sense.
  if (curenv && curenv->env_status == ENV_RUNNING)
    env_run(curenv);
  else
    sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
  uint32_t fault_va, cur_esp;
  struct UTrapframe *utf;

  // Read processor's CR2 register to find the faulting address
  fault_va = rcr2();

  // Handle kernel-mode page faults.

  // LAB 3: Your code here.
  if (tf->tf_cs == GD_KT) {
    panic("page_fault_handler: kernel-mode page fault");
  }

  // We've already handled kernel-mode exceptions, so if we get here,
  // the page fault happened in user mode.

  // Call the environment's page fault upcall, if one exists.  Set up a
  // page fault stack frame on the user exception stack (below
  // UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
  //
  // The page fault upcall might cause another page fault, in which case
  // we branch to the page fault upcall recursively, pushing another
  // page fault stack frame on top of the user exception stack.
  //
  // The trap handler needs one word of scratch space at the top of the
  // trap-time stack in order to return.  In the non-recursive case, we
  // don't have to worry about this because the top of the regular user
  // stack is free.  In the recursive case, this means we have to leave
  // an extra word between the current top of the exception stack and
  // the new stack frame because the exception stack _is_ the trap-time
  // stack.
  //
  // If there's no page fault upcall, the environment didn't allocate a
  // page for its exception stack or can't write to it, or the exception
  // stack overflows, then destroy the environment that caused the fault.
  // Note that the grade script assumes you will first check for the page
  // fault upcall and print the "user fault va" message below if there is
  // none.  The remaining three checks can be combined into a single test.
  //
  // Hints:
  //   user_mem_assert() and env_run() are useful here.
  //   To change what the user environment runs, modify 'curenv->env_tf'
  //   (the 'tf' variable points at 'curenv->env_tf').

  // LAB 4: Your code here.
  // If there's no page fault upcall.
  if (!curenv->env_pgfault_upcall) {
    cprintf("[%08x] user fault va %08x ip %08x\n",
      curenv->env_id, fault_va, tf->tf_eip); 
  } else {
    // If the environment didn't allocate a page for its exception stack,
    // or can't write to it,
    // or the exception stack overflows.
    if (tf->tf_esp >= UXSTACKTOP - PGSIZE && tf->tf_esp <= UXSTACKTOP - 1) {
      cur_esp = tf->tf_esp - 4; 
    } else {
      cur_esp = UXSTACKTOP; 
    }
    user_mem_assert(curenv, (void*)cur_esp - sizeof(struct UTrapframe),
      sizeof(struct UTrapframe), PTE_W);

    // Set up a page fault stack frame on the user exception stack
    // (below UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
    utf = (struct UTrapframe*)(cur_esp - sizeof(struct UTrapframe));
    utf->utf_fault_va = fault_va;
    utf->utf_err = tf->tf_err;
    utf->utf_regs = tf->tf_regs;
    utf->utf_eip = tf->tf_eip;
    utf->utf_eflags = tf->tf_eflags;
    utf->utf_esp = tf->tf_esp;

    cur_esp -= sizeof(struct UTrapframe);
    tf->tf_esp = cur_esp;
    tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;

    env_run(curenv); 
  }

  // Destroy the environment that caused the fault.
  print_trapframe(tf);
  env_destroy(curenv);
}
