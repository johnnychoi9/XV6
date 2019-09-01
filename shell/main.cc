#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <cctype>
#include <locale>
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iterator>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* String trimming comes from https://stackoverflow.com/a/217605 */
static inline void trim(std::string &s) {
       s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
       return !std::isspace(ch);}));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
       return !std::isspace(ch);}).base(), s.end());
}

/* String splitting comes from https://stackoverflow.com/a/236803 */
template<typename Out>
void split(const std::string &s, char delim, Out result) {
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
                *(result++) = item;
        }
}
std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> elems;
        split(s, delim, std::back_inserter(elems));
        return elems;
}

// string delimiting code modified from stackoverflow.com/questions/14265581
std::vector<std::string> splitBigBoy(std::string s, std::string delim){
  size_t pos = 0;
  std::string token;
  std::vector<std::string> theGuys;
  while((pos = s.find(delim)) != std::string::npos){
    token = s.substr(0, pos);
    theGuys.push_back(token);
    s.erase(0, pos+delim.length());
  }
  theGuys.push_back(s);
  return theGuys;
}

bool replaceWhitespace(std::string& str, const std::string& d, const std::string& to) {
    size_t st = str.find(d);
    if(st == std::string::npos)
        return false;
    str.replace(st, d.length(), to);
    return true;
}

void parse_and_run_command(const std::string &command, std::vector<std::pair<int, std::string> > &ongoing, int in, int out, bool p) {
    /* TODO: Implement this. */
    /* Note that this is not the correct way to test for the exit command.
       For example the command "   exit  " should also exit your shell.
     */
	std::string comd = command;
	trim(comd);

	while(replaceWhitespace(comd,"\t"," ")) {
	  continue;
	}

	while(replaceWhitespace(comd,"\r"," ")){
	  continue;
	}
	while( replaceWhitespace(comd,"\n"," ")){
	  continue;
	}
	while ( replaceWhitespace(comd,"\f"," ")){
	  continue;
	}
	while( replaceWhitespace(comd,"\v"," ")){
	  continue;
	}
	while(replaceWhitespace(comd, "  "," ")){
	    continue;
	  }
	std::vector<std::string> tokens = split(comd, ' ');
	//	for(long unsigned int i = 0; i <tokens.size(); i++){
	//trim(tokens[i]);
	//}
	
	// first parse the tokens into the following five categories
	// outputDirection, inputDirection, the executablePath, and extraneous args
	if(comd == "exit"){
        exit(0);
        }
	else if(comd == ""){
	  return;
	}
	char dataEatsThings = 'f';
	std::string outputDir = "";
	std::string inputDir = "";
	std::string programPath = "";
	int background = 0;
	std::vector <char *> arguments;
	arguments.push_back(&dataEatsThings);
	//Parse the tokens, lord this took so long and I'm still not entirely sure why this works
	for(long unsigned int i = 0; i < tokens.size(); i++){
	  //first catch individual commands that are being backgrounded
	  if(tokens[i] == "&" && i == tokens.size() -1)
	    {
	     background = 1;
	    }
	  else if(tokens[i] == "&")
	    {
	      std::cerr<<"invalid command. \n";
	      return;
	    }
	  //Now handle operators, so long as it only has a word come after it
        else if(tokens[i] == "<"  && inputDir == "")
	    {
		i++;
		if( i >= tokens.size() || tokens[i] == ">" || tokens[i] == "<" || tokens[i] == "&"){
		    std::cerr<<"invalid command. \n";
		    return;
		  }
		inputDir = tokens[i];
	    }
	  else if (tokens[i] == "<" && inputDir != "")
	    {
	      std::cerr << "Invalid command.\n";
	      return;
	    }
	  else if(tokens[i] == ">"  && outputDir == "")
	    {
		i++;
		if( i >= tokens.size() || tokens[i] == ">" || tokens[i] == "<" || tokens[i] == "&")
		  {
		    std::cerr <<"invalid command. \n";
		    return;
		  }
		outputDir = tokens[i];
	    }
	  else if (tokens[i] == ">" && outputDir != "")
	    {
	      std::cerr << "invalid command. \n";
	      return;
	    }
	  else if (programPath == "")
	    {
	      programPath = tokens[i];
	    }
	  else{
	    // Gotten from cppreference.com from their page on emplace_back
	    auto const& arg = tokens[i];
	    arguments.emplace_back(const_cast<char *>(arg.c_str()));
	  }
	}
	if(programPath == ""){
	  std::cerr <<"invalid command. \n";
	  return;
	}
	  arguments.push_back(nullptr);
	  //This was for checking my parsing, Next time I should make these all a universal var for debugging lol
	  //std::cout << "program is" << programPath << std::endl;
	  //std::cout << "input is " << inputDir << std::endl;
	  //std::cout << "output is"<< outputDir << std::endl;
	  int pstatus = 0;
	  pid_t pid = fork();
	  if(pid < 0){
	    perror("fork failure");
	  }
	  else if(pid == 0){
	    if(!p){
	    //This is like the original single command handling
	    int file;
	    file = open(inputDir.c_str(), O_RDONLY);
	    dup2(file, STDIN_FILENO);
	    close(file);
	    if(outputDir != ""){
	      //This guy only happens if outputDir is wrong
	    int file2;
	    file2 = open(outputDir.c_str(), O_WRONLY | O_CREAT| O_TRUNC , S_IRUSR | S_IWUSR | S_IROTH);
	    dup2(file2, STDOUT_FILENO);
	    close(file2);}
	    execv(programPath.c_str(), arguments.data());    
	    std::cerr << "Command not found\n";
	    return;}
	    else{
	      if(out != STDOUT_FILENO){
		dup2(out, 1);
		close(out);
	      if(in != STDIN_FILENO){
		dup2(in, 0);
	        close(in);
	      }
	    }
	    //Pass everything into the arguments thing
	    execv(programPath.c_str(), arguments.data());    
	    std::cerr << "Command not found\n";
	    return;	      
	    }}
	  else if(background == 0){
	    //this nonsense is right off the website, still don't really understand why I need this except in my main
	  int wstatus;
	  waitpid(pid, &wstatus,0);
	  pstatus = WEXITSTATUS(wstatus);
	  std::cout << programPath <<" exit status: " << pstatus<< std::endl;
	  }else{
	    ongoing.push_back(std::pair<int, std::string>(pid, programPath));
	  }
}

int main(void) {
  std::vector<std::pair<int, std::string> > ongoing;
  int init = dup(STDIN_FILENO);
    while (true) {
        std::string command;
        std::cout << "> ";
        std::getline(std::cin, command);
	trim(command);
	//int background = 0;
	if(command.back() == '|' || command.at(0) == '|'){
	   std::cerr<<"invalid command. \n";
	   continue;
	}
	else {
	  if(command.back() == '&'){
	    //  background = 1;
	    //not gonna handle this hear, old code already does it in parse_and_run
	  }
	std::vector<std::string> commands = splitBigBoy(command, " | ");
	if(commands.size() == 1){
	  parse_and_run_command(command, ongoing, STDIN_FILENO, STDOUT_FILENO, false);
	}else{
	  //now handle all the piping nonsense. 
	  int fd[2];
	  int in = 0;
	  for(unsigned long int d = 0; d < commands.size()-1; d++){
	    int pipefail = pipe(fd);
	    if(pipefail < 0){
	      std::cerr<<"pipe fail"<<std::endl;
	      continue;
	    };
	    std::string newCommand = commands.at(d);
	    parse_and_run_command(newCommand, ongoing, in, fd[1], true);
	    //this came straight out of slides, still don't really understand it.
	    close(fd[1]);
	    in = fd[0];
	  }
	  if(in != 0){
	    dup2(in, 0);
	  }
	  //Run the final command in the queue
	  std::string newCommand = commands.at(commands.size()-1);
	  parse_and_run_command(newCommand, ongoing, in, STDOUT_FILENO, false);
	  dup2(init, 0);
	  close(in);
	}
	int x;
	  for( unsigned long int i = 0; i < ongoing.size(); i++){
	    std::pair<int, std::string> p = ongoing[i];
	    //  printf("pid: %d, command name: %s\n", p.first, p.second.c_str());
	    int newStatus = -1989;
	    x = waitpid(p.first, &newStatus, WNOHANG);
	    if( x != 0){
	      ongoing.erase(ongoing.begin() + i);
	      i--;
	      std::cout << p.second.c_str() <<" exit status: " << newStatus << std::endl;
	    }
	  }
	}
    }
    return 0;
}
