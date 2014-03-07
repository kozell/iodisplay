/*
 * http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/filesystems/proc.txt?id=HEAD#l1305
 * @author Kozell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>

struct pio_s {
	int pid;
	long r;
	long w;
	char run;
	struct pio_s *next;
};

uid_t our_uid;
uid_t target_uid;
struct pio_s *head;
char run;
int flag_same = 0;
int flag_given = 0;
int flag_all = 0;
int flag_fancy = 0;
int flag_only_pid = 0;
int flag_debug = 0;
unsigned int tsleep = 5;

struct pio_s *get_pio(int pid)
{
	struct pio_s *tmp = head;
	
	while(tmp && (tmp->pid != pid))
		tmp = tmp->next;
	return tmp;
}

void add_pio(int pid, long r, long w)
{
	struct pio_s *tmp = head, *new;
	
	new = malloc(sizeof(struct pio_s));
	new->pid = pid;
	new->r = r;
	new->w = w;
	new->run = run;
	new->next = NULL;
	
	if(tmp)
	{
		while(tmp && tmp->next && tmp->pid < pid)
			tmp = tmp->next;
		new->next = tmp->next;
		tmp->next = new;
	} else {
		head = new;
	}	
}

void del_pio(int pid)
{
	struct pio_s *tmp = head, *tmp_next;
	
	/* If first is the match. */
	if(head && head->pid == pid)
	{
		head = head->next;
		free(tmp);
		return;
	}
	
	/* Look for the one before the matching. */
	while(tmp && tmp->next && tmp->next->pid != pid)
		tmp = tmp->next;
	
	/* If found one, unlink it. */
	if(tmp)
	{
		tmp_next = tmp->next->next;
		free(tmp->next);
		tmp->next = tmp_next;
	}
}

void free_pio_list(void)
{
	struct pio_s *tmp = head;

	while (tmp)
	{
		head = head->next;
		free(tmp);
		tmp = head;
	}
	
	head = NULL;
}

uid_t getuid_by_loginname(const char *name)
{
	struct passwd *pwd;
	if(name) {
		pwd = getpwnam(name); /* don't free, see getpwnam() for details */
		if(pwd)
			return pwd->pw_uid;
	}
	
	return (uid_t)-1;
}

char is_target_owner(const char *procfs_io_name)
{
	struct stat status;
	
	if (stat(procfs_io_name, &status) == -1) {
		//fprintf(stderr, "stat error on %s\n", procfs_io_name);
		return 0;
	}	
		
	return (status.st_uid == target_uid);
}

void process_procfs_io(int pid, const char *procfs_io_name)
{
	FILE *fp;
	struct pio_s *tmp;
	char line[255];
	const char *emsg;
	long num;
	long r = 0;
	long w = 0;	
	
	tmp = get_pio(pid);

	fp = fopen(procfs_io_name, "r");
	if(NULL == fp)
	{
		switch(errno)
		{
			case EACCES: emsg = "Access not allowed, you do not have permission to read/write this file, or file/directory does not exists."; break;
			case ELOOP: emsg = "Too many symbolic links were encountered in resolving pathname."; break;
			case EMFILE: emsg = "The process already has the maximum number of files open."; break;			
			case ENAMETOOLONG: emsg = "The pathname was too long."; break;
			case ENFILE: emsg = "The system limit on the total number of open files has been reached."; break;			
			case ENOMEM: emsg = "Insufficient kernel memory was available."; break;
			case EOVERFLOW: emsg = "The pathname refers to a regular file that is too large to be opened by this executable."; break;
			default: emsg = "Unknown error occured.";
		}
		//fprintf(stderr, "Cannot stat %d, skipping. %s\n", pid, emsg);
		return; // Silently skip, we need answers, not whining.
	}
	
	while(fp && !feof(fp))
	{
		if(fscanf(fp, "%s %li", line, &num) != EOF)
		{
			if(!strcmp("read_bytes:", line))
				r = num;
			if(!strcmp("write_bytes:", line))
				w += num;
			/* I've decided not to substract cancelled writes, as it would confuse people using other tools in parallel, that only logs writes. But I'll leave it here, if you might need it. */
			/*
			if(!strcmp("cancelled_write_bytes:", line))
				w -= num;
			*/
		} else
			break;
	}
	
	fclose(fp);
	
	if(tmp)
	{
		tmp->r = r - tmp->r;
		tmp->w = w - tmp->w;
		tmp->run = run;
	} else {
		add_pio(pid, r, w);
	}
}

void walk_dir(const char *path)
{
    int pid;
	char tmp[256];
	struct dirent *entry;
    DIR *dir;
    dir = opendir (path);

    while ((entry = readdir(dir)) != NULL) {
		if (strtol(entry->d_name, NULL, 10))
		{
			pid = atoi(entry->d_name);
			sprintf(tmp, "%s/%s/io", path, entry->d_name);
			if((!flag_same && !flag_given) || ((flag_same || flag_given) && is_target_owner(tmp)))
				process_procfs_io(pid, tmp);
		}
    }
}

char *fancy(long l)
{
	char suffix[2] = ""; /* a suffix and a null */
	char *tmp = (char *)calloc((size_t)255, sizeof(char)); /* TODO realloc later */
	long val = l;
	
	if(val > 1100000)
	{
		val /= 1024576;
		suffix[0] = 'M';
	} else if(val > 1500)
	{
		val /= 1024;
		suffix[0] = 'K';
	}
	
	if(flag_fancy)
		sprintf(tmp, "%li %sB/s", val, suffix);
	else
		sprintf(tmp, "%li", l);
	
	return tmp;
}

void print_result(struct pio_s *header)
{
	char path[32], timestamp[65] = "";
	struct pio_s *tmp = header;
	time_t now  = time(NULL);    
	struct tm *t = localtime(&now);
	
	strftime(timestamp, (size_t)64, "%Y-%m-%d %H:%M:%S\t", t);
	
	if(flag_fancy)
		printf("timestamp\t\tpid\t    read\t   write\texecutable\n");	
	
	for(; tmp; tmp = tmp->next)
	{
		if(2 == tmp->run && ((tmp->r || tmp->w) || flag_all))
		{
			sprintf(path, "/proc/%d/exe", tmp->pid);
			printf("%s%d\t%*s\t%*s\t%s\n", timestamp, tmp->pid, 8, fancy(tmp->r/tsleep), 8, fancy(tmp->w/tsleep), realpath(path, NULL));
		} else if(1 == tmp->run && flag_fancy)
		{
			printf("%s%d\tdid not lived long enough.\n", timestamp, tmp->pid);
		}
	}
}

int main(int argc, char ** argv)
{
	int opt, loop = 1;
	
	our_uid = geteuid();
	
	while ((opt = getopt(argc, argv, "afh?st:u:d")) != -1)
	{
		switch (opt)
		{
			case 'a':
				flag_all = 1;
				break;
			case 'f':
				flag_fancy = 1;
				break;
			case '?':
			case 'h':
				fprintf(stderr, "Usage: sudo %s [ -h | -? ] ^ [[ -a ] [ -f ] [-t {sleep seconds}<5> ] [ [ -s ]^[-u {loginname} ] ] ]  [ {loops}<1> [ {sleep seconds, overrides -t}<5> ] ]\n", argv[0]);
				exit(EXIT_SUCCESS);
				break;
			case 's':
				flag_same = 1;
				target_uid = our_uid;
				break;
			case 'u':
				flag_given = 1;
				target_uid = getuid_by_loginname(optarg);
				if((uid_t)-1 == target_uid)
				{
					fprintf(stderr, "Unknown user: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 't':
				tsleep = atoi(optarg);
				break;
			case 'd':
				flag_debug = 1;
				break;
		}
	}
	
	if (optind < argc)
	{
		loop = atoi(argv[optind++]);
		loop = loop > 0 ? loop : 1;
		if (optind < argc)
		{
			opt = atoi(argv[optind++]);
			tsleep = opt > 0 ? (unsigned)opt : tsleep;
		}
	}

	if(!flag_debug && our_uid != 0 && !flag_same)
	{
		fprintf(stderr, "You are not root, but ID %u (%s)! Use sudo!\n", our_uid, getlogin());
		return -1;
	}

	do {
		run = 1;
		walk_dir("/proc");
		sleep(tsleep);
		run = 2;
		walk_dir("/proc");
		/* Print results */
		print_result(head);
		/* Clear storage */
		free_pio_list();
	} while(--loop);
	
	return 0;
}
