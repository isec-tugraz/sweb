#include "IDEDriver.h"

#include "BDManager.h"
#include "BDVirtualDevice.h"
#include "ATADriver.h"
#include "ports.h"
#include "kstring.h"
#include "ArchInterrupts.h"
#include "kprintf.h"

uint32 IDEDriver::doDeviceDetection()
{
  static const uint16 ATA_PRIMARY_PORT = 0x1F0;
  static const uint16 ATA_PRIMARY_CONTROL_PORT = 0x3F6;
  static const uint8 ATA_MASTER = 0xA0;
  static const char* name = "idea";

  uint32 jiffies = 0;
  const uint16 sector_count_port = ATA_PRIMARY_PORT + 2;
  const uint16 sector_number_port = ATA_PRIMARY_PORT + 3;

  debug(IDE_DRIVER, "doDetection: Detecting primary master\n");
  outportbp(ATA_PRIMARY_CONTROL_PORT, 0x00);
  outportb(ATA_PRIMARY_PORT + 6, ATA_MASTER);
  outportb(0x80, 0x00);

  outportb(sector_count_port, 0x55);
  outportb(0x80, 0x00);
  outportb(sector_number_port, 0xAA);
  outportb(0x80, 0x00);
  outportb(sector_count_port, 0xAA);
  outportb(0x80, 0x00);
  outportb(sector_number_port, 0x55);
  outportb(0x80, 0x00);
  outportb(sector_count_port, 0x55);
  outportb(0x80, 0x00);
  outportb(sector_number_port, 0xAA);
  outportb(0x80, 0x00);

  uint8 sector_count = inportb(sector_count_port);
  outportb(0x80, 0x00);
  uint8 sector_number = inportb(sector_number_port);
  outportb(0x80, 0x00);

  if (sector_count != 0x55 || sector_number != 0xAA)
  {
    debug(IDE_DRIVER, "doDetection: Primary master not found!\n");
    return 0;
  }

  outportbp(ATA_PRIMARY_CONTROL_PORT, 0x04);
  outportbp(ATA_PRIMARY_CONTROL_PORT, 0x00);

  while (!(inportbp(ATA_PRIMARY_PORT + 7) & 0x58) && jiffies++ < IO_TIMEOUT)
    ArchInterrupts::yieldIfIFSet();

  if (jiffies >= IO_TIMEOUT)
  {
    debug(IDE_DRIVER, "doDetection: Still busy after reset!\n");
    return 0;
  }

  outportbp(ATA_PRIMARY_PORT + 6, ATA_MASTER);

  jiffies = 0;
  while (inportbp(ATA_PRIMARY_PORT + 7) & 0x80 && jiffies++ < IO_TIMEOUT);

  uint8 c1 = inportbp(ATA_PRIMARY_PORT + 2);
  uint8 c2 = inportbp(ATA_PRIMARY_PORT + 3);
  debug(IDE_DRIVER, "c1 = %x, c2 = %x\n", c1, c2);
  if (c1 != 0x01 && c2 != 0x01)
  {
    debug(IDE_DRIVER, "doDetection: Not found after reset!\n");
    return 0;
  }

  uint8 status = inportbp(ATA_PRIMARY_PORT + 7);
  uint8 cylinder_low = inportbp(ATA_PRIMARY_PORT + 4);
  uint8 cylinder_high = inportbp(ATA_PRIMARY_PORT + 5);

  if (((cylinder_low == 0x14) && (cylinder_high == 0xEB)) ||
      ((cylinder_low == 0x69) && (cylinder_high == 0x96)))
  {
    debug(IDE_DRIVER, "doDetection: Found ATAPI, CDROM not supported\n");
    return 0;
  }

  if (status == 0)
  {
    debug(IDE_DRIVER, "doDetection: Unknown harddisk!\n");
    return 0;
  }

  if ((cylinder_low == 0x00) && (cylinder_high == 0x00))
    debug(IDE_DRIVER, "doDetection: Found PATA\n");
  else if ((cylinder_low == 0x3C) && (cylinder_high == 0xC3))
    debug(IDE_DRIVER, "doDetection: Running SATA device as PATA in compatibility mode\n");
  else
  {
    debug(IDE_DRIVER, "doDetection: Unknown harddisk!\n");
    return 0;
  }

  ATADriver* drv = new ATADriver();
  BDVirtualDevice* bdv = new BDVirtualDevice(drv, 0, drv->getNumSectors(), drv->getSectorSize(), name, true);
  BDManager::getInstance()->addVirtualDevice(bdv);
  processMBR(drv, 0, drv->SPT, name);

  return 0;
}

int32 IDEDriver::processMBR(BDDriver* drv, uint32 sector, uint32 SPT, const char *name)
{
  uint32 offset = 0, numsec = 0;
  uint16 buff[256]; // read buffer
  debug(IDE_DRIVER, "processMBR:reading MBR\n");

  static uint32 part_num = 0;
//   char part_num_str[2];
//   char part_name[10];

  uint32 read_res = ((ATADriver*)drv)->rawReadSector(sector, 1, (void *) buff);

  if (read_res != 0)
  {
    debug(IDE_DRIVER, "processMBR: drv returned BD_ERROR\n");
    return -1;
  }

  MBR *mbr = (MBR *) buff;

  if (mbr->signature == 0xAA55)
  {
    debug(IDE_DRIVER, "processMBR: | Valid PC MBR | \n");
    FP * fp = (FP *) mbr->parts;
    uint32 i;
    for (i = 0; i < 4; i++, fp++)
    {
      switch (fp->systid)
      {
        case 0x00:
          // do nothing
          break;
        case 0x05: // DOS extended partition
        case 0x0F: // Windows extended partition
        case 0x85: // linux extended partition
          debug(IDE_DRIVER, "ext. part. at: %d \n", fp->relsect);
          if (processMBR(drv, sector + fp->relsect, SPT, name) == -1)
            processMBR(drv, sector + fp->relsect - SPT, SPT, name);
          break;
        default:
          // offset = fp->relsect - SPT;
          offset = fp->relsect;
          numsec = fp->numsect;

          char part_name[6];
          strncpy(part_name, name, 4);
          part_name[4] = part_num + '0';
          part_name[5] = 0;
          part_num++;
          BDVirtualDevice *bdv = new BDVirtualDevice(drv, offset, numsec, drv->getSectorSize(), part_name, true);

          // set Partition Type (FileSystem identifier)
          bdv->setPartitionType(fp->systid);

          BDManager::getInstance()->addVirtualDevice(bdv);
          break;
      }
    }
  }
  else
  {
    debug(IDE_DRIVER, "processMBR: | Invalid PC MBR %d | \n", mbr->signature);
    return -1;
  }

  debug(IDE_DRIVER, "processMBR:, done with partitions \n");
  return 0;
}
