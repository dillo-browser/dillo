#include <stdio.h>
#include <stdlib.h>  /* for exit */
#include <string.h>  /* for bzero */
#include <unistd.h>  /* for read and write */
#include <ctype.h>   /* for isxdigit */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include "../dlib/dlib.h"
#include "../dpip/dpip.h"

#define MSG_ERR(...) printf("** ERROR **: " __VA_ARGS__);

char *CMD_REGISTER = "<cmd='register_all' '>";
char *CMD_STOP     = "<cmd='DpiBye' '>";

static char SharedKey[32];

static void print_usage(const char *prgname)
{
   fprintf(stderr,"Control program for the Dillo plugin daemon\n"
                  "Usage: %s {stop|register|chat}\n\n", prgname);
}

static void error(char *msg)
{
   perror(msg);
   exit(1);
}

/*
 * Read dpid's communication keys from its saved file.
 * Return value: 1 on success, -1 on error.
 */
static int Dpi_read_comm_keys(int *port)
{
   FILE *In;
   char *fname, *rcline = NULL, *tail;
   int i, ret = -1;

   fname = dStrconcat(dGethomedir(), "/.dillo/dpid_comm_keys", NULL);
   if ((In = fopen(fname, "r")) == NULL) {
      MSG_ERR("[Dpi_read_comm_keys] %s\n", dStrerror(errno));
   } else if ((rcline = dGetline(In)) == NULL) {
      MSG_ERR("[Dpi_read_comm_keys] empty file: %s\n", fname);
   } else {
      *port = strtol(rcline, &tail, 10);
      for (i = 0; *tail && isxdigit(tail[i+1]); ++i)
         SharedKey[i] = tail[i+1];
      SharedKey[i] = 0;
      ret = 1;
   }
   dFree(rcline);
   dFree(fname);

   return ret;
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    char buffer[256];

    if (argc != 2) {
       print_usage(argv[0]);
       exit(1);
    }

    /* Read dpid's port number from saved file */
    if (Dpi_read_comm_keys(&portno) == -1) {
       MSG_ERR("main: Can't read dpid's port number\n");
       exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    snprintf(buffer, sizeof(buffer), "<cmd='auth' msg='%s' '>", SharedKey);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
         error("ERROR writing to socket");

    if (strcmp(argv[1], "stop") == 0) {
       strcpy(buffer, CMD_STOP);
    } else if (strcmp(argv[1], "register") == 0) {
       strcpy(buffer, CMD_REGISTER);
    } else if (strcmp(argv[1], "chat") == 0) {
       printf("Please enter the message: ");
       bzero(buffer,256);
       if (fgets(buffer,255,stdin) == NULL)
          MSG_ERR("dpidc: Can't read the message\n");
    } else {
       MSG_ERR("main: Unknown operation '%s'\n", argv[1]);
       print_usage(argv[0]);
       exit(1);
    }

    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0)
         error("ERROR writing to socket");
/*
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0)
         error("ERROR reading from socket");
    printf("%s\n",buffer);
*/
    dClose(sockfd);
    return 0;
}
