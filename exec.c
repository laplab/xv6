#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

#define BANG_BUF_SIZE 128
#define BANG_MAX_DEPTH 5

int
exec_impl(char *path, char **argv, int depth)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir = 0, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);

  // Check #!
  char header[2];
  if (readi(ip, header, 0, sizeof(header)) == sizeof(header) && header[0] == '#' && header[1] == '!') {    
    // try to read bang line
    char line[BANG_BUF_SIZE];
    int len = readi(ip, line, 2, sizeof(line));

    iunlockput(ip);
    end_op();

    if (len <= 0) {
      cprintf("exec: bang fail\n");
      return -1;
    }

    if (depth == BANG_MAX_DEPTH) {
      cprintf("exec: bang max depth exceeded\n");
      return -1;
    }

    // treat \n as an end of bang line
    char *terminal_end = line + BANG_BUF_SIZE - 1, *end = line;
    while (end < terminal_end) {
      if (*end == '\n') {
        break;
      }
      end++;
    }
    *end = '\0';

    char *argv_new[MAXARG], **argv_ptr = argv_new, **argv_end = argv_new + MAXARG;

    // skip space characters if any
    char *ptr = line;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ptr++;

    // extract interpreter path argument
    char *in_path = ptr;
    while (ptr < end) {
      if (*ptr == ' ' || *ptr == '\t') {
        *ptr = '\0';
        break;
      }

      ptr++;
    }

    // add interpreter path argument
    *argv_ptr = in_path;
    argv_ptr++;

    // take next character after delimiter
    ptr++;

    // skip space characters if any
    while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ptr++;

    // add optional arg
    if (ptr+1 < end) {
      *argv_ptr = ptr;
      argv_ptr++;
    }
    
    while (argv_ptr+1 < argv_end && *argv) {
      *argv_ptr = *argv;
      argv_ptr++;
      argv++;
    }
    *argv_ptr = 0;

    return exec_impl(in_path, argv_new, depth+1);
  }

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

int
exec(char *path, char **argv) {
  return exec_impl(path, argv, 0);
}

