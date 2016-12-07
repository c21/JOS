#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
  binaryname = "ns_input";
  int r;
  struct jif_pkt *ppkt = &nsipcbuf.pkt;

  // LAB 6: Your code here:
  //   - read a packet from the device driver
  //   - send it to the network server
  // Hint: When you IPC a page to the network server, it will be
  // reading from it for a while, so don't immediately receive
  // another packet in to the same physical page.
  while (1) {
    memset(ppkt, 0, PGSIZE); 
    while ((r = sys_try_receive_pkt(ppkt->jp_data)) < 0) {
      sys_yield();
    }
    ppkt->jp_len = r;

    ipc_send(ns_envid, NSREQ_INPUT, ppkt, PTE_U | PTE_P); 

    // Don't immediately receive another packet in to the same physical page.
    sys_yield();
    sys_yield();
    sys_yield();
  }
}
