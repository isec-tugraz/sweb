#include "MinixFSFileHandle.h"
#include "MinixFSInode.h"
#include "Inode.h"

MinixFSFileHandle::MinixFSFileHandle(Inode* inode, Dentry* dentry, uint32 flag) :
    KernelFileHandle(inode, dentry, flag)
{
  f_superblock_ = inode->getSuperblock();
  offset_ = 0;
}

MinixFSFileHandle::~MinixFSFileHandle()
{
}

int32 MinixFSFileHandle::read(char *buffer, size_t count, l_off_t offset)
{
  if (((flag_ & O_RDONLY) || (flag_ & O_RDWR)) && (f_inode_->getMode() & A_READABLE))
  {
    int32 read_bytes = f_inode_->readData(offset_ + offset, count, buffer);
    offset_ += read_bytes;
    return read_bytes;
  }
  else
  {
    // ERROR_FF
    return -1;
  }
}

int32 MinixFSFileHandle::write(const char *buffer, size_t count, l_off_t offset)
{
  if (((flag_ & O_WRONLY) || (flag_ & O_RDWR)) && (f_inode_->getMode() & A_WRITABLE))
  {
    int32 written = f_inode_->writeData(offset_ + offset, count, buffer);
    offset_ += written;
    return written;
  }
  else
  {
    // ERROR_FF
    return -1;
  }
}

int32 MinixFSFileHandle::flush()
{
  ((MinixFSInode *) f_inode_)->flush();
  return 0;
}

