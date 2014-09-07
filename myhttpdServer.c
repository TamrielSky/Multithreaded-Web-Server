/*
 * myhttpdServer.c - A multithreaded web server
 *
 * $Author: Rajesh Balasubramanian $
   Person Number: 50097507
 */

static char svnid[] = "$Id: myhttpdServer.c 6 2009-07-03 03:18:54Z  $";

#define	BUF_LEN	9192

#include        <semaphore.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<inttypes.h>
#include        <sys/stat.h>
#include        <pthread.h>
#include        <dirent.h>
#include        <time.h>
#include        <arpa/inet.h>
#include        <pwd.h>
#include        <errno.h>

void printUsage();
char* str_replace ( const char *string, const char *substr, const char *replacement );
char *progname;
char buf[BUF_LEN];
pid_t pid;
void delSpace(char *);
void usage();
//int setup_client();
int setup_server(int);
void queueHttpRequest(void *);
void schedHttpRequest();
void execHttpRequest();
void send_new(int fd, char *msg);



char *currentDir;
unsigned int fsize(char *); 
fd_set readfds;
fd_set masterfds;
char *abspath;
int  ch, server, done, aflg;
int soctype = SOCK_STREAM;
char *host = NULL;
char *port = NULL;
extern char *optarg;
extern int optind;
char sched[50];
int queueTime;
char* logFile = NULL;
int debug = 0;
sem_t countThreads;
sem_t signalThread;

pthread_mutex_t fileWrite_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readqueue_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t execqueue_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t block_cond_emptyqueue = PTHREAD_COND_INITIALIZER;
pthread_cond_t block_cond_exec = PTHREAD_COND_INITIALIZER;

typedef struct Element
{
	struct Element* next;
	int fileDes;
	char* request;
	int fileSize;
	char *fileName;
	char *filePath;
	char requestType[60];
	char ipAddress[100]; 
	time_t queuingTime;
	time_t execTime;
	int responseSize;
	int requestStatus;   

}readyQueueElement;

readyQueueElement* findmin(readyQueueElement*);

readyQueueElement* queueHead;
readyQueueElement* sharedElement;

int main(int argc,char *argv[])
{
	pthread_t queueThread, schedThread;
	pthread_t *execThread;


	int numofthreads=4;
	int s, i, sock;
	struct sockaddr_in msgfrom;
	int msgsize;
	union {
		uint32_t addr;
		char bytes[4];
	} fromaddr;
	abspath = argv[0];
	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;
	while ((ch = getopt(argc, argv, "adhs:p:n:l:r:t:")) != -1)
		switch(ch) {
			case 'a':
				aflg++;		/* print address in output */
				break;
			case 'd':
				debug = 1;
				break;
			case 's':
				//	sched= (char*)malloc(sizeof(optarg));
				strcpy(sched, optarg);

				break;
			case 'p':
				port = optarg;
				break;
			case 'n':
				numofthreads = atoi(optarg);
				break;
			case 'l':
				logFile = (char*)malloc(strlen(optarg));
				strcpy(logFile, optarg);
				break; 
			case 'r':
				currentDir = (char*)malloc(strlen(optarg));
                                
                                strcat(currentDir, optarg);
				break;

			case 'h':
				printUsage();
				break;
			case 't':
				queueTime = atoi(optarg);
				break;  
			case '?':
			default:
				usage();
		}
	server = 1;
	argc -= optind;
	if (argc != 0)
		usage();
	if (!server && (host == NULL || port == NULL))
		usage();
	if (server && host != NULL)
		usage();
	/*
	 * Create socket on local host.
	 */
         
        if(debug != 1)
        {

          pid = fork(); 
	   
	   /* If the pid is less than zero,
	      something went wrong when forking */
	      if (pid < 0) {
	          exit(EXIT_FAILURE);
		  }
		   
		   /* If the pid we got back was greater
		      than zero, then the clone was
		         successful and we are the parent. */
	      if (pid > 0) 
	      {
	         exit(EXIT_SUCCESS);
	      }
			      
			      /* If execution reaches this point we are the child */


              umask(0); 
	      pid_t sid;
	       
	       /* Try to create our own process group */
	       sid = setsid();
	       if (sid < 0) {
		       exit(EXIT_FAILURE);
		       }
	       close(stdin);
	       close(stdout);
	       close(stderr);

        }
      
	if ((s = socket(AF_INET, soctype, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	//       if (!server)
	//		sock = setup_client();
	//	else
	//
	sock = setup_server(s);
	execThread = (pthread_t*)malloc(numofthreads*sizeof(pthread_t));

	/*              
	 * Set up select(2) on both socket and terminal, anything that comes
	 * in on socket goes to terminal, anything that gets typed on terminal
	 * goes out socket...
	 */

	sem_init(&countThreads, 0, numofthreads);
	sem_init(&signalThread, 0, 0);          
	pthread_create(&queueThread, NULL, (void*) &queueHttpRequest, (void*) &sock);
	pthread_create(&schedThread, NULL, (void*) &schedHttpRequest, NULL);      
	for(i=0;i<numofthreads;i++)
	{
		pthread_create(&execThread[i], NULL, (void*) &execHttpRequest, NULL);  
	}  
	//pthread_create(&schedThread, NULL, schedHttpRequest, &queueHead);
	/*	while (!done) {
		FD_ZERO(&ready);
		FD_SET(sock, &ready);
		FD_SET(fileno(stdin), &ready);
		if (select((sock + 1), &ready, 0, 0, 0) < 0) {
		perror("select");
		exit(1);
		}
		if (FD_ISSET(fileno(stdin), &ready)) {
		if ((bytes = read(fileno(stdin), buf, BUF_LEN)) <= 0)
		done++;
		send(sock, buf, bytes, 0);
		}
		msgsize = sizeof(msgfrom)	;
		if (FD_ISSET(sock, &ready)) {
		if ((bytes = recvfrom(sock, buf, BUF_LEN, 0, (struct sockaddr *)&msgfrom, &msgsize)) <= 0) {
		done++;
		} else if (aflg) {
		fromaddr.addr = ntohl(msgfrom.sin_addr.s_addr);
		fprintf(stderr, "%d.%d.%d.%d: ", 0xff & (unsigned int)fromaddr.bytes[0],
		0xff & (unsigned int)fromaddr.bytes[1],
		0xff & (unsigned int)fromaddr.bytes[2],
		0xff & (unsigned int)fromaddr.bytes[3]);
		}
		write(fileno(stdout), buf, bytes);
	 *///		}
	//	}
	pthread_join(queueThread, NULL);
	return(0);
}

/*
 * setup_client() - set up socket for the mode of soc running as a
 *		client connecting to a port on a remote machine.
 */
/*
   int
   setup_client() {

   struct hostent *hp, *gethostbyname();
   struct sockaddr_in serv;
   struct servent *se;


 * Look up name of remote machine, getting its address.

 if ((hp = gethostbyname(host)) == NULL) {
 fprintf(stderr, "%s: %s unknown host\n", progname, host);
 exit(1);
 }

 * Set up the information needed for the socket to be bound to a socket on
 * a remote host.  Needs address family to use, the address of the remote
 * host (obtained above), and the port on the remote host to connect to.

 serv.sin_family = AF_INET;
 memcpy(&serv.sin_addr, hp->h_addr, hp->h_length);
 if (isdigit(*port))
 serv.sin_port = htons(atoi(port));
 else {
 if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
 perror(port);
 exit(1);
 }
 serv.sin_port = se->s_port;
 }

 if (connect(s, (struct sockaddr *) &serv, sizeof(serv)) < 0) {
 perror("connect");
 exit(1);
 } else
 fprintf(stderr, "Connected...\n");
 return(s);
 }
 */
/*
 * setup_server() - set up socket for mode of soc running as a server.

 */
int
setup_server(int s) {
	struct sockaddr_in serv, remote;
	struct servent *se;
	int newsock, len;

	len = sizeof(remote);
	memset((void *)&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	if (port == NULL)
        {
        
		serv.sin_port = htons(8080);
        }
	else if (isdigit(*port))
		serv.sin_port = htons(atoi(port));
	else {
		if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
			perror(port);
			exit(1);
		}
		serv.sin_port = se->s_port;
	}
	if (bind(s, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
		perror("bind");
		exit(1);
	}
	if (getsockname(s, (struct sockaddr *) &remote, &len) < 0) {
		perror("getsockname");
		exit(1);
	}
	if(listen(s, 10) == -1)
	{
		fprintf(stderr, "listen error");
	}
	newsock = s;
	//if (soctype == SOCK_STREAM) {
	//	fprintf(stderr, "Entering accept() waiting for connection.\n");
	//	newsock = accept(s, (struct sockaddr *) &remote, &len);
	//	}
	return(newsock);
}

/*
 * usage - print usage string and exit
 */

void handleTild(char* filePath)
{
        
        char cwd[3000];
	char tildPath[80];
	FILE *in;
	
   	char* username = (char*)malloc(2000);
        char* remainPath = (char*)malloc(2000);
        
        char* myname;


	struct passwd *user;
        char c = *(filePath +1); 
        char tild = 126;
	if(c == tild)
	{
		strcpy(tildPath, filePath);
		int i = 2;
                int j = 0;
		while(tildPath[i] != '/')
		{
			username[j] = tildPath[i];
				i++;
                                j++;   

		}
		username[j] = '\0';
                myname = (char*)malloc(strlen(username));
 
                int k = 0;
                while(!isspace(tildPath[i])) 
                {
                  remainPath[k] = tildPath[i];
                  i++;
                  k++;
                }
                remainPath[k] = '\0';
                
                strncpy(myname, username, strlen(username));   
		if ((user = getpwnam((const char*)myname)) == NULL)
			perror("getpwnam() error");

		else
                  { 
                   strcpy(cwd, user->pw_dir);
                   strcpy(cwd, "/myhttpd");
                   strcat(cwd, remainPath);
                   strcpy(filePath, cwd);
                  }

           }
       

}

unsigned int fsize(char *filePath)
{

	struct stat st; 

	FILE *in;
	char cwd[3024];
        


		if(currentDir == NULL)
		{
			getcwd(cwd, sizeof(cwd));

			strcat(cwd, filePath);
			in = fopen(cwd,"rb");
			if (!in) 
			{
				//exit(1);
			}
			else{
				if (stat(cwd, &st) == 0)
				{
					fclose(in);
					return st.st_size;
				}
			}
		}
		else
		{ 
			strcpy(cwd, currentDir);
			strcat(cwd, filePath);
			in = fopen(cwd,"rb");
			if (!in) 
			{
				//exit(1);
			}
			if (stat(cwd, &st) == 0){
				fclose(in);
				return st.st_size;
			}
		}
	

	return 0; 
}













void fileLastModTime(char* filePath, char* lastModTime)
{
	char cwd[1024];
	FILE* in; 
	struct stat st;
	struct tm * ptm;


	if(currentDir == NULL)
	{
		getcwd(cwd, sizeof(cwd));

		strcat(cwd, filePath);
		in = fopen(cwd,"rb");
		if (!in)
		{

			strcpy(lastModTime,  "undefined as file not found");
		}
		else{
			if (stat(cwd, &st) == 0){
				fclose(in);
				ptm = gmtime(&st.st_mtime);
				strftime (lastModTime,60,"%c",ptm);

			}
			else
			{ 
			} 
		}
	}
	else
	{
		strcpy(cwd, currentDir);
		strcat(cwd, filePath);
		in = fopen(cwd,"rb");
		if (!in)
		{
			strcpy(lastModTime, "undefined as file not found");
                        //exit(1);
		}
		if (stat(cwd, &st) == 0){
			fclose(in);  
			ptm = gmtime(&st.st_mtime);
			strftime (lastModTime,60,"%c",ptm);


		}
	}      


}





int findFileInDir(char* file, char* filePath)
{
	char *cwd = (char*)malloc(1024);
	char * splitter = (char*)malloc(5024);
	DIR *dir;
	struct dirent *ent;

	if(currentDir == NULL)
	{
		getcwd(cwd, 1024);
		// delSpace(filePath);
		int pathLen = strlen(filePath);
		strcat(cwd, filePath);
		if(strcmp((filePath + (pathLen-2)), "/"))                    
			strcat(cwd, "/");

		if ((dir = opendir (cwd)) != NULL) {
			/* print all the files and directories within directory */
			while ((ent = readdir (dir)) != NULL) {
				if(strcmp(file,ent->d_name) == 0)
				{
					return 1;
				} 
			}
			closedir(dir);
		}else{
			/* could not open directory */
			return 0;

		}

	}
	else{
		strcpy(cwd, currentDir);
		strcat(cwd, filePath);
		int pathLen = strlen(filePath);
		if(strcmp((filePath + (pathLen-2)), "/"))
			strcat(cwd, "/");

		if((dir = opendir (cwd)) != NULL) {
			/* print all the files and directories within directory */
			while ((ent = readdir (dir)) != NULL) {
				if(strcmp(file,ent->d_name) == 0) 
					return 1;


			}
			closedir(dir);
		} else {
			/* could not open directory */
			return 0;
		}

	}

	return 0;
}





int printDirectoryListing(char *filePath, char* listDir)
{
	char *cwd = (char*)malloc(1024);
	char * splitter = (char*)malloc(5024);
	DIR *dir;
	struct dirent *ent;
        
	
	if(currentDir == NULL)
	{
		getcwd(cwd, 1024);
		// delSpace(filePath);
		strcat(cwd, filePath);
		strcat(cwd, "/");

		if ((dir = opendir (cwd)) != NULL) {
			/* print all the files and directories within directory */
			while ((ent = readdir (dir)) != NULL) {
				if(strcmp(".",ent->d_name) == 0 || strcmp("..",ent->d_name) == 0)
					continue;
				strcat(listDir, ent->d_name); 
				strcat(listDir, "<br>");

			}
			closedir(dir);
		} else {
			/* could not open directory */
			return 0;

		}

	}
	else
	{
		strcpy(cwd, currentDir);
		strcat(cwd, filePath);
		strcat(cwd, "/");

		if ((dir = opendir (cwd)) != NULL) {
			/* print all the files and directories within directory */
			while ((ent = readdir (dir)) != NULL) {
				if(strcmp(".",ent->d_name) == 0 || strcmp("..",ent->d_name) == 0)
					continue;

				strcat(listDir, ent->d_name);
				strcat(listDir, "<br>");

			}                
			closedir(dir);
		} else {
			/* could not open directory */
			return 0;
		}

	}
  
 
 return 1;

}

void delSpace (char *Str)
{
	int Pntr = 0;
	int Dest = 0;

	while (Str [Pntr])
	{
		if (Str [Pntr] != '\n')
			Str [Dest++] = Str [Pntr];
		Pntr++;
	}

	Str [Pntr] = 0;
}

void execHttpRequest()
{
	
	char* mimeTypes[9] = {"text/html", "text/xml","text/plain", "text/css", "image/png", "image/gif", "image/jpg", "image/jpeg", "application/zip"};

	char* mimeExtension[9] = {"html", "xml","plain", "css", "png", "gif", "jpg", "jpeg", "zip"};

	char* error404 = "<html><h1>You've 404'd it!</h1><br><h2>Surfin ain't easy, and right now, you're lost at sea.</h2></html>"; 


	while(1)
	{
		pthread_mutex_lock(&execqueue_mutex);
		pthread_cond_wait(&block_cond_exec, &execqueue_mutex);
                

		// execute the head element
		sem_post(&signalThread);

                int dirListing = 0;
	char *fullFilePath;
	int fileSize, fileDes;
	char *request;
	char *fileName;
	char *filePath;
	char requestType[6];
	char* buffer;
	char cwd[2024];
	int bytesread;
	char* headers;
	char* headerData;
	int headerSize;
	time_t now;
	char listBuffer[1000] = "";

	int isHead = 0;
	char queuingTime[60];
	char execTime[60];
	char lastModTime[60];
	char nowTime[60];
	char *requestStatus;
	int responseSize;
	char ipAddress[100];
	int byteswrite;
	char* message;
	int requestCode;
        char *tildedPath = (char*)malloc(3000);  
	readyQueueElement *headElement, *deleteElement;
	FILE *f;
               
 

		fileSize = sharedElement->fileSize;
		fileDes = sharedElement->fileDes;
		fileName = (char*)malloc(strlen(sharedElement->fileName)+ 1);
		request = (char*)malloc(strlen(sharedElement->request)+1);
		fullFilePath = (char*)malloc(strlen(sharedElement->request) + 1 );
               
		filePath = (char*)malloc(strlen(sharedElement->filePath) + 1);
		strcpy(fileName, sharedElement->fileName);
		strcpy(request, sharedElement->request);
		strcpy(filePath, sharedElement->filePath);
		strcpy(requestType, sharedElement->requestType);
		strcpy(ipAddress, sharedElement->ipAddress);
		strftime (queuingTime,60,"%c", gmtime(&sharedElement->queuingTime));

		strftime(execTime, 60, "%c", gmtime(&sharedElement->execTime));


		pthread_mutex_unlock(&execqueue_mutex);
                
		if(strstr(requestType, "GET") != NULL)
		{
                       
                                
  
			if(strstr(fileName, ".") != NULL)
			{
				buffer = (char*)malloc(fileSize + 1);

                                if(currentDir != NULL)
				{ 
					strcpy(fullFilePath, currentDir);
					strcat(fullFilePath, filePath);
					f = fopen(fullFilePath, "rb");
					if(!f){
						requestStatus = "404 Not Found"; 
						requestCode = 404;
                                                bytesread = strlen(error404);

					}  
					else{
						requestStatus = "200 OK";
						requestCode = 200;
						bytesread = fread(buffer, 1, fileSize, f);
						close(f);
					}
				}
				else
				{ 



					getcwd(cwd, sizeof(cwd));
					strcat(cwd, filePath);
					f = fopen(cwd, "rb");
					if(!f)
					{ 
						requestStatus = "404 Not Found";
						requestCode = 404;
                                                bytesread = strlen(error404);

					} 
					else{
						requestStatus = "200 OK";
						requestCode = 200;
						bytesread = fread(buffer, 1, fileSize, f);
						close(f);
					}



				}
			}
			else
			{
				dirListing = 1; 
                                
				if(!printDirectoryListing(filePath, listBuffer))
				{

					requestCode = 404;
					requestStatus = "404 Not Found";
                                        bytesread = strlen(error404);
				}
				else
				{
					bytesread = strlen(listBuffer);
					requestCode = 200;
					requestStatus = "200 OK";

				}
			}
		} 
		else{
			isHead = 1;


		}

		// Send headers first

		char* compareType = NULL;
		char* type = NULL;  

		if(strstr(fileName, ".") != NULL)
		{   
			strtok(fileName, ".");
			compareType = strtok(NULL, ".");
		}  

		headers = "HTTP/1.1 %s\r\nDate : %s\r\nLast-Modified : %s\r\nContent-Type : %s\r\nContent-Length : %d\r\nServer : myHttpd\r\n\r\n";
		time(&now);


		headerData = (char*)malloc(900);
		strftime (nowTime,60,"%c", gmtime(&now));
		fileLastModTime(filePath, lastModTime);
                if(lastModTime == NULL)
                   strcpy(lastModTime,  "Undefined as file not found"); 

		if(compareType != NULL)
		{   

			int cmp = 0;
			int cmp1 = 0;
			for(cmp = 0; cmp < 9; cmp++)
			{
				if(!strcmp(mimeExtension[cmp], compareType)) 
				{  
					for(cmp1 = 0; cmp1 < 9; cmp1++)
					{
						if(strstr(mimeTypes[cmp1], mimeExtension[cmp]) != NULL) 
						{
							type = mimeTypes[cmp1];
						}
					}
					break;
				}
			}   

			if(type == NULL)
				type = "text/html";   

		}
		else
		{
			type = "text/html";
		}       
		if(dirListing)
		{

			sprintf(headerData, headers, requestStatus, nowTime, lastModTime, "text/html", bytesread);
		}
		else
			sprintf(headerData, headers, requestStatus, nowTime, lastModTime, type, bytesread);

		headerSize = strlen(headerData);
		if((sendAll(fileDes, headerData, headerSize) == -1))
			fprintf(stderr, "send error \n");     

		//Send data and handle partial send

		if(!isHead)
		{   
			if(dirListing)
			{
				dirListing = 0; 
				if(requestCode != 404)
				{
					if((sendAll(fileDes, listBuffer, bytesread) == -1 ))
					{   
						fprintf(stderr, "send error\n");

					}   
				}

				else
				{  
					if((sendAll(fileDes, error404, strlen(error404) ) == -1 ))
					{
						fprintf(stderr, "send error\n");

					}
				}

			} 

			else
			{ 
				if(requestCode != 404)
				{
					if((sendAll(fileDes, buffer, bytesread) == -1 ))
					{    

						fprintf(stderr, "send error in data \n");

					}
				}
				else
				{   
					if((sendAll(fileDes, error404, strlen(error404)) == -1 ))
					{
						fprintf(stderr, "send error\n");

					}
				}  
			}
		}
		close(fileDes);
		//FD_CLR(fileDes, &masterfds);

		char logMessage[1000];
		if(logFile != NULL)
		{
                                       pthread_mutex_lock(&fileWrite_mutex);

			f = fopen(logFile, "a+");
			if(!f)
                         { }
			else{

				message = "%s - [%s - 0600] [%s - 0600] \"%s\" %d %d\n";
				if(requestCode == 404)
					responseSize = 0; 
				else
					responseSize = bytesread;


				int i = 0;
				int j = 0; 
				char* mainRequest = (char*)malloc(1000);

				while(j < 3)
				{
					mainRequest[i] = request[i];
					i++;
					if(isspace(request[i]))
						j++;

					if(i == strlen(request))
						break;

				}

				mainRequest[i] = '\0';	
				sprintf(logMessage, message, ipAddress, queuingTime, execTime, mainRequest, requestCode, responseSize); 
				byteswrite = fwrite(logMessage, 1, strlen(logMessage), f);
				if(debug){ 
					fprintf(stderr, "%s\n", logMessage);
                                 }
				fflush(f);
				close(f);
                                                pthread_mutex_unlock(&fileWrite_mutex);

			}


		}

		if(logFile == NULL && debug){
                   message = "%s - [%s - 0600] [%s - 0600] \"%s\" %d %d\n";
                                if(requestCode == 404)
                                        responseSize = 0;
                                else
                                        responseSize = bytesread;


                                int i = 0;
                                int j = 0;
                                char* mainRequest = (char*)malloc(1000);

                                while(j < 3)
                                {
                                        mainRequest[i] = request[i];
                                        i++;
                                        if(isspace(request[i]))
                                                j++;

                                        if(i == strlen(request))
                                                break;

                                }

                                mainRequest[i] = '\0';
                                sprintf(logMessage, message, ipAddress, queuingTime, execTime, mainRequest, requestCode, responseSize);

			fprintf(stderr, " %s\n", logMessage); 
                       
                   }
		

		isHead = 0;
                sem_post(&countThreads);
	}

}


//handle partial send
int sendAll(int fd, char *buffer, int bufflen)
{
	int leftover=bufflen;
	int bytessent = 0;  

	while(leftover != 0)
	{
		bytessent = send(fd, buffer+bytessent, leftover, 0);
		if(bytessent == -1)
			return -1;
		leftover = bufflen - bytessent;

	}
	return 1;
}



void schedHttpRequest()
{


	readyQueueElement *minElement;
	readyQueueElement *tempElement;
	readyQueueElement *minElementPrevious;
	int tempfileSize, tempfileDes;
	char *temprequest;
	char *tempfileName;
	char *tempfilePath;
	int makeHeadNull = 0; 
	readyQueueElement *headElement;
	if(queueTime)
		sleep(queueTime);
	while(1)
	{
                sem_wait(&countThreads);
 
		pthread_mutex_lock(&readqueue_mutex);


		while(queueHead == NULL)
		{
			pthread_cond_wait(&block_cond_emptyqueue, &readqueue_mutex);
			//sleep(30);
		} 

		if(!strcmp(sched, "SJF"))
		{   


			minElement = findmin(queueHead);


			pthread_mutex_lock(&execqueue_mutex);


			//allocate memory to shared Element

			sharedElement = (readyQueueElement*)malloc(sizeof(readyQueueElement));
			sharedElement->fileName = (char*)malloc(strlen(minElement->fileName)+1 );
			sharedElement->request = (char*)malloc(strlen(minElement->request) + 1);
			sharedElement->filePath = (char*)malloc(strlen(minElement->filePath)+1);



			//assign values to shared Element
			strcpy(sharedElement->fileName, minElement->fileName);
			strcpy(sharedElement->request, minElement->request); 
			strcpy(sharedElement->filePath, minElement->filePath);
			strcpy(sharedElement->requestType, minElement->requestType);
			sharedElement->fileDes = minElement->fileDes;
			sharedElement->fileSize = minElement->fileSize;
			time(&sharedElement->execTime);
			strcpy(sharedElement->ipAddress, minElement->ipAddress);     
			sharedElement->queuingTime = minElement->queuingTime; 

			pthread_cond_signal(&block_cond_exec);

			pthread_mutex_unlock(&execqueue_mutex);

			if(minElement == queueHead)
			{  
				queueHead = queueHead->next;

				free(minElement);
			}
			else
			{
				tempElement = queueHead;
				while(tempElement != minElement)
				{
					minElementPrevious = tempElement;
					tempElement = tempElement->next;
				}

				minElementPrevious->next = minElement->next;
				free(minElement);     
			}

		}

		else
		{

			pthread_mutex_lock(&execqueue_mutex);

			sharedElement = (readyQueueElement*)malloc(sizeof(readyQueueElement));
			sharedElement->fileName = (char*)malloc(strlen(queueHead->fileName)+1 );
			sharedElement->request = (char*)malloc(strlen(queueHead->request) + 1);
			sharedElement->filePath = (char*)malloc(strlen(queueHead->filePath)+1);


			//assign values to shared Element
			strcpy(sharedElement->fileName, queueHead->fileName);
			strcpy(sharedElement->request, queueHead->request);
			strcpy(sharedElement->filePath, queueHead->filePath);
			strcpy(sharedElement->requestType, queueHead->requestType);
			sharedElement->fileDes = queueHead->fileDes;
			sharedElement->fileSize = queueHead->fileSize;
			time(&sharedElement->execTime);
			strcpy(sharedElement->ipAddress, queueHead->ipAddress);
			sharedElement->queuingTime = queueHead->queuingTime;


			pthread_mutex_unlock(&execqueue_mutex);  
			pthread_cond_signal(&block_cond_exec);



			if(queueHead->next == NULL)
			{ 
				free(queueHead);
				queueHead = NULL;


			}
			else
			{
				minElement = queueHead;
				queueHead = queueHead->next;
				free(minElement);  
			}

		} 
		pthread_mutex_unlock(&readqueue_mutex);
		sem_wait(&signalThread);
	}          


}



readyQueueElement* findmin(readyQueueElement *head)
{
	readyQueueElement *minElement;



	minElement = head;

	while(head != NULL )
	{
		if(minElement->fileSize > head->fileSize)
		{   

			minElement = head;
		}

		head = head->next;

	}
	return minElement;
}


void queueHttpRequest(void *sock)
{       
	char* splitme;
	char* splitmeagain;
	char* getFileName;
	char* getFilePath;
	char* nospacepath;
	char* getBuffer;
	char* delimiterSlash = "/";
	char* delimiterSpace = " ";
	int i, readcount,  k, len, newClient, bytes;
	int socket = *((int*)sock);
	readyQueueElement* queueElement;
	readyQueueElement* newElement;
	int maxfd, j;
	struct sockaddr_in fromaddr;

	FD_ZERO(&masterfds);
	FD_ZERO(&readfds);
	FD_SET(socket, &masterfds);
	maxfd = socket;
	len = sizeof(fromaddr);

	while(1)
	{      

		if((newClient = accept(socket, (struct sockaddr*)&fromaddr, &len)) == -1)
		{
			printf("server accept error");
		}
		else
		{


		}
		if((bytes = recv(newClient, buf, sizeof(buf), 0)) <= 0)
		{
			close(newClient);
			// FD_CLR(i, &masterfds); 	
		}
		else
		{   
			if(strstr(buf, "favicon.ico") != NULL)
			{   
				close(newClient);
				continue;

			} 
			splitme = (char*)malloc(sizeof(buf));
			splitmeagain = (char*)malloc(sizeof(buf) + 1);
			getFileName = (char*)malloc(sizeof(buf) + 1);
			getFilePath = (char*)malloc(sizeof(buf)+ 1);
			getBuffer = (char*)malloc(sizeof(buf)+ 1);
			nospacepath = (char*)malloc(sizeof(buf)+ 1);
			strncpy(getFilePath, buf, strlen(buf));
			strncpy(getBuffer, buf, strlen(buf)); 

			splitme = strtok(getBuffer, " ");



			splitme = strtok(NULL ," ");


			strncpy(splitmeagain, splitme, strlen(splitme));
			j = 1;
			k = 1;
			strtok(splitme, delimiterSlash);

			while(splitme !=NULL)
			{
				strcpy(splitmeagain, splitme);

				splitme = strtok(NULL, delimiterSlash);
				k++;

			}
			if(k==2)
			{
				strcpy(getFileName, &splitmeagain[1]); 
			}
			else
				strcpy(getFileName, splitmeagain); 

			getFilePath = strtok(getFilePath, " ");
			getFilePath = strtok(NULL, " ");

			pthread_mutex_lock(&readqueue_mutex);

			if(queueHead == NULL)
			{

				queueHead = (readyQueueElement*)malloc(sizeof(readyQueueElement));

				time(&queueHead->queuingTime);    
				queueHead->request = (char*)malloc(sizeof(buf));
				strncpy(queueHead->request, buf, strlen(buf));

				queueHead->filePath = (char*)malloc(1000);

				j = 0;
				while(!isspace(getFilePath[j]))
				{														                                      nospacepath[j] = getFilePath[j];
					j++;
				}
				nospacepath[j] = '\0'; 		 												                                        getFilePath = nospacepath;

				queueHead->fileName = (char*)malloc(1000);




				if(strstr(queueHead->request, "GET") != NULL)
				{


					strcpy(queueHead->requestType, "GET");
                                        
                                        handleTild(getFilePath);

					if(strstr(getFileName, ".") == NULL)
					{
						if(findFileInDir("index.html", getFilePath))
						{  

							strcat(getFilePath, "/index.html");
							getFileName = "index.html";  
						}
					}



					strncpy(queueHead->fileName, getFileName, strlen(getFileName));
					strncpy(queueHead->filePath, getFilePath, strlen(getFilePath));

					queueHead->fileSize = fsize(queueHead->filePath);						     

				}
				else
				{
                                        handleTild(getFilePath);
					strncpy(queueHead->fileName, getFileName, strlen(getFileName));
					strncpy(queueHead->filePath, getFilePath, strlen(getFilePath));
					queueHead->fileSize = 0;   
					strcpy(queueHead->requestType, "HEAD");
				}


				queueHead->fileDes = newClient;
				strcpy(queueHead->ipAddress, inet_ntoa(fromaddr.sin_addr));
				queueHead->next = NULL;
				pthread_cond_signal(&block_cond_emptyqueue);

			}
			else
			{

				queueElement = queueHead;
				while(queueElement->next!=NULL)
				{
					queueElement = queueElement->next;
				}

				newElement = (readyQueueElement*)malloc(sizeof(readyQueueElement));
				time(&newElement->queuingTime);  
				newElement->request = (char*)malloc(sizeof(buf));
				strcpy(newElement->request, buf);

				newElement->fileName = (char*)malloc(1000);
				strcpy(newElement->fileName, getFileName);

				newElement->filePath = (char*)malloc(sizeof(buf));

				j = 0;
				while(!isspace(getFilePath[j]))
				{
					nospacepath[j] = getFilePath[j];
					j++;
				}
				nospacepath[j] = '\0';  
				getFilePath = nospacepath;          


				if(strstr(newElement->request, "GET") != NULL)
				{
					strcpy(newElement->requestType, "GET");
                                        handleTild(getFilePath);
					if(strstr(getFileName, ".") == NULL)
					{
						if(findFileInDir("index.html", getFilePath))
						{   
							strcat(getFilePath, "index.html");
							newElement->fileName = "index.html";

						}  
					}

					strncpy(newElement->fileName, getFileName, strlen(getFileName));
					strncpy(newElement->filePath, getFilePath, strlen(getFilePath));

					newElement->fileSize = fsize(newElement->filePath);

				}

				else
				{
                                        handleTild(getFilePath);
					strncpy(newElement->fileName, getFileName, strlen(getFileName));
					strncpy(newElement->filePath, getFilePath, strlen(getFilePath));

					newElement->fileSize = 0;
					strcpy(newElement->requestType, "HEAD");
				}


				newElement->fileDes = newClient;
				strcpy(newElement->ipAddress, inet_ntoa(fromaddr.sin_addr));

				newElement->next = NULL;
				queueElement->next = newElement;                

			}
			pthread_mutex_unlock(&readqueue_mutex);





			//memset(buf, 0, sizeof(buf));
			//memset(&splitme[0], 0, sizeof(splitme));
			//memset(&splitmeagain[0], 0, sizeof(splitmeagain));
			//memset(&getFileName[0], 0, sizeof(getFileName));
			//   splitme = NULL;
			//splitmeagain = NULL;
			//getFileName = NULL;
			//free(splitme);
			//free(buf);
			//free(getFilePath);

			//getFilePath = NULL;
		}    
		memset(buf, 0, 9192 );


		//memset(&buf[0], 0, sizeof(buf));
		//memset(&splitme[0], 0, sizeof(splitme));
		//memset(&splitmeagain[0], 0, sizeof(splitmeagain));
		//memset(&getFileName[0], 0, sizeof(getFileName));

	}

}

void usage()
{
	fprintf(stderr, "usage: %s -h host -p port\n", progname);
	fprintf(stderr, "usage: %s -s [-p port]\n", progname);
	exit(1);
}


void printUsage()
{
  fprintf(stderr, "−d : Enter debugging mode. That is, do not daemonize, only accept one connection at a time and enable logging to stdout. Without this option, the web server should run as a daemon process in the background\n−h : Print a usage summary with all options and exit.\n−l file : Log all requests to the given file. See LOGGING for details\n.−p port : Listen on the given port. If not provided, myhttpd will listen on port 8080.\n−r dir : Set the root directory for the http server to dir.\n−t time : Set the queuing time to time seconds. The default should be 60 seconds.\n−n threadnum: Set number of threads waiting ready in the execution thread pool to threadnum.\nThe default should be 4 execution threads.\n−s sched : Set the scheduling policy. It can be either FCFS or SJF. The default will be FCFS\n");
exit(1);
}
