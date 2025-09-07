#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    
    /*Opening logger*/
    openlog("Assignment2_Logs",LOG_PID | LOG_CONS, LOG_USER);

    /*Validate the Argument Count*/

    if (argc <=2) {

        syslog(LOG_ERR, "Two mandatory parameters were not specified");
        return 1;

    }

    /*Assign the Command Line Arguments*/
    const char* writefile = argv[1];
    const char* writestr = argv[2];

    /*Validate the Arguments*/    
    if (strlen(writestr) == 0) {
        syslog(LOG_ERR, "Write String cannot be empty");
        return 1;
    }

    /*File Opening Block*/
    int fd;
    fd = creat(writefile, 0644);
    if (fd == -1){
        syslog(LOG_ERR, "Error while Opening file with creat: %m");
        return 1;
    }
    
    /*File Writing Block*/
    ssize_t nw;
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    nw = write(fd, writestr, strlen (writestr));
    if (nw == -1){
        syslog(LOG_ERR, "Error while Writing file with write: %m");
        return 1;
    }

    
    closelog();
    return 0;
}