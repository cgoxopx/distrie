#ifndef distrie_config
#define distrie_config
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
namespace distrie{
    class Config{
        public:
            int backupNum;
            int trytimes;
            Config(){
                FILE * fp;
                char buf[4096];
                fp=fopen("config/utils.conf","r");
                if(fp!=NULL){
                    while(!feof(fp)){
                        bzero(buf,4096);
                        fgets(buf,4095,fp);
                        if(buf[0]=='#')
                            continue;
                        std::istringstream iss(buf);
                        std::string k,v;
                        iss>>k;
                        
                        if(k=="backupNum")
                            iss>>this->backupNum;
                        else
                        if(k=="trytimes")
                            iss>>this->trytimes;
                        
                    }
                    fclose(fp);
                }
            }
    }config;
}
#endif