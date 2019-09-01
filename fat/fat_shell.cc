#define CXX_API

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dlfcn.h>

#ifdef CXX_API
#include "fat.h"
#else
#include "myfat.h"
#endif

//char *cur_dir = (char *)malloc(2048 * sizeof(char));

void readNonEmptyLine(char *line);
int validLine(char* line);
unsigned char *readFile(int fd, int numbytes, int offset);
unsigned char *copyFile(char *name, int numbytes, int offset);
//char *convertName(char *name);
char *trimwhitespace(char *str);
int tokenize(char *line, char **args, char *argDelim);

//char *map[128];
int map[128];
int number_open = 0;


int main(int argc, char *argv[]) {
	//int i = 0;
	
	char line[1024];
	char argDelim[2] = " ";

	while(1) {
		
		// read input
		printf("# ");
		fflush(stdout);
		readNonEmptyLine(line);
		printf("%s\n", line);
		fflush(stdout); 
		if (strcmp(line, "exit") == 0){ 
			//printf("\n");
			break;
		}

		// check if the line is valid or not
		if (!validLine(line)) {
			printf("ERROR: invalid input\n");
			fflush(stdout);
			continue;
		}

		
		int i = 0;
		int argCount = 0;
		char *args[15];
		argCount = tokenize(line, args, argDelim) ;
		//printf("argsCount = %d \n", argCount);
		
		if(strcmp(args[0], "cd") == 0){
			// format cd dirpath
			
			printf("changing directory to %s\n", args[1]);
			int status = fat_cd(args[1]);
			if(status == 1){ 
				printf("Successful\n");
			}
			else { 
				printf("Not Successful\n");
			}				
			fflush(stdout);
		}
		
		else if(strcmp(args[0], "cp") == 0){
			// format cp source target size
			
			printf("copying file %s of size %d to %s \n", args[1], atoi(args[3]), args[2]);
			unsigned char *readbuf = copyFile(args[1], atoi(args[3]), 0);
			FILE *write_ptr;
			write_ptr = fopen(args[2],"wb"); 
			fwrite(readbuf,atoi(args[3]),1,write_ptr);
			fclose(write_ptr);
			fflush(stdout);
		}
		
		else if(strcmp(args[0], "read") == 0){
			// format read fd nbyte offset
		
			if(argCount >= 4) {
				unsigned char *readout = readFile(atoi(args[1]), atoi(args[2]), atoi(args[3]));
				if(readout != NULL)
					printf("\n%s\n", readout);
				       
			} 
			else printf("ERROR: not enough argument for read");
			fflush(stdout);
		}
		
		else if(strcmp(args[0], "open") == 0){
			// format open filepath
			
			printf("Opening file %s\n", args[1]);
			int fd = fat_open(args[1]);
			if(fd != -1){ 
				printf("File opened successfully with file descriptor %d\n", fd);
			}
			else printf("File open error \n");
			fflush(stdout);
		}
		
		else if(strcmp(args[0], "close") == 0){
			// format close fd
			
			printf("close file %s\n", args[1]);
			int status = fat_close(atoi(args[1]));
			printf("status: %d\n", status);
			fflush(stdout);
			
			//printf("After CD: %s\n", cwdPath);
		}

		else if (strcmp(args[0], "closetest") == 0){
			printf("Opening file %s\n", args[1]);
			int fd = fat_open(args[1]);
			if (fd != -1){
				printf("File opened successfully with file descriptor %d\n", fd);
				int status = fat_close(atoi(args[1]));
				printf("file closed with status: %d\n", status);
				unsigned char *readout = readFile(fd, 100, 0);
				if (readout != NULL)
					printf("\n%s\n", readout);
				fflush(stdout);

			}
			else{
				printf("File open error \n");
			}
							//printf("After CD: %s\n", cwdPath);
		}

		else if (strcmp(args[0], "opentest") == 0){
			printf("Opening file %s\n", args[1]);
			int fd = fat_open(args[1]);
			if (fd != -1){
				printf("File opened successfully with file descriptor %d\n", fd);
				unsigned char *readout = readFile(fd, 100, 0);
				if (readout != NULL)
					printf("\n%s\n", readout);
				fflush(stdout);
			}
			else{
				printf("File open error \n");
			}
			//printf("After CD: %s\n", cwdPath);
		}
		
		else if(strcmp(args[0], "lsdir") == 0){
			// format lsdir dirpath 
			// if lsdir is tried without an argument, we will try lsdir .
			
			char *path = (char *) malloc(1024 * sizeof(char));
			
			if(argCount < 2){
				strcpy(path, ".");
			}
			else{
				strcpy(path, args[1]);
			}
			printf("listing directory %s\n", path);
#ifdef CXX_API
                        std::vector<AnyDirEntry> raw_dirs = fat_readdir(path);
                        DirEntry *dirs = (DirEntry*) &raw_dirs[0];
#define dir_name DIR_Name
#define dir_attr DIR_Attr
#define dir_fileSize DIR_FileSize
                        int dirCount = raw_dirs.size();
#else
			dirEnt *dirs = fat_readDir(path);
                        int dirCount;
                        for (dirCount = 0; dirCount < 1000 && dirs[dirCount]; ++dirCount);
#endif
                        free(path);
			
			if(
#ifdef CXX_API
                                raw_dirs.size() != 0
#else
                                dirs != NULL
#endif
                                ){
			//printf("%12s %4s %4s\n", "File Name", "Attr", "Size");
				int t = 0;
				for(i=0; i<dirCount; i++) {
					if(dirs[i].dir_name[0] == 0) {
						printf("listed %d entries\n", t);
						break;
					}
					else if(dirs[i].dir_attr == 0x10 || dirs[i].dir_attr == 0x20 || dirs[i].dir_attr == 0){
					  if(dirs[i].dir_name[0] == 0xE5) continue;
					  char dir_name[12];
					  dir_name[11] = 0;
					  int j = 0, k = 0;
					  for(j = 0; j < 11; j++){ 
					    if(dirs[i].dir_name[j] == ' '){
					      continue;
					    } 
					    dir_name[k++] = dirs[i].dir_name[j];
					  }
					  dir_name[k] = '\0';
	      
					  printf("%12s %04x %4ld\n", dir_name, dirs[i].dir_attr, (long) dirs[i].dir_fileSize);
					  t++;
					}
				}
			}
			
			else{
				printf("ERROR: %s is not a directory\n", path);
			}
			
			fflush(stdout);
		}
		
		else if(strcmp(args[0], "mount") == 0){
			// format mount filename
			
			if(fat_mount(args[1])){
				printf("mounting '%s' successful\n", args[1]);
			}
			else printf("ERROR: file mount error\n");
			fflush(stdout);
		}
		
		else{
			printf("ERROR: invalid command\n");
		}

                for (int i = 0; i < argCount; ++i) {
                        free(args[i]);
                }
	}
	
	
	
	return 0;
}

int tokenize(char *line, char **args, char *argDelim){
	char *token;
	int i = 0;
	token = strtok(line,argDelim);
	
	while( token != NULL ){
		//printf("%d\n", strlen(token));
		args[i] = (char *) malloc ((1 + strlen(token)) * sizeof(char));
		strcpy(args[i], token);
		i++; 
		token = strtok(NULL,argDelim);
	}
	return i;
}



char *trimwhitespace(char *str){
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

void readNonEmptyLine(char *line) {
        while (1) {
                if (!fgets(line, 1024, stdin)) break;
                trimwhitespace(line);
                if (strlen(line) > 0) break;
        }
}

int validLine(char *chArray) {
	int i = 0;
	int length = strlen(chArray);
	for (i = 0; i < length; i++) {
		char ch = chArray[i];
		if (!((ch >= 'A' && ch <= 'Z') || ( ch >= 'a' && ch <= 'z') 		// letters
				|| (ch >= '0' && ch <= '9') 				// numbers
				|| ch == '.' || ch == '-' || ch == '_' || ch == '/'	// special characters
				|| ch == ' ' || ch == '\t'				// white spaces 
				|| ch == '<' || ch == '>' 
				|| ch == '~')) {		// operators
			return 0;
		}
	}
	if (chArray[length - 1] == '|') return 0;					// meaningless pipe at the end
	return 1;
}


unsigned char *readFile(int fd, int numbytes, int offset){
  
  if(fd == -1){
    printf("ERROR: Invalid FileName\n");
    return NULL;
  }
  unsigned char *buf = (unsigned char *)malloc(numbytes*sizeof(unsigned char));
  //buf[0] = '\0';
  int bytesRead = fat_pread(fd, buf, numbytes, offset);
  printf("Reading %d bytes from file %d starting from offset %d\n", bytesRead, fd, offset);
  return buf;
}


unsigned char *copyFile(char *name, int numbytes, int offset){
  int fd = fat_open(name);
  if(fd == -1){
    printf("ERROR: Invalid FileName\n");
    return NULL;
  }
  unsigned char *buf = (unsigned char *)malloc(numbytes*sizeof(unsigned char));
  //buf[0] = '\0';
  int bytesRead = fat_pread(fd, buf, numbytes, offset);
  printf("Reading %d bytes from file %s starting from offset %d\n", bytesRead, name, offset);
  return buf;
}


