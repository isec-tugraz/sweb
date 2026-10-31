#pragma once

#include "BDDriver.h"
#include "Mutex.h"
#include "IRQSpinLock.h"

class BDRequest;

class ATADriver : public BDDriver
{
  public:

    typedef enum BD_ATA_MODE_
    {
      BD_PIO_NO_IRQ, BD_PIO, BD_DMA, BD_UDMA
    } BD_ATA_MODES;

    /**
     * Execute one request at a time and wait for completion before releasing the controller.
     */
    uint32 addRequest(BDRequest* br);
    ATADriver(uint16 baseport, uint16 getdrive, uint16 irqnum);
    virtual ~ATADriver()
    {
    }
    ;

    /**
     * sets the current mode to BD_PIO_NO_IRQ while the readSector
     * function is being executed
     *
     */
    int32 rawReadSector(uint32, uint32, void *);

    /**
     * @param 1 sector where it should be started to read
     * @param 2 number of sectors
     * @param 3 buffer where to save all that was read
     *
     */
    int32 readSector(uint32, uint32, void *);

    /**
     * @param 1 sector where it should be started to write
     * @param 2 number of sectors
     * @param 3 buffer, which content should be written to the sectors
     *
     */
    int32 writeSector(uint32, uint32, void *);

    uint32 getNumSectors()
    {
      return numsec;
    }
    ;
    uint32 getSectorSize()
    {
      return 512;
    }
    ;
    void serviceIRQ();

    /**
     * tests if there is an Interrupt Request waiting
     *
     */
    void testIRQ();

    /**
     * tests if the Controller is available
     * false if it is not or the time is elapsed
     *
     */
    bool waitForController(bool resetIfFailed);

    uint32 HPC, SPT; // HEADS PER CYLINDER and SECTORS PER TRACK

  private:

    int32 selectSector(uint32 start_sector, uint32 num_sectors);
    void completeRequest(BDRequest* br);

    uint32 numsec;

    uint16 port;
    uint16 drive;

    uint32 jiffies;

    BD_ATA_MODES mode; // mode see enum BD_ATA_MODES

    BDRequest* active_request_ = nullptr;
    IRQSpinLock request_lock_;

    Mutex lock_;
};

