#include "Superblock.h"
#include "assert.h"
#include "Dentry.h"
#include "Inode.h"
#include "KernelFileHandle.h"
#include "FileSystemType.h"

Superblock::Superblock(FileSystemType* fs_type, size_t s_dev) :
    s_magic_(0), s_type_(fs_type), s_dev_(s_dev), s_flags_(0), s_root_(0), s_mountpoint_(0)
{
    debug(SUPERBLOCK, "%s Superblock created\n", s_type_->getFSName());
}

Superblock::~Superblock()
{
    debug(SUPERBLOCK, "%s Superblock destroyed\n", s_type_->getFSName());
}

void Superblock::deleteInode(Inode* inode)
{
  assert(inode != 0);
  dirty_inodes_.remove(inode);
  used_inodes_.remove(inode);
  all_inodes_.remove(inode);
  delete inode;
}

Dentry* Superblock::getRoot()
{
  return s_root_;
}

Dentry* Superblock::getMountPoint()
{
  return s_mountpoint_;
}

void Superblock::setMountPoint(Dentry* mountpoint)
{
    s_mountpoint_ = mountpoint;
}

FileSystemType* Superblock::getFSType()
{
  return (FileSystemType*) s_type_;
}


int Superblock::fileHandleOpened(KernelFileHandle* file_handle)
{
    Inode* inode = file_handle->getInode();
    assert(inode->getSuperblock() == this);

    if(ustl::find(s_file_handles_.begin(), s_file_handles_.end(), file_handle) != s_file_handles_.end())
    {
        return -1;
    }

    s_file_handles_.push_back(file_handle);
    used_inodes_.push_back(inode);

    return 0;
}

int Superblock::fileHandleReleased(KernelFileHandle* file_handle)
{
    Inode* inode = file_handle->getInode();
    assert(inode->getSuperblock() == this);

    s_file_handles_.remove(file_handle);

    if (inode->getNumOpenFileHandles() == 0)
    {
        used_inodes_.remove(inode);
    }

    return 0;
}

void Superblock::releaseAllOpenFiles()
{
    debug(SUPERBLOCK, "Releasing all open files\n");
    while(!s_file_handles_.empty())
    {
        KernelFileHandle* file_handle = s_file_handles_.front();
        file_handle->getInode()->release(file_handle);
    }

    assert(s_file_handles_.empty());
}

void Superblock::deleteAllInodes()
{
    for (Inode* inode : all_inodes_)
    {
        while(!inode->getDentrys().empty())
        {
            delete inode->getDentrys().front();
        }

        debug(SUPERBLOCK, "~Superblock write inode to disc\n");
        writeInode(inode);

        debug(SUPERBLOCK, "~Superblock delete inode %p\n", inode);
        delete inode;
    }

    all_inodes_.clear();
}
