#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
  binaryname = "ns_output";
  uint32_t req, whom;
  int perm;
  struct jif_pkt *pkt = (struct jif_pkt*)REQVA;

  // LAB 6: Your code here:
  //   - read a packet from the network server
  //  - send the packet to the device driver
  while (1) {
    perm = 0;
    // Read a packet from the network server.
    req = ipc_recv((int32_t*)&whom, pkt, &perm);

    // All requests must contain an argument page.
    if (!(perm & PTE_P)) {
      cprintf("Invalid request from %08x: no argument page\n",
        whom);
      continue; // just leave it hanging...
    }

    // Send the packet to the device driver.
    if (req == NSREQ_OUTPUT) {
      sys_try_transmit_pkt(pkt->jp_data, pkt->jp_len);
    } else {
      cprintf("Invalid request code %d from %08x\n", req, whom);
    }

    sys_page_unmap(0, pkt); 
  } 
}
