#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MAX_NR_SECTIONS 13

//the function expects the address in memory of a mapped file and its size, and reads in nr_sections and sections the respective information about the file
//the logical offset, real offset and size of the sections will be stored in sections[i][0], sections[i][1] and sections[i][2]
void getHeaderData(void* mapped_file, int sizeOfFIle, int*nr_sections, int sections[MAX_NR_SECTIONS][3])
{
    int header_size=0;
    memcpy(&header_size, mapped_file+sizeOfFIle-4, 2);

    int index=sizeOfFIle-header_size+4;
    
    *nr_sections=0;
    memcpy(nr_sections,mapped_file+index,1);

    index+=1;
    int nr=0;

    for(int i=0;i<*nr_sections;i++)
    {
        nr=0;
        memcpy(&nr, mapped_file+index+15*i+6+1,4);//offset
        sections[i][1]=nr;
        nr=0;
        memcpy(&nr, mapped_file+index+15*i+6+1+4,4);//size
        sections[i][2]=nr;
    }
    sections[0][0]=0; //logical offset
    int k=0;
    for(int i=1;i<*nr_sections;i++){
        while(k*3072-sections[i-1][0]-sections[i-1][2]<0){
            k++;
        }
        sections[i][0]=k*3072;
    }
}

int main(int argc, char **argv)
{
    int ret=0, nrRead=0, nrResponse;
    int shm_fd;
    void *shared_mem=NULL;
    void *mapped_file=NULL;
    int request_size=2;
    char** request=(char**)malloc(request_size*sizeof(char*));
    for(int i=0;i<request_size;i++){
        request[i]=(char*)malloc(40*sizeof(char));
    }

    int fd;
    int sizeFile;

    int nr_sections;
    int sections[MAX_NR_SECTIONS][3]; //sections[i][0] logical offset, sections[i][1] real offset, sections[i][2] size of the section

    if(mkfifo("RESP_PIPE_41232",0666)<0){
        printf("ERROR\ncannot create the response pipe\n");
        ret=1;
        goto cleanUPandEXIT;
    }
    int fdReq=open("REQ_PIPE_41232",O_RDONLY);
    if(fdReq<0){
        printf("ERROR\ncannot open the request pipe\n");
        ret=1;
        goto cleanUPandEXIT;
    }
    int fdResp;
    if((fdResp=open("RESP_PIPE_41232",O_WRONLY))<0){
        printf("ERROR\ncannot open the response pipe for writing\n");
        ret=1;
        goto cleanUPandEXIT;
    }
    nrResponse=7;
    write(fdResp,&nrResponse,1);
    write(fdResp,"CONNECT",7);
    printf("SUCCESS\n");

    while(1){
        nrRead=0;
        read(fdReq,&nrRead,1);
        read(fdReq,request[0],nrRead);
        *(request[0]+nrRead)='\0';

        if(strcmp(request[0],"PING")==0){
            nrResponse=4;
            write(fdResp,&nrResponse,1);
            write(fdResp,"PING",nrResponse);
            write(fdResp,&nrResponse,1);
            write(fdResp,"PONG",nrResponse);
            unsigned int nrTest=41232;
            write(fdResp,&nrTest,sizeof(unsigned int));
        }
        else {
                if(strcmp(request[0],"CREATE_SHM")==0){
                    unsigned int size;
                    read(fdReq,&size,sizeof(unsigned int));
                    shm_fd=shm_open("/xfbMaatR",O_CREAT | O_RDWR, 0664);
                    if(shm_fd<0){
                        nrResponse=10;
                        write(fdResp,&nrResponse,1);
                        write(fdResp,"CREATE_SHM",nrResponse);
                        nrResponse=5;
                        write(fdResp,&nrResponse,1);
                        write(fdResp,"ERROR",nrResponse);
                    }
                    else{
                        ftruncate(shm_fd,size);
                        shared_mem=(void*)mmap(0,size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
                        nrResponse=10;
                        write(fdResp,&nrResponse,1);
                        write(fdResp,"CREATE_SHM",nrResponse);
                        nrResponse=7;
                        write(fdResp,&nrResponse,1);
                        write(fdResp,"SUCCESS",nrResponse);
                    }
                }
                else{
                    if(strcmp(request[0],"WRITE_TO_SHM")==0){
                        unsigned int offset, value;
                        read(fdReq,&offset,sizeof(unsigned int));
                        read(fdReq,&value,sizeof(unsigned int));
                        if(offset<0 || offset+sizeof(unsigned int)>1407227){
                            nrResponse=12;
                            write(fdResp,&nrResponse,1);
                            write(fdResp,"WRITE_TO_SHM",nrResponse);
                            nrResponse=5;
                            write(fdResp,&nrResponse,1);
                            write(fdResp,"ERROR",nrResponse);
                        }
                        else{
                            memcpy(shared_mem+offset, (void*)&value, sizeof(unsigned int));
                            nrResponse=12;
                            write(fdResp,&nrResponse,1);
                            write(fdResp,"WRITE_TO_SHM",nrResponse);
                            nrResponse=7;
                            write(fdResp,&nrResponse,1);
                            write(fdResp,"SUCCESS",nrResponse);
                        }
                    }
                    else{
                        if(strcmp(request[0],"MAP_FILE")==0){
                            nrRead=0;
                            read(fdReq,&nrRead,1);
                            read(fdReq,request[1],nrRead);
                            *(request[1]+nrRead)='\0';

                            fd=open(request[1],O_RDONLY);
                            if(fd==-1)
                            {
                                nrResponse=8;
                                write(fdResp,&nrResponse,1);
                                write(fdResp,"MAP_FILE",nrResponse);
                                nrResponse=5;
                                write(fdResp,&nrResponse,1);
                                write(fdResp,"ERROR",nrResponse);
                            }
                            else{
                                sizeFile=lseek(fd,0,SEEK_END);
                                mapped_file=(void*) mmap(0,sizeFile,PROT_READ,MAP_PRIVATE, fd,0);
                                if(mapped_file==MAP_FAILED){
                                    nrResponse=8;
                                    write(fdResp,&nrResponse,1);
                                    write(fdResp,"MAP_FILE",nrResponse);
                                    nrResponse=5;
                                    write(fdResp,&nrResponse,1);
                                    write(fdResp,"ERROR",nrResponse);
                                }
                                else{
                                    nrResponse=8;
                                    write(fdResp,&nrResponse,1);
                                    write(fdResp,"MAP_FILE",nrResponse);
                                    nrResponse=7;
                                    write(fdResp,&nrResponse,1);
                                    write(fdResp,"SUCCESS",nrResponse);
                                }
                            }
                        }
                        else{
                                if(strcmp(request[0],"READ_FROM_FILE_OFFSET")==0){
                                    int offset, nrBytes;
                                    read(fdReq,&offset,sizeof(unsigned int));
                                    read(fdReq,&nrBytes,sizeof(unsigned int));
                                    if(shared_mem==NULL || shared_mem==MAP_FAILED || mapped_file==NULL || mapped_file==MAP_FAILED){
                                        nrResponse=21;
                                        write(fdResp,&nrResponse,1);
                                        write(fdResp,"READ_FROM_FILE_OFFSET",nrResponse);
                                        nrResponse=5;
                                        write(fdResp,&nrResponse,1);
                                        write(fdResp,"ERROR",nrResponse);
                                    }
                                    else{
                                        if(offset+nrBytes>sizeFile){
                                            nrResponse=21;
                                            write(fdResp,&nrResponse,1);
                                            write(fdResp,"READ_FROM_FILE_OFFSET",nrResponse);
                                            nrResponse=5;
                                            write(fdResp,&nrResponse,1);
                                            write(fdResp,"ERROR",nrResponse);
                                        }
                                        else{
                                            memcpy(shared_mem, mapped_file+offset, nrBytes);
                                            nrResponse=21;
                                            write(fdResp,&nrResponse,1);
                                            write(fdResp,"READ_FROM_FILE_OFFSET",nrResponse);
                                            nrResponse=7;
                                            write(fdResp,&nrResponse,1);
                                            write(fdResp,"SUCCESS",nrResponse);
                                        }
                                    }
                                }
                                else{
                                            if(strcmp(request[0],"READ_FROM_LOGICAL_SPACE_OFFSET")==0){
                                                unsigned int logicalOffset, nrB;
                                                read(fdReq,&logicalOffset,sizeof(unsigned int));
                                                read(fdReq,&nrB,sizeof(unsigned int));
                                                getHeaderData(mapped_file,sizeFile, &nr_sections, sections);
                                                int section=0;
                                                while(section<nr_sections){
                                                    if(sections[section][0]>logicalOffset) break;
                                                    section++;
                                                }
                                                section--;
                                                if(logicalOffset+nrB>sections[section][0]+sections[section][2]){
                                                    //wrong
                                                    nrResponse=30;
                                                    write(fdResp,&nrResponse,1);
                                                    write(fdResp,"READ_FROM_LOGICAL_SPACE_OFFSET",nrResponse);
                                                    nrResponse=5;
                                                    write(fdResp,&nrResponse,1);
                                                    write(fdResp,"ERROR",nrResponse);
                                                }
                                                else{
                                                    unsigned int realOffset=sections[section][1]+logicalOffset-sections[section][0];
                                                    memcpy(shared_mem,mapped_file+realOffset, nrB);
                                                    nrResponse=30;
                                                    write(fdResp,&nrResponse,1);
                                                    write(fdResp,"READ_FROM_LOGICAL_SPACE_OFFSET",nrResponse);
                                                    nrResponse=7;
                                                    write(fdResp,&nrResponse,1);
                                                    write(fdResp,"SUCCESS",nrResponse);
                                                }

                                            }   
                                            else{
                                                if(strcmp(request[0],"EXIT")==0){
                                                    goto cleanUPandEXIT;
                                                }
                                                else{//"READ_FROM_FILE_SECTION"
                                                    unsigned int sectNr, offset, nrB;
                                                    read(fdReq,&sectNr,sizeof(unsigned int));
                                                    read(fdReq,&offset,sizeof(unsigned int));
                                                    read(fdReq,&nrB,sizeof(unsigned int));
                                                    getHeaderData(mapped_file,sizeFile, &nr_sections, sections);
                                                    if(shared_mem==NULL || shared_mem==MAP_FAILED || mapped_file==NULL || mapped_file==MAP_FAILED){
                                                        nrResponse=22;
                                                        write(fdResp,&nrResponse,1);
                                                        write(fdResp,"READ_FROM_FILE_SECTION",nrResponse);
                                                        nrResponse=5;
                                                        write(fdResp,&nrResponse,1);
                                                        write(fdResp,"ERROR",nrResponse);
                                                    }
                                                    else{
                                                        if(offset+nrB>sections[sectNr-1][2]){
                                                            nrResponse=22;
                                                            write(fdResp,&nrResponse,1);
                                                            write(fdResp,"READ_FROM_FILE_SECTION",nrResponse);
                                                            nrResponse=5;
                                                            write(fdResp,&nrResponse,1);
                                                            write(fdResp,"ERROR",nrResponse);
                                                        }
                                                        else{
                                                            memcpy(shared_mem,mapped_file+sections[sectNr-1][1]+offset, nrB);
                                                            nrResponse=22;
                                                            write(fdResp,&nrResponse,1);
                                                            write(fdResp,"READ_FROM_FILE_SECTION",nrResponse);
                                                            nrResponse=7;
                                                            write(fdResp,&nrResponse,1);
                                                            write(fdResp,"SUCCESS",nrResponse);
                                                        }
                                                    }
                                                }
                                            }
                                }
                        }
                    }
                }
        }    
    }


    cleanUPandEXIT:
        unlink("RESP_PIPE_41232");
        for(int i=0;i<request_size;i++){
            free(request[i]);
        }
        free(request);
        close(fdResp);
        close(fdReq);

    return ret;
}