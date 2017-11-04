/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 50 // max number of bytes we can get at once

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;//errno holds the error code from the last system call

    /* DEFINITION OF WAITPID... Wait for a child matching PID to die.
   If PID is greater than 0, match any process whose process ID is PID.
   If PID is (pid_t) -1, match any process.
   If PID is (pid_t) 0, match any process with the
   same process group as the current process.
   If PID is less than -1, match any process whose
   process group is the absolute value of PID.
   If the WNOHANG bit is set in OPTIONS, and that child
   is not already dead, return (pid_t) 0.  If successful,
   return PID and store the dead child's status in STAT_LOC.
   Return (pid_t) -1 for errors.  If the WUNTRACED bit is
   set in OPTIONS, return status for stopped children; otherwise don't.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
    //Keeps checking for a process -1 if no processes, 0 if unterminated child exists.
    //Wait until process is killed.
    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
//The kernel supplies this value.  You can get IPV4 or IPV6.
//IPV4 example: 192.0.2.11
//IPV6 example: 2001:0db8:c9d2:aee5:73e3:934a:a5ae:9551
void *get_in_addr(struct sockaddr *sa)
{
    //If AF_INET == IPV4 then we are using protocol IPV4.
    if (sa->sa_family == AF_INET) {//sa_family: address family, of socket address.  IPV4 or IPV6?
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(void)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    char buf[MAXDATASIZE + 1];//Add 1 since the last char must be a null char.
    /*
     * Used to prep the socket address structures for invention.
    struct addrinfo {
        int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
        int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
        int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
        int              ai_protocol;  // use 0 for "any"
        size_t           ai_addrlen;   // size of ai_addr in bytes
        struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
        char            *ai_canonname; // full canonical hostname

        struct addrinfo *ai_next;      // linked list, next node
    };
    */

    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;

    /*
     * Designed to be large enough to hold IPV4 and IPV6 structs.
    struct sockaddr_storage {
        sa_family_t  ss_family;     // address family

        // all this is padding, implementation specific, ignore it:
        char      __ss_pad1[_SS_PAD1SIZE];
        int64_t   __ss_align;
        char      __ss_pad2[_SS_PAD2SIZE];
    };
    */

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;

    //Reaps zombie process that appear as the fork();  If you make zombies and don't reap them,
    //your system administrator will become agitated.
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];  //Create a char array of size 46. https://stackoverflow.com/questions/39443413/why-is-inet6-addrstrlen-defined-as-46-in-c
    int rv;


    memset(&hints, 0, sizeof hints);//Variable, value, size.

    //addrinfo struct
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
                                                //addrinfo struct
    //getaddrinfo returns a pointer to a new linked list of these structures filled with info we need.
    /*
    int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP
                    const char *service,  // e.g. "http" or port number
                    const struct addrinfo *hints,
                    struct addrinfo **res);

       Translate name of a service location and/or a service name to set of
       socket addresses.

       This function is a possible cancellation point and therefore not
       marked with __THROW.
    */
    //Needs to return 0 to be successful.
    //Returns a pointer to a linked list.
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    //Grab the first socket that is available.
    for(p = servinfo; p != NULL; p = p->ai_next) {
        //Create a socket, set it to sockfd, if we can't assign to that location
        //we get -1 back.
        /*
         Create a new socket of type TYPE in domain DOMAIN, using
         protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
         Returns a file descriptor for the new socket, or -1 for errors.
         */
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        /* Set socket FD's option OPTNAME at protocol level LEVEL
        to *OPTVAL (which is OPTLEN bytes long).
        Returns 0 on success, -1 for errors.
         Fatal error, this should never enter the if statement.
         */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        //If we are here, we have found a socket to bind to.
        /* Give the socket FD (socket num?) the local address ADDR (which is LEN bytes long*/
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    /* Free `addrinfo' structure AI including associated storage.  */
    freeaddrinfo(servinfo); // all done with this structure

    //p is the serverinfo.
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    //Tell socket to listen for incoming for incoming connections.
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    /* Clear all signals from SET.  */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    /* Get and/or set the action for signal SIG.  */
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    int exitCount = 0;
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        /* Await a connection on socket FD.
        When a connection arrives, open a new socket to communicate with it,
        set *ADDR (which is *ADDR_LEN bytes long) to the address of the connecting
        peer and *ADDR_LEN to the address's actual length, and return the
        new socket's descriptor, or -1 for errors. */
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        /* Convert a Internet address in binary network format for interface
           type AF in buffer starting at CP to presentation form and place
           result in buffer of length LEN astarting at BUF. */
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        //fork - clone the calling proccess, creating an exact copy
        //return -1 for errors or 0 to the new process and the process ID
        //of the new process to the old process

        /*
        if (recv(sockfd, buf, MAXDATASIZE-1, 0) == -1)
        {
            perror("recv failed to read bytes into BUF from socket FD");
            exit(0);
        }
         */

        //fork - clone the calling proccess, creating an exact copy
        //return -1 for errors or 0 to the new process and the process ID
        //of the new process to the old process

        /*
        if (recv(sockfd, buf, MAXDATASIZE-1, 0) == -1)
        {
            perror("recv failed to read bytes into BUF from socket FD");
            exit(0);
        }
         */

        int numBytes = 0;
        if (!fork()) { // this is the child process
            close(sockfd);
            if ((numBytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1)
                perror("recv failed to read bytes into BUF from socket FD");
            close(new_fd);

            buf[numBytes] = '\0'; //Clear last character of buf
            printf("client: received '%s'\n",buf);
            exit(0);

            /*close(sockfd); // child doesn't need the listener
             if (send(new_fd, "Hello, world!", 13, 0) == -1)
                 perror("send");
             close(new_fd);
             exit(0); */
        } else
        {
            close(new_fd);
        }


//       int numBytes = 0;
//       if (!fork()) { // this is the child process
//           //close(sockfd);
//            if ((numBytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1)
//                perror("recv failed to read bytes into BUF from socket FD");
//           close(new_fd);
//           if (exitCount == 3)
//           {
//               exit(0);
//           }
//           exitCount++;
//
//           buf[numBytes] = '\0'; //Clear last character of buf
//           printf("client: received '%s'\n",buf);
//
//
//           /*close(sockfd); // child doesn't need the listener
//            if (send(new_fd, "Hello, world!", 13, 0) == -1)
//                perror("send");
//            close(new_fd);
//            exit(0); */
//        }//End of fork if.
//        else
//            close(new_fd);  // parent doesn't need this
    }
    return 0;
}