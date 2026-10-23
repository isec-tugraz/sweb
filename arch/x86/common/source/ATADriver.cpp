#include "ATADriver.h"

#include "BDManager.h"
#include "BDRequest.h"
#include "ArchInterrupts.h"
#include "APIC.h"
#include "ports.h"

#include "Scheduler.h"
#include "kprintf.h"

#include "Thread.h"

#define TIMEOUT_WARNING() do { kprintfd("%s:%d: timeout. THIS MIGHT CAUSE SERIOUS TROUBLE!\n", __PRETTY_FUNCTION__, __LINE__); } while (0)

#define TIMEOUT_CHECK(CONDITION,BODY) jiffies = 0;\
                                       while((CONDITION) && jiffies++ < IO_TIMEOUT)\
                                         ArchInterrupts::yieldIfIFSet();\
                                       if(jiffies >= IO_TIMEOUT)\
                                       {\
                                         BODY;\
                                       }

ATADriver::ATADriver( uint16 baseport, uint16 getdrive, uint16 irqnum ) : lock_("ATADriver::lock_")
{
  debug(ATA_DRIVER, "ctor: Entered with irgnum %d and baseport %d!!\n", irqnum, baseport);

  jiffies = 0;
  port = baseport;
  drive= (getdrive == 0 ? 0xA0 : 0xB0);

  debug(ATA_DRIVER, "ctor: Requesting disk geometry !!\n");

  outportbp (port + 6, drive);  // Get first drive
  outportbp (port + 7, 0xEC);   // Get drive info data
  TIMEOUT_CHECK(inportbp(port + 7) != 0x58,TIMEOUT_WARNING(); return;);

  uint16 dd[256];

  for (uint32 dd_off = 0; dd_off != 256; dd_off++) // Read "sector" 512 b
    dd [dd_off] = inportw ( port );

  debug(ATA_DRIVER, "max. original PIO support: %x, PIO3 support: %x, PIO4 support: %x\n", (dd[51] >> 8), (dd[64] & 0x1) != 0, (dd[64] & 0x2) != 0);

  debug(ATA_DRIVER, "ctor: Disk geometry read !!\n");

  HPC = dd[3];
  SPT = dd[6];
  uint32 CYLS = dd[1];
  numsec = CYLS * HPC * SPT;

  bool interrupt_context = ArchInterrupts::disableInterrupts();
  ArchInterrupts::enableInterrupts();

  APIC::enableIRQ( irqnum );

  testIRQ( );
  if( !interrupt_context )
    ArchInterrupts::disableInterrupts();
  irq = irqnum;
  debug(ATA_DRIVER, "ctor: mode: %d !!\n", mode );

  debug(ATA_DRIVER, "ctor: Driver created !!\n");
  return;
}

void ATADriver::testIRQ( )
{
  mode = BD_PIO;

  BDManager::getInstance()->probeIRQ = true;
  readSector( 0, 1, 0 );

  TIMEOUT_CHECK(BDManager::getInstance()->probeIRQ,mode = BD_PIO_NO_IRQ;);
}

int32 ATADriver::rawReadSector ( uint32 start_sector, uint32 num_sectors, void *buffer )
{
  assert(system_state == BOOTING && "Raw reads are only used during device detection");
  BD_ATA_MODES old_mode = mode;
  mode = BD_PIO_NO_IRQ;
  uint32 result = readSector ( start_sector, num_sectors, buffer );
  mode = old_mode;

  return result;
}

int32 ATADriver::selectSector(uint32 start_sector, uint32 num_sectors)
{
  /* Wait for drive to clear BUSY */
  TIMEOUT_CHECK(inportbp(port + 7) & 0x80,TIMEOUT_WARNING(); return -1;);

  //LBA: linear base address of the block
  //CYL: value of the cylinder CHS coordinate
  //HPC: number of heads per cylinder for the disk
  //HEAD: value of the head CHS coordinate
  //SPT: number of sectors per track for the disk
  //SECT: value of the sector CHS coordinate
  //TEMP: buffer to hold a temporary value

  uint32 LBA = start_sector;
  uint32 cyls = LBA / (HPC * SPT);
  uint32 TEMP = LBA % (HPC * SPT);
  uint32 head = TEMP / SPT;
  uint32 sect = TEMP % SPT + 1;

  uint8 high = cyls >> 8;
  uint8 lo = cyls & 0x00FF;

  //debug(ATA_DRIVER, "readSector:(drive | head): %d, num_sectors: %d, sect: %d, lo: %d, high: %d!!\n",(drive | head),num_sectors,sect,lo,high);

  outportbp(port + 6, (drive | head)); // drive and head selection
  outportbp(port + 2, num_sectors); // number of sectors to read
  outportbp(port + 3, sect); // starting sector
  outportbp(port + 4, lo); // cylinder low
  outportbp(port + 5, high); // cylinder high

  /* Wait for drive to set DRDY */
  TIMEOUT_CHECK((!inportbp(port + 7)) & 0x40,TIMEOUT_WARNING(); return -1;);

  return 0;
}

int32 ATADriver::readSector ( uint32 start_sector, uint32 num_sectors, void *buffer )
{
  assert(buffer || (start_sector == 0 && num_sectors == 1));
  if (selectSector(start_sector, num_sectors) != 0)
    return -1;

  for (int i = 0;; ++i)
  {
    /* Write the command code to the command register */
    outportbp(port + 7, 0x20); // command

    if (mode != BD_PIO_NO_IRQ)
      return 0;

    jiffies = 0;
    while (inportbp(port + 7) != 0x58 && jiffies++ < IO_TIMEOUT)
      ArchInterrupts::yieldIfIFSet();
    if (jiffies >= IO_TIMEOUT)
    {
      if (i == 3)
      {
        TIMEOUT_WARNING();
        return -1;
      }
    }
    else
      break;
  }

  if (buffer)
  {
    uint32 counter;
    uint16 *word_buff = (uint16 *) buffer;
    for (counter = 0; counter != (256*num_sectors); counter++)  // read sector
        word_buff [counter] = inportw ( port );
  }
  /* Wait for drive to clear BUSY */
  TIMEOUT_CHECK(inportbp(port + 7) & 0x80,TIMEOUT_WARNING(); return -1;);

  //debug(ATA_DRIVER, "readSector:Read successfull !!\n");
  return 0;  
}

int32 ATADriver::writeSector ( uint32 start_sector, uint32 num_sectors, void * buffer )
{
  assert(buffer);
  if (selectSector(start_sector, num_sectors) != 0)
    return -1;

  uint16 *word_buff = (uint16 *) buffer;

  /* Write the command code to the command register */
  outportbp( port + 7, 0x30 );           // command

  TIMEOUT_CHECK(inportbp(port + 7) != 0x58,TIMEOUT_WARNING(); return -1;);


  uint32 count2 = (256*num_sectors);
  if( mode != BD_PIO_NO_IRQ )
    count2 = 256;

  uint32 counter;
  for (counter = 0; counter != count2; counter++) 
      outportw ( port, word_buff [counter] );
 
  /* Wait for drive to clear BUSY */
  TIMEOUT_CHECK(inportbp(port + 7) & 0x80,TIMEOUT_WARNING(); return -1;);

  /* Write flush code to the command register */
  outportbp (port + 7, 0xE7);
    
  /* Wait for drive to clear BUSY */
  TIMEOUT_CHECK(inportbp(port + 7) & 0x80,TIMEOUT_WARNING(); return -1;);

  return 0;
}

uint32 ATADriver::addRequest(BDRequest* br)
{
  ScopeLock lock(lock_); // Only one command may use the controller at a time.
  bool interrupts_enabled = request_lock_.acquire();
  assert(!active_request_);
  if (mode != BD_PIO_NO_IRQ)
    active_request_ = br;

  int32 result = -1;
  switch (br->getCmd())
  {
    case BDRequest::BD_READ:
      result = readSector(br->getStartBlock(), br->getNumBlocks(), br->getBuffer());
      break;
    case BDRequest::BD_WRITE:
      result = writeSector(br->getStartBlock(), br->getNumBlocks(), br->getBuffer());
      break;
    default:
      break;
  }

  if (result != 0 || mode == BD_PIO_NO_IRQ)
  {
    active_request_ = nullptr;
    br->setStatus(result != 0 ? BDRequest::BD_ERROR : BDRequest::BD_DONE);
  }

  while (currentThread && br->getStatus() == BDRequest::BD_QUEUED)
  {
    // The IRQ takes the same lock: completion cannot slip between this check and sleeping.
    currentThread->setState(Sleeping);
    request_lock_.release(false);
    ArchThreads::yield(); // Switch with interrupts still disabled, as in Scheduler::sleep().
    request_lock_.acquire();
  }
  request_lock_.release(interrupts_enabled);
  return 0;
}

bool ATADriver::waitForController( bool resetIfFailed = true )
{
  uint32 jiffies = 0;
  while( inportbp( port + 7 ) != 0x58  && jiffies++ < IO_TIMEOUT)
    ArchInterrupts::yieldIfIFSet();

  if(jiffies >= IO_TIMEOUT )
  {
    debug(ATA_DRIVER, "waitForController: controler still not ready\n");
    if( resetIfFailed )
    {
      debug(ATA_DRIVER, "waitForController: reseting\n");
      outportbp( port + 0x206, 0x04 );
      outportbp( port + 0x206, 0x00 ); // RESET
    }
    return false;
  }
  return true;
}

void ATADriver::completeRequest(BDRequest* br)
{
  assert(active_request_ == br);
  active_request_ = nullptr;
  if (br->getThread())
    Scheduler::instance()->wake(br->getThread());
}

void ATADriver::serviceIRQ()
{
  ScopeIRQSpinLock lock(request_lock_);
  if( mode == BD_PIO_NO_IRQ )
    return;

  if( active_request_ == 0 )
  {
    debug(ATA_DRIVER, "serviceIRQ: IRQ without request!!\n");
    inportbp(port + 7); // Acknowledge a late interrupt after completing or rejecting a command.
    return; // not my interrupt
  }

  BDRequest* br = active_request_;
  debug(ATA_DRIVER, "serviceIRQ: Found active request!!\n");

  uint16* word_buff = (uint16*) br->getBuffer();
  uint32 counter;
  uint32 blocks_done = br->getBlocksDone();

  if( br->getCmd() == BDRequest::BD_READ )
  {
    if( !waitForController() )
    {
      br->setStatus( BDRequest::BD_ERROR );
      completeRequest(br);
      return;
    }

    for(counter = blocks_done * 256; counter!=(blocks_done + 1) * 256; counter++ )
      word_buff [counter] = inportw ( port );

    blocks_done++;
    br->setBlocksDone( blocks_done );

    if( blocks_done == br->getNumBlocks() )
    {
      br->setStatus( BDRequest::BD_DONE );
      completeRequest(br);
    }
  }
  else if( br->getCmd() == BDRequest::BD_WRITE )
  {
    blocks_done++;
    if( blocks_done == br->getNumBlocks() )
    {
      debug(ATA_DRIVER, "serviceIRQ:All done!!\n");
      br->setStatus( BDRequest::BD_DONE );
      debug(ATA_DRIVER, "serviceIRQ:Waking up thread!!\n");
      completeRequest(br);
    }
    else
    {
      if( !waitForController() )
      {
        br->setStatus( BDRequest::BD_ERROR );
        completeRequest(br);
        return;
      }
  
      for(counter = blocks_done*256; counter != (blocks_done + 1) * 256; counter++ )
        outportw ( port, word_buff [counter] );

      br->setBlocksDone( blocks_done );
    }
  }
  else
  {
    blocks_done = br->getNumBlocks();
    br->setStatus( BDRequest::BD_ERROR );
    completeRequest(br);
  }

  debug(ATA_DRIVER, "serviceIRQ:Request handled!!\n");
}
