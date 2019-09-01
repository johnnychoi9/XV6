#include "fat_internal.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <cctype>
#include <clocale>
#include <vector>
#include <cctype>
// Agenda For Today
// get open, the entry finder 
struct fileDed{
	bool open;
	uint32_t clusterNumber;
	unsigned int fileSize;
};

std::vector<char> buff; // use for packing in the info
char* theDisk = NULL; //holds in the data, honestly just a more convenient buffer because 
Fat32BPB bpb;
struct fileDed * openFiles = NULL;
int clusRoot;
int clusCurr;
int dirSize = sizeof(DirEntry);
char * volume_label;

// Things to remember - Files and directories may span multiple clusters

std::string dir_name_to_rec_string(uint8_t * dirName){ // Need this because Dir_Name is stored as a unint8_t
	std::string name(dirName, dirName + 8);
	return name.substr(name.find_first_not_of(' '), name.find_first_of(' '));
}

uint32_t get_first_byte_of_cluster(uint32_t clus_num){
	//find Out the First Data Sector, using formulas from documentation and the number of sectors our root takes up
	uint32_t first_data_sector = (bpb.BPB_NumFATs * bpb.BPB_FATSz32) + bpb.BPB_RsvdSecCnt + (((bpb.BPB_BytsPerSec -1) + (bpb.BPB_rootEntCnt * 32)) / bpb.BPB_BytsPerSec);
	//then get the normal calculation from the first byte formula out of the notes
	return (first_data_sector + ((clus_num - 2) * bpb.BPB_SecPerClus)) * bpb.BPB_BytsPerSec;
}

int getclusterForEntry(DirEntry entry){
	return (entry.DIR_FstClusHI << 16) + entry.DIR_FstClusLO;
}

//get a fat entry based on a cluster number thrown in
//pretty necessary as a helper function, it's going to get 
//I use buffer for FAT access and then theDisk for entry access
uint32_t getFAT(uint32_t numCluster){
	int offset = (numCluster * 4) % bpb.BPB_BytsPerSec;
	int fatSec = bpb.BPB_RsvdSecCnt + ((numCluster * 4) / bpb.BPB_BytsPerSec);
	char filler[32];
	// this actually gets the fat
	int fat = bpb.BPB_BytsPerSec * fatSec + offset;
	for (int i = 0; i < 32; i++){
		filler[i] = buff.at(fat + i);
	}
	uint32_t * filled = (uint32_t *) filler;
	return (*filled & 0x0FFFFFFF);
}

bool fat_mount(const std::string &path) {
	//see if some stuff has already been mounted, if so, clear stuff
	if(theDisk == NULL){
		clusCurr = 0;
		clusRoot = clusCurr;
		delete[] theDisk;
		buff.clear();
	}
	//find the length of the opened file
    std::ifstream takeFile(path);
	takeFile.seekg(0, takeFile.end);
	size_t length = takeFile.tellg();
	takeFile.seekg(0, takeFile.beg);
	//make everything work if length >= 1;
	if(length < 1){
		return false;
	}
	buff.resize(length);
	// Open and close a lot of the stuff that stores the Disk info in easily accessible forms
	theDisk = new char[length];
	takeFile.read(&buff[0], length);
	takeFile.seekg(0, takeFile.beg);
	takeFile.read((char *)theDisk, length);
	takeFile.seekg(0, takeFile.beg);
	takeFile.read((char *)&bpb, sizeof(Fat32BPB));
	// set our global variables, clussCurr will come into play later. 
	clusRoot = bpb.BPB_RootClus;
	clusCurr = clusRoot;
	openFiles = new fileDed[128];
	for (int i = 0; i < 128; i++){
		openFiles[i].open = false;
		openFiles[i].clusterNumber = bpb.BPB_RootClus;
		openFiles[i].fileSize = 528; // THESE ARE ALL TEMP VALUES AND THEREFORE IRRELEVANT
	}
	return true;
}

std::string remove_extension(std::string fileName){ //Maybe edit a bit but don't fuck with it
	std::string newName = "";
	for( unsigned int i = 0; i < fileName.length() && i < 8; i++){
		if(fileName[i] != '.'){
			newName += fileName[i];
		} else{
			break;
		}
	}
	std::cout<<newName<<std::endl;
	return newName;
}
std::vector<AnyDirEntry> fat_readdir(const std::string &path);

int fat_open(const std::string &path) {
	//first check and see if the disk is mounted
	if(theDisk == NULL){
		return -1;}
	//std::cout<< " I got here"<<std::endl;
	// then turn everything upper case, cause the instructions said to
   	std::string directory_path = path;
	struct DirEntry theEntry;
	for( unsigned int i = 0; i < directory_path.length(); i++){
		directory_path[i] = toupper(directory_path[i]);
	}
	// Now we discover what's the file and what's the path to the File
	std::string fileName = "";
	int last_slash_pos = 0;
	for ( unsigned int i = 0; i < directory_path.length(); i++){
		if(directory_path[i] != '/'){
			fileName += directory_path[i];
		}
		if(directory_path[i] == '/'){
			fileName = "";
			last_slash_pos = i;
		}
	}
	// actually fix the directory path!
	directory_path = directory_path.substr(0, last_slash_pos);
	if( directory_path == "/" || directory_path == "" || directory_path == "."){
		directory_path = "./";
	}
	fileName = remove_extension(fileName);
	//std::cout<<fileName<<std::endl;
	//std::cout<<directory_path<<std::endl;
	std::vector<AnyDirEntry> entries = fat_readdir(directory_path);
	int clusterNumber = -1;
	for( unsigned int i = 0; i < entries.size(); i++){
		theEntry = entries[i].dir;
		if(fileName == dir_name_to_rec_string(theEntry.DIR_Name) && theEntry.DIR_Attr != DIRECTORY){
		clusterNumber = getclusterForEntry(theEntry);
//		std::cout<<getclusterForEntry(theEntry)<<std::endl;
		break;
		}
	}
	
	for (int i = 0; i < 128; i++){
		if(clusterNumber == -1){
			//std::cout<<"I FAILED"<<std::endl;
			return -1;
		}
		else if(openFiles[i].open == false){
			//std::cout<<"File ID"<<clusterNumber<<std::endl;
			openFiles[i].clusterNumber = clusterNumber;
			openFiles[i].open = true;
			openFiles[i].fileSize = theEntry.DIR_FileSize;
			return i;
		}
	}
	//std::cout<< "How did I get here" <<std::endl;
	return -1;
}
//all right, this was getting me for a while, but open and close do not actually use file opening and stuff like in shell
//it's just like, what's being accessed
bool fat_close(int fd) {
    /*if(close(fd) == 0){
		for(int x = 0; x < openedFiles; x++){
			if(openedFiles[x][0] == fd){
				std::cout<< openedFiles[x][0] << " " << fd << std::endl;
				openedFiles.erase(myvector.begin()+ x);
				break;
			}
		}
		return true;
	}else{
		return false;
	}*/
	if(openFiles[fd].open == false){
		return false;
	}
		openFiles[fd].open = false;
		return true;
}

char *  do_a_read(int fd, int count, int offset){	
	char * newCopy = new char[openFiles[fd].fileSize];
	unsigned int count2 = 0;
	// got to cast the buffer, cause it's passed as a void for some weird reason
	int starting_cluster = openFiles[fd].clusterNumber;
	while (count2 < openFiles[fd].fileSize){
		if(starting_cluster >= 0x0FFFFFF8){
     	 break;}
		int count3 = 0; 
		//then snag that first byte and 
		int first_byte = get_first_byte_of_cluster(starting_cluster);
	 	while( count3 < (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus) && count2 < openFiles[fd].fileSize){
			newCopy[count2++] = theDisk[first_byte + count3];
			count3 = count3 + 1;
		}
		starting_cluster = getFAT(starting_cluster);
		//std::cout<<starting_cluster<<std::endl;
	}
	return newCopy;
}


int fat_pread(int fd, void *buff, int count, int offset) { //Need to Modify
	if(theDisk == NULL || fd < 0 || fd >= 128){
		return -1;
	}
	if(openFiles[fd].open == false){
	return -1;
	}
	// create stuff needed for copying, and check to make sure that count + offset aren't larger than the fileSize
	//std::cout<<openFiles[fd].clusterNumber<<" "<<openFiles[fd].fileSize<<std::endl;
	unsigned int goodToGo = count;
	unsigned int total = count + offset;
	///BOUNDS CHECKIN BOI
	if(total > openFiles[fd].fileSize){
		goodToGo = openFiles[fd].fileSize - offset;
	}
	// got to cast the buffer, cause it's passed as a void for some weird reason
	char * new_buffer = (char *) buff;
	char * newCopy = do_a_read(fd, count, offset);
	//std::cout<<"BYTES READ "<<count2<<std::endl;
	unsigned count2 = 0;
	while(count2 < goodToGo){
		new_buffer[count2] = newCopy[count2+offset];
		count2++;
	}
	delete[] newCopy;
    return goodToGo;
}
//Summary of Finding files/Directories
//Find the first cluster number in directory entry of interesting file or directory
// Figure out the sector to read using cluster number and FirstSectorofCluster equation
// 3. Read that cluster
//Figure out if file or directory continues past current cluster by calling FAT(current_cluster_number)
//If EoC, then stop,
//If Else go to new cluster (step three) with cluster = FAT(current cluster number)
bool validity_check(struct DirEntry entry);

//this function both handles generating a list of all entries in a cluster and modifying a particular entry from a cluster
bool read_entry_from_cluster(struct DirEntry * entryToModify, std:: string Name, int cluster){
//	std::cout<<Name<<" Porcupine"<<std::endl;
	while (true){
		if( cluster >= 0x0FFFFFF8){
			break;
		}
		// this is the end of the storage for that directory, if not, the cluster number points to  
		//next part of the entry
		
		unsigned int start = get_first_byte_of_cluster(cluster);
		char * start_of_sector = theDisk;
		start_of_sector = start_of_sector + start;
		long processed = dirSize * 2;
		// We need an entry to check againsts
		DirEntry entry = *((DirEntry *) start_of_sector);
	//	std::cout<<"I GET HERE #$"<<std::endl;
		int bytes_in_cluster = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
		start_of_sector += dirSize;
		while(processed < bytes_in_cluster && entry.DIR_Name[0] != 0x00 && entry.DIR_Name[0] != 0xE5){
				// if its a valid entry, then create a clone and then push into a vector to return
				// 
				entry = *((DirEntry *) start_of_sector);
				if(Name == dir_name_to_rec_string(entry.DIR_Name)){
					*entryToModify = entry;
					return true;
				}
			//progress through the cluster
				processed += dirSize;
				start_of_sector += dirSize;
			}	
		// next, plug the clusterStart into finding the first Sector of the Cluster, go to that sector
		// 
		cluster = getFAT(cluster);
	}
	return false;
}


std::vector<AnyDirEntry> readAll(int clusterNumber){
	std::vector<AnyDirEntry> theEntries;
	while (clusterNumber < 0x0FFFFFF8){
		// this is the end of the storage for that directory, if not, the cluster number points to  
		//next part of the entry
		unsigned int start = get_first_byte_of_cluster(clusterNumber);
		char * start_of_sector = theDisk;
		start_of_sector = start_of_sector + start;
		long processed = dirSize;
		DirEntry entry = *((DirEntry *) start_of_sector);
		int bytes_in_cluster = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
		processed += dirSize;
		start_of_sector += dirSize;
		while(processed < bytes_in_cluster && entry.DIR_Name[0] != 0x00 && entry.DIR_Name[0] != 0xE5){
				// if its a valid entry, then create a clone and then push into a vector to return
				if (validity_check(entry)){
					AnyDirEntry fillerEntry;
					fillerEntry.dir = entry;
					theEntries.push_back(fillerEntry);
				}
			//progress through the cluster
				processed += dirSize;
				start_of_sector += dirSize;
				// set new entry
				entry = *((DirEntry *) start_of_sector);
			
		}	
		// next, plug the clusterStart into finding the first Sector of the Cluster, go to that sector
		// 
		clusterNumber = getFAT(clusterNumber);
	}
	return theEntries;
}
// Need to check if a provided Directory Entry is valid, check against all the invalid entry markers
//MODIFY
bool validity_check(struct DirEntry entry){
	// got these out of the fat doc
	uint8_t invalid[16] = {0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C};
	int count = 16;
	int lengthOfDirName = 11;
	uint8_t* name = entry.DIR_Name;
	// run all the checks to see if at any point the name is invalid
	for ( int i = 0; i < lengthOfDirName; i++){
		for( int j = 0; j < count; j++){
		// return false if the name is invalid at any point
			if(name[i] == invalid[j]){
				return false;}
		}
		if(name[0] == 0x20){
		return false;
		}
		if(name[0] == 0xE5){
		return false;
		}
		if(name[0] < 0x20 && !(i == 0 && name[0] == 0x05)){
		return false;
		}
	}
	return true;
}
//First Charactr being 0 means end of direcory, 0xe5 means unused

bool is_it_root(std::string path){
	if(path[0] == '/'){
		return true;
	}else{
		return false;
	}
}
std:: pair<bool, int> travelThrough(DirEntry * anEntry, std::string thePath, int theCluster){
	std::string file = "";
	bool worked = true;
	int currentCluster = theCluster;
	for(unsigned int i = 0; i < thePath.length(); i++){
		if(thePath[i] != '/'){
			file += thePath[i];
		}else{
//		std::cout<<file<<std::endl;
		worked = read_entry_from_cluster(anEntry, file, currentCluster);
		if(!worked){
		//	std::cout<<"FUCK"<<std::endl;
		}
		if(file == "."){
//					  std::cout<<"1"<<std::endl;
			worked = true;
		}
		else if(getclusterForEntry(*anEntry) == 0 && worked && file == ".."){
	//				  std::cout<<"2"<<std::endl;
			currentCluster = clusRoot;
		}
		else if(worked && file == ".."){
		//			  std::cout<<"3"<<std::endl;
			currentCluster = getclusterForEntry(*anEntry);
		}
		else if(currentCluster == clusRoot && file == ".."){
	//				  std::cout<<"4"<<std::endl;
			worked = true;
		} else if (worked && validity_check(*anEntry)){
	//				  std::cout<<"5"<<std::endl;
			currentCluster = getclusterForEntry(*anEntry);
		}
		else{
	//				  std::cout<<"6"<<std::endl;
			worked = false;
			}
		file = "";
		}
	}
	return std::make_pair (worked, currentCluster);
}
std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
	//Need to Incorporate current cluster
	std::vector<AnyDirEntry> result;
	if(theDisk == NULL){return result;}
	std::string thePath = path;
	int currentCluster = clusCurr;
	// OG CODE THAT I KNOW FREAKING WORKS FOR FILES IN JUST ROOT
	if(thePath == "/"){
	DirEntry entry;
	uint32_t clusterNumber = clusCurr;
	int amountProcessed = 0;
	auto first = buff.begin() + get_first_byte_of_cluster(clusterNumber);
	auto last = first + (sizeof(DirEntry));
	std::vector<char> loadUp;
	while(true){
		loadUp = std::vector<char>(first, last);
		std::copy(loadUp.begin(), loadUp.end(), reinterpret_cast<char *>(&entry));
		if(entry.DIR_Name[0] == 0x00){
			break;
		}
		if(entry.DIR_Attr == 0x08){
			volume_label = (char *)entry.DIR_Name;
		}
		if(validity_check(entry)){
			AnyDirEntry any;
			any.dir = entry;
			result.push_back(any);
		}
		first = last;
		amountProcessed += sizeof(DirEntry);
		if(bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus <= amountProcessed){
			clusterNumber = getFAT(clusterNumber);
			first = buff.begin() + get_first_byte_of_cluster(clusterNumber);
			amountProcessed = 0;
		}
		last = sizeof(DirEntry) + first;
		}
  	  return result;
	}
	// bool worked = true;
	for( unsigned int i = 0; i < thePath.length(); i++){
	thePath[i] = toupper(thePath[i]);
	}
	if(thePath[thePath.length() -1] != '/'){
		thePath += '/';
	}
	if(is_it_root(thePath)){
		currentCluster = clusRoot;
		thePath = thePath.substr(1);
	}
	std::vector<std::string> theDirectories;
	for(unsigned int i = 0; i < thePath.length(); i++){
		std::string temp = "";
		if(thePath[i] != '/'){
			temp += thePath[i];
		}else{
			theDirectories.push_back(temp);
			temp = "";
		}
	}
	struct DirEntry * anEntry = new struct DirEntry;
	//std::vector<AnyDirEntry> filler;
	std::pair <bool, int> returnGuy = travelThrough(anEntry, thePath, currentCluster);
	if(returnGuy.first == true){
		result = readAll(returnGuy.second);
	}	
	delete anEntry;
	return result;
}

bool fat_cd(const std::string &path) {
    //Need to Incorporate current cluster
	if(theDisk == NULL){return false;}
	std::string thePath = path;
	int currentCluster = clusCurr;
	// OG CODE THAT I KNOW FREAKING WORKS FOR FILES IN JUST ROOT
	if(thePath == "/"){
		clusCurr = clusRoot;
		return true;
	}
	for( unsigned int i = 0; i < thePath.length(); i++){
	thePath[i] = toupper(thePath[i]);
	}
	if(is_it_root(thePath)){
		currentCluster = clusRoot;
		thePath = thePath.substr(1);
	}
	if(thePath[thePath.length() -1] != '/'){
		thePath += '/';
	}
	std::vector<std::string> theDirectories;
	for(unsigned int i = 0; i < thePath.length(); i++){
		std::string temp = "";
		if(thePath[i] != '/'){
			temp += thePath[i];
		}else{
			theDirectories.push_back(temp);
			std::cout<<temp<<std::endl;
			temp = "";
		}
	}
	struct DirEntry * anEntry = new struct DirEntry;
	std::pair <bool, int> returnGuy = travelThrough(anEntry, thePath, currentCluster);
	delete anEntry;
	if(returnGuy.first == true){
		clusCurr = returnGuy.second;
		return true;
	}	
	return false;
}
