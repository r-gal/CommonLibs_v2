#ifndef FATCOMMON_H
#define FATCOMMON_H

#define MAX_SUPPORTED_SECTORS_PER_CLUSTER 8 /* Max 4kB allocation unit size */
#define MAX_FILE_NAME 64

#define TASK_LOCAL_STORAGE_CWD_IDX 5
#define MAX_PATH_LENGTH 256



#define FILE_BUFFER_SECTORS 4 /* in sectors */


class FileSystemInterface_c
{
  public:

  virtual int32_t Write( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount) = 0;
  virtual int32_t Read( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount) = 0;
};





#endif