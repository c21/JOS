#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

/*
 * Vendor ID and device ID for PCI E1000 device.
 */
#define E1000_VENDOR_ID_82540EM         0x8086
#define E1000_DEV_ID_82540EM            0x100E

/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */
#define E1000_STATUS   (0x00008/4)  /* Device Status - RO */
  /* Transmit related registers */
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW */
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW */
#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    (0x03804/4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818/4)  /* TX Descripotr Tail - RW */
  /* Receive related registers */
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW */
#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    (0x02804/4)  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW */
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW */
#define E1000_MTA      (0x05200/4)  /* Multicast Table Array - RW Array */
#define E1000_RAL      (0x05400/4)  /* Receive Address Low - RW */
#define E1000_RAH      (0x05404/4)  /* Receive Address High - RW */


/* Register Bit Masks */
  /* Transmit Descriptor bit definitions */
#define E1000_TXD_CMD_EOP    0x00000001    /* End of Packet */
#define E1000_TXD_CMD_RS     0x00000008    /* Report Status */
#define E1000_TXD_STAT_DD    0x00000001    /* Descriptor Done */
  /* Transmit Control */
#define E1000_TCTL_EN        0x00000002    /* enable tx */
#define E1000_TCTL_PSP       0x00000008    /* pad short packets */
  /* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD    0x01          /* Descriptor Done */
  /* Receive Control */
#define E1000_RCTL_EN        0x00000002    /* enable */
#define E1000_RCTL_LBM_NO    0x00000000    /* no loopback mode */
#define E1000_RCTL_BAM       0x00008000    /* broadcast enable */
#define E1000_RCTL_SZ_2048   0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC     0x04000000    /* Strip Ethernet CRC */ 
  /* Receive Address */
#define E1000_RAH_AV         0x80000000    /* Receive descriptor valid */

#define NTXDESC   16    /* Number of descriptors in transmit descriptor array */
#define NRXDESC   128   /* Number of descriptors in receive descriptor array */
#define PKTSIZE   1518  /* The maximal bytes of Ethernet packet */
#define RXBSIZE   2048  /* The bytes of one receive buffer */

/* 
 * Transmit Descriptor
 */
struct tx_desc
{
  uint64_t addr;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;
};

/*
 * Transmit Packet Buffer
 */
struct tx_buf
{
  uint8_t data[PKTSIZE];  /* Packet to transmit */
};

/* 
 * Receive Descriptor
 */
struct rx_desc
{
  uint64_t buffer_addr;
  uint16_t length;
  uint16_t csum;
  uint8_t status;
  uint8_t errors;
  uint16_t special;
};

/*
 * Receive Packet Buffer
 */
struct rx_buf
{
  uint8_t data[RXBSIZE];  /* Packet to receive */       
};

int pci_e1000_attach(struct pci_func *f);
int try_transmit_pkt(void *srcva, size_t len);
int try_receive_pkt(void *dstva);

#endif  // JOS_KERN_E1000_H
