#include <inc/error.h>
#include <inc/string.h>
#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here

// Memory-mapped I/O base address for registers
volatile uintptr_t *epic;

// Transmit descriptor array
struct tx_desc tx_desc_array[NTXDESC]
__attribute__ ((aligned(sizeof(struct tx_desc))));

// Transmit packet buffer array
struct tx_buf tx_buf_array[NTXDESC];

// Receive descriptor array
struct rx_desc rx_desc_array[NRXDESC]
__attribute__ ((aligned(sizeof(struct rx_desc))));

// Receive packet buffer array
struct rx_buf rx_buf_array[NRXDESC];

// Transmit initialization.
static void
tx_init(void)
{
  size_t i;

  // *0. Set all transmit descriptors' DD bit to 1b.
  for (i = 0; i < NTXDESC; i++) {
    tx_desc_array[i].status |= E1000_TXD_STAT_DD;
  }

  // 1. Set the Transmit Descriptor Base Address (TDBAL/TDBAH)
  // with the address of transmit descriptor list.
  epic[E1000_TDBAL] = PADDR(tx_desc_array);
  epic[E1000_TDBAH] = 0;
  
  // 2. Set the Transmit Descriptor Length (TDLEN)
  // to the size (in bytes) of transmit descriptor list.
  // This register must be 128-byte aligned. 
  epic[E1000_TDLEN] = sizeof(tx_desc_array);

  // 3. Set the Transmit Descriptor Head and Tail (TDH/TDT) to 0b.
  epic[E1000_TDH] = 0;
  epic[E1000_TDT] = 0;  

  // 4. Set the Transmit Control Register (TCTL)
  // Set the Enable (TCTL.EN) bit to 1b for normal operation.
  epic[E1000_TCTL] = 0;
  epic[E1000_TCTL] |= E1000_TCTL_EN;
  // Set the Pad Short Packets (TCTL.PSP) bit to 1b.
  epic[E1000_TCTL] |= E1000_TCTL_PSP; 
  // Set the Collision Threshold (TCTL.CT) to the Ethernet standard (10h).
  // This setting only has meaning in half duplex mode.
  epic[E1000_TCTL] |= (0x10 << 4);
  // Set the Collision Distance (TCTL.COLD) to its expected value.
  // For full duplex operation, this value should be set to 40h.
  // For gigabit half duplex, this value should be set to 200h.
  // For 10/100 half duplex, this value should be set to 40h.
  epic[E1000_TCTL] |= (0x40 << 12); 
   
  // 5. Set the Transmit IPG (TIPG)
  // Set the IPG Transmit Time (IPGT 9:0) to 10.
  epic[E1000_TIPG] = 0;
  epic[E1000_TIPG] |= 10;
  // Set the IPG Receive Time 1 (IPGR1 19:10) to 8.
  epic[E1000_TIPG] |= (8 << 10);
  // Set the IPG Receive Time 2 (IPGR2 29:20) to 6.
  epic[E1000_TIPG] |= (6 << 20);
}

// Receive initialization.
static void
rx_init(void)
{
  size_t i;

  // *0. Set all receive descriptors' status to 0b,
  // and set recevie descriptor's buffer address.
  for (i = 0; i < NRXDESC; i++) {
    rx_desc_array[i].status = 0;
    rx_desc_array[i].buffer_addr = PADDR(&rx_buf_array[i]);
  }

  // 1. Set the Receive Address Register(s) (RAL/RAH)
  // with the desired Ethernet addresses 52:54:00:12:34:56.
  epic[E1000_RAL] = 0x12005452;
  epic[E1000_RAH] = 0x5634;
  // Set the "Address Valid" bit in RAH to 1b.
  epic[E1000_RAH] |= E1000_RAH_AV;


  // 2. Set the MTA (Multicast Table Array) to 0b.
  memset((void*)&epic[E1000_MTA], 0, &epic[E1000_RAL] - &epic[E1000_MTA]);

  // 3. Set the Receive Descriptor Base Address (RDBAL/RDBAH) 
  // with the address of the region.
  epic[E1000_RDBAL] = PADDR(rx_desc_array);
  epic[E1000_RDBAH] = 0;  
 
  // 4. Set the Receive Descriptor Length (RDLEN)
  // to the size (in bytes) of receive descriptor list.
  // This register must be 128-byte aligned. 
  epic[E1000_RDLEN] = sizeof(rx_desc_array);  

  // 5. Set the Receive Descriptor Head and Tail (RDH/RDT).
  // Head should point to the first valid receive descriptor in the
  // descriptor ring.
  // Tail should point to one descriptor beyond the last valid descriptor
  // in the descriptor ring.
  epic[E1000_RDH] = 0;
  epic[E1000_RDT] = NRXDESC - 1;

  // 6. Set the Receive Control (RCTL)
  // Set Loopback Mode (RCTL.LBM) to 00b for normal operation.
  epic[E1000_RCTL] = 0;
  epic[E1000_RCTL] |= E1000_RCTL_LBM_NO;
  // Set the Broadcast Accept Mode (RCTL.BAM) bit to 1b
  // allowing the hardware to accept broadcast packets.
  epic[E1000_RCTL] |= E1000_RCTL_BAM;
  // Set the Receive Buffer Size (RCTL.BSIZE) bits with 2048.
  epic[E1000_RCTL] |= E1000_RCTL_SZ_2048;
  // Set the Strip Ethernet CRC (RCTL.SECRC) bit.
  epic[E1000_RCTL] |= E1000_RCTL_SECRC;
  // Set the receiver Enable (RCTL.EN) bit to 1b for normal operation.
  epic[E1000_RCTL] |= E1000_RCTL_EN;  
}

// Attach function to initialize the E1000.
int
pci_e1000_attach(struct pci_func *f)
{
  // Enable the E1000 device via pci_func_enable.
  pci_func_enable(f);

  // Create a virtual memory mapping for the E1000's BAR 0.
  epic = mmio_map_region(f->reg_base[0], f->reg_size[0]); 
  
  cprintf("device status register: %08x\n", epic[E1000_STATUS]);

  // Transmit initialization.
  tx_init(); 

  // Receive initialization.
  rx_init();

  return 1; 
}

// Try to transmit a packet to E1000.
// 
// Returns 0 on success, < 0 on error.
// Errors are:
// -E_NO_MEM if the transmit array of E1000 is full.
int
try_transmit_pkt(void *srcva, size_t len)
{
  // Check if the transmit array of E1000 is full.
  size_t idx = epic[E1000_TDT];
  if (!(tx_desc_array[idx].status & E1000_TXD_STAT_DD)) {
    return -E_NO_MEM;
  }

  // Copy the packet data into buffer.
  memmove(&tx_buf_array[idx], srcva, len);

  // Set transmit descriptor.
  // 1. Set buffer address.
  tx_desc_array[idx].addr = PADDR(&tx_buf_array[idx]);
  // 2. Set length of packet.
  tx_desc_array[idx].length = len;
  // 3. Clear checksum offset.
  tx_desc_array[idx].cso = 0;
  // 4. Set Report Status (RS) in command field to 1b,
  // and set End of Packet to 1b,
  // clear all other bits. 
  tx_desc_array[idx].cmd = 0;
  tx_desc_array[idx].cmd |= E1000_TXD_CMD_RS;
  tx_desc_array[idx].cmd |= E1000_TXD_CMD_EOP;

  // 6. Clear checksum start field.
  tx_desc_array[idx].css = 0;
  // 7. Clear special field.
  tx_desc_array[idx].special = 0;

  // 8. Clear status field.
  tx_desc_array[idx].status = 0;
  
  // Increment TDT.
  epic[E1000_TDT] = (epic[E1000_TDT] + 1) % NTXDESC; 
  return 0;
}

// Try to receive a packet to E1000.
// 
// Returns length of packet on success, < 0 on error.
// Errors are:
// -E_NO_MEM if the receive array of E1000 is empty.
int
try_receive_pkt(void *dstva)
{
  int len;
  size_t idx = (epic[E1000_RDT] + 1) % NRXDESC;
  
  // Check if the transmit array of E1000 is full.
  if (!(rx_desc_array[idx].status & E1000_RXD_STAT_DD)) {
    return -E_NO_MEM;
  }

  // Copy the packet data from buffer.
  len = rx_desc_array[idx].length;
  memmove(dstva, (void*)KADDR(rx_desc_array[idx].buffer_addr), len);

  // Clear status field.
  rx_desc_array[idx].status = 0;

  // Increment RDT.
  epic[E1000_RDT] = idx;
  
  return len;
}
