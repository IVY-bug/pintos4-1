#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp,
                  char ** fp);
//struct lock file_lock;
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
//  lock_init(&file_lock);
  //printf("process execute...\n");
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  //Get file name only without arguments
  //call to strtok_r gives us first token here which is file name
  //char *fp;
  //file_name = strtok_r((char *)file_name, " ", &fp);

  int i;
  char * name = (char *) malloc (sizeof (char));
  for (i = 0; i < strlen (file_name); i++)
   {
     if (file_name[i] == ' ')
      {
        break;
      }
      name[i] = file_name[i];
   }
   name[i] = '\0';



/*
  //alocating memory for child_process
  struct child_process *child_p = malloc (sizeof (struct child_process));

  //Check for success
  if(child_p == NULL)
     {
        palloc_free_page (fn_copy);
        return TID_ERROR;
     }

  //Add to the list
  list_push_back (&thread_current ()->child_list, &child_p->elem);
*/
   /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (name, PRI_DEFAULT, start_process, fn_copy);
  //1.get child thread. 2. pass in child sema. 3. check if child load success  
  
 
 // lock_release(&file_lock);i
//  if (tid == TID_ERROR)
//     {
        //remove process from child list
       // list_remove (&child_p->elem);
        //free (child_p);
//        palloc_free_page (fn_copy); 
//        return TID_ERROR;  
//     }
//   struct thread *t = thread_by_tid (tid);
//   sema_down (&t->chp->sema_wait);
//   if (t->ret_status == -1)
//



//    tid = TID_ERROR;
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  else {
    enum intr_level old_level = intr_disable();
    thread_block();
    intr_set_level(old_level);
    if (thread_current()->child_create_error)
      tid = TID_ERROR;
  }

/*
     //Get child ref pointer
     struct child_process *child_p  =  &thread_current()->chp;
    //update child  pid with tid
     //child_p->pid = tid;

     //Wait for child to get load
     lock_acquire (&thread_current()->child_cond_lock);
     int child_load_status = child_p->status;
     //Wait till status of child gets changes from NOT-LOADED
     while(child_load_status == NOT_LOADED)
       {
          cond_wait (&thread_current()->child_condition,
                     &thread_current()->child_cond_lock);
          child_load_status = child_p->status;
       }
      lock_release (&thread_current()->child_cond_lock);
      
      //Now that load status of child has changed, we check for  FAILED status
      if(child_load_status == LOAD_FAIL)
       {
          list_remove (&child_p->elem);
          free (child_p);
          return TID_ERROR;
       } 
     //successful loading of child so return tid  
     
   */

   return tid;  
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;


  //Get file name only without arguments
  //call to strtok_r gives us first token here, which is file name
  char *fp;
  file_name = strtok_r(file_name, " ", &fp);


  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
 
 //Will send fp pointer to file to load here
  success = load (file_name, &if_.eip, &if_.esp, &fp);
//   thread_current ()->load_status = true; 
  //set status of processes if load successfull
/*  if (success == 1)
     {
      thread_current ()->chp->load = LOAD_SUCCESS;
      sema_up (&thread_current()->chp->sema_wait);
     }
  else
    {
      sema_up (&thread_current()->chp->sema_wait);
      thread_current()->ret_status = -1;

      thread_current ()->chp->load = LOAD_FAIL;
    } 
*/
 /* Now we know whether the thread successfully started or not.
   * Let its parent know this. */
  struct thread * parent = thread_current()->parent;
  parent->child_create_error = !success;

  /* If load failed, quit. */
  if (!success)
  {
    palloc_free_page (file_name);
    thread_unblock (parent);
    thread_exit ();
  }
  else
  {
    /* Command successfully started. Put the arguments in the stack. */
   // parse_args_onto_stack(&if_.esp, command);
    palloc_free_page (file_name);
    thread_unblock (parent);
  }
  
//1.if success sema up, wake up the parent waiting thread.
//2.pass back some indicator back to parent.
   

 
// printf("Completed load...\n");
 // printf("success: %d\n", success);

  /* If load failed, quit. */
//  palloc_free_page (file_name);
//  if (!success) 
//    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

//get process for given pid
struct child_process * 
get_process_for_pid (int pid)
{
 
   struct list_elem *e = list_begin (&thread_current()->child_list);
   struct list_elem *next;
   while (e != list_end (&thread_current()->child_list))
   {
     struct child_process *chp = list_entry (e, struct child_process,
                                          elem);
     next = list_next(e);
     if (pid == chp->pid)
       {
        return chp;
       }
     e = next;
   }
   return NULL;
  
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
//--------------
/*  
  struct child_process *child = get_process_for_pid(child_tid);
  if(!child)
  {
    return -1;
  }
  if(child->wait)
  {
    return -1;
  }
//  sema_down (&child->sema_wait);

  int status = child->status;
  
 
  //sema_up (&child->sema_exit);
   child->wait = true;

   
  while(!child->exit)
  {
    barrier();
  }
 // printf("Waiting..\n");

 // printf("child status : %d\n",status);
  list_remove(&child->elem);
  free(child);
 // printf("return from wait..\n");
 return status;
 */


//-------
#ifdef USERPROG

  struct thread * this_thread = thread_current();

  enum intr_level old_level = intr_disable ();

  struct child_thread_data * child_data = thread_get_child_data (this_thread, child_tid);
  if (child_data == NULL)
  {
    intr_set_level (old_level);
    return -1;
  }

  struct thread * child = thread_by_tid(child_tid);

  if (child != NULL)
  {
    sema_down(&child_data->s);
  }

  int retval = child_data->exit_status;
  list_remove (&child_data->elem);
  free (child_data);
  intr_set_level (old_level);
  return retval;

#else
  /* In case USERPROG was not defined (you can ignore/not implement this part). */
  return -1;
#endif
 /*
struct child_process * chp = &thread_current ()->chp;
lock_acquire (chp->wait_lock);

cond_wait (chp->condition, chp->wait_lock);
*/

 /*
 struct child_process * chp = &thread_current ()->chp;
 //acquire lock
 lock_acquire (chp->wait_lock);

 //get status from chp
 if(chp == NULL)
   {
    lock_release (chp->wait_lock);
    return -1;
   }

  while(!chp->exit)
   {
    cond_wait ()
   } 
 */
}

/* Free the current process's resources. */
void
process_exit (void)
{

/*
 struct child_process * chp = &thread_current ()->chp;
lock_acquire (chp->wait_lock);

cond_signal (chp->condition, chp->wait_lock);
*/
//removes child processes

/*   struct list_elem *e = list_begin (&thread_current()->child_list);
   struct list_elem *next;
   while (e != list_end (&thread_current()->child_list))
   {
     struct child_process *chp = list_entry (e, struct child_process,
                                          elem);
     next = list_next(e);
     list_remove (&chp->elem);
     free (chp);
     e = next;
   }
*/
//added 
//  struct thread *t = thread_current ();
//   while (!list_empty (&thread_current()->chp->sema_wait.waiters))
//      sema_up (&thread_current()->chp->sema_wait);
   //if(thread_current()->parent != NULL)
      //sema_down (&thread_current()->chp->sema_exit); 
//Close files owned
   close_all_files();
   

  struct thread *cur = thread_current ();
  uint32_t *pd;
  
 // printf("exiting process...\n");  
//  if(thread_exist_in_all_list(cur->parent))
//   {
///      cur->chp->exit = true;  
//   } 
  if (thread_current()->cwd)
  {
     dir_close(thread_current()->cwd);
  }  
  printf("%s: exit(%d)\n", cur->name, cur->exit_status);  
  
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
//-----------------added
sema_up(&(thread_get_child_data (cur->parent, cur->tid))->s);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char* file_name, char** fp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp, 
      char ** fp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }


  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
    //passing file stack pointer, file name, and pointer to name
  if (!setup_stack (esp, file_name, fp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
/*
static bool
setup_stack (void **esp, const char* file_name, char** fp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        {
          palloc_free_page (kpage);
          return success;
        }
    }
   //moved inside else part above 
  //return success;

  
  //adding code here
    //printf("Setting up stack here...\n");
    //printf("File name: %s\n", file_name);
  
  //pushing on stack
    char *token;
    //allocating memory more than a char required
    char ** argv = malloc (2 * sizeof(char *));
    int argc = 0;
    int counter = 2;
    int i;
  for(token = (char *) file_name; token != NULL;
      token = strtok_r (NULL, " ", fp))
      {
       // printf("%s\n", token);
        //decrement esp by length
        //+1 is for null
        *esp -= strlen (token) + 1; 

        //add value at esp into argv at argc position
        argv[argc] = *esp;

        argc++;

        //if more arguments than allocated memory then reallocate memory
        if (argc > counter)
           {
            counter *= 2; //double counter
            argv = realloc (argv, counter * sizeof (char *));
           }

        //memory copy
        memcpy (*esp, token, strlen (token) + 1);
        printf("%s : %x\n", *esp, *esp);
      }  

      //padding argv with 0 as
      //argv[argc] =  0;

      //As we need to copy addresses of args that we have stored now
      //we need to decrement esp afterwords, by size of char
      //and to do that we need to make sure that currently stored data
      //to be multiple of sizeof(char *),
      //on diff machines it will be 4/8 , so we resize here by 4

      i = (size_t) *esp % 4;
      //int * ptr = (int *) 0;
      if(i > 0)
       {
        //decrement esp by i
        *esp -= i;
        memcpy (*esp, &argv[argc], i);

       }  

      //printf("decrement esp by %d \n",i);
      //printf("%s : %p\n", *esp, *esp);  

      //Adding Null after all args are saved
      *esp -= sizeof (char *);
      memcpy (*esp, &argv[argc], sizeof (char *));

      //copy addresses of all arguments
      //will start copying addresses from last arg in argv
      for (i = argc + 1; i >= 0; i--)
         {
            *esp -= sizeof (char *);
            memcpy (*esp, &argv[i], sizeof (char *));
         }

//       printf("%s : %p\n", *esp, *esp);   
      //Now we have to store address of arg[0]
      //esp is pointing currently to arg[0]
      char *first_arg = *esp;  
      *esp -= sizeof (char **); //double pointer to access size of arg[0]
      memcpy (*esp, &first_arg, sizeof (char **));

  //     printf("%s : %x\n", *esp, *esp);
      //Now storing argc
      //decrement esp by size of int
      *esp -= sizeof (int);
      memcpy (*esp, &argc, sizeof (int));

    //   printf("%s : %x\n", *esp, *esp);
      //Now adding fake return address
      //writing NULL pointer in this place
      *esp -= sizeof (void *);
      memcpy (*esp, &argv[argc], sizeof (void *));

     //  printf("%s : %x\n", *esp, *esp);
      //  hex_dump (*esp, *esp, PHYS_BASE - *esp, true);
    return success;   
}

*/

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *file_name, char **save_ptr) 
{
  //printf("\n setting up stack:..... %s\n", file_name);
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

 //printf ("\nInitial : %x : %c %d \n",esp, *esp, *esp);
  
  //printf ("\n Initial setup_stack :   %x %x \n",*esp,esp);
  //printf("%s\n", esp);
  
  char *token;
  int length_token = 0;
  //char **address = malloc(2*sizeof(char *));
  int arg_count = 0;
  //int address_size = 2;
  char *start = NULL;
  char *sp = *esp;
  char *actual_arg = NULL;
  start = *esp;
  for(token = (char *) file_name; token != NULL; token = strtok_r(NULL, " ", save_ptr)){
    //printf("\n%s\n", token);
        //*esp -= strlen(token);
        //*esp = token;
        //printf ("\n setup_stack :   %x %x \n",*esp,esp);
  arg_count++;
  length_token = strlen(token) + 1;
        *esp -= length_token;
        memcpy(*esp, token, length_token);
        //printf ("\n%x : %s %x\n",esp, *esp, *esp); 
        //printf ("\n setup_stack :   %x %x\n",*esp,esp);
        //printf ("\n %x : %c %d \n",esp, *esp, *esp);
  }
  //*start = *esp + length_token;
  //char *aesp = *esp;
  actual_arg = *esp;

   //printf("\n actual arg: %x, %x and start: %x, %x \n", actual_arg, *actual_arg, start, *start);

  //hex_dump(*esp, *esp, PHYS_BASE - *esp, true); 
 
  int *align = (int *)0;

  //printf("\n printing esp:");
  int word_align = (size_t)*esp % 4;
  //printf("\n word_align %d\n", word_align);
  if(word_align > 0) {
    *esp -= word_align;
    memcpy(*esp, &align, word_align); 
        //printf("\nword align\n");
        //printf ("\n%x : %c %x\n",esp, *esp, *esp);
        
  }

  //printf("\n after word align: %x %x\n", *esp, esp);
   
  //NULL characater
  *esp -= sizeof(char *);
   memcpy(*esp, &align, sizeof(char *));
  //printf("\n after null: %x %x\n", *esp, esp);

   //printf("\n actual arg: %x, %x and start: %x, %x \n", actual_arg, *actual_arg, start, *start);
  //actual_arg += (sizeof(char *) + word_align);
  //*esp -= sizeof(char *);
  //memcpy(*esp, &actual_arg, sizeof(char *));
 
  //printf("\n after first_push: %x %x\n", *esp, esp);

  //printf ("\n%x : %s %x\n",esp, *esp, *esp); 
  while(actual_arg != start){
    //printf("\n actual_arg: %x %c, start: %x %c\n", actual_arg, *actual_arg, start, *start);
        if(*(actual_arg - 1)== NULL && actual_arg + 1 != start){

    ///printf("\n actual_arg storing: %x %c\n", actual_arg, *actual_arg);
    *esp -= sizeof(char *);
    memcpy(*esp, &actual_arg, sizeof(char *));
  }
        actual_arg +=1;
  }


  //printf ("\n%x : %s %x\n",esp, *esp, *esp); 

  char *arg_address = NULL;
  arg_address = *esp;
  *esp -= sizeof(char **);
  memcpy(*esp, &arg_address, sizeof(char **));
 
  //printf ("\n%x : %s %x\n",esp, *esp, *esp); 

  *esp -= sizeof(int);
  memcpy(*esp, &arg_count, sizeof(int));   

  //printf ("\n%x : %s %x\n",esp, *esp, *esp); 
  *esp -= sizeof(void *);
  memcpy(*esp, &align, sizeof(void *));

  
  return success;
}



/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
