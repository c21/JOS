// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW    0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
  void *addr = (void *) utf->utf_fault_va;
  uint32_t err = utf->utf_err;
  int r;
  pte_t pte;

  // Check that the faulting access was (1) a write, and (2) to a
  // copy-on-write page.  If not, panic.
  // Hint:
  //   Use the read-only page table mappings at uvpt
  //   (see <inc/memlayout.h>).

  // LAB 4: Your code here.
  pte = uvpt[PGNUM(addr)];    
  if (!(err & FEC_WR) || !(pte & PTE_COW)) {
    panic("pgfault: err %u, addr %08x", err, addr); 
  }

  // Allocate a new page, map it at a temporary location (PFTEMP),
  // copy the data from the old page to the new page, then move the new
  // page to the old page's address.
  // Hint:
  //   You should make three system calls.

  // LAB 4: Your code here.
  // panic("pgfault not implemented");
  if ((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_W | PTE_P)) < 0) {
    panic("pgfault: sys_page_alloc %e", r); 
  }
  memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
  if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE),
    PTE_U | PTE_W | PTE_P)) < 0) {
    panic("pgfault: sys_page_map %e", r); 
  }
  if ((r = sys_page_unmap(0, PFTEMP)) < 0) {
    panic("pgfault: sys_page_unmap %e", r);
  }  
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
  int r;

  // LAB 4: Your code here.
  // panic("duppage not implemented");
  uintptr_t va = pn * PGSIZE;
  int perm = PTE_U | PTE_P;
  pte_t pte = uvpt[pn];
  if ((pte & PTE_W) || (pte & PTE_COW)) {
    perm |= PTE_COW;
  }

  if ((r = sys_page_map(0, (void*)va, envid, (void*)va, perm)) < 0) {
    panic("duppage: sys_page_map %e", r);
  } 
  if ((r = sys_page_map(0, (void*)va, 0, (void*)va, perm)) < 0) {
    panic("duppage: sys_page_map %e", r);
  }
  return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
  // LAB 4: Your code here.
  // panic("fork not implemented");
  uintptr_t va, pn;  
  envid_t  envid;
  pde_t pde;
  pte_t pte;
  size_t i, j;
  int r;

  // 1. Install pgfault() as page fault handler.
  set_pgfault_handler(pgfault); 

  // 2. Create a child environment.
  envid = sys_exofork();
  if (envid < 0) {
    panic("fork: sys_exofork %e", envid);
  }
  if (envid == 0) {
    // We're the child.
    // The copied value of the global variable 'thisenv'
    // is no longer valid (it refers to the parent!).
    // Fix it and return 0.
    thisenv = &envs[ENVX(sys_getenvid())];
    return 0; 
  }
 
  // 3. Map pages.
  for (i = 0; i < PDX(UTOP); i++) {
    pde = uvpd[i];
    if (!(pde & PTE_P)) {
      continue;
    }
    // Check every page table entry.
    for (j = 0; j < NPTENTRIES; j++) {
      pn = (i << 10) | j;
      pte = uvpt[pn];
      if (!(pte & PTE_P)) {
        continue;
      }
      // Map page.
      if (pn != PGNUM(UXSTACKTOP - PGSIZE)) {
        duppage(envid, pn); 
      } else {
        // The exception stack is not remapped.
        if ((r = sys_page_alloc(envid, (void*)UXSTACKTOP - PGSIZE,
          PTE_U | PTE_W | PTE_P)) < 0) {
          panic("fork: sys_page_alloc %e", r);
        }
      } 
    } 
  }

  // 4. Set the user page fault entrypoint for the child.
  extern void _pgfault_upcall(void);
  if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0) {
    panic("fork: sys_env_set_pgfault_upcall %e", r);
  } 
  
  // 5. Mark child runnable.
  if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0) {
    panic("fork: sys_env_set_status %e", r);
  }
  
  return envid; 
}

// Challenge!
int
sfork(void)
{
  panic("sfork not implemented");
  return -E_INVAL;
}
