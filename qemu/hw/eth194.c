/*
 * QEMU ETH194 emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw.h"
#include "pci/pci.h"
#include "net/net.h"
#include "eth194.h"
#include "loader.h"
#include "sysemu/sysemu.h"
#include <stdio.h>

/* debug ETH194 card */
//#define DEBUG_ETH194

#define MAX_ETH_FRAME_SIZE 1514

#define E8390_CMD	0x00  /* The command register (for all pages) */
/* Page 0 register offsets. */
#define EN0_CLDALO	0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	0x01	/* Starting page of ring bfr WR */
#define EN0_CLDAHI	0x02	/* High byte of current local dma addr  RD */
#define EN0_STOPPG	0x02	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR		0x04	/* Transmit status reg RD */
#define EN0_TPSR	0x04	/* Transmit starting page WR */
#define EN0_NCR		0x05	/* Number of collision reg RD */
#define EN0_TCNTLO	0x05	/* Low  byte of tx byte count WR */
#define EN0_FIFO	0x06	/* FIFO RD */
#define EN0_TCNTHI	0x06	/* High byte of tx byte count WR */
#define EN0_ISR		0x07	/* Interrupt status reg RD WR */
#define EN0_CRDALO	0x08	/* low byte of current remote dma address RD */
#define EN0_RSARLO	0x08	/* Remote start address reg 0 */
#define EN0_CRDAHI	0x09	/* high byte, current remote dma address RD */
#define EN0_RSARHI	0x09	/* Remote start address reg 1 */
#define EN0_RCNTLO	0x0a	/* Remote byte count reg WR */
#define EN0_RTL8029ID0	0x0a	/* Realtek ID byte #1 RD */
#define EN0_RCNTHI	0x0b	/* Remote byte count reg WR */
#define EN0_RTL8029ID1	0x0b	/* Realtek ID byte #2 RD */
#define EN0_RSR		0x0c	/* rx status reg RD */
#define EN0_RXCR	0x0c	/* RX configuration reg WR */
#define EN0_TXCR	0x0d	/* TX configuration reg WR */
#define EN0_COUNTER0	0x0d	/* Rcv alignment error counter RD */
#define EN0_DCFG	0x0e	/* Data configuration reg WR */
#define EN0_COUNTER1	0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR		0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	0x0f	/* Rcv missed frame error counter RD */

#define EN1_PHYS        0x11
#define EN1_CURPAG      0x17
#define EN1_MULT        0x18

#define EN2_STARTPG	0x21	/* Starting page of ring bfr RD */
#define EN2_STOPPG	0x22	/* Ending page +1 of ring bfr RD */

#define EN3_CONFIG0	0x33
#define EN3_CONFIG1	0x34
#define EN3_CONFIG2	0x35
#define EN3_CONFIG3	0x36

#define EN3_CURR0       0x32
#define EN3_CURR1       0x34
#define EN3_CURR2       0x37
#define EN3_CURR3       0x38
#define EN3_CURW0       0x3A
#define EN3_CURW1       0x3B
#define EN3_CURW2       0x3C
#define EN3_CURW3       0x3D

/*  Register accessed at EN_CMD, the 8390 base addr.  */
#define E8390_STOP	0x01	/* Stop and reset the chip */
#define E8390_START	0x02	/* Start the chip, clear reset */
#define E8390_TRANS	0x04	/* Transmit a frame */
#define E8390_RREAD	0x08	/* Remote read */
#define E8390_RWRITE	0x10	/* Remote write  */
#define E8390_NODMA	0x20	/* Remote DMA */
#define E8390_PAGE0	0x00	/* Select page chip registers */
#define E8390_PAGE1	0x40	/* using the two high-order bits */
#define E8390_PAGE2	0x80	/* Page 3 is invalid. */

/* Bits in EN0_ISR - Interrupt status register */
#define ENISR_RX	0x01	/* Receiver, no error */
#define ENISR_TX	0x02	/* Transmitter, no error */
#define ENISR_RX_ERR	0x04	/* Receiver, with error */
#define ENISR_TX_ERR	0x08	/* Transmitter, with error */
#define ENISR_OVER	0x10	/* Receiver overwrote the ring */
#define ENISR_COUNTERS	0x20	/* Counters need emptying */
#define ENISR_RDC	0x40	/* remote dma complete */
#define ENISR_RESET	0x80	/* Reset completed */
#define ENISR_ALL	0x3f	/* Interrupts we will enable */

/* Bits in received packet status byte and EN0_RSR*/
#define ENRSR_RXOK	0x01	/* Received a good packet */
#define ENRSR_CRC	0x02	/* CRC error */
#define ENRSR_FAE	0x04	/* frame alignment error */
#define ENRSR_FO	0x08	/* FIFO overrun */
#define ENRSR_MPA	0x10	/* missed pkt */
#define ENRSR_PHY	0x20	/* physical/multicast address */
#define ENRSR_DIS	0x40	/* receiver disable. set in monitor mode */
#define ENRSR_DEF	0x80	/* deferring */

/* Transmitted packet status, EN0_TSR. */
#define ENTSR_PTX 0x01	/* Packet transmitted without error */
#define ENTSR_ND  0x02	/* The transmit wasn't deferred. */
#define ENTSR_COL 0x04	/* The transmit collided at least once. */
#define ENTSR_ABT 0x08  /* The transmit collided 16 times, and was deferred. */
#define ENTSR_CRS 0x10	/* The carrier sense was lost. */
#define ENTSR_FU  0x20  /* A "FIFO underrun" occurred during transmit. */
#define ENTSR_CDH 0x40	/* The collision detect "heartbeat" signal was lost. */
#define ENTSR_OWC 0x80  /* There was an out-of-window collision. */

typedef struct PCIETH194State {
    PCIDevice dev;
    ETH194State eth194;
} PCIETH194State;

void eth194_reset(ETH194State *s)
{
    s->isr = ENISR_RESET;

    s->curr = 0;
    s->curw = 0;
}

static void eth194_update_irq(ETH194State *s)
{
    int isr;
    isr = (s->isr & s->imr) & 0x7f;
#if defined(DEBUG_ETH194)
    printf("ETH194: Set IRQ to %d (%02x %02x)\n",
	   isr ? 1 : 0, s->isr, s->imr);
#endif
    qemu_set_irq(s->irq, (isr != 0));
}

static int eth194_buffer_full(ETH194State *s)
{
    return s->curw == 0;
}

int eth194_can_receive(NetClientState *nc)
{
    ETH194State *s = qemu_get_nic_opaque(nc);

    if (s->cmd & E8390_STOP)
        return 1;
    return !eth194_buffer_full(s);
}

#define MIN_BUF_SIZE 60

ssize_t eth194_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    ETH194State *s = qemu_get_nic_opaque(nc);
    struct eth194_fb fb;
    uint8_t buf1[60];

#if defined(DEBUG_ETH194)
    printf("ETH194: received len=%d\n", size);
#endif

    if (s->cmd & E8390_STOP || eth194_buffer_full(s))
        return -1;

    /* if too small buffer, then expand it */
    if (size < MIN_BUF_SIZE) {
        memcpy(buf1, buf, size);
        memset(buf1 + size, 0, MIN_BUF_SIZE - size);
        buf = buf1;
        size = MIN_BUF_SIZE;
    }

    /* Check if there's no remaining buffer */
    if (s->curw == 0)
	return size;

    fb.df = 0x01;
    cpu_physical_memory_write(s->curw, &fb, 1);
    fb.hf = 0x00;
    cpu_physical_memory_read(s->curw + 2, &fb.nphy, 4);
    fb.cnt = size;
    memcpy(fb.d, buf, size);
    cpu_physical_memory_write(s->curw, &fb, sizeof(fb));
    fb.df = 0x03;
    cpu_physical_memory_write(s->curw, &fb, 1);
    s->curw = fb.nphy;

    s->rsr = ENRSR_RXOK;

    /* FIXME: Actually determine if this is a multicast or not. */
    s->rsr |= ENRSR_PHY;

    /* now we can signal we have received something */
    s->isr |= ENISR_RX;
    eth194_update_irq(s);

    return size;
}

static void eth194_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    ETH194State *s = opaque;
    int offset, page;
    struct eth194_fb fb;

    addr &= 0xf;
#ifdef DEBUG_ETH194
    printf("ETH194: write addr=0x%x val=0x%02x\n", addr, val);
#endif
    if (addr == E8390_CMD) {
        /* control register */
        s->cmd = val;
        if (!(val & E8390_STOP)) { /* START bit makes no sense on RTL8029... */
            s->isr &= ~ENISR_RESET;
            /* test specific case: zero length transfer */
            if ((val & (E8390_RREAD | E8390_RWRITE)) &&
                s->rcnt == 0) {
                s->isr |= ENISR_RDC;
                eth194_update_irq(s);
            }
            if (val & E8390_TRANS) {
		while (s->curr != 0) {
		    cpu_physical_memory_read(s->curr, &fb, sizeof(fb));
		    fb.df = 0x04;
		    cpu_physical_memory_write(s->curr, &fb, 1);
		    qemu_send_packet(qemu_get_queue(s->nic), fb.d, fb.cnt);
		    fb.df = 0x0C;
		    cpu_physical_memory_write(s->curr, &fb, 1);
		    s->curr = fb.nphy;

		    s->tsr = ENTSR_PTX;
		    s->isr |= ENISR_TX;
		    s->isr |= ENISR_RDC;
		    eth194_update_irq(s);
		}

		s->isr |= ENISR_TX;
		s->cmd &= ~E8390_TRANS;
		eth194_update_irq(s);
            }
        }
    } else {
        page = s->cmd >> 6;
        offset = addr | (page << 4);
        switch(offset) {
        case EN0_IMR:
            s->imr = val;
            eth194_update_irq(s);
            break;
        case EN0_TCNTLO:
            s->tcnt = (s->tcnt & 0xff00) | val;
            break;
        case EN0_TCNTHI:
            s->tcnt = (s->tcnt & 0x00ff) | (val << 8);
            break;
        case EN0_RCNTLO:
            s->rcnt = (s->rcnt & 0xff00) | val;
            break;
        case EN0_RCNTHI:
            s->rcnt = (s->rcnt & 0x00ff) | (val << 8);
            break;
        case EN0_RXCR:
            s->rxcr = val;
            break;
        case EN0_ISR:
            s->isr &= ~(val & 0x7f);
            eth194_update_irq(s);
            break;
        case EN1_PHYS ... EN1_PHYS + 5:
            s->phys[offset - EN1_PHYS] = val;
            break;
        case EN1_MULT ... EN1_MULT + 7:
            s->mult[offset - EN1_MULT] = val;
            break;
	case EN3_CURR0:
	    s->rv = 0x07;
	    s->curr = (s->curr & 0xffffff00) | (val <<  0);
	    break;
	case EN3_CURR1:
	    s->rv = 0x03;
	    s->curr = (s->curr & 0xffff00ff) | (val <<  8);
	    break;
	case EN3_CURR2:
	    s->rv = 0x01;
	    s->curr = (s->curr & 0xff00ffff) | (val << 16);
	    break;
	case EN3_CURR3:
	    s->rv = 0x00;
	    s->curr = (s->curr & 0x00ffffff) | (val << 24);
	    break;
	case EN3_CURW0:
	    s->wv = 0x07;
	    s->curw = (s->curw & 0xffffff00) | (val <<  0);
	    break;
	case EN3_CURW1:
	    s->wv = 0x03;
	    s->curw = (s->curw & 0xffff00ff) | (val <<  8);
	    break;
	case EN3_CURW2:
	    s->wv = 0x01;
	    s->curw = (s->curw & 0xff00ffff) | (val << 16);
	    break;
	case EN3_CURW3:
	    s->wv = 0x00;
	    s->curw = (s->curw & 0x00ffffff) | (val << 24);
	    break;
        }
    }
}

static uint32_t eth194_ioport_read(void *opaque, uint32_t addr)
{
    ETH194State *s = opaque;
    int offset, page, ret;

    addr &= 0xf;
    if (addr == E8390_CMD) {
        ret = s->cmd;
    } else {
        page = s->cmd >> 6;
        offset = addr | (page << 4);
        switch(offset) {
        case EN0_TSR:
            ret = s->tsr;
            break;
        case EN0_ISR:
            ret = s->isr;
            break;
        case EN1_PHYS ... EN1_PHYS + 5:
            ret = s->phys[offset - EN1_PHYS];
            break;
        case EN1_MULT ... EN1_MULT + 7:
            ret = s->mult[offset - EN1_MULT];
            break;
        case EN0_RSR:
            ret = s->rsr;
            break;
	case EN0_RTL8029ID0:
	    ret = 0x50;
	    break;
	case EN0_RTL8029ID1:
	    ret = 0x43;
	    break;
	case EN3_CONFIG0:
	    ret = 0;		/* 10baseT media */
	    break;
	case EN3_CONFIG2:
	    ret = 0x40;		/* 10baseT active */
	    break;
	case EN3_CONFIG3:
	    ret = 0x40;		/* Full duplex */
	    break;
        default:
            ret = 0x00;
            break;
	case EN3_CURR0:
	    ret = (s->curr & 0xffffff00);
	    break;
	case EN3_CURR1:
	    ret = (s->curr & 0xffff00ff);
	    break;
	case EN3_CURR2:
	    ret = (s->curr & 0xff00ffff);
	    break;
	case EN3_CURR3:
	    ret = (s->curr & 0x00ffffff);
	    break;
	case EN3_CURW0:
	    ret = (s->curw & 0xffffff00);
	    break;
	case EN3_CURW1:
	    ret = (s->curw & 0xffff00ff);
	    break;
	case EN3_CURW2:
	    ret = (s->curw & 0xff00ffff);
	    break;
	case EN3_CURW3:
	    ret = (s->curw & 0x00ffffff);
	    break;
        }
    }
#ifdef DEBUG_ETH194
    printf("ETH194: read addr=0x%x val=%02x\n", addr, ret);
#endif
    return ret;
}

static int eth194_post_load(void* opaque, int version_id)
{
    ETH194State* s = opaque;

    if (version_id < 2) {
        s->rxcr = 0x0c;
    }
    return 0;
}

const VMStateDescription vmstate_eth194 = {
    .name = "eth194",
    .version_id = 0,
    .minimum_version_id = 0,
    .minimum_version_id_old = 0,
    .post_load = eth194_post_load,
    .fields      = (VMStateField []) {
        VMSTATE_UINT8_V(rxcr, ETH194State, 2),
        VMSTATE_UINT8(cmd, ETH194State),
        VMSTATE_UINT8(tsr, ETH194State),
        VMSTATE_UINT16(tcnt, ETH194State),
        VMSTATE_UINT16(rcnt, ETH194State),
        VMSTATE_UINT8(rsr, ETH194State),
        VMSTATE_UINT8(isr, ETH194State),
        VMSTATE_UINT8(imr, ETH194State),
        VMSTATE_UINT32(curr, ETH194State),
        VMSTATE_UINT32(curw, ETH194State),
        VMSTATE_UINT8(rv, ETH194State),
        VMSTATE_UINT8(wv, ETH194State),
        VMSTATE_BUFFER(phys, ETH194State),
        VMSTATE_BUFFER(mult, ETH194State),
        VMSTATE_UNUSED(4), /* was irq */
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_pci_eth194 = {
    .name = "eth194",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_PCI_DEVICE(dev, PCIETH194State),
        VMSTATE_STRUCT(eth194, PCIETH194State, 0, vmstate_eth194, ETH194State),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t eth194_read(void *opaque, hwaddr addr,
                            unsigned size)
{
    ETH194State *s = opaque;

    if (addr == 0x1f)
    {
	eth194_reset(s);
	return 0;
    }

    return eth194_ioport_read(s, addr);
}

static void eth194_write(void *opaque, hwaddr addr,
                         uint64_t data, unsigned size)
{
    ETH194State *s = opaque;

    eth194_ioport_write(s, addr, data);
}

static const MemoryRegionOps eth194_ops = {
    .read = eth194_read,
    .write = eth194_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***********************************************************/
/* PCI ETH194 definitions */

void eth194_setup_io(ETH194State *s, unsigned size)
{
    memory_region_init_io(&s->io, &eth194_ops, s, "eth194", size);
}

static void eth194_cleanup(NetClientState *nc)
{
    ETH194State *s = qemu_get_nic_opaque(nc);

    s->nic = NULL;
}

static NetClientInfo net_eth194_info = {
    .type = NET_CLIENT_OPTIONS_KIND_NIC,
    .size = sizeof(NICState),
    .can_receive = eth194_can_receive,
    .receive = eth194_receive,
    .cleanup = eth194_cleanup,
};

static int pci_eth194_init(PCIDevice *pci_dev)
{
    PCIETH194State *d = DO_UPCAST(PCIETH194State, dev, pci_dev);
    ETH194State *s;
    uint8_t *pci_conf;

    pci_conf = d->dev.config;
    pci_conf[PCI_INTERRUPT_PIN] = 1; /* interrupt pin A */

    s = &d->eth194;
    eth194_setup_io(s, 0x100);
    pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->io);
    s->irq = d->dev.irq[0];

    qemu_macaddr_default_if_unset(&s->c.macaddr);
    eth194_reset(s);

    s->nic = qemu_new_nic(&net_eth194_info, &s->c,
                          object_get_typename(OBJECT(pci_dev)), pci_dev->qdev.id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->c.macaddr.a);

    add_boot_device_path(s->c.bootindex, &pci_dev->qdev, "/ethernet-phy@0");

    return 0;
}

static void pci_eth194_exit(PCIDevice *pci_dev)
{
    PCIETH194State *d = DO_UPCAST(PCIETH194State, dev, pci_dev);
    ETH194State *s = &d->eth194;

    memory_region_destroy(&s->io);
    qemu_del_nic(s->nic);
}

static Property eth194_properties[] = {
    DEFINE_NIC_PROPERTIES(PCIETH194State, eth194.c),
    DEFINE_PROP_END_OF_LIST(),
};

static void eth194_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = pci_eth194_init;
    k->exit = pci_eth194_exit;
    k->romfile = "pxe-ne2k_pci.rom",
    k->vendor_id = 0x0CA1;
    k->device_id = 0xE194;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    dc->vmsd = &vmstate_pci_eth194;
    dc->props = eth194_properties;
}

static const TypeInfo eth194_info = {
    .name          = "eth194",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIETH194State),
    .class_init    = eth194_class_init,
};

static void eth194_register_types(void)
{
    type_register_static(&eth194_info);
}

type_init(eth194_register_types)
