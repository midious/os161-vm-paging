#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <coremap.h>
//#include <swapfile.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
static struct spinlock victim_lock = SPINLOCK_INITIALIZER;
static struct coremap_entry *coremap = NULL;
static int coremapActive = 0;
static int nRamFrames=0;
static unsigned long tail = 0;
static unsigned long head = 0;

/*
 * Checks if the coremap is active
 */
static int isCoremapActive()
{
	int active;
	spinlock_acquire(&coremap_lock);
	active = coremapActive;
	spinlock_release(&coremap_lock);
	return active;
}

//Alloca gli array con le informazioni della memoria e inizializza la coremap
void coremap_init(void)
{
	int i;
	nRamFrames = ((int)ram_getsize()) / PAGE_SIZE;
	/* alloc coremap */
	coremap = kmalloc(sizeof(struct coremap_entry) * nRamFrames);
	if (coremap == NULL)
	{
		panic("Failed coremap initialization");
	}

	for (i = 0; i < nRamFrames; i++)
	{
		coremap[i].occupied= 0;
		coremap[i].freed= 0;
		coremap[i].allocSize = 0;
        coremap[i].prevAllocated = -1;
		coremap[i].nextAllocated = -1;
        coremap[i].as = NULL;
        coremap[i].vaddr = 0;
	}

	tail = -1; //indice ultima allocata
	head = -1; //indice prima allocata

	spinlock_acquire(&coremap_lock);
	coremapActive = 1;
	spinlock_release(&coremap_lock);
}

void coremap_shutdown(void) {
	/* Deactivate coremap */
	spinlock_acquire(&coremap_lock);
	coremapActive = 0;
	spinlock_release(&coremap_lock);
	/*
	 * Free the array
	 * we can't acquire the lock because it is needed by
	 * the kfree()
	 */
	kfree(coremap);
}


//Cerca se c'è uno slot lungo npages libero da poter utilizzare. Se c'è lo occupa e ritorna l'indirizzo fisico di base
static paddr_t getfreeppages(unsigned long npages, struct addrspace *as,vaddr_t vaddr) {
  paddr_t addr;	
  long i,first, found;
  long np = (long)npages;

  if (!isCoremapActive()) return 0; 

  spinlock_acquire(&coremap_lock);
  for (i=0,first=found=-1; i<nRamFrames; i++) {
    if (coremap[i].freed) { //se è stato liberato
      if (i==0 || !coremap[i-1].freed)  //se è il primo slot della coremap o il precedente non era stato liberato significa
        first = i; //che è il primo slot libero dell'intervallo 
      if (i-first+1 >= np) { //ne cerca liberi, fino a che non solo il numero di pagine richiesto
        found = first;  //ha trovato tutti gli slot liberi che gli servivano
        break;
      }
    }
  }
  if(found>=0){ //se ha trovato lo spazio libero, setta anche l'addrspace e virtual address se presenti (USER), altrimenti NULL e 0 (KERNEL)
	for(i=found; i<found+np;i++){
		coremap[i].occupied=1;
		coremap[i].freed=0;
		coremap[i].as=as;
		coremap[i].vaddr=vaddr;
	}
	coremap[found].allocSize = np;
	addr = (paddr_t)found * PAGE_SIZE;  //addr=i*PAGE_SIZE ->trova l'indirizzo fisico di base
  }else{
    addr=0;
  }

  spinlock_release(&coremap_lock);
  return addr;
}

//Chiamata solo dal KERNEL, perchè l'user può allocare 1 sola pagina alla volta.
//Cerca prima in free pages, altrimenti chiama ram_stealmem()
static paddr_t getppages(unsigned long npages)
{
  paddr_t addr;
  unsigned long i;

  /* try freed pages first */
  addr = getfreeppages(npages,NULL,0);

  //se non trova niente fa la ram_stealmem
  if (addr == 0) {
    /* call stealmem */
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }
  //aggiornamento tracciamento coremap delle pagine/frame ottenuti
  if (addr!=0 && isCoremapActive()) {
    spinlock_acquire(&coremap_lock);
    for (i=0;i<npages;i++){
        int j=(addr/PAGE_SIZE)+i; //da indirizzo fisico a indice
        coremap[j].occupied=1;
		coremap[j].freed=0;
    }
    coremap[addr / PAGE_SIZE].allocSize = npages;
    spinlock_release(&coremap_lock);
  } 

  return addr;
}

//Libera un numero desiderato di pagine a partire da addr
static int freeppages(paddr_t addr, unsigned long npages)
{
	long i, first, np = (long)npages;

	if (!isCoremapActive())
		return 0;
	first = addr / PAGE_SIZE;
	KASSERT(nRamFrames > first);

	spinlock_acquire(&coremap_lock);
	for (i = first; i < first + np; i++)
	{
		coremap[i].occupied = 0;
		coremap[i].freed=1;
		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
	}
	coremap[first].allocSize = 0;
	spinlock_release(&coremap_lock);

	return 1;
}

//alloca alcune pagine virtuali dello spazio kernel
vaddr_t alloc_kpages(unsigned npages)
{
	paddr_t pa;

	can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa); //converte da indirizzo fisico a indirizzo virtuale dello spazio kernel
}

//Libera alcune pagine precedentemente allocate da alloc_kpages. E' usato anche in kfree()
void free_kpages(vaddr_t addr)
{
	if (isCoremapActive())
	{
		paddr_t paddr = addr - MIPS_KSEG0;
		long first = paddr / PAGE_SIZE;
		KASSERT(nRamFrames > first);
		freeppages(paddr, coremap[first].allocSize);
	}
}

//ALLOCAZIONE PER I PROCESSI USER (1 PAGINA)

static paddr_t getppage_user(vaddr_t proc_vaddr){
	struct addrspace *as;
	paddr_t addr;
	unsigned long last_alloc, victim, newvictim;

	as=proc_getas();  //ritorna addrspace del processo corrente
	KASSERT(as != NULL); //getppage non può essere chiamata prima che la VM sia stata inizializzata

	KASSERT((proc_vaddr & PAGE_FRAME) == proc_vaddr); //l'indirizzo virtuale deve essere quello di inizio di una pagina

	addr= getfreeppages(1,as,proc_vaddr); //cerco una pagina libera nei frame liberati

	if (addr == 0)
	{
		//se non trova niente, effettua la ram_stealmem per ottenere 1 pagina
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(1);
		spinlock_release(&stealmem_lock);
	}

	if(isCoremapActive()){ //aggiornamento Coremap
		//leggo gli indici senza andare in conflitto. Ci lavoro. Li riaggiornerò dopo con un altro lock, senza dover mettere tutto in lock durante le operazioni.
		spinlock_acquire(&victim_lock);
		last_alloc = tail;
		victim = head;
		spinlock_release(&victim_lock);

		//Se ha trovato spazio nella RAM
		if(addr!=0){ 
			spinlock_acquire(&coremap_lock);
			int index = (addr / PAGE_SIZE);
			coremap[index].occupied = 1;
			coremap[index].freed=0;
			coremap[index].allocSize = 1;
			coremap[index].as = as;
			coremap[index].vaddr = proc_vaddr;
			
			if(last_alloc!=-1){
				//si è già allocata una pagina, si collega alle altre aggiornando la linkedlist
				coremap[last_alloc].nextAllocated=index;
				coremap[index].prevAllocated=last_alloc;
				coremap[index].nextAllocated=-1;
			}else{
				//forse si può pure togliere (nell'init è già tutto azzerato)
				coremap[index].prevAllocated=-1;
				coremap[index].nextAllocated=-1;
			}
			spinlock_release(&coremap_lock);

			//aggiornamento FIFO head e tail
			spinlock_acquire(&victim_lock);
			if (victim == -1) //prima pagina
				head = index;
			tail = index;
			spinlock_release(&victim_lock);

		}else{
			//Se non c'è più spazio in RAM

			//To Do: SWAP
		}
	}
	return addr;
}

//libera una pagina user ed aggiorna la linked list della coremap
void freeppage_user(paddr_t paddr)
{
	unsigned long last_alloc, victim;

	if (isCoremapActive())
	{
		unsigned long index = paddr / PAGE_SIZE;
		KASSERT((unsigned int)nRamFrames > index);
		KASSERT(coremap[index].allocSize == 1);

		//per gestione vittima - tiene in memoria le ultime pagine aggiunte
		spinlock_acquire(&victim_lock);
		last_alloc = tail;
		victim= head;
		spinlock_release(&victim_lock);

		//aggiornamento linked list
		spinlock_acquire(&coremap_lock);
		//se non ha elementi precedenti
		if (coremap[index].prevAllocated == -1)
		{
			//se non ha elementi successivi
			if (coremap[index].nextAllocated == -1)
			{
				//c'è solo un elemento, invalido indici di tail e head - non c'è più vittima, verrà tolta
				victim = -1;
				last_alloc = -1;
			}
			else
			{
				//ha elementi successivi (è solo la prima pagina)
				KASSERT(index == victim);  //head
				coremap[coremap[index].nextAllocated].prevAllocated = -1;
				victim= coremap[index].nextAllocated; //aggiorno head
			}
		}
		else
		{
			//ha degli elementi precedenti

			//non ha degli elementi successivi
			if (coremap[index].nextAllocated == -1)
			{
				// è l'ultima pagina inserita della linked list
				KASSERT(index == last_alloc);
				coremap[coremap[index].prevAllocated].nextAllocated = -1;
				//aggiornamento ultimo allocato - tail 
				last_alloc = coremap[index].prevAllocated;
			}
			else
			{
				//in mezzo alla linked list
				coremap[coremap[index].nextAllocated].prevAllocated = coremap[index].prevAllocated;
				coremap[coremap[index].prevAllocated].nextAllocated = coremap[index].nextAllocated;
			}
		}

		//elimino collegamenti della linked lista dalla entry da eliminare
		coremap[index].nextAllocated = -1;
		coremap[index].prevAllocated = -1;

		spinlock_release(&coremap_lock);

		//libero una pagina
		freeppages(paddr, 1);

		//aggiornamento finale FIFO tail e head, con lock
		spinlock_acquire(&victim_lock);
		head = victim;
		tail= last_alloc;
		spinlock_release(&victim_lock);
	}

	paddr_t alloc_upage(vaddr_t vaddr)
	{
		paddr_t pa;

		can_sleep();
		pa = getppage_user(vaddr);

		return pa;
	}
}
