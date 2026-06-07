#define _GNU_SOURCE
#include "embfeatures.h"

#ifdef SUPPORT_64BIT_FILESIZE
#define _FILE_OFFSET_BITS 64
#endif

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <utime.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <ftw.h>
#include <stdio.h>
#include <alloca.h>
#include <stdint.h>

#ifdef __linux__
#include <sys/sendfile.h>
#include <sys/sysmacros.h>
#else
#define NEED_SENDFILE
#endif

#ifdef __sun__
#include <sys/mkdev.h>
#endif

#include "write12.h"

#ifdef __MINGW32__
#include <windows.h>
#undef TAR_SUPPORT_SYMBOLIC_UID
#define mkdir(a,b) mkdir(a)
#define readlink(a,b,c) -1
#define chown(a,b,c)
#define geteuid() 1
#define mknod(a,b,c) -1
#define link(a,b) -1
#define symlink(a,b) -1
#define lstat(a,b) stat(a,b)
#else
#include <sys/mman.h>
#define UIDCACHE
#endif

#ifdef TAR_SUPPORT_SYMBOLIC_UID
#include <pwd.h>
#include <grp.h>
#ifndef UIDCACHE
#define DONT_NEED_PASSWD
#define DONT_NEED_GECOS
#define DONT_NEED_DIR
#define DONT_NEED_SHELL
#include "mygetpwnam.c"
#define DONT_NEED_MEMBERS
#include "mygetgrnam.c"
#endif
#endif

#ifdef PATH_MAX
#define MAXNAME PATH_MAX
#else
#define MAXNAME 4096
#endif

char filename[MAXNAME+1];
char linkname[MAXNAME+1];

#ifdef UIDCACHE
struct uid {
    char* name;
    id_t id;
    struct uid* next;
} *uids, *gids;

static char* mmap_read(const char* filename, unsigned long* filesize) {
    int fd = open(filename, O_RDONLY);
    char *map;
    if (fd >= 0) {
        *filesize = lseek(fd, 0, SEEK_END);
        if ((map = (char*)mmap(0, *filesize, PROT_READ, MAP_SHARED, fd, 0)) == (char*)-1) map = 0;
        close(fd);
        return map;
    }
    return 0;
}

static void populate(const char* filename, struct uid** root, unsigned int fields) {
    unsigned long len;
    char* map;
    if ((map = mmap_read(filename, &len))) {
        size_t tmp = 0;
        while (tmp < len) {
            size_t nl, colon;
            struct uid u;
            size_t o[8], on;
            for (nl = tmp; nl < len && map[nl] != '\n'; ++nl);
            on = 0;
            for (colon = tmp; colon < nl; ++colon)
                if (map[colon] == ':' && on < 8) o[on++] = colon;
            if (on == fields) {
                if ((u.name = (char*)malloc(o[0] - tmp + 1))) {
                    memcpy(u.name, map + tmp, o[0] - tmp);
                    u.name[o[0] - tmp] = 0;
                    u.id = 0;
                    for (colon = o[1] + 1; colon < o[2]; ++colon) {
                        unsigned char digit = map[colon] - '0';
                        if (digit > '9') { free(u.name); u.name = 0; break; }
                        u.id = u.id * 10 + digit;
                    }
                }
                if (u.name) {
                    struct uid* x = (struct uid*)malloc(sizeof(struct uid));
                    if (x) { memcpy(x, &u, sizeof(struct uid)); x->next = *root; *root = x; }
                }
            }
            tmp = nl + 1;
        }
        munmap(map, len);
    }
}

static void initcache(void) {
    populate("/etc/passwd", &uids, 6);
    populate("/etc/group", &gids, 3);
}

static struct uid* name2uid(struct uid* list, const char* name) {
    while (list) { if (!strcmp(list->name, name)) return list; list = list->next; }
    return 0;
}

static struct uid* uid2name(struct uid* list, id_t uid) {
    while (list) { if (list->id == uid) return list; list = list->next; }
    return 0;
}
#endif

struct previous {
    char* name;
    dev_t device;
    ino_t inode;
    struct previous* next;
};

#include "fmt_str.c"
#include "fmt_ulong.c"
#include "fmt_8long.c"
#include "buffer.c"

int tarfd;
unsigned long long written;
char buffer_2_space[BUFFER_INSIZE];
static buffer it2 = BUFFER_INIT((void*)write, 2, buffer_2_space, sizeof buffer_2_space);
buffer *buffer_2 = &it2;

static int fmt_digit(char* dest, unsigned long l, unsigned int digits) {
    int i = digits;
    while (digits) { dest[--digits] = (l % 10) + '0'; l /= 10; }
    return i;
}

static int fmt_pad(char *dest, char *src, char pad, int padto, int len) {
    int i;
    char *d = dest;
    while (len < padto) { *d++ = pad; padto--; }
    for (i = 0; i < len; ++i) *d++ = src[i];
    return (int)(d - dest);
}

static void die(const char *message) { __write2(message); close(0); exit(1); }
static void couldnot(const char *what, const char *name) {
    __write2("Could not "); __write2(what); __write2(" \"");
    __write2(name); __write2("\": "); __write2(strerror(errno)); __write2("\n");
}

struct posix_header {
    char name[100], mode[8], uid[8], gid[8], size[12], mtime[12], chksum[8];
    char typeflag;
    char linkname[100], magic[6], version[2], uname[32], gname[32], devmajor[8], devminor[8], prefix[155], filler[12];
};

void __m2s(int i, char *s) { s[0] = (i & 4) ? 'r' : '-'; s[1] = (i & 2) ? 'w' : '-'; s[2] = (i & 1) ? 'x' : '-'; }

void mode2str(mode_t m, char *s) {
    m &= ~S_IFREG;
    if ((m & ~04777) == 0) s[0] = '-';
    else if (S_ISCHR(m)) s[0] = 'c';
    else if (S_ISDIR(m)) s[0] = 'd';
    else if (S_ISBLK(m)) s[0] = 'b';
    else if (S_ISFIFO(m)) s[0] = 'p';
    else if (S_ISSOCK(m)) s[0] = 's';
    else if (S_ISLNK(m)) s[0] = 'l';
    else s[0] = '?';
    __m2s(m >> 6, s + 1); __m2s(m >> 3, s + 4); __m2s(m, s + 7);
    if (m & S_ISUID) s[1] = 's'; if (m & S_ISGID) s[4] = 's';
}

void time2str(time_t t, char *s) {
    struct tm *T = localtime(&t);
    s += fmt_digit(s, T->tm_year + 1900, 4); *s++ = '-';
    s += fmt_digit(s, T->tm_mon + 1, 2); *s++ = '-';
    s += fmt_digit(s, T->tm_mday, 2); *s++ = ' ';
    s += fmt_digit(s, T->tm_hour, 2); *s++ = ':';
    s += fmt_digit(s, T->tm_min, 2); *s++ = ':';
    s += fmt_digit(s, T->tm_sec, 2); *s = 0;
}

int piperead(int fd, void *buf, int len) {
    int x = 0;
    char *ptr = (char *)buf;
    while (len > 0) {
        int tmp = read(fd, ptr, len);
        if (tmp < 0) return -1;
        if (tmp == 0) return x;
        x += tmp; len -= tmp; ptr += tmp;
    }
    return x;
}

void domkdir(char *path) {
    char *tmp = path;
    while (*tmp) {
        char old;
        for (; *tmp && *tmp != '/'; ++tmp);
        if ((old = *tmp)) {
            if (tmp[1] == 0) break;
            *tmp = 0; mkdir(path, 0755); *tmp = old; ++tmp;
        } else break;
    }
}

buffer* db = 0;
int verbose;

static void display(const struct stat* ss, const struct posix_header* ph, const char* filename) {
    if (ph->typeflag == 'L' || ph->typeflag == 'K') return;
    if (!db) db = (tarfd == 1) ? buffer_2 : buffer_1;
    if (verbose >= 1) {
        if (verbose >= 2) {
            char tmp[30];
            mode2str(ss->st_mode, tmp);
            buffer_put(db, tmp, 10);
            buffer_putnlflush(db);
        } else {
            buffer_puts(db, filename);
            buffer_putnlflush(db);
        }
    }
}

static void calcchecksum(struct posix_header* ph) {
    char buf[10]; unsigned int sum = 0, i;
    memset(ph->chksum, ' ', sizeof(ph->chksum));
    for (i = 0; i < sizeof(*ph); ++i) sum += ((unsigned char*)ph)[i];
    ph->chksum[fmt_pad(ph->chksum, buf, '0', 6, fmt_8long(buf, sum))] = 0;
}

/* el-cheapo tar that can only create and extract archives */
int main(int argc,char *argv[]) {
  int myuid=geteuid();
  int compress=0;
  int unlink_first=0;
  const char *file=0;
  char **filez;
  int filefd;
  char buf[10240];
  int len;
  char* destdir=0;
  char* compressprg="gzip";
  enum {
    NOOP,
    CREATE,
    EXTRACT,
    TEST
  } todo=NOOP;
#ifdef __MINGW32__
  _fmode=O_BINARY;
#endif
  if (argc<2)
    die("usage: tar czvf file.tar.gz filename...\n"
	"       tar xzvUf file.tar.gz\n");
  switch (*argv[1]) {
  case 'c': todo=CREATE; break;
  case 't': todo=TEST; break;
  case 'x': todo=EXTRACT; break;
  default:
	    die("unknown command, expected [ctx]\n");
  }
  {
    char *tmp=argv[1];
    while ((tmp=strchr(tmp,'v'))) {
      ++verbose;
      ++tmp;
    }
  }
  if (strchr(argv[1],'z')) compress=1;
  if (strchr(argv[1],'I')) compress=2;
  if (strchr(argv[1],'j')) compress=2;
  if (strchr(argv[1],'U')) unlink_first=1;
  filez=argv+2;
  if (strchr(argv[1],'f')) file=*filez++;

#ifdef UIDCACHE
  initcache();
#endif

  {
    int i;
    for (i=1; i<argc; ++i) {
      if (!strcmp(argv[i],"-C"))
	destdir=argv[i+1];
      else if (!strcmp(argv[i],"--use-compress-program")) {
	compress=1; compressprg=argv[i+1];
      } else if (!strncmp(argv[i],"--use-compress-program=",23)) {
	compress=1; compressprg=argv[i]+23;
      }
    }
  }

  switch (todo) {
  case CREATE:
    if (strcmp(file,"-")) {
      tarfd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0600);
    } else tarfd=1;

    if (destdir) {
      if (chdir(destdir)==-1)
	die("chdir");
    }

    if (compress) {
#ifdef __MINGW32__
      fprintf(stderr,"compression not supported on win32!\n");
#else
      char *argv[]={compressprg,"-c",0};
      int fds[2];
      if (compress==2) argv[0]="bzip2";
      if (pipe(fds)) die("pipe failed\n");
      switch (fork()) {
      case -1:
	die("could not fork to exec gzip/bzip2");
      case 0:	/* child */
	if (dup2(fds[0],0)<0 || dup2(tarfd,1)<0) die("dup2 failed\n");
	close(fds[0]); close(tarfd); close(fds[1]);
	execvp(argv[0],argv);
	die("could not execvp gzip/bzip2");
      default:
	if (dup2(fds[1],tarfd)<0) die("dup2 failed\n");
	close(fds[1]);
	close(fds[0]);
      }
#endif
    }

    while (*filez) {
      struct stat ss;
      if (!strcmp(*filez,"-C")) {
	filez+=2;
	continue;
      } if (!strncmp(*filez,"--use-compress-program",22)) {
	if ((*filez)[23]!='=') ++filez;
	++filez;
	continue;
      }
      if (!lstat(*filez,&ss)) {
	prefixlen=0;
	if (S_ISDIR(ss.st_mode)) {
	  if (*filez[0]!='/') {
	    prefix=(char*)alloca(MAXNAME);
	    if (getcwd(prefix,MAXNAME-1))
	      prefixlen=strlen(prefix);
	  }
	  ftw(*filez,tarcreate,100);
	} else {
	  prefix=(char*)alloca(MAXNAME);
	  if (getcwd(prefix,MAXNAME-1))
	    prefixlen=strlen(prefix);
	  tarcreate(*filez,&ss,0);
	}
      }
      ++filez;
    }
    {
      int todo=10*1024-(written%(10*1024));
      char buf[10*1024];
      memset(buf,0,todo);
      write(tarfd,buf,todo);
    }
    close(tarfd);
    buffer_flush(buffer_1);
    break;
  case TEST:
    ++verbose;
  case EXTRACT:
    if (file && file[0]=='-' && !file[1]) file=0;
    if (!file) tarfd=0; else { close(0); tarfd=open(file,O_RDONLY); }
    if (tarfd<0) die("could not open tar file\n");
    if (tarfd!=0)
      if (dup2(tarfd,0)<0) die("dup2 failed\n");
    tarfd=0;

    if (destdir) {
      if (chdir(destdir)==-1)
	die("chdir");
    }

    if (compress) {
#ifdef __MINGW32__
      fprintf(stderr,"compression not supported on win32!\n");
#else
      char *argv[]={compressprg,"-dc",0};
      int fds[2];
      if (compress==2) argv[0]="bzip2";
      if (pipe(fds)) die("pipe failed\n");
      switch (fork()) {
      case -1:
	die("could not fork to exec gzip/bzip2");
      case 0:	/* child */
	if (dup2(fds[1],1)<0) die("dup2 failed\n");
	close(fds[1]); close(fds[0]);
	execvp(argv[0],argv);
	die("could not execvp gzip/bzip2");
      default:
	if (dup2(fds[0],0)<0) die("dup2 failed\n");
	close(fds[0]); close(fds[1]);
      }
#endif
    }

    umask(0);
    do {
#if 0
      {
	char tmp[100];
	strcpy(tmp,"ofs ");
	tmp[4+__ltostr(tmp+4,90,lseek(0,0,SEEK_CUR),10,0)]=0;
	write(2,tmp,strlen(tmp)); write(2,"\n",1);
      }
#endif
      len=piperead(0,buf,512);
      if (len<512) {
	if (len==0) break;
	die("unexpected end of tar file\n");
      }
      if (!buf[0]) {
	/* normally, we should break here.  However, the mono people
	 * create broken tar files with a useless 512 byte header with a
	 * null file name in the middle.  I have no idea how they do it. */
	int i,allnull;
	allnull=1;
	for (i=0; i<512; ++i)
	  if (buf[i]) {
	    allnull=0;
	    break;
	  }
	if (allnull) break;
      }
      {
	struct posix_header *ph=(struct posix_header*)buf;
	struct stat ss;
	if (memcmp(ph->magic,"ustar",5)) {
	  static int warned=0;
	  if (!warned) {
	    __write2("tar: warning: pre-POSIX tar file!\n");
	    warned=1;
	  }
	}
	{
	  int i;
	  for (i=0; i<7; ++i)
	    if (ph->mode[i] !=' ' && (ph->mode[i]<'0' || ph->mode[i]>'7'))
	      die("tar: this does not look like a tar file to me\n");
	}
	ph->uname[31]=0; ph->gname[31]=0;
	memset(&ss,0,sizeof(ss));
	ss.st_size=strtoull(ph->size,0,8);
	ss.st_mode=strtoul(ph->mode,0,8);
	ss.st_dev=0;
	ss.st_uid=strtoul(ph->uid,0,8);
	ss.st_gid=strtoul(ph->gid,0,8);
#ifdef TAR_SUPPORT_SYMBOLIC_UID
#ifdef UIDCACHE
	{
	  struct uid* t;
	  if ((t=name2uid(uids,ph->uname)))
	    ss.st_uid=t->id;
	  if ((t=name2uid(gids,ph->gname)))
	    ss.st_gid=t->id;
	}
#else
	{
	  struct passwd *pwd;
	  struct group *grp;
	  if ((pwd=getpwnam(ph->uname)))
	    ss.st_uid=pwd->pw_uid;
	  if ((grp=getgrnam(ph->gname)))
	    ss.st_gid=grp->gr_gid;
	}
#endif
#endif
	{
	  int maj,min;
	  maj=strtoul(ph->devmajor,0,8);
	  min=strtoul(ph->devminor,0,8);
	  ss.st_dev=makedev(maj,min);
	}
	ss.st_mtime=strtoul(ph->mtime,0,8);

	switch (ph->typeflag) {
	case '5': ss.st_mode|=S_IFDIR; break;
	case '2': ss.st_mode|=S_IFLNK; break;
	case '6': ss.st_mode|=S_IFSOCK; break;
	}
	if (ph->typeflag>='1' && ph->typeflag<='6') {
	  ss.st_size=0;
	  if (ph->typeflag=='3') ss.st_mode|=S_IFCHR; else
	  if (ph->typeflag=='4') ss.st_mode|=S_IFBLK;
	}
/*	printf("typeflag %c\n",ph->typeflag); */

	if (ph->typeflag!='L' && ph->typeflag!='M' && ph->typeflag!='K') {
	  if (!filename[0]) {
	    int len=0;
	    if (ph->prefix[0]) {
	      memcpy(filename,ph->prefix,sizeof(ph->prefix));
	      filename[sizeof(ph->prefix)]=0;
	      len=strlen(filename);
	      if (filename[len-1]!='/') {
		filename[len]='/';
		++len;
	      }
	    }
	    memcpy(filename+len,ph->name,100);
	    filename[len+100]=0;
	  }
	  if (!linkname[0]) {
	    memcpy(linkname,ph->linkname,100);
	    linkname[100]=0;
	  }
	}

	{
	  char* x=strrchr(filename,'/');
	  if (x && x[1]==0 && ss.st_size==0 && ph->typeflag==0)
	    ss.st_mode |= S_IFDIR;
	}

	display(&ss,ph,filename);
	if ((ph->typeflag=='L' || ph->typeflag=='K') && (ss.st_size==0 || ss.st_size>4096))
	  die("invalid GNU extension tar header\n");
	if (ph->typeflag=='L') {
	  /* long file name */
	  long realsize=(ss.st_size+511)&(~511);
	  if (realsize >= sizeof(filename)) die("file name too long");
	  if (piperead(0,filename,realsize)!=realsize) die("short read in long file name");
	  filename[ss.st_size]=0;
	  ss.st_size=0;
/*	  printf("got long file name: \"%s\"\n",filename); */
	} else if (ph->typeflag=='K') {
	  /* long link target */
	  long realsize=(ss.st_size+511)&(~511);
	  if (realsize >= sizeof(linkname)) die("link target too long");
	  if (piperead(0,linkname,realsize)!=realsize) die("short read in link target");
	  linkname[ss.st_size]=0;
	  ss.st_size=0;
/*	  printf("got long link target: \"%s\"\n",linkname); */
	}

	if (todo==EXTRACT) {
	  {
	    char* c=strrchr(filename,'/');
	    if (c && !c[1]) *c=0;
	  }
	  filefd=-1;
	  if (unlink_first)
	    if (unlink(filename))
	      couldnot("delete file",filename);
	  domkdir(filename);
	  if (ph->typeflag=='1') {
	    if (link(linkname,filename))
	      couldnot("create link",filename);
	  } else if (S_ISLNK(ss.st_mode)) {
	    if (symlink(linkname,filename))
	      couldnot("create symlink",filename);
	  } else if (S_ISCHR(ss.st_mode) || S_ISBLK(ss.st_mode) || S_ISFIFO(ss.st_mode)) {
	    if (mknod(filename,ss.st_mode,ss.st_dev))
	      couldnot("mknod",filename);
	    if (myuid==0)
	      chown(filename,ss.st_uid,ss.st_gid);
	  } else if (S_ISDIR(ss.st_mode)) {
	    if (mkdir(filename,ss.st_mode))
	      couldnot("create directory",filename);
	  } else if (ph->typeflag=='L' || ph->typeflag=='K') {
	    /* skip */
	  } else {
	    filefd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,ss.st_mode);
	    if (filefd<0)
	      couldnot("create file",filename);
	  }
	} else filefd=-1;
	if (ss.st_size>0) {
	  long long realsize=(ss.st_size+511)&(~511);
	  long long towrite=ss.st_size;
	  while (realsize>0) {
	    long long foo=(realsize>1024?1024:realsize);
	    char tmp[1024];
	    int len=piperead(0,tmp,foo);
	    int need=len>towrite?towrite:len;
	    if (len<foo) die("short read reading file data");
	    if (filefd>=0) {
	      if (write(filefd,tmp,need)<need)
		die("short write\n");
	      else
		towrite-=need;
	    }
	    realsize-=foo;
	  }
	  if (filefd>=0)
	    close(filefd);
	}
	if (filefd>=0 && !S_ISLNK(ss.st_mode)) {
	  struct utimbuf ut;
	  ut.actime=ut.modtime=ss.st_mtime;
	  if (utime(filename,&ut))
	    couldnot("set utime of ",ph->name);
	}
	if (ph->typeflag!='L' && ph->typeflag!='M' && ph->typeflag!='K')
	  filename[0]=linkname[0]=0;
      }
    } while (1);
  }
  return 0;
}