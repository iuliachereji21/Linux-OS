#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_NR_SECTIONS 100000

//function is used to token the arg string by the '=' character, 
//it returns -1 if arg does not contain '=' and 0 if success
//the part before '=' is put in ret1 and the other part in ret2 
int tokenize(char* arg, char* ret1, char* ret2)
{
    char *i=strchr(arg,'=');
    if (i==NULL) return 1; //error
    int index=(int)(i-arg);
    
    ret1=strncpy(ret1, arg, index);
    *(ret1+index)='\0';
    ret2=strcpy(ret2,(arg+index+1));
    return 0;
}

//it checks the conditions in the case of the "parse" option
//returns 0 if ok, 1 if wrong magic, 2 if wrong version, 3 if wrong sect_nr, 4 if wrong sect_types
int parseCheck(char* magic, int version, int nr_of_sections, int sections[50][3]) 
{
    if(strcmp(magic, "zK")!=0) return 1;

    if(version<79 || version> 161) return 2;

    if(nr_of_sections<7 || nr_of_sections>10) return 3;

    for(int i=0;i<nr_of_sections;i++)
    {
        int type=sections[i][0];
        if(type != 82 && type != 48 && type != 56 && type != 78 && type != 71)
            return 4;
    }
    return 0;
}

//the function expects a file descriptor (fd) and reads in version, nr_sections, names, sectins and magic the respective information about the file
//the names of the sections will be stored in "names"
//the type, offset and size of the sections will be stored in sections[i][0], sections[i][1] and sections[i][2]
void getHeaderData(int fd, int*version, int*nr_sections, char** names, int sections[MAX_NR_SECTIONS][3], char* magic)
{
    lseek(fd, -2, SEEK_END);
    read(fd, magic, 2);
    *(magic+2) = '\0';
    
    int header_size = 0;
    lseek(fd, -4, SEEK_END);
    read(fd, &header_size, 2);

    lseek(fd, -header_size, SEEK_END);
    read(fd, version, 4);
    
    *nr_sections=0;
    lseek(fd, -header_size+4, SEEK_END);
    read(fd, nr_sections, 1);
    int nr;

    for(int i=0;i<*nr_sections;i++)
    {
        lseek(fd, -header_size + 5 + 15*i, SEEK_END);
        read(fd, *(names+i), 6);
        *(*(names+i)+6) = '\0';
        nr=0;
        read(fd,&nr,1); //type
        sections[i][0]=nr;
        read(fd,&nr,4); //offset
        sections[i][1]=nr;
        read(fd,&nr,4); //size
        sections[i][2]=nr;
    }
}

//function used to read the input, argc is the no. of arguments, argv is the array of arguments 
//returns 1(variant), 2(list), 3(parse), 4(extract), 5(findall), -1(failure)
//pathvalue is the string given after "path=", filter and filtervalue are the filters that will be applied in case of the list option
//extractOptions[0] is the section, extractOptions[1] is the line in case of the extract 
//recursive is 1 if the word "recursive" appears and 0 otherwise
int readArguments(int argc, char** argv, char* pathvalue, char* filter, char* filtervalue, int extractOptions[2], int* recursive)
{
    
    if(argc>5 || argc<2)
        return -1;
    if(argc==2)
    {
        if(strcmp(argv[1],"variant")==0)
            return 1;
        return -1;
    }
    int ret=0; //return value
    *recursive=0;
    *filter='\0';
    *filtervalue='\0';
    *pathvalue='\0';
    extractOptions[0]=-1;
    extractOptions[1]=-1;
    char* section_number=(char*)malloc(10*sizeof(char)); //will store the section and the line in case of the extract option, before attempting to convert them to integer values
    char* auxWord =(char*)malloc(10*sizeof(char)); //will store temporarly the strings "path", "section", "line" that we don't need 
    int converted=0; //number of successfully converted (identified) arguments
    for(int i=1;i<argc && ret!=-1; i++)
    {
        if(strcmp(argv[i],"list")==0) 
        {
            if(ret==0)
            {
                ret=2;
                converted++;
            }
            else ret=-1; //there was already aother option found
        }
        if(strcmp(argv[i],"parse")==0)
        {
            if(ret==0)
            {
                ret=3;
                converted++;
            }
            else ret=-1; //there was already aother option found
        }
        if(strcmp(argv[i],"extract")==0) 
        {
            if(ret==0)
            {
                ret=4;
                converted++;
            }
            else ret=-1; //there was already aother option found
        }
        if(strcmp(argv[i],"findall")==0) 
        {
            if(ret==0)
            {
                ret=5;
                converted++;
            }
            else ret=-1; //there was already aother option found
        }
        if(strcmp(argv[i],"recursive")==0) 
        {
            *recursive=1;
            converted++;
        }
        if(strstr(argv[i],"path=")==argv[i])
        {
            if(tokenize(argv[i],auxWord,pathvalue)==1)
                ret=-1;
            converted++;
        }
        if((strstr(argv[i],"size_greater=")==argv[i])||(strstr(argv[i],"permissions")==argv[i]))
        {
            if(tokenize(argv[i],filter,filtervalue)==1)
                ret=-1;
            converted++;
        }
        if(strstr(argv[i],"section=")==argv[i])
        {
            if(tokenize(argv[i],auxWord,section_number)==1)
                ret=-1;
            extractOptions[0]=atoi(section_number);
            if(extractOptions[0]==0)
                extractOptions[0]=-1;
            converted++;
        }
        if(strstr(argv[i],"line=")==argv[i])
        {
            if(tokenize(argv[i],auxWord,section_number)==1)
                ret=-1;
            extractOptions[1]=atoi(section_number);
            if(extractOptions[1]==0)
                extractOptions[1]=-1;
            converted++;
        }
    }

    //free the memory before return
    free(auxWord);
    free(section_number);
    if(ret==-1) return -1;

    if(*pathvalue=='\0') return -1; //the path is compulsory
    if(ret==2 && (extractOptions[0]!=-1 || extractOptions[1]!=-1)) return -1; //list and other parameters
    if(ret==3 && argc!=3) return -1; //parse and other oprions
    if((ret==4 && argc!=5) || (ret==4 && (extractOptions[0]==-1 || extractOptions[1]==-1))) return -1; //extract and other options
    if(ret==5 && argc!=3) return -1; //findall and other options

    if(converted!=argc-1) //there were unidentified fields => wrong format
        return -1;

    return ret;
}

//used for the findall option
//takes the information from the file whose path is given and returns 1 if ok, 0 otherwise
//it checks if all the sizes of all the sections of the file are less than 1323
int validate2(char* filePath)
{
    int fd = open(filePath,O_RDONLY);
    if(fd==-1)
    {
        return 0;
    }

    int version, nr_sections; 
    int sections[MAX_NR_SECTIONS][3];
    char* magic=(char*)malloc(3*sizeof(char));
    char** names; //array of names
    names=(char**)malloc(MAX_NR_SECTIONS*sizeof(char*));
    for(int i=0; i<MAX_NR_SECTIONS;i++)
        names[i]=(char*)malloc(7*sizeof(char));

    getHeaderData(fd, &version, &nr_sections, names, sections, magic);
    int ok=1;
    for(int i=0; i<nr_sections;i++)
    {
        if(sections[i][2]>1323)
            ok=0;
    }
    if(parseCheck(magic, version, nr_sections, sections)!=0) return 0;
    free(magic);
    for(int i=0; i<MAX_NR_SECTIONS;i++)
        free(names[i]);
    free(names);
    return ok;
}

//used for the list option, returns 1 if ok, 0 otherwise
int validate1(char* filePath, char* filter, char* filterValue) 
{
    if(*filter==0 || *filterValue==0) return 1;
    struct stat inode;
    lstat(filePath,&inode);
    if(strcmp(filter,"size_greater")==0)
    {
        if(S_ISREG(inode.st_mode))
        {
            if(inode.st_size<=atoi(filterValue))
                return 0; //not ok
            else 
            { 
                return 1; //ok
            }
        }
        else return 0;
    }

    if(strcmp(filter,"permissions")==0)
    {
        char perm[9];
        if(inode.st_mode & S_IRUSR)
            perm[0]='r';
        else perm[0]='-';
        if(inode.st_mode & S_IWUSR)
            perm[1]='w';
        else perm[1]='-';
        if(inode.st_mode & S_IXUSR)
            perm[2]='x';
        else perm[2]='-';
        if(inode.st_mode & S_IRGRP)
            perm[3]='r';
        else perm[3]='-';
        if(inode.st_mode & S_IWGRP)
            perm[4]='w';
        else perm[4]='-';
        if(inode.st_mode & S_IXGRP)
            perm[5]='x';
        else perm[5]='-';
        if(inode.st_mode & S_IROTH)
            perm[6]='r';
        else perm[6]='-';
        if(inode.st_mode & S_IWOTH)
            perm[7]='w';
        else perm[7]='-';
        if(inode.st_mode & S_IXOTH)
            perm[8]='x';
        else perm[8]='-';
        if(strcmp(perm,filterValue)!=0)
            return 0;
        else return 1;
    }
    return 0; // not ok
}

//used for the list option
//expects the directory and its path (used when printing the result)
//validateVersion is used to choose which validate function to use;
//if validateVersion=1 then the "list" option was chosen, and the function is validate1
//if validateVersion=2 then the "findall" option was chosen, and the function is validate2
//filter and flterValue are the filters that will be applied in case of validate1
//if rec=1 the list is done recursively, if rec=0 only the files from the current directory are displayed without going deeper
void listDir(DIR* dir, char* path, char* filter, char* filterValue, int rec, int validateVersion)
{
    struct dirent *dirEntry;
    rewinddir(dir);
    DIR* directory;
    char *str=(char*)malloc(1000*sizeof(char)); 
    
    while((dirEntry=readdir(dir))!=0)
    {
        if(strcmp(dirEntry->d_name,".")!=0 && strcmp(dirEntry->d_name,"..")!=0)
        {
            strcpy(str,path);
            strcat(str,"/");
            strcat(str,dirEntry->d_name);
            if(validateVersion==1)
            {
                if(validate1(str, filter, filterValue)==1)
                {
                    printf("%s\n",str);
                }
            }
            if(validateVersion==2)
            {
                directory=opendir(str);
                if(directory==0) //is not a directory
                {
                    if(validate2(str)==1)
                    {
                        printf("%s\n",str);
                    }
                }
            }
            
            if(rec==1)
            {
                directory=opendir(str);
                if(directory!=0) //is a directory
                {
                    listDir(directory, str, filter, filterValue, rec, validateVersion);
                    closedir(directory);
                }
            }
        }    
    }
    free(str);
}

//function used in the extract option
//returns 0 if ok, -1 if failure
//the read line will be stored in returnLine in case of success
int readLine(int fd, int offsetSection, int sizeSection, int line, char* returnLine) 
{
    if(line<1) return -1;
    lseek(fd, offsetSection,SEEK_SET);
    char section[sizeSection+1];
    read(fd,section,sizeSection);
    section[sizeSection]='\0';
    int currentline=1;
    int pos=sizeSection-1;
    while(pos>-1 && line-currentline!=0)
    {
        if(section[pos]=='\n')
            currentline++;
        pos--;
    }
    if(pos==-1) return -1;

    section[pos]='\0';
    pos--;
    while(pos>-1 && section[pos]!='\n')
        pos--;

    strcpy(returnLine, section+pos+1);

    return 0;
}

int main(int argc, char **argv){


    int ret=0; //what to return, 0 if ok, other number if there is a problem

    //variables needed for 'list' option
    char* filter=(char*)malloc(100*sizeof(char)); //the filter that must be applied in case of list (eg. permissions)
    char* filtervalue=(char*)malloc(100*sizeof(char)); //the value that must be applied (eg. rwx------)
    char* pathvalue=(char*)malloc(100*sizeof(char)); //the value of the path (after '=')
    int rec=0; //1 if listing is recursive, 0 if not
    struct stat fileMetadata;
    DIR* dir;

    //variables needed for 'parse' option
    int fd, version, nr_sections, result;
    int sections[MAX_NR_SECTIONS][3]; //sections[i][0] type, sections[i][1] offset, sections[i][2] size of the section
    char* magic=(char*)malloc(3*sizeof(char)); //magic of file
    char** names; //array of names of sections
    names=(char**)malloc(MAX_NR_SECTIONS*sizeof(char*));
    for(int i=0; i<MAX_NR_SECTIONS;i++)
        names[i]=(char*)malloc(7*sizeof(char));

    //variables needed for 'extract' option
    int extractOptions[2]; 
    extractOptions[0]=-1; //section number
    extractOptions[1]=-1; //line number
    char* line=(char*)malloc(200000*sizeof(char)); //the line that will be read and printed


    int option=readArguments(argc,argv, pathvalue,filter, filtervalue, extractOptions, &rec);

    switch(option)
    {
        case -1: //error reading
            printf("ERROR\nWrong input format\n");
            ret=1;
            break;
        case 1: //variant
            printf("41232\n");
            break;
        case 2: //list
            if(stat(pathvalue, &fileMetadata)<0)
            {
                printf("ERROR\nNo such directory\n");
                ret=2;
                goto cleanUPandEXIT;
            }
            if(!S_ISDIR(fileMetadata.st_mode))
            {
                printf("ERROR\nIt is not a directory\n");
                ret=3;
                goto cleanUPandEXIT;
            }
            if(*filter!=0 && *filtervalue!=0)
                if(strcmp(filter,"size_greater")==0)
                    if(atoi(filtervalue)==0)
                    {
                        printf("ERROR\nThe size is not an integer\n");
                        ret=4;
                        goto cleanUPandEXIT;
                    }

            dir = opendir(pathvalue);
            if(dir==0) 
            {
                printf("ERROR\nCouldn't open directory\n");
                ret=5;
                goto cleanUPandEXIT;
            }

            printf("SUCCESS\n");
            listDir(dir,pathvalue, filter, filtervalue, rec, 1);
            closedir(dir);
            break;
        case 3: //parse
            fd = open(pathvalue,O_RDONLY);
            if(fd==-1)
            {
                printf("ERROR\nCouldn't open file\n");
                ret=6;
                goto cleanUPandEXIT;
            }

            getHeaderData(fd, &version, &nr_sections, names, sections, magic);
            if((result=parseCheck(magic, version, nr_sections, sections))==0)
            {
                printf("SUCCESS\nversion=%d\nnr_sections=%d\n",version,nr_sections);
                for(int i=0;i<nr_sections;i++)
                {
                    printf("section%d: %s %d %d\n",i+1, names[i], sections[i][0], sections[i][2]);
                }
            }
            else
            {
                printf("ERROR\nwrong ");
                switch(result)
                {
                    case 1: printf("magic\n"); break;
                    case 2: printf("version\n"); break;
                    case 3: printf("sect_nr\n"); break;
                    case 4: printf("sect_types\n"); break;
                }
            }
            break;
        case 4: //extract
            fd = open(pathvalue,O_RDONLY);
            if(fd==-1)
            {
                printf("ERROR\nCouldn't open file\n");
                ret=6;
                goto cleanUPandEXIT;
            }

            getHeaderData(fd, &version, &nr_sections, names, sections, magic);
            if((result=parseCheck(magic, version, nr_sections, sections))!=0)
            {
                printf("ERROR\ninvalid file\n");
                ret=7;
                goto cleanUPandEXIT;
            }
            if(extractOptions[0]<1 || extractOptions[0]> nr_sections) //section number
            {
                printf("ERROR\ninvalid section\n");
                ret=8;
                goto cleanUPandEXIT;
            }
            if(readLine(fd, sections[extractOptions[0]-1][1],sections[extractOptions[0]-1][2], extractOptions[1], line)!=0)
            {
                printf("ERROR\ninvalid line\n");
                ret=9;
                goto cleanUPandEXIT;
            }
            else 
            {
                printf("SUCCESS\n%s\n",line);
                break;
            }
        case 5: //findall
            if(stat(pathvalue, &fileMetadata)<0)
            {
                printf("ERROR\nInvalid directory path\n");
                ret=10;
                goto cleanUPandEXIT;
            }
            if(!S_ISDIR(fileMetadata.st_mode))
            {
                printf("ERROR\nInvalid directory path\n");
                ret=11;
                goto cleanUPandEXIT;
            }
            dir = opendir(pathvalue);
            if(dir==0) 
            {
                printf("ERROR\nInvalid directory path\n");
                ret=12;
                goto cleanUPandEXIT;
            }

            printf("SUCCESS\n");
            listDir(dir,pathvalue, filter, filtervalue, 1, 2);
            closedir(dir);
            break;

    }
    

    cleanUPandEXIT: //deallocate the memory used
    free(filter);
    free(filtervalue);
    free(pathvalue);

    free(magic);
    for(int i=0; i<MAX_NR_SECTIONS;i++)
        free(names[i]);
    free(names);
    free(line);

    return ret;
}