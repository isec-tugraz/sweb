#include "KernelFileHandle.h"
#include "Inode.h"
#include "assert.h"
#include "Superblock.h"
#include "FileSystemType.h"


KernelFileHandle::KernelFileHandle(Inode* inode, Dentry* dentry, uint32 flag) :
    uid(0), gid(0), version(0), f_superblock_(0), f_inode_(inode), f_dentry_(dentry), flag_(flag),
    file_descriptor_count_(0)
{
    f_inode_->incRefCount();
}

KernelFileHandle::~KernelFileHandle()
{
  debug(VFS_FILE, "Destroying file\n");

  assert(file_descriptor_count_ == 0 && "KernelFileHandle to be destroyed still has open file descriptors");
  assert(f_inode_);

  if(!f_inode_->decRefCount())
  {
      debug(VFS_FILE, "No more references to %s inode %p left after closing file, deleting\n",
            f_inode_->getSuperblock()->getFSType()->getFSName(), f_inode_);
    f_inode_->getSuperblock()->deleteInode(f_inode_);
  }
}

uint32 KernelFileHandle::getSize()
{
  return f_inode_->getSize();
}

l_off_t KernelFileHandle::lseek(l_off_t offset, uint8 origin)
{
  if (origin == SEEK_SET)
    offset_ = offset;
  else if (origin == SEEK_CUR)
    offset_ += offset;
  else if (origin == SEEK_END)
    offset_ = f_inode_->getSize() + offset;
  else
    return -1;

  return offset_;
}




void KernelFileHandle::addFileDescriptor()
{
  ++file_descriptor_count_;
}

int KernelFileHandle::closeFileDescriptor()
{
  assert(file_descriptor_count_ > 0);
  if (--file_descriptor_count_ == 0)
  {
    debug(VFS_FILE, "Releasing file\n");
    return getInode()->release(this);
  }
  return 0;
}
