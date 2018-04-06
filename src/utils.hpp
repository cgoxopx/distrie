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
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <mutex>
#include <atomic>
#include "config.hpp"
#include "serverlist.hpp"
namespace distrie{
    
    std::atomic<int64_t> count;
    
    // host long 64 to network
    uint64_t   hl64ton(uint64_t   host){
        uint64_t   ret = 0;
        uint32_t   high,low;
        low    =   host & 0xFFFFFFFF;
        high   =   (host >> 32) & 0xFFFFFFFF;
        low    =   htonl(low);
        high   =   htonl(high);
        ret    =   low;
        ret    <<= 32;
        ret   |=   high;
        return     ret;
    }

    //network to host long 64
    uint64_t  ntohl64(uint64_t   host){
        uint64_t   ret = 0;
        uint32_t   high,low;
        low    =   host & 0xFFFFFFFF;
        high   =   (host >> 32) & 0xFFFFFFFF;
        low    =   ntohl(low);   
        high   =   ntohl(high);
        ret    =   low;
        ret    <<= 32;
        ret   |=   high;
        return     ret;
    }
    
    struct GUID{
        int32_t host;
        int64_t id;
        GUID()=default;
        GUID(const GUID &)=default;
        GUID & operator=(const GUID & ig){
            host=ig.host;
            id=ig.id;
            return *this;
        }
        void init(){
            host=serverlist.myId;
            id=++count;
        }
        inline GUID ton()const{
            GUID r;
            r.host=htonl(host);
            r.id=hl64ton(id);
            return r;
        }
        inline GUID toh()const{
            GUID r;
            r.host=ntohl(host);
            r.id=ntohl64(id);
            return r;
        }
    };
    
    struct package{
        typedef enum{
            INSERT          =0x01,
            INSERT_BACKUP   =0xB1,
            INSERT_OK       =0x02,
            BACKUP          =0xB0,
            FIND            =0x04,
            FIND_OK         =0x05,
            SEEK            =0x20,
            SEEK_OK         =0x21,
            SETNEXT         =0xA0
        }Method;
        
        Method  method;
        
        union{
            char c;
            int32_t i;
            char arr[256];
            struct{
                char c;
                int32_t serverId;
                GUID guid;
            }backup,seek;
        }data;
        
        int32_t error;
        
        int32_t workId;
        int32_t serverId;
        GUID    dataId;
        int32_t backup;
        inline int32_t getWorkId()const{
            return ntohl(workId);
        }
        inline int32_t getServerId()const{
            return ntohl(serverId);
        }
        inline int32_t getBackupNum()const{
            return ntohl(backup);
        }
        inline GUID getDataId()const{
            return dataId.toh();
        }
        inline void setWorkId(int32_t n){
            this->workId=htonl(n);
        }
        inline void setServerId(int32_t n){
            this->serverId=htonl(n);
        }
        inline void setBackupNum(int32_t n){
            this->backup=htonl(n);
        }
        inline void setDataId(const GUID & n){
            this->dataId=n.ton();
        }
    };
    typedef std::pair<int32_t,GUID> position;
    
    void getNextServers(int begin,std::list<int> & s){
        if(serverlist.servers.empty())return;
        int id=begin;
        int bnum=config.backupNum>serverlist.servers.size() ? serverlist.servers.size() : config.backupNum;
        for(int i=0;i<bnum;i++){
            s.push_back(id);
            ++id;
            if(id>=serverlist.servers.size())id=0;
        }
    }
    
    void getNewPositions(std::list<position> & ps){
        int r=(++count)%serverlist.servers.size();
        std::list<int> sv;
        position p;
        p.second.init();
        getNextServers(r,sv);
        for(auto it:sv){
            p.first=it;
            ps.push_back(p);
        }
    }
    
    inline void char2hex(unsigned char num,char * out){
        static const char strs[]="0123456789abcdef";
        unsigned char h=(num>>4) & 0x0f;
        unsigned char l=(num   ) & 0x0f;
        out[0]=strs[h];
        out[1]=strs[l];
    }
    
    inline void int322hex(uint32_t num,char * out){
        static const char strs[]="0123456789abcdef";
        unsigned char a=(num>>28) & 0x0f;
        unsigned char b=(num>>24) & 0x0f;
        unsigned char c=(num>>20) & 0x0f;
        unsigned char d=(num>>16) & 0x0f;
        unsigned char e=(num>>12) & 0x0f;
        unsigned char f=(num>>8)  & 0x0f;
        unsigned char g=(num>>4)  & 0x0f;
        unsigned char h=(num   )  & 0x0f;
        out[0]=strs[a];
        out[1]=strs[b];
        out[2]=strs[c];
        out[3]=strs[d];
        out[4]=strs[e];
        out[5]=strs[f];
        out[6]=strs[g];
        out[7]=strs[h];
    }
    
    inline void int642hex(uint64_t num,char * out){
        static const char strs[]="0123456789abcdef";
        unsigned char a=(num>>60) & 0x0f;
        unsigned char b=(num>>56) & 0x0f;
        unsigned char c=(num>>52) & 0x0f;
        unsigned char d=(num>>48) & 0x0f;
        unsigned char e=(num>>44) & 0x0f;
        unsigned char f=(num>>40) & 0x0f;
        unsigned char g=(num>>36) & 0x0f;
        unsigned char h=(num>>32) & 0x0f;
        unsigned char i=(num>>28) & 0x0f;
        unsigned char j=(num>>24) & 0x0f;
        unsigned char k=(num>>20) & 0x0f;
        unsigned char l=(num>>16) & 0x0f;
        unsigned char m=(num>>12) & 0x0f;
        unsigned char n=(num>>8)  & 0x0f;
        unsigned char o=(num>>4)  & 0x0f;
        unsigned char p=(num   )  & 0x0f;
        out[0] =strs[a];
        out[1] =strs[b];
        out[2] =strs[c];
        out[3] =strs[d];
        out[4] =strs[e];
        out[5] =strs[f];
        out[6] =strs[g];
        out[7] =strs[h];
        out[8] =strs[i];
        out[9] =strs[j];
        out[10]=strs[k];
        out[11]=strs[l];
        out[12]=strs[m];
        out[13]=strs[n];
        out[14]=strs[o];
        out[15]=strs[p];
    }
    
    inline uint64_t hex24bit(unsigned char c){
        if(c>='0' && c<='9')
            return c-'0';
        else
        if(c>='a' && c<='f')
            return c-'a'+10;
        else
        if(c>='A' && c<='F')
            return c-'A'+10;
        else
            return 0;
    }
    
    inline int32_t hex2int32(const unsigned char * str){
        return (
            (hex24bit(str[0])<<28) |
            (hex24bit(str[1])<<24) |
            (hex24bit(str[2])<<20) |
            (hex24bit(str[3])<<16) |
            (hex24bit(str[4])<<12) |
            (hex24bit(str[5])<<8 ) |
            (hex24bit(str[6])<<4 ) |
            (hex24bit(str[7])    )
        );
    }
    inline int64_t hex2int64(const unsigned char * str){
        int64_t num=(
            (hex24bit(str[0] )<<60) |
            (hex24bit(str[1] )<<56) |
            (hex24bit(str[2] )<<52) |
            (hex24bit(str[3] )<<48) |
            (hex24bit(str[4] )<<44) |
            (hex24bit(str[5] )<<40) |
            (hex24bit(str[6] )<<36) |
            (hex24bit(str[7] )<<32) |
            (hex24bit(str[8] )<<28) |
            (hex24bit(str[9] )<<24) |
            (hex24bit(str[10])<<20) |
            (hex24bit(str[11])<<16) |
            (hex24bit(str[12])<<12) |
            (hex24bit(str[13])<<8 ) |
            (hex24bit(str[14])<<4 ) |
            (hex24bit(str[15])    )
        );
        return num;
    }
    
    
    inline int32_t hex2int32(const char * s){
        return hex2int32((const unsigned char *)s);
    }
    inline int64_t hex2int64(const char * s){
        return hex2int64((const unsigned char *)s);
    }
    void getKey(char * s,int32_t a,int64_t b){
        char * p=s;
        *p='#';
        ++p;
        
        int322hex(a,p);
        p+=8;
        
        int642hex(b,p);
        p+=16;
        
        *p='\0';
    }
    void getKey(char * s,int32_t a,int64_t b,char c){
        char * p=s;
        *p='#';
        ++p;
        
        int322hex(a,p);
        p+=8;
        
        int642hex(b,p);
        p+=16;
        
        char2hex(c,p);
        p+=2;
        *p='\0';
    }
}
#endif