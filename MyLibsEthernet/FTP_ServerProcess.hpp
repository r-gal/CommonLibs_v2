#ifndef FTP_PROCESS_H
#define FTP_PROCESS_H

#include "SignalList.hpp"
#include "GeneralConfig.h"

#include "FileSystem.hpp"
#include "EthernetTcp.hpp"


#define FTP_CMD_SOCKET_BUFFER_SIZE 512
#define FTP_DATA_SOCKET_BUFFER_SIZE 8192

#define FILE_BUFFER_SIZE 512
#define FTP_CMD_BUFFER_SIZE 128
#define DATA_BUFFER_SIZE (8192*2)
#define READ_DATA_BUFFER_SIZE (2048)

#define CURRENT_DIRECTORY_SIZE 32


#define ZERO_COPY_STOR 1
#define NEW_DATASOCKET_ALGO 0


class FtpProcess_c : public process_c
{
  SocketTcp_c* serverSocket;
  SocketTcp_c* clientSockets[MAX_NO_OF_FTP_CLIENTS];
 
  public :

  FtpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId);

  void main(void);

  

};
struct FtpCommand_st
{
  const char* commandStr;
};

class FtpClientProcess_c
{
  enum FtpCommand_et
  {
    FTP_COM_USER,
    FTP_COM_PASS,
    FTP_COM_SYST,
    FTP_COM_FEAT,
    FTP_COM_PWD,
    FTP_COM_TYPE,
    FTP_COM_PASV,
    FTP_COM_PORT,
    FTP_COM_LIST,
    FTP_COM_CWD,
    FTP_COM_SIZE,
    FTP_COM_MDTM,
    FTP_COM_RETR,
    FTP_COM_STOR,
    FTP_COM_NOOP,
    FTP_COM_QUIT,
    FTP_COM_APPE,
    FTP_COM_ALLO,
    FTP_COM_MKD,
    FTP_COM_RMD,
    FTP_COM_DELE,
    FTP_COM_RNFR,
    FTP_COM_RNTO,
    FTP_COM_REST,
    FTP_COM_UNKN,
    FTP_COM_NOOFCOM
  };

  enum FtpAnswer_et
  {
    FTP_ANS_125_STARTING_TRANSFER = 125,
    FTP_ANS_150_ABOUT_TO_DATA_CON = 150,
    FTP_ANS_200_COMMAND_OK = 200,
    FTP_ANS_202_NOT_IMPL_SUPERFLUOUS = 202,
    FTP_ANS_220_READY_FOR_NEW_USER = 220,
    FTP_ANS_226_CLOSING_DATA_CON = 226,
    FTP_ANS_227_ENTERING_PASSIVE_MODE = 227,
    FTP_ANS_230_USER_LOGGED = 230,
    FTP_ANS_250_RE_FILE_OK = 250,
    FTP_ANS_257_PATH = 257,
    FTP_ANS_331_USER_OK_NEED_PASS = 331,
    FTP_ANS_350_REQ_FILE_PENING = 350,
    FTP_ANS_425_CANT_OPEN_DATACON = 425,
    FTP_ANS_426_CONNECTION_CLOSED = 426,
    FTP_ANS_501_SYNTAX_ERROR = 501,
    FTP_ANS_502_COMMAND_NOT_IMPLEMENTED = 502,
    FTP_ANS_503_BAD_SEQUENCE = 503,
    FTP_ANS_504_COMM_NOT_IMPL_FOR_PARAM = 504,
    FTP_ANS_550_REQ_NOT_TAKEN = 550


  };


  enum FtpClientState_et
  {
    FTP_CLIENT_IDLE,
    FTP_CLIENT_AUTH,
    FTP_CLIENT_READY,
    FTP_CLIENT_PASVEST,
    FTP_CLIENT_PASVCOM,
    FTP_CLIENT_INCOMING_DATA
  };

  enum ConnectionType_et
  {
    CONTYPE_ASCII,
    CONTYPE_BINARY
  };

  TickType_t lastCtrlActivityTick;

  SocketTcp_c* socket;
  SocketTcp_c* dataSocket;

  char* cmdBuffer;   
  uint8_t* dataBuffer;

  uint32_t sendBytes;
  /* data to zero copy STOR */
  int bytesInBuffer;

  //bool writeToFront;



  /******************/


  char currentDirectory[CURRENT_DIRECTORY_SIZE];
  ConnectionType_et connectionType;
  FtpClientState_et connectionState;

  FtpCommand_et prevCmd;
  char* prevCmdArg;

  void SendAnswer(FtpAnswer_et answer,char separator,const char* additionalString);
  void FetchCommand(int receivedBytes);

  void CreateDataSocket(void);
  void DeleteDataSocket(void);
  void HandleData(void);
  void HandleDataEvent(void);
  int  fileOffset;
  int fileRestartOffset;
  File_c* currentFile;

  void CtrlSocketKeepAliveCheck(void);

  void InsertTimeString(char* buf,SystemTime_st* time);

  void SetState(FtpClientState_et newState);

  void HandleTYPE(char* arg);
  void HandleCWD(char* arg);
  void HandleLIST(char* arg);
  void HandlePASV(char* arg);
  void HandleRETR(char* arg);
  void HandleMKD(char* arg);
  void HandleRMD(char* arg);

  void HandleRNFR(char* arg);
  void HandleRNTO(char* arg);

  void HandleSTOR(char* arg);
  void HandleREST(char* arg);

  void HandleDELE(char* arg);

  public:

  static void FtpClientTaskWrapper( void * pvParameters )
  {
    ((FtpClientProcess_c*)pvParameters)->MainLoop();
  }

  FtpClientProcess_c(SocketTcp_c* socket_,uint8_t idx);
  ~FtpClientProcess_c(void);

  void MainLoop(void);

  void Close(void);



};

#endif