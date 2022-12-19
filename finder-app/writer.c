#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	FILE *f;
	
	openlog(NULL, 0, LOG_USER);
	
	
	if(argc != 3)
	{
		syslog(LOG_ERR, "WRONG PARAM COUNT ! PARAM COUNT : %d", argc - 1);
		closelog();
		return 1;
	}
	
	syslog(LOG_DEBUG, "FILE PATH : %s\tTEXT : %s", argv[1], argv[2]);
	
	f = fopen(argv[1], "w");
	
	if(f == NULL)
	{
		syslog(LOG_ERR, "THE FILE DOESN'T EXIST");
		closelog();
		return 1;
	}
	
	if(fprintf(f, argv[2]) < 0)
	{
		syslog(LOG_ERR, "WRITE OPERTION ERROR !");
		closelog();
		return 1;
	}
	
	fclose(f);
	
	syslog(LOG_DEBUG, "FILE CLOSED !");
	closelog();
	return 0;
}
