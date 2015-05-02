#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "buf.h"
#include "fs.h"
#include "file.h"

#define PROCFS_DEV 9
#define PROCDIR_OFF 1000
#define IMEMINFO 1
#define ICPUINFO 2

extern
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


extern
struct cpu cpus[NCPU];

extern 
struct inode* iget(uint dev, uint inum, struct inode* parent);

struct inode_functions procfs_i_func = { procfs_ipopulate, procfs_iupdate, procfs_readi, procfs_writei };

void init_procfs(){
  struct inode *ip = namei("/proc");
  if(ip != 0)
	  ip->i_func = &procfs_i_func; 
}  

void procfs_iupdate(struct inode *ip)
{
	
}

void
procfs_ipopulate(struct inode* ip){
	ip->major = 0;
	ip->minor = 0;
	ip->nlink = 1;
	ip->flags |= I_VALID;
	ip->mounted = PROCFS_DEV;
	ip->size = 0;

	//Only real directory /proc
	if(ip->dev != PROCFS_DEV){
		ip->type = T_DIR;

		iget(PROCFS_DEV, IMEMINFO, ip);
		iget(PROCFS_DEV, ICPUINFO, ip);

		ip->size = 2*sizeof(struct dirent);

		struct proc *p;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->state != UNUSED){
				iget(PROCFS_DEV,p->pid+PROCDIR_OFF,ip);
				ip->size += sizeof(struct dirent);
			}
		}
	
		return;
	}

	//Virtual Proccess Directory	
	if(ip->inum > PROCDIR_OFF && ip->inum < (2*PROCDIR_OFF)){
		ip->type = T_DIR;
		ip->size = 2*sizeof(struct dirent);

		return;
	}

	//Virtual File
	ip->type = T_FILE;
}



void toString(int x, char *buf, int max){
	if(x < 0 && max < 1) return;

	int i = 0;
	do{
		buf[i++] = '0' + (x % 10);
	} while((x /= 10) != 0 && i < max);

	buf[i] = '\0';
}
int
procfs_readi(struct inode *ip, char *dst, uint off, uint n)
{
	if(ip->dev != PROCFS_DEV){
		if(ip->type != T_DIR) return -1;

		struct dirent de;
		int c = off / sizeof(de);

		if(c-- == 0){
			de.inum = IMEMINFO;
			memmove(de.name,"meminfo",DIRSIZ);
		}
		else if(c-- == 0){
			de.inum = ICPUINFO;
			memmove(de.name,"cpuinfo",DIRSIZ);
		}
		else{
			char buf[DIRSIZ];
			struct proc *p;
			for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
				if(p->state != UNUSED && c-- == 0){
					toString(p->pid, buf, DIRSIZ);
					de.inum = p->pid + PROCDIR_OFF;
					memmove(de.name,buf,DIRSIZ); 
					break;
				}

			if(c >= 0) return -1;
		}
		memmove(dst,&de,sizeof(de));
		
		return n;
	}


	if(ip->inum > PROCDIR_OFF && ip->inum < 2*PROCDIR_OFF){
		if(ip->type != T_DIR) return -1;
		
		struct dirent de;
		int c = off / sizeof(de);
		
		if(c > 1) return -1;

		int inum = ip->inum - PROCDIR_OFF;
		inum = 10*inum + 2*PROCDIR_OFF;

		if(c==0){
			iget(PROCFS_DEV,inum,ip);
			de.inum = inum;
			memmove(de.name,"name",DIRSIZ);
		}
		else{
			iget(PROCFS_DEV,inum+1,ip);
			de.inum = inum+1;
			memmove(de.name,"parent",DIRSIZ);
		}

		memmove(dst,&de,sizeof(de));
	}


	if(ip->inum < PROCDIR_OFF){
		if(ip->type != T_FILE) return -1;
		if(off > 16 ) return -1;

		if(off + n > 16) n = 16 - off;

		char cpuinfo[] = "CPUS: ";
		char buf[10];
		char buf2[10];
		int num = 0;
		switch(ip->inum){
			case IMEMINFO:


				break;
			
			case ICPUINFO:
				{
				struct cpu *c;
				for(c = cpus; c < &cpus[NCPU];c++)
					if(c->cpu != 0) num++;
	
				toString(num,buf2,2);
				
				char *t;
				int i;
				for(t = cpuinfo, i = 0; *t != '\0'; t++, i++)
					buf[i] = *t;
				
				
				for(t = buf2; *t != '\0'; t++, i++)
					buf[i] = *t;

				buf[i] = '\n';
				buf[i+1] = '\0';
					
				memmove(dst, buf+off,n);
				break;
				}
				
			default: cprintf("huh?\n"); return -1; break;
		}
	}


	if(ip->inum > 2*PROCDIR_OFF){
		if(ip->type != T_FILE) return -1;

		if(off > 16 ) return -1;

		if(off + n > 16) n = 16 - off;

		int pid = (ip->inum - 2*PROCDIR_OFF) / 10;
		int info = (ip->inum - 2*PROCDIR_OFF) % 10;

		char buf[16];
		struct proc *p;
		int slen;
		switch(info){
			case 0:
				for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
					if(p->state != UNUSED && p->pid == pid){
						slen = strlen(p->name);
						memmove(buf,p->name,slen); 
						break;
					}

				buf[slen+1] = '\n';
				buf[slen+2] = '\0';

				memmove(dst,buf+off,n);
				break;
				
			case 1:
				for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
					if(p->state != UNUSED && p->pid == pid){
						slen = strlen(p->name);
						memmove(buf,p->parent->name,slen); 
						break;
					}

				buf[slen+1] = '\n';
				buf[slen+2] = '\0';

				memmove(dst,buf+off,n);
				break;
			default: 
				cprintf("huh?\n");
				return -1;
				break;
		}
	}

	return n;
}

int procfs_writei(struct inode *ip, char *src, uint off, uint n){
	return 0;
}
