#include "FileDescriptorTable.h"
#include "kprintf.h"
#include "debug.h"
#include "assert.h"
#include "Mutex.h"
#include "ScopeLock.h"
#include "KernelFileHandle.h"

FileDescriptorTable global_file_descriptor_table;

FileDescriptorTable::FileDescriptorTable() :
    file_descriptors_(), fd_lock_("File descriptor table lock"), next_fd_(3)
{
}

FileDescriptorTable::~FileDescriptorTable()
{
  while (!file_descriptors_.empty())
  {
    auto it = file_descriptors_.begin();
    KernelFileHandle* file_handle = it->second;
    file_descriptors_.erase(it->first);
    file_handle->closeFileDescriptor();
  }
}

uint32 FileDescriptorTable::add(KernelFileHandle* file_handle)
{
  assert(file_handle);
  ScopeLock lock(fd_lock_);
  uint32 fd = next_fd_++;
  assert(file_descriptors_.find(fd) == file_descriptors_.end());
  file_descriptors_[fd] = file_handle;
  file_handle->addFileDescriptor();
  debug(VFS_FILE, "File descriptor table, add fd %u for file handle %p\n", fd, file_handle);
  return fd;
}

KernelFileHandle* FileDescriptorTable::remove(uint32 fd)
{
  ScopeLock lock(fd_lock_);
  auto it = file_descriptors_.find(fd);
  if (it == file_descriptors_.end())
    return 0;

  KernelFileHandle* file_handle = it->second;
  file_descriptors_.erase(fd);
  debug(VFS_FILE, "File descriptor table, remove fd %u\n", fd);
  return file_handle;
}

int FileDescriptorTable::close(uint32 fd)
{
  KernelFileHandle* file_handle = remove(fd);
  if (!file_handle)
    return -1;

  file_handle->closeFileDescriptor();
  return 0;
}

KernelFileHandle* FileDescriptorTable::get(uint32 fd)
{
  ScopeLock lock(fd_lock_);
  auto it = file_descriptors_.find(fd);
  return it == file_descriptors_.end() ? 0 : it->second;
}
