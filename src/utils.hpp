#ifndef distrie_utils
#define distrie_utils
#include <map>
#include <set>
#include <list>
#include <atomic>
#include <sys/epoll.h>
#include <memory.h>
#include <string>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/file.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <mutex>
#include "config.hpp"
namespace distrie{
    struct package{
        typedef enum{
            INSERT      =0x01,
            INSERT_OK   =0x02,
            BACKUP      =0x03,
            FIND        =0x04,
            FIND_OK     =0x05,
            SETNEXT     =0xA0
        }Method;
        
        Method  method;
        union{
            char c;
            int32_t i;
            bool arr[256];
        }data;
        int32_t workId;
        int32_t serverId;
        int64_t dataId;
        int32_t backup;
        int getWorkId(){
            
        }
        int getServerId(){
            
        }
        int getBackupNum(){
            
        }
        int64_t getDataId(){
            
        }
        void setWorkId(int){
            
        }
        void setServerId(int){
            
        }
        void setBackupNum(int){
            
        }
        void setDataId(int64_t){
            
        }
    };
    typedef std::pair<int32_t,int64_t> position;
    
    void getNewPositions(std::list<position> &){
        
    }
    int getFdByServerId(int id){
        
    }
}
#endif