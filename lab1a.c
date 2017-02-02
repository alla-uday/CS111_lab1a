#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>

struct termios original_attributes;
int childPid;
int controlD;
int input_pipe[2];
int output_pipe[2];
struct arg_struct {
  int src;
  int dst;
};
void orig_mode(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_attributes);
}

void  handler(int sig)
{
  kill(childPid,SIGINT);
}

void sigPipehandler()
{
  exit(1);
}
void shell_status(void){
  pid_t checkID;
  int status;
  checkID = waitpid(childPid, &status, WNOHANG|WUNTRACED);
  if (checkID == -1) {
    perror("waitpid error");
   
  }
  else if (checkID == 0) {
 
    printf("Parent waiting for child");
 
  }
  else if (checkID == childPid) {
    if (WIFEXITED(status)){
      printf("Shells exit status : %d",WEXITSTATUS(status));
      printf("\n");
    }
    else if (WIFSIGNALED(status)){
      printf("Child ended abnormally  \n");
    }
    else if (WIFSTOPPED(status)){
      printf("Child process stopped \n ");
    }
  }
}
void no_echo_mode(void)
{
  struct termios tattributes;

  //Save the original attributes for restoring to original mode later
  if (tcgetattr (STDIN_FILENO, &original_attributes) < 0)
    perror("tcsetattr()");
  // tcgetattr (STDIN_FILENO, &original_attributes);
  atexit(orig_mode);

  //checking to see if stdin is a terminal
  if(isatty (STDIN_FILENO)==0)
    {
      fprintf (stderr, "ERROR:stdin not a terminal.\n");
      exit (EXIT_FAILURE);
    }

  //Setting the new terminal modes
  tcgetattr (STDIN_FILENO, &tattributes);
  tattributes.c_lflag = tattributes.c_lflag &  ~(ICANON|ECHO);
  tattributes.c_cc[VMIN] = 1;
  tattributes.c_cc[VTIME] = 0;
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattributes);
}
void *read_all(void *arguments) {
  signal(SIGINT, handler);
  signal(SIGPIPE, sigPipehandler);
  struct arg_struct *args = (struct arg_struct *)arguments;
  char c;int x;
  while(1){
    if(args->dst>args->src){
      x=read(args->src, &c, 1);
      if(x==0){
	 break;
      }
      if(c=='\004'){	
	close(input_pipe[1]);
       	close(output_pipe[0]);
	fflush(stdout);
	controlD=1;
	kill(childPid,SIGHUP);
	break;
      }
      else{
	if(c=='\r' || c== '\n'){
	  char temp ='\n';
	  write(args->dst, &temp, 1);
	  temp='\r';
	  write(STDOUT_FILENO, &temp, 1);
	  temp='\n';
	  write(STDOUT_FILENO, &temp, 1);
	}
	else{
	  write(args->dst, &c, 1);
	  write(STDOUT_FILENO, &c, 1);
	}
      }
    }
    else{
      x=read(args->src, &c, 1);
      if(x==0){
	if(controlD==1){
	  break;
	}
	exit(1);
      }
      if(c==EOF){
	exit(1);
      }
      else{
	write(args->dst, &c, 1);
      }
    }
  }
}
int main(int argc, char** argv) {
  int shell = 0;
  int opt=0;
  int longIndex=0;
  static const char *optString = "s";
  static struct option long_options[] = {
   {"shell", no_argument, NULL, 's'},
   {NULL, no_argument, NULL, 0}
  };
  opt = getopt_long( argc, argv, optString, long_options, &longIndex );
  while( opt != -1 ) {
    switch( opt ) {
    case 's':
      shell=1;
      break;
    default:
      exit(4);
      break;
    }
    opt = getopt_long( argc, argv, optString, long_options, &longIndex );
  }             
  if(shell==1){
    no_echo_mode();
    atexit(shell_status);
    pipe(input_pipe);
    pipe(output_pipe);
    pid_t rc = fork();
    if (rc < 0) { // fork failed; exit
      fprintf(stderr, "fork error\n");
      exit(1);
    }
    else if (rc  == 0) {
      // child process
      close(input_pipe[1]);
      close(output_pipe[0]);
      dup2(input_pipe[0], STDIN_FILENO);
      dup2(output_pipe[1], STDOUT_FILENO);
      dup2(output_pipe[1], STDERR_FILENO);
      
      // exec the given program
      char *myargs[2];
      myargs[0] = strdup("/bin/bash");
      myargs[1]=NULL;
      if (execvp(myargs[0] ,myargs ) == -1) {
	perror("failed to start subprocess");
	return EXIT_FAILURE;
      }
    }
    childPid=rc;
    pthread_t tid1,tid2;
    struct arg_struct args1;
    struct arg_struct args2;
    args1.src=STDIN_FILENO;
    args1.dst=input_pipe[1];
    args2.src=output_pipe[0];
    args2.dst=STDOUT_FILENO;

    // parent process
    close(input_pipe[0]);
    close(output_pipe[1]);
    pthread_create(&tid1, NULL, &read_all, (void *)&args1);
    pthread_create(&tid2, NULL, &read_all, (void *)&args2);
    pthread_join(tid1,NULL);
    pthread_join(tid2,NULL);
    close(input_pipe[1]);
    fflush(stdout);
    close(output_pipe[0]);
    exit(0);
  }
  else{
    char c;
    no_echo_mode();
    while(1)
      {
	read (STDIN_FILENO, &c, 1);
	if (c == '\004'){
	  break;
	}
	else {
	  if(c=='\r' || c== '\n'){
	    char tempChar = '\r';
	    write(STDOUT_FILENO, &tempChar, 1);
	    tempChar = '\n';
	    write(STDOUT_FILENO, &tempChar, 1);
	  }
	  else{
	    write(STDOUT_FILENO, &c, 1);
	  }
	}
      }
    exit(0);
  }
}
