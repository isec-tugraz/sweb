#ifdef EXE2MINIXFS
#include "types.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "Dentry.h"
#include "FileSystemInfo.h"
#include "Superblock.h"
#include "MinixFSType.h"
#include "MinixFSSuperblock.h"
#include "VfsSyscall.h"
#include "VfsMount.h"

Superblock* superblock_;
FileSystemInfo* default_working_dir;

struct __attribute__((packed)) PartitionEntry
{
  uint8 bootable;
  uint8 first_head;
  uint8 first_sector;
  uint8 first_cylinder;
  uint8 type;
  uint8 last_head;
  uint8 last_sector;
  uint8 last_cylinder;
  uint32 first_lba;
  uint32 num_sectors;
};

struct __attribute__((packed)) MasterBootRecord
{
  uint8 boot_code[446];
  PartitionEntry partitions[4];
  uint16 signature;
};

static_assert(sizeof(MasterBootRecord) == 512, "Invalid MBR layout");


FileSystemInfo* getcwd() { return default_working_dir; }

// obviously NOT atomic, we need this for compatability in single threaded host code
size_t atomic_add(size_t& x,size_t y)
{
  x += y;
  return x-y;
}

int main(int argc, char *argv[])
{
  if (argc < 3 || argc % 2 == 0)
  {
    printf("Syntax: %s <filename of minixfs-formatted image> <partition number> [file1-src file1-dest [file2-src file2-dest [....]]]\n",
        argv[0]);
    return -1;
  }

  FILE* image_fd = fopen(argv[1], "r+b");

  if (image_fd == 0)
  {
    printf("exe2minixfs: Error opening %s\n", argv[1]);
    return -1;
  }

  char* end;
  size_t partition = strtoul(argv[2],&end,10);
  if (strlen(end) != 0)
  {
    fclose(image_fd);
    printf("exe2minixfs: partition has to be a number!\n");
    return -1;
  }
  if (partition > 4)
  {
    fclose(image_fd);
    printf("exe2minixfs: partition has to be 0 for the whole image or in the range 1-4\n");
    return -1;
  }

  size_t partition_offset = 0;
  size_t partition_size;
  if (partition)
  {
    MasterBootRecord mbr;
    fseek(image_fd, 0, SEEK_SET);
    if (fread(&mbr, sizeof(mbr), 1, image_fd) != 1)
    {
      fclose(image_fd);
      printf("exe2minixfs: failed to read the MBR from %s\n", argv[1]);
      return -1;
    }
    if (mbr.signature != 0xaa55)
      printf("exe2minixfs: warning: invalid MBR signature\n");

    PartitionEntry* entry = &mbr.partitions[partition - 1];
    if (!entry->type || !entry->num_sectors)
    {
      fclose(image_fd);
      printf("exe2minixfs: partition %zu does not exist\n", partition);
      return -1;
    }
    partition_offset = (size_t)entry->first_lba * 512;
    partition_size = (size_t)entry->num_sectors * 512;
  }
  else
  {
    fseek(image_fd, 0, SEEK_END);
    partition_size = ftell(image_fd);
  }

  MinixFSType* minixfs_type = new MinixFSType();

  superblock_ = (Superblock*) new MinixFSSuperblock(minixfs_type, (size_t)image_fd,
                                                   partition_offset, partition_size);
  Dentry *root = superblock_->getRoot();
  superblock_->setMountPoint(root);
  Dentry *mount_point = superblock_->getMountPoint();
  mount_point->setMountedRoot(mount_point);

  VfsMount vfs_dummy_(nullptr, mount_point, root, superblock_, 0);


  default_working_dir = new FileSystemInfo();
  Path root_path(root, &vfs_dummy_);
  default_working_dir->setRoot(root_path);
  default_working_dir->setPwd(root_path);

  for (int32 i = 2; i <= argc / 2; i++)
  {
    FILE* src_file = fopen(argv[2 * i - 1], "rb");

    if (src_file == 0)
    {
      printf("exe2minixfs: Failed to open host file %s\n", argv[2 * i - 1]);
      break;
    }

    fseek(src_file, 0, SEEK_END);
    size_t size = ftell(src_file);

    char *buf = new char[size];

    fseek(src_file, 0, SEEK_SET);
    assert(fread(buf, 1, size, src_file) == size && "exe2minixfs: fread was not able to read all bytes of the file");
    fclose(src_file);

    VfsSyscall::rm(argv[2 * i]);
    int32 fd = VfsSyscall::open(argv[2 * i], 4 | 8); // i.e. O_RDWR | O_CREAT
    if (fd < 0)
    {
      printf("exe2minixfs: Failed to open SWEB file %s\n", argv[2 * i]);
      delete[] buf;
      continue;
    }
    int32 write_status = VfsSyscall::write(fd, buf, size);
    if((size_t)write_status != size)
    {
      printf("exe2minixfs: Writing %s failed with retval %d (expected %zu)\n", argv[2 * i], write_status, size);
    }
    VfsSyscall::close(fd);

    delete[] buf;
  }

  delete default_working_dir;
  delete superblock_;
  delete minixfs_type;
  fclose(image_fd);

  return 0;
}

#endif
