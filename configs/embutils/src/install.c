#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <utime.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include "write12.h"

#ifdef __linux__
#include <sys/sendfile.h>
#endif

/* Global Configuration */
int d = 0, D = 0, g = -1, o = -1, s = 0, v = 0, p = 0, res = 0, b = 0;
mode_t mode = 0755;

/* Prototypes */
void usage(void);
void warn(const char* filename, const char* message);
void errormsg(const char* filename);
int doinstall(char *src, const char *dest, int mustbedir);
void minusp(char *s, int m);
void domkdir(char *s, int m, int iexist, char *full);

static void myname(void) { __write2("install: "); }

void warn(const char* filename, const char* message) {
    myname();
    if (filename) { __write2(filename); __write2(": "); }
    __write2(message); __write2("\n");
}

void errormsg(const char* filename) {
    const char *msg = "unknown error";
    switch (errno) {
        case EPERM: msg = "permission denied"; break;
        case ENOENT: msg = "file not found"; break;
        case EACCES: msg = "permission denied"; break;
        case EROFS: msg = "read-only file system"; break;
        case EISDIR: msg = "is a directory"; break;
        case EEXIST: msg = "file exists"; break;
    }
    warn(filename, msg);
    res = 1;
}

/* Replaced alloca with heap allocation to avoid stack exhaustion */
void oops2(const char* syscall, const char* filename) {
    char *buf = malloc(strlen(syscall) + strlen(filename) + 5);
    if (buf) {
        sprintf(buf, "%s: %s", syscall, filename);
        errormsg(buf);
        free(buf);
    }
}

void usage(void) {
    __write2("usage: install [options] source dest\n"
             "       install [options] source... directory\n"
             "       install [options] -d directory...\n");
    exit(1);
}

uid_t parseuid(const char* c) {
    if (isdigit((unsigned char)*c)) return (uid_t)atoi(c);
    struct passwd* p = getpwnam(c);
    return p ? p->pw_uid : 0;
}

gid_t parsegid(const char* c) {
    if (isdigit((unsigned char)*c)) return (gid_t)atoi(c);
    struct group* gr = getgrnam(c);
    return gr ? gr->gr_gid : 0;
}

/* Refactored copymove to ensure clean file descriptor closure */
int copymove(const char *src, const char *dest) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) { errormsg(src); return -1; }
    
    struct stat ss;
    fstat(sfd, &ss);
    
    int dfd = open(dest, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0600);
    if (dfd < 0) { close(sfd); errormsg(dest); return -1; }

    char buf[16384];
    ssize_t len;
    while ((len = read(sfd, buf, sizeof(buf))) > 0) {
        if (write(dfd, buf, len) != len) { 
            warn(dest, "short write"); 
            close(sfd); close(dfd); return -1; 
        }
    }
    
    close(sfd);
    close(dfd);

    if (s) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("strip", "strip", dest, (char *)NULL);
            exit(1);
        } else if (pid > 0) {
            wait(NULL);
        }
    }
    
    if (o >= 0 || g >= 0) chown(dest, (uid_t)o, (gid_t)g);
    chmod(dest, mode);
    return 0;
}

/* mustbedir=0 means "may be dir"
 * mustbedir=1 means "must be dir"
 * mustbedir=-1 means "must not be dir" */
int doinstall(char *src,const char *dest,int mustbedir) {
  char *d=(char*)dest;
  int idx=str_rchr(src,'/');
  const char *F;
  struct stat ss,ss2;
  if (src[idx]=='/' && src[idx+1]==0) {
    src[idx]=0;
    while (idx>0 && src[idx]!='/') --idx;
  }
  F=src[idx]?src+idx+1:src;
  if (stat(dest,&ss)) {
    if (mustbedir>0) {
      oops2(dest,"last argument must be a directory");
      return 0;
    }
  } else {
    int ok=0;
    if (S_ISDIR(ss.st_mode)) {
      if (mustbedir<0) {
	int len;
	d=alloca(strlen(dest)+100);
	myname();
	len=str_copy(d+len,"cannot overwrite directory '");
	len+=str_copy(d+len,dest);
	str_copy(d+len,"'!\n");
	__write2(d);
	return 0;
      } else {
	char *e;
	d=alloca(strlen(dest)+strlen(F)+2);
	e=d+str_copy(d,dest);
	*e='/'; ++e;
	e+=str_copy(e,F);
	*e=0;
/*      printf("%slink %s to %s\n",s?"sym":"",src,d); */
	return doinstall(src,d,-1);
      }
    }
    if (!stat(src,&ss2)) {
      if (ss2.st_dev==ss.st_dev &&
	  ss2.st_ino==ss.st_ino &&
	  ss2.st_size==ss.st_size) {
	char *tmp=alloca(strlen(src)+strlen(dest)+50);
	int len;
	myname();
	len=str_copy(tmp+len,src);
	len+=str_copy(tmp+len," and ");
	len+=str_copy(tmp+len,dest);
	len+=str_copy(tmp+len," are the same file.\n");
	__write2(tmp);
	return 0;
      } else {
	if (b) {
	  size_t l=strlen(dest);
	  char* bakfilename=alloca(l+2);
	  memcpy(bakfilename,dest,l);
	  bakfilename[l]='~';
	  bakfilename[l+1]=0;
	  if (rename(dest,bakfilename)==-1) {
	    if (errno!=ENOENT) {
	      char* x=alloca(l+l+15);
	      strcpy(x,"backup of ");
	      strcat(x,dest);
	      errormsg(x);
	      return -1;
	    }
	  }
	}
	unlink(dest);
      }
    }
  }
  if (v) {
    char *tmp=alloca(strlen(src)+strlen(dest)+50);
    int len;
    len=str_copy(tmp,"\"");
    len+=str_copy(tmp+len,src);
    len+=str_copy(tmp+len,"\" -> \"");
    len+=str_copy(tmp+len,dest);
    len=str_copy(tmp+len,"\"\n");
    __write2(tmp);
  }
  return copymove(src,dest);
}


int main(int argc,char *argv[]) {
  int k,first,num,last;
  mode_t and,or;

  if (argc<2) usage();
  first=num=last=0;
  for (k=1; k<argc; ++k) {
    if (argv[k][0]=='-') {
      unsigned int j;
      for (j=1; j<strlen(argv[k]); j++) {
	switch (argv[k][j]) {
	case 'd': d=1;
	case 'D': D=1;
	case 'c': break;
	case 's': s=1; break;
	case 'v': v=1; break;
	case 'p': p=1; break;
	case 'b': b=1; break; /* backup */
	case 'm':
		  if (argv[k][j+1]) {
		    parsemode(argv[k]+j+1,&and,&or);
		    mode=(mode&and)|or;
		    goto fini;
		  } else {
minusm:
		    if (!argv[k+1]) usage();
		    parsemode(argv[k+1],&and,&or);
		    mode=(mode&and)|or;
parsed:		    argv[++k]=0;
fini:		    argv[k]=0;
		    goto done;
		  }
		  break;
	case 'o':
		  if (argv[k][j+1]) {
		    o=parseuid(argv[k]+j+1);
		    goto fini;
		  } else {
minuso:
		    if (!argv[k+1]) usage();
		    o=parseuid(argv[k+1]);
		    goto parsed;
		  }
		  break;
	case 'g':
		  if (argv[k][j+1]) {
		    g=parseuid(argv[k]+j+1);
		    goto fini;
		  } else {
minusg:
		    if (!argv[k+1]) usage();
		    g=parseuid(argv[k+1]);
		    goto parsed;
		  }
		  break;
	case '-': /* double dash.  May those GNU idiots rot in hell */
		  {
		    char *x=argv[k]+2;
		    j=strlen(argv[k]);
		    if (!strcmp(x,"strip")) {
		      s=1; break;
		    } else if (!strcmp(x,"mode"))
		      goto minusm;
		    else if (!strcmp(x,"owner"))
		      goto minuso;
		    else if (!strcmp(x,"group"))
		      goto minusg;
		    else if (!strcmp(x,"directory")) {
		      d=D=1; break;
		    }
		  }
	default: usage();
	}
      }
      argv[k]=0;
    } else {
      if (first==0) first=k;
      ++num;
      last=k;
    }
done: ;
  }
  if (d) {
    /* directory mode. */
    int i;
    for (i=1; i<argc; ++i)
      if (argv[i]) {
	minusp(argv[i],0755);
	domkdir(argv[i],mode,1,argv[i]);
      }
    return 0;
  }
  if (num<2) usage();
  switch (num) {
  case 1:
    doinstall(argv[first],".",0);
    break;
  case 2:
    if (D) minusp(argv[last],0755);
    doinstall(argv[first],argv[last],0);
    break;
  default:
    if (D) minusp(argv[last],0755);
    while (num>1) {
      if (argv[first] && argv[first][0]!='-') {
	doinstall(argv[first],argv[last],1);
	--num;
      }
      ++first;
    }
  }
  return res;
}
