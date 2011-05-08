#include <u.h>
#include "fns.h"

#define GETSHORT(p) (*(ushort *)(p))
#define GETLONG(p) (*(uint *)(p))

enum {
	Sectsz = 0x200,
	Dirsz = 0x20,
	Maxpath = 64,
	Fat12 = 1,
	Fat16 = 2,
	Fat32 = 4,
};

typedef struct Extend Extend;
typedef struct Dir Dir;
typedef struct Pbs Pbs;
typedef struct Fat Fat;

struct Fat
{
	ulong ver;
	int drive;
	ulong clustsize;
	ulong eofmark;
	ulong partlba;
	ulong fatlba;
	ulong dirstart; /* LBA for FAT16, cluster for FAT32 */
	ulong dirents;
	ulong datalba;
};

struct Extend
{
	Fat *fat;
	ulong lba;
	ulong clust;
	ulong lbaoff;
	ulong len;
	uchar *rp;
	uchar *ep;
	uchar buf[Sectsz];
};

struct Dir
{
	char name[11];
	uchar attr;
	uchar reserved;
	uchar ctime;
	uchar ctime[2];
	uchar cdate[2];
	uchar adate[2];
	uchar starthi[2];
	uchar mtime[2];
	uchar mdate[2];
	uchar startlo[2];
	uchar len[4];
};

struct Pbs
{
	uchar magic[3];
	uchar version[8];
	uchar sectsize[2];
	uchar clustsize;
	uchar nreserv[2];
	uchar nfats;
	uchar rootsize[2];
	uchar volsize[2];
	uchar mediadesc;
	uchar fatsize[2];
	uchar trksize[2];
	uchar nheads[2];
	uchar nhidden[4];
	uchar bigvolsize[4];
	union
	{
		struct
		{
			uchar driveno;
			uchar reserved0;
			uchar bootsig;
			uchar volid[4];
			uchar label[11];
			uchar type[8];
		} fat16;
		struct
		{
			uchar fatsize[4];
			uchar flags[2];
			uchar ver[2];
			uchar rootclust[4];
			uchar fsinfo[2];
			uchar bootbak[2];
			uchar reserved0[12];
			uchar driveno;
			uchar reserved1;
			uchar bootsig;
			uchar volid[4];
			uchar label[11];
			uchar type[8];
		} fat32;
	};
};

int readsect(ulong drive, ulong lba, void *buf);

void
unload(void)
{
}

static ulong
readnext(Extend *ex, ulong clust)
{
	Fat *fat = ex->fat;
	uint b = fat->ver;
	ulong sect, off;
	
	sect = clust * b / Sectsz;
	off = clust * b % Sectsz;
	if(readsect(fat->drive, fat->fatlba + sect, ex->buf))
		memset(ex->buf, 0xff, 4);
	switch(fat->ver){
	case Fat16:
		return GETSHORT(&ex->buf[off]);
	case Fat32:
		return GETLONG(&ex->buf[off])& 0x0fffffff;
	}
	return 0;
}

int
read(void *f, void *data, int len)
{
	Extend *ex = f;
	Fat *fat = ex->fat;

	if(ex->len > 0 && ex->rp >= ex->ep){
		if(ex->clust != ~0U){
			if(ex->lbaoff % fat->clustsize == 0){
				if((ex->clust >> 4) == fat->eofmark)
					return -1;
				ex->lbaoff = (ex->clust - 2) * fat->clustsize;
				putc('.');
				ex->clust = readnext(ex, ex->clust);
				ex->lba = ex->lbaoff + fat->datalba;
			}
			ex->lbaoff++;
		}
		if(readsect(fat->drive, ex->lba++, ex->rp = ex->buf))
			return -1;
	}
	if(ex->len < len)
		len = ex->len;
	if(len > (ex->ep - ex->rp))
		len = ex->ep - ex->rp;
	memmove(data, ex->rp, len);
	ex->rp += len;
	ex->len -= len;
	return len;
}

void
open(Fat *fat, void *f, ulong lba)
{
	Extend *ex = f;

	ex->fat = fat;
	ex->lba = lba;
	ex->len = 0;
	ex->lbaoff = 0;
	ex->clust = ~0U;
	ex->rp = ex->ep = ex->buf + Sectsz;
}

void
close(void *)
{
}

static int
dirname(Dir *d, char buf[Maxpath])
{
	char c, *x;

	if(d->attr == 0x0F || *d->name <= 0)
		return -1;
	memmove(buf, d->name, 8);
	x = buf+8;
	while(x > buf && x[-1] == ' ')
		x--;
	if(d->name[8] != ' '){
		*x++ = '.';
		memmove(x, d->name+8, 3);
		x += 3;
	}
	while(x > buf && x[-1] == ' ')
		x--;
	*x = 0;
	x = buf;
	while(c = *x){
		if(c >= 'A' && c <= 'Z'){
			c -= 'A';
			c += 'a';
		}
		*x++ = c;
	}
	return x - buf;
}

static ulong
dirclust(Dir *d)
{
	return *((ushort*)d->starthi)<<16 | *((ushort*)d->startlo);
}

static int
fatwalk(Extend *ex, Fat *fat, char *path)
{
	char name[Maxpath], *end;
	int i, j;
	Dir d;

	if(fat->ver == Fat32){
		open(fat, ex, 0);
		ex->clust = fat->dirstart;
		ex->len = ~0U;
	}else{
		open(fat, ex, fat->dirstart);
		ex->len = fat->dirents * Dirsz;
	}
	for(;;){
		if(readn(ex, &d, Dirsz) != Dirsz)
			break;
		if((i = dirname(&d, name)) <= 0)
			continue;
		while(*path == '/')
			path++;
		if((end = strchr(path, '/')) == 0)
			end = path + strlen(path);
		j = end - path;
		if(i == j && memcmp(name, path, j) == 0){
			open(fat, ex, 0);
			ex->clust = dirclust(&d);
			ex->len = *((ulong*)d.len);
			if(*end == 0)
				return 0;
			else if(d.attr & 0x10){
				ex->len = fat->clustsize * Sectsz;
				path = end;
				continue;
			}
			break;
		}
	}
	return -1;
}

static int
conffat(Fat *fat, void *buf)
{
	Pbs *p = buf;
	uint fatsize, volsize, datasize, reserved;
	uint ver, dirsize, dirents, clusters;
	
	/* sanity check */
	if(GETSHORT(p->sectsize) != Sectsz){
		print("sectsize != 512\r\n");
		halt();
	}
	
	/* load values from fat */
	fatsize = GETSHORT(p->fatsize);
	if(fatsize == 0)
		fatsize = GETLONG(p->fat32.fatsize);
	volsize = GETSHORT(p->volsize);
	if(volsize == 0)
		volsize = GETLONG(p->bigvolsize);
	reserved = GETSHORT(p->nreserv);
	dirents = GETSHORT(p->rootsize);
	dirsize = (dirents * Dirsz + Sectsz - 1) / Sectsz;
	datasize = volsize - (reserved + fatsize * p->nfats + dirsize);
	clusters = datasize / p->clustsize;
	
	/* determine fat type */
	if(clusters < 4085)
		ver = Fat12;
	else if(clusters < 65525)
		ver = Fat16;
	else
		ver = Fat32;
	
	/* another check */
	if(ver == Fat12){
		print("TODO: implement FAT12\r\n");
		halt();
	}
	
	/* fill FAT descriptor */
	fat->ver = ver;
	fat->dirents = dirents;
	fat->clustsize = p->clustsize;
	fat->fatlba = fat->partlba + reserved;
	fat->dirstart  = fat->fatlba + fatsize * p->nfats;
	if(ver == Fat32){
		fat->datalba = fat->dirstart;
		fat->dirstart  = GETLONG(p->fat32.rootclust);
		fat->eofmark = 0xffffff;
	}else{
		fat->datalba = fat->dirstart + dirsize;
		fat->eofmark = 0xfff;
	}	
	return 0;
}

static int
findfat(Fat *fat, int drive)
{
	struct {
		uchar status;
		uchar bchs[3];
		uchar typ;
		uchar echs[3];
		uchar lba[4];
		uchar len[4];
	} *p;
	uchar buf[Sectsz];
	int i;

	if(readsect(drive, 0, buf))
		return -1;
	if(buf[0x1fe] != 0x55 || buf[0x1ff] != 0xAA)
		return -1;
	p = (void*)&buf[0x1be];
	for(i=0; i<4; i++){
		if(p[i].status != 0x80)
			continue;
		fat->drive = drive;
		fat->partlba = *((ulong*)p[i].lba);
		if(readsect(drive, fat->partlba, buf))
			continue;
		if(conffat(fat, buf))
			continue;
		return 0;
	}
	return -1;
}

void
start(void *sp)
{
	char path[Maxpath], *kern;
	int drive;
	Extend ex;
	Fat fat;
	void *f;

	/* drive passed in DL */
	drive = ((ushort*)sp)[5] & 0xFF;

	print("9bootfat\r\n");
	if(findfat(&fat, drive)){
		print("no fat\r\n");
		halt();
	}
	if(fatwalk(f = &ex, &fat, "plan9.ini")){
		print("no config\r\n");
		f = 0;
	}
	for(;;){
		kern = configure(f, path); f = 0;
		if(fatwalk(&ex, &fat, kern)){
			print("not found\r\n");
			continue;
		}
		print(bootkern(&ex));
		print(crnl);
	}
}

