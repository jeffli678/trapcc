
#include <stdint.h>
#undef VERBOSE
	
typedef volatile struct __tss_struct { /* For GDB only */
    unsigned short   link;
    unsigned short   link_h;

    unsigned int   esp0;
    unsigned short   ss0;
    unsigned short   ss0_h;

    unsigned int   esp1;
    unsigned short   ss1;
    unsigned short   ss1_h;

    unsigned int   esp2;
    unsigned short   ss2;
    unsigned short   ss2_h;

    unsigned int   cr3;
    unsigned int   eip;
    unsigned int   eflags;

    unsigned int   eax;
    unsigned int   ecx;
    unsigned int   edx;
    unsigned int    ebx;

    unsigned int   esp;
    unsigned int   ebp;

    unsigned int   esi;
    unsigned int   edi;

    unsigned short   es;
    unsigned short   es_h;

    unsigned short   cs;
    unsigned short   cs_h;

    unsigned short   ss;
    unsigned short   ss_h;

    unsigned short   ds;
    unsigned short   ds_h;

    unsigned short   fs;
    unsigned short   fs_h;

    unsigned short   gs;
    unsigned short   gs_h;

    unsigned short   ldt;
    unsigned short   ldt_h;

    unsigned short   trap;
    unsigned short   iomap;

} tss_struct;	
	
#define	PG_P 0x001
#define PG_W 0x002
#define PG_U 0x004
/* Skip cache stuff */
#define PG_A 0x020
#define	PG_M 0x040
#define	PG_PS 0x080
#define	PG_G 0x100

/* Multiboot parameters*/
extern uint32_t magic;
extern void *mbd;


/* Screen printing */
#define NUM_LINES 24
#define NUM_CHARS 80
int g_x=1,g_y=1;
static inline void setchar(unsigned int line, unsigned int off, unsigned char character){
   unsigned char *videoram = (unsigned char *)0xB8000;
   videoram[line * NUM_CHARS * 2+2*off] = character; /* character 'A' */
   videoram[line * NUM_CHARS * 2+ 2 *off + 1] = 0x07; /* light grey (7) on black (0). */
}
#define OUTPUT(x,y,char) setchar(y,x,char)
static inline void clear_screen(){
  for(int l=0;l<24;l++){
    for(int c=0;c<80;c++){
      setchar(l,c,' ');
    }
  }
  g_x=g_y=0;
}
static inline void next_line()
{
  g_x = 0;
   if(++g_y == NUM_LINES)
     g_y = 0; 
}
static inline void print_character(unsigned char x){  
  if(++g_x == NUM_CHARS)
    next_line();
  setchar(g_y,g_x,x);
}
static inline void hex_digit(unsigned char digit){
  if(digit < 10)
    print_character(digit + '0');
  else 
    print_character(digit + 'A' - 10);
}
static inline void hex_byte(unsigned char byte){
  hex_digit(byte >> 4);
  hex_digit(byte & 0xF);
}
static inline void hex_word(unsigned short word){
  hex_byte(word >> 8);
  hex_byte(word & 0xFF);
}
static inline void hex_dword(unsigned int word){
  hex_word(word >> 16);
  hex_word(word & 0xFFFF);

}
static inline void bin_dword(unsigned int word){
  for(int i = 32;i>0;i--){
    if(word & 0x80000000)
      print_character('1');
    else
      print_character('0');
    word <<= 1;
  }  
}
static inline void print_string( char * data){
  while(*data != 0){
    if(*data == '\n')
      next_line();
    else
      print_character(*(unsigned char *)data);
    data++;
  }
}
#define PRINT_STRING(x)  {char str## __LINE__ [] = x; print_string (str ## __LINE__);}
#define PRINT_STRING2(x)  {char str2## __LINE__ [] = x; print_string (str2 ## __LINE__);}
#define DEBUG_ADDR(x) {PRINT_STRING("Contents of "#x"("); hex_dword((unsigned int)x); PRINT_STRING2(") are:"); hex_dword(*(unsigned int *)x); next_line(); }

/* Utility functions, copied from FreeBSD*/
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
void load_cr0(u_int data)
{
	__asm __volatile("movl %0,%%cr0" : : "r" (data) : "memory");
}

u_int rcr0(void)
{
	u_int	data;

	__asm __volatile("movl %%cr0,%0" : "=r" (data));
	return (data);
}
u_int rcr2(void)
{
	u_int	data;

	__asm __volatile("movl %%cr2,%0" : "=r" (data));
	return (data);
}
void load_cr3(u_int data)
{
	__asm __volatile("movl %0,%%cr3" : : "r" (data) : "memory");
}

u_int rcr3(void)
{
	u_int	data;

	__asm __volatile("movl %%cr3,%0" : "=r" (data));
	return (data);
}

u_int reflags(void)
{
	u_int	data;

	__asm __volatile("pushf \n pop %0" : "=r" (data));
	return (data);
}

void
load_cr4(u_int data)
{
	__asm __volatile("movl %0,%%cr4" : : "r" (data));
}

 u_int
rcr4(void)
{
	u_int	data;

	__asm __volatile("movl %%cr4,%0" : "=r" (data));
	return (data);
}
 u_int
rsp(void)
{
	u_int	data;

	__asm __volatile("movl %%esp,%0" : "=r" (data));
	return (data);
}

/* End FreeBSD */


/* GDT, TSS */
#define GDT_ADDRESS 0x01800000
typedef unsigned char gdt_entry[8];
gdt_entry *g_gdt = (gdt_entry *)GDT_ADDRESS;
char g_tss[104];
unsigned int g_tss_ptr= 0;
extern void asmSetGDTR(void *GDT,unsigned int size);
extern void asmSetIDTR(void *IDT,unsigned int size);
#define PAGE_OFFSET(x) (x* (1ul<<12))
#define HUGEPAGE_OFFSET(x) (x * (1ul << 22))
#define IDT_ADDRESS 0x1000000
#define MAX_PDES 512
#define PAGE_DIRECTORY ( HUGEPAGE_OFFSET(31)) // Start pagetables at 124 MB ram.
#define MAP_PAGE(virt,phys) pde[virt]= (phys << 22) | PG_P | PG_W | PG_U | PG_PS;

void init_paging(){
  int *pde = (int *)PAGE_DIRECTORY;
  for(int i=0;i<MAX_PDES;i++){ //Direct mapping
    MAP_PAGE(i,i);
  }
  load_cr3((u_int)pde);
  load_cr4(rcr4() | 0x10);
  //load_cr4(rcr4() |  0x00000080);      /* Page global enable */
  load_cr0(rcr0()  | (1<<31));
}
static inline void encode_gdt(gdt_entry t,unsigned char type,
			      unsigned int base,
			      unsigned int limit)
{
  limit = limit >> 12;
  ((u_short *)t)[0]=(unsigned short) limit& 0xFFFF;
  ((u_short *)t)[1]= base & 0xFFFF;
  t[4] = (unsigned char) (base >> 16) ;
  t[5] =  (unsigned char) type;
  t[6] =  (unsigned char) 0xC0 | (unsigned char)(limit>>16); /* 32 bit and page granular */
  t[7] =  (unsigned char)(base >> 24) ;
}
#define TSS_ALIGN -48
static inline void init_gdt(){
  int i,j;
  for(i=0;i<8192;i++)
    *((unsigned long *)&g_gdt[i]) = 0; 
  encode_gdt(g_gdt[1],0x9A,0,0xFFFFFFFF); /* code 0x08*/
  encode_gdt(g_gdt[2],0x92,0,0xFFFFFFFF); /* data 0x10 */
  encode_gdt(g_gdt[3],0x89,g_tss_ptr,0xFFFFFF); /*TSS0 0x18*/ 
  asmSetGDTR(g_gdt,0xFFFF);    /* sets TSS to 0x18*/
  for(j=0;j<16;j++){ //TODO: Re-increase
    i = (j * 0x1000 +  0xFF8) / 8;
    encode_gdt(g_gdt[i],0x89,1024 * 4096  + j *65536 + TSS_ALIGN,0xFFFFFF);/* See interrupt_program.rb */
  }
  // encode_gdt(g_gdt[(0xFFE0/8)],0x9A,0,0xFFFFFE00); /* Causes GPF*/
}
static inline void init_tss(){ /* TODO: refactor */
  g_tss_ptr = (u_int) &g_tss;
  *((u_int *)(g_tss + 28)) = PAGE_DIRECTORY;
}

static void interrupt_program();
static void begin_computation();
void kmain(void)
{  
   if ( magic != 0x2BADB002 )
  {
    while(1){}
   }
   u_int lower_mem = ((unsigned int*)mbd)[1];
   u_int higher_mem = ((unsigned int*)mbd)[1];  
   
   clear_screen();
   init_paging();   
   PRINT_STRING("We are now paging\n");
   init_tss();
   PRINT_STRING("We have a TSS\n");
   init_gdt();
   PRINT_STRING("And loaded a GDT\n");   
   //init_idt();
   //PRINT_STRING("And it's neighbour the IDT\n");4
   PRINT_STRING("Arrange the buffet\n");
   interrupt_program();
   asmSetIDTR(IDT_ADDRESS,256*8 - 1);
   /* Pagefault. this will save the TSS state*/
   // begin_computation();
   PRINT_STRING("Get the party started!\n");
   while(1){
     int i =0;
     volatile int j = 0;
     begin_computation();  
     // encode_gdt(g_gdt[3],0x89,g_tss_ptr,0xFFFFFF); /*TSS0 0x18*/  
      for(i=0; i< (1<<24); i++){
       j=4;
       } 
   }
   //__asm __volatile ("lcall  $0x30, $0x0");
   //lcr3(INIT_PAGETABLE);
     while(1){}
}

void memset(void *s,int c,unsigned int sz){
  char *data = (char *)s;
  while(sz> 0){
    sz--;
    *(data++)  = (unsigned char )c;
  }
}
u_int *_tmp[4096];
/* STANDALONE specific code */
#define ALLOC_PTEPTR_ARRAY() _tmp /* Allocation is used only once */
/* TODO: Free */
#define PFN2VIRT(x) ((char *)( (x)<<12))
#define VIRT2PFN(x) (((u_int)(x))>>12)
const u_int base_pfn =  32 *  1024;
    void zero_memory();
    void interrupt_program(){
      zero_memory();
 /* dec_odd : oddcounter <- evencounter , dec_even , reset */ 
 
{ 
 u_int **pte_ptr = ALLOC_PTEPTR_ARRAY(); 
u_int *pde_ptr = PFN2VIRT((base_pfn+19)) /* pd dec_odd */; int i; 
    for(i = 0; i< 1024; i++){
        pde_ptr[i] = PG_U | PG_A | PG_W;
    }
pte_ptr[0] = PFN2VIRT((base_pfn+25)) /* pt dec_odd 0 */;
pde_ptr[0] |= PG_P| ((base_pfn+25) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[0][i] = PG_A| PG_U;
      }
pte_ptr[0][0] |= PG_P| PG_W | ((base_pfn+20) << 12);/* 0 -> 'stack_page' */ 
pte_ptr[0][1023] |= PG_P| PG_W | ((base_pfn+3) << 12);/* 3ff000 -> 'gdt 0' */ 
pte_ptr[1] = PFN2VIRT((base_pfn+26)) /* pt dec_odd 1 */;
pde_ptr[1] |= PG_P| ((base_pfn+26) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[1][i] = PG_A| PG_U;
      }
pte_ptr[1][0] |= PG_P| PG_W | ((base_pfn+2) << 12);/* 400000 -> 'var oddcounter' */ 
pte_ptr[1][15] |= PG_P| PG_W | ((base_pfn+22) << 12);/* 40f000 -> 'ins dec_even' */ 
pte_ptr[1][16] |= PG_P| PG_W | ((base_pfn+2) << 12);/* 410000 -> 'var oddcounter' */ 
pte_ptr[1][31] |= PG_P| PG_W | ((base_pfn+23) << 12);/* 41f000 -> 'ins reset' */ 
pte_ptr[1][32] |= PG_P| PG_W | ((base_pfn+0) << 12);/* 420000 -> 'var reset' */ 
pte_ptr[4] = PFN2VIRT((base_pfn+27)) /* pt dec_odd 4 */;
pde_ptr[4] |= PG_P| ((base_pfn+27) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[4][i] = PG_A| PG_U;
      }
pte_ptr[4][0] |= PG_P| PG_W | ((base_pfn+24) << 12);/* 1000000 -> 'IDT dec_odd' */ 
pte_ptr[6] = PFN2VIRT((base_pfn+28)) /* pt dec_odd 6 */;
pde_ptr[6] |= PG_P| ((base_pfn+28) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[6][i] = PG_A| PG_U;
      }
pte_ptr[6][0] |= PG_P| PG_W | ((base_pfn+3) << 12);/* 1800000 -> 'gdt 0' */ 
pte_ptr[6][1] |= PG_P| PG_W | ((base_pfn+4) << 12);/* 1801000 -> 'gdt 1' */ 
pte_ptr[6][2] |= PG_P| PG_W | ((base_pfn+5) << 12);/* 1802000 -> 'gdt 2' */ 
pte_ptr[6][3] |= PG_P| PG_W | ((base_pfn+6) << 12);/* 1803000 -> 'gdt 3' */ 
pte_ptr[6][4] |= PG_P| PG_W | ((base_pfn+7) << 12);/* 1804000 -> 'gdt 4' */ 
pte_ptr[6][5] |= PG_P| PG_W | ((base_pfn+8) << 12);/* 1805000 -> 'gdt 5' */ 
pte_ptr[6][6] |= PG_P| PG_W | ((base_pfn+9) << 12);/* 1806000 -> 'gdt 6' */ 
pte_ptr[6][7] |= PG_P| PG_W | ((base_pfn+10) << 12);/* 1807000 -> 'gdt 7' */ 
pte_ptr[6][8] |= PG_P| PG_W | ((base_pfn+11) << 12);/* 1808000 -> 'gdt 8' */ 
pte_ptr[6][9] |= PG_P| PG_W | ((base_pfn+12) << 12);/* 1809000 -> 'gdt 9' */ 
pte_ptr[6][10] |= PG_P| PG_W | ((base_pfn+13) << 12);/* 180a000 -> 'gdt 10' */ 
pte_ptr[6][11] |= PG_P| PG_W | ((base_pfn+14) << 12);/* 180b000 -> 'gdt 11' */ 
pte_ptr[6][12] |= PG_P| PG_W | ((base_pfn+15) << 12);/* 180c000 -> 'gdt 12' */ 
pte_ptr[6][13] |= PG_P| PG_W | ((base_pfn+16) << 12);/* 180d000 -> 'gdt 13' */ 
pte_ptr[6][14] |= PG_P| PG_W | ((base_pfn+17) << 12);/* 180e000 -> 'gdt 14' */ 
pte_ptr[6][15] |= PG_P| PG_W | ((base_pfn+18) << 12);/* 180f000 -> 'gdt 15' */ 
pde_ptr[3] = PG_P | PG_PS | PG_U | PG_W | PG_A | PG_PS | (3 << 22); 
}
 /* dec_even : evencounter <- oddcounter , dec_odd , reset */ 
 
{ 
 u_int **pte_ptr = ALLOC_PTEPTR_ARRAY(); 
u_int *pde_ptr = PFN2VIRT((base_pfn+29)) /* pd dec_even */; int i; 
    for(i = 0; i< 1024; i++){
        pde_ptr[i] = PG_U | PG_A | PG_W;
    }
pte_ptr[0] = PFN2VIRT((base_pfn+31)) /* pt dec_even 0 */;
pde_ptr[0] |= PG_P| ((base_pfn+31) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[0][i] = PG_A| PG_U;
      }
pte_ptr[0][0] |= PG_P| PG_W | ((base_pfn+20) << 12);/* 0 -> 'stack_page' */ 
pte_ptr[0][1023] |= PG_P| PG_W | ((base_pfn+21) << 12);/* 3ff000 -> 'ins dec_odd' */ 
pte_ptr[1] = PFN2VIRT((base_pfn+32)) /* pt dec_even 1 */;
pde_ptr[1] |= PG_P| ((base_pfn+32) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[1][i] = PG_A| PG_U;
      }
pte_ptr[1][0] |= PG_P| PG_W | ((base_pfn+1) << 12);/* 400000 -> 'var evencounter' */ 
pte_ptr[1][15] |= PG_P| PG_W | ((base_pfn+4) << 12);/* 40f000 -> 'gdt 1' */ 
pte_ptr[1][16] |= PG_P| PG_W | ((base_pfn+1) << 12);/* 410000 -> 'var evencounter' */ 
pte_ptr[1][31] |= PG_P| PG_W | ((base_pfn+23) << 12);/* 41f000 -> 'ins reset' */ 
pte_ptr[1][32] |= PG_P| PG_W | ((base_pfn+0) << 12);/* 420000 -> 'var reset' */ 
pte_ptr[4] = PFN2VIRT((base_pfn+33)) /* pt dec_even 4 */;
pde_ptr[4] |= PG_P| ((base_pfn+33) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[4][i] = PG_A| PG_U;
      }
pte_ptr[4][0] |= PG_P| PG_W | ((base_pfn+30) << 12);/* 1000000 -> 'IDT dec_even' */ 
pte_ptr[6] = PFN2VIRT((base_pfn+34)) /* pt dec_even 6 */;
pde_ptr[6] |= PG_P| ((base_pfn+34) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[6][i] = PG_A| PG_U;
      }
pte_ptr[6][0] |= PG_P| PG_W | ((base_pfn+3) << 12);/* 1800000 -> 'gdt 0' */ 
pte_ptr[6][1] |= PG_P| PG_W | ((base_pfn+4) << 12);/* 1801000 -> 'gdt 1' */ 
pte_ptr[6][2] |= PG_P| PG_W | ((base_pfn+5) << 12);/* 1802000 -> 'gdt 2' */ 
pte_ptr[6][3] |= PG_P| PG_W | ((base_pfn+6) << 12);/* 1803000 -> 'gdt 3' */ 
pte_ptr[6][4] |= PG_P| PG_W | ((base_pfn+7) << 12);/* 1804000 -> 'gdt 4' */ 
pte_ptr[6][5] |= PG_P| PG_W | ((base_pfn+8) << 12);/* 1805000 -> 'gdt 5' */ 
pte_ptr[6][6] |= PG_P| PG_W | ((base_pfn+9) << 12);/* 1806000 -> 'gdt 6' */ 
pte_ptr[6][7] |= PG_P| PG_W | ((base_pfn+10) << 12);/* 1807000 -> 'gdt 7' */ 
pte_ptr[6][8] |= PG_P| PG_W | ((base_pfn+11) << 12);/* 1808000 -> 'gdt 8' */ 
pte_ptr[6][9] |= PG_P| PG_W | ((base_pfn+12) << 12);/* 1809000 -> 'gdt 9' */ 
pte_ptr[6][10] |= PG_P| PG_W | ((base_pfn+13) << 12);/* 180a000 -> 'gdt 10' */ 
pte_ptr[6][11] |= PG_P| PG_W | ((base_pfn+14) << 12);/* 180b000 -> 'gdt 11' */ 
pte_ptr[6][12] |= PG_P| PG_W | ((base_pfn+15) << 12);/* 180c000 -> 'gdt 12' */ 
pte_ptr[6][13] |= PG_P| PG_W | ((base_pfn+16) << 12);/* 180d000 -> 'gdt 13' */ 
pte_ptr[6][14] |= PG_P| PG_W | ((base_pfn+17) << 12);/* 180e000 -> 'gdt 14' */ 
pte_ptr[6][15] |= PG_P| PG_W | ((base_pfn+18) << 12);/* 180f000 -> 'gdt 15' */ 
pde_ptr[3] = PG_P | PG_PS | PG_U | PG_W | PG_A | PG_PS | (3 << 22); 
}
 /* reset : evencounter <- reset , dec_odd , dec_even */ 
 
{ 
 u_int **pte_ptr = ALLOC_PTEPTR_ARRAY(); 
u_int *pde_ptr = PFN2VIRT((base_pfn+35)) /* pd reset */; int i; 
    for(i = 0; i< 1024; i++){
        pde_ptr[i] = PG_U | PG_A | PG_W;
    }
pte_ptr[0] = PFN2VIRT((base_pfn+37)) /* pt reset 0 */;
pde_ptr[0] |= PG_P| ((base_pfn+37) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[0][i] = PG_A| PG_U;
      }
pte_ptr[0][0] |= PG_P| PG_W | ((base_pfn+20) << 12);/* 0 -> 'stack_page' */ 
pte_ptr[0][1023] |= PG_P| PG_W | ((base_pfn+21) << 12);/* 3ff000 -> 'ins dec_odd' */ 
pte_ptr[1] = PFN2VIRT((base_pfn+38)) /* pt reset 1 */;
pde_ptr[1] |= PG_P| ((base_pfn+38) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[1][i] = PG_A| PG_U;
      }
pte_ptr[1][0] |= PG_P| PG_W | ((base_pfn+1) << 12);/* 400000 -> 'var evencounter' */ 
pte_ptr[1][15] |= PG_P| PG_W | ((base_pfn+22) << 12);/* 40f000 -> 'ins dec_even' */ 
pte_ptr[1][16] |= PG_P| PG_W | ((base_pfn+2) << 12);/* 410000 -> 'var oddcounter' */ 
pte_ptr[1][31] |= PG_P| PG_W | ((base_pfn+5) << 12);/* 41f000 -> 'gdt 2' */ 
pte_ptr[1][32] |= PG_P| PG_W | ((base_pfn+1) << 12);/* 420000 -> 'var evencounter' */ 
pte_ptr[4] = PFN2VIRT((base_pfn+39)) /* pt reset 4 */;
pde_ptr[4] |= PG_P| ((base_pfn+39) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[4][i] = PG_A| PG_U;
      }
pte_ptr[4][0] |= PG_P| PG_W | ((base_pfn+36) << 12);/* 1000000 -> 'IDT reset' */ 
pte_ptr[6] = PFN2VIRT((base_pfn+40)) /* pt reset 6 */;
pde_ptr[6] |= PG_P| ((base_pfn+40) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[6][i] = PG_A| PG_U;
      }
pte_ptr[6][0] |= PG_P| PG_W | ((base_pfn+3) << 12);/* 1800000 -> 'gdt 0' */ 
pte_ptr[6][1] |= PG_P| PG_W | ((base_pfn+4) << 12);/* 1801000 -> 'gdt 1' */ 
pte_ptr[6][2] |= PG_P| PG_W | ((base_pfn+5) << 12);/* 1802000 -> 'gdt 2' */ 
pte_ptr[6][3] |= PG_P| PG_W | ((base_pfn+6) << 12);/* 1803000 -> 'gdt 3' */ 
pte_ptr[6][4] |= PG_P| PG_W | ((base_pfn+7) << 12);/* 1804000 -> 'gdt 4' */ 
pte_ptr[6][5] |= PG_P| PG_W | ((base_pfn+8) << 12);/* 1805000 -> 'gdt 5' */ 
pte_ptr[6][6] |= PG_P| PG_W | ((base_pfn+9) << 12);/* 1806000 -> 'gdt 6' */ 
pte_ptr[6][7] |= PG_P| PG_W | ((base_pfn+10) << 12);/* 1807000 -> 'gdt 7' */ 
pte_ptr[6][8] |= PG_P| PG_W | ((base_pfn+11) << 12);/* 1808000 -> 'gdt 8' */ 
pte_ptr[6][9] |= PG_P| PG_W | ((base_pfn+12) << 12);/* 1809000 -> 'gdt 9' */ 
pte_ptr[6][10] |= PG_P| PG_W | ((base_pfn+13) << 12);/* 180a000 -> 'gdt 10' */ 
pte_ptr[6][11] |= PG_P| PG_W | ((base_pfn+14) << 12);/* 180b000 -> 'gdt 11' */ 
pte_ptr[6][12] |= PG_P| PG_W | ((base_pfn+15) << 12);/* 180c000 -> 'gdt 12' */ 
pte_ptr[6][13] |= PG_P| PG_W | ((base_pfn+16) << 12);/* 180d000 -> 'gdt 13' */ 
pte_ptr[6][14] |= PG_P| PG_W | ((base_pfn+17) << 12);/* 180e000 -> 'gdt 14' */ 
pte_ptr[6][15] |= PG_P| PG_W | ((base_pfn+18) << 12);/* 180f000 -> 'gdt 15' */ 
pde_ptr[3] = PG_P | PG_PS | PG_U | PG_W | PG_A | PG_PS | (3 << 22); 
}
{ 
 u_int **pte_ptr = ALLOC_PTEPTR_ARRAY(); 
u_int *pde_ptr = PFN2VIRT((base_pfn+41)) /* pd initial_pd */; int i; 
    for(i = 0; i< 1024; i++){
        pde_ptr[i] = PG_U | PG_A | PG_W;
    }
pte_ptr[0] = PFN2VIRT((base_pfn+42)) /* pt initial_pd 0 */;
pde_ptr[0] |= PG_P| ((base_pfn+42) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[0][i] = PG_A| PG_U;
      }
pte_ptr[0][0] |= PG_P| PG_W | ((base_pfn+20) << 12);/* 0 -> 'stack_page' */ 
pte_ptr[0][1023] |= PG_P| PG_W | ((base_pfn+21) << 12);/* 3ff000 -> 'ins dec_odd' */ 
pte_ptr[1] = PFN2VIRT((base_pfn+43)) /* pt initial_pd 1 */;
pde_ptr[1] |= PG_P| ((base_pfn+43) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[1][i] = PG_A| PG_U;
      }
pte_ptr[1][0] |= PG_P| PG_W | ((base_pfn+1) << 12);/* 400000 -> 'var evencounter' */ 
pte_ptr[6] = PFN2VIRT((base_pfn+44)) /* pt initial_pd 6 */;
pde_ptr[6] |= PG_P| ((base_pfn+44) << 12);
      for(i=0; i<1024; i++){
        pte_ptr[6][i] = PG_A| PG_U;
      }
pte_ptr[6][0] |= PG_P| PG_W | ((base_pfn+3) << 12);/* 1800000 -> 'gdt 0' */ 
pte_ptr[6][1] |= PG_P| PG_W | ((base_pfn+4) << 12);/* 1801000 -> 'gdt 1' */ 
pte_ptr[6][2] |= PG_P| PG_W | ((base_pfn+5) << 12);/* 1802000 -> 'gdt 2' */ 
pte_ptr[6][3] |= PG_P| PG_W | ((base_pfn+6) << 12);/* 1803000 -> 'gdt 3' */ 
pte_ptr[6][4] |= PG_P| PG_W | ((base_pfn+7) << 12);/* 1804000 -> 'gdt 4' */ 
pte_ptr[6][5] |= PG_P| PG_W | ((base_pfn+8) << 12);/* 1805000 -> 'gdt 5' */ 
pte_ptr[6][6] |= PG_P| PG_W | ((base_pfn+9) << 12);/* 1806000 -> 'gdt 6' */ 
pte_ptr[6][7] |= PG_P| PG_W | ((base_pfn+10) << 12);/* 1807000 -> 'gdt 7' */ 
pte_ptr[6][8] |= PG_P| PG_W | ((base_pfn+11) << 12);/* 1808000 -> 'gdt 8' */ 
pte_ptr[6][9] |= PG_P| PG_W | ((base_pfn+12) << 12);/* 1809000 -> 'gdt 9' */ 
pte_ptr[6][10] |= PG_P| PG_W | ((base_pfn+13) << 12);/* 180a000 -> 'gdt 10' */ 
pte_ptr[6][11] |= PG_P| PG_W | ((base_pfn+14) << 12);/* 180b000 -> 'gdt 11' */ 
pte_ptr[6][12] |= PG_P| PG_W | ((base_pfn+15) << 12);/* 180c000 -> 'gdt 12' */ 
pte_ptr[6][13] |= PG_P| PG_W | ((base_pfn+16) << 12);/* 180d000 -> 'gdt 13' */ 
pte_ptr[6][14] |= PG_P| PG_W | ((base_pfn+17) << 12);/* 180e000 -> 'gdt 14' */ 
pte_ptr[6][15] |= PG_P| PG_W | ((base_pfn+18) << 12);/* 180f000 -> 'gdt 15' */ 
pde_ptr[3] = PG_P | PG_PS | PG_U | PG_W | PG_A | PG_PS | (3 << 22); 
}
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 8)))/* var reset + 8 */ = 20 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 24)))/* var reset + 24 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 28)))/* var reset + 28 */ = 0x8 /*CS*/ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 32)))/* var reset + 32 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 36)))/* var reset + 36 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 40)))/* var reset + 40 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 44)))/* var reset + 44 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+0) + 48)))/* var reset + 48 */ = 0x0 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 8)))/* var evencounter + 8 */ = 10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 24)))/* var evencounter + 24 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 28)))/* var evencounter + 28 */ = 0x8 /*CS*/ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 32)))/* var evencounter + 32 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 36)))/* var evencounter + 36 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 40)))/* var evencounter + 40 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 44)))/* var evencounter + 44 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+1) + 48)))/* var evencounter + 48 */ = 0x0 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 8)))/* var oddcounter + 8 */ = 18 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 24)))/* var oddcounter + 24 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 28)))/* var oddcounter + 28 */ = 0x8 /*CS*/ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 32)))/* var oddcounter + 32 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 36)))/* var oddcounter + 36 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 40)))/* var oddcounter + 40 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 44)))/* var oddcounter + 44 */ = 0x10 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+2) + 48)))/* var oddcounter + 48 */ = 0x0 ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 8)))/* gdt 0 + 8 */ = 65535 | ((0 & 0xFFFF) << 16) /* Base: 0 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 12)))/* gdt 0 + 12 */ = ((0 &0x00FF0000) >> 16) | (154 << 8)|(255) << 16 |( 0 & 0xFF000000) /* Type 154 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 16)))/* gdt 0 + 16 */ = 65535 | ((0 & 0xFFFF) << 16) /* Base: 0 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 20)))/* gdt 0 + 20 */ = ((0 &0x00FF0000) >> 16) | (146 << 8)|(255) << 16 |( 0 & 0xFF000000) /* Type 146 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 24)))/* gdt 0 + 24 */ = 255 | ((g_tss_ptr & 0xFFFF) << 16) /* Base: g_tss_ptr */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 28)))/* gdt 0 + 28 */ = ((g_tss_ptr &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( g_tss_ptr & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 4088)))/* gdt 0 + 4088 */ = 255 | ((4194256 & 0xFFFF) << 16) /* Base: 4194256 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+3) + 4092)))/* gdt 0 + 4092 */ = ((4194256 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4194256 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+4) + 4088)))/* gdt 1 + 4088 */ = 255 | ((4259792 & 0xFFFF) << 16) /* Base: 4259792 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+4) + 4092)))/* gdt 1 + 4092 */ = ((4259792 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4259792 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+5) + 4088)))/* gdt 2 + 4088 */ = 255 | ((4325328 & 0xFFFF) << 16) /* Base: 4325328 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+5) + 4092)))/* gdt 2 + 4092 */ = ((4325328 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4325328 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+6) + 4088)))/* gdt 3 + 4088 */ = 255 | ((4390864 & 0xFFFF) << 16) /* Base: 4390864 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+6) + 4092)))/* gdt 3 + 4092 */ = ((4390864 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4390864 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+7) + 4088)))/* gdt 4 + 4088 */ = 255 | ((4456400 & 0xFFFF) << 16) /* Base: 4456400 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+7) + 4092)))/* gdt 4 + 4092 */ = ((4456400 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4456400 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+8) + 4088)))/* gdt 5 + 4088 */ = 255 | ((4521936 & 0xFFFF) << 16) /* Base: 4521936 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+8) + 4092)))/* gdt 5 + 4092 */ = ((4521936 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4521936 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+9) + 4088)))/* gdt 6 + 4088 */ = 255 | ((4587472 & 0xFFFF) << 16) /* Base: 4587472 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+9) + 4092)))/* gdt 6 + 4092 */ = ((4587472 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4587472 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+10) + 4088)))/* gdt 7 + 4088 */ = 255 | ((4653008 & 0xFFFF) << 16) /* Base: 4653008 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+10) + 4092)))/* gdt 7 + 4092 */ = ((4653008 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4653008 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+11) + 4088)))/* gdt 8 + 4088 */ = 255 | ((4718544 & 0xFFFF) << 16) /* Base: 4718544 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+11) + 4092)))/* gdt 8 + 4092 */ = ((4718544 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4718544 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+12) + 4088)))/* gdt 9 + 4088 */ = 255 | ((4784080 & 0xFFFF) << 16) /* Base: 4784080 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+12) + 4092)))/* gdt 9 + 4092 */ = ((4784080 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4784080 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+13) + 4088)))/* gdt 10 + 4088 */ = 255 | ((4849616 & 0xFFFF) << 16) /* Base: 4849616 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+13) + 4092)))/* gdt 10 + 4092 */ = ((4849616 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4849616 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+14) + 4088)))/* gdt 11 + 4088 */ = 255 | ((4915152 & 0xFFFF) << 16) /* Base: 4915152 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+14) + 4092)))/* gdt 11 + 4092 */ = ((4915152 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4915152 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+15) + 4088)))/* gdt 12 + 4088 */ = 255 | ((4980688 & 0xFFFF) << 16) /* Base: 4980688 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+15) + 4092)))/* gdt 12 + 4092 */ = ((4980688 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 4980688 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+16) + 4088)))/* gdt 13 + 4088 */ = 255 | ((5046224 & 0xFFFF) << 16) /* Base: 5046224 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+16) + 4092)))/* gdt 13 + 4092 */ = ((5046224 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 5046224 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+17) + 4088)))/* gdt 14 + 4088 */ = 255 | ((5111760 & 0xFFFF) << 16) /* Base: 5111760 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+17) + 4092)))/* gdt 14 + 4092 */ = ((5111760 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 5111760 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+18) + 4088)))/* gdt 15 + 4088 */ = 255 | ((5177296 & 0xFFFF) << 16) /* Base: 5177296 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+18) + 4092)))/* gdt 15 + 4092 */ = ((5177296 &0x00FF0000) >> 16) | (137 << 8)|(192) << 16 |( 5177296 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+21) + 4076)))/* ins dec_odd + 4076 */ = (base_pfn+19) << 12 /*CR3: pd dec_odd */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+21) + 4080)))/* ins dec_odd + 4080 */ = 0xfffefff /*EIP */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+21) + 4084)))/* ins dec_odd + 4084 */ = reflags() ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+21) + 4088)))/* ins dec_odd + 4088 */ = 65535 | ((4194256 & 0xFFFF) << 16) /* Base: 4194256 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+21) + 4092)))/* ins dec_odd + 4092 */ = ((4194256 &0x00FF0000) >> 16) | (137 << 8)|(255) << 16 |( 4194256 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+22) + 4076)))/* ins dec_even + 4076 */ = (base_pfn+29) << 12 /*CR3: pd dec_even */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+22) + 4080)))/* ins dec_even + 4080 */ = 0xfffefff /*EIP */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+22) + 4084)))/* ins dec_even + 4084 */ = reflags() ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+22) + 4088)))/* ins dec_even + 4088 */ = 65535 | ((4259792 & 0xFFFF) << 16) /* Base: 4259792 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+22) + 4092)))/* ins dec_even + 4092 */ = ((4259792 &0x00FF0000) >> 16) | (137 << 8)|(255) << 16 |( 4259792 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+23) + 4076)))/* ins reset + 4076 */ = (base_pfn+35) << 12 /*CR3: pd reset */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+23) + 4080)))/* ins reset + 4080 */ = 0xfffefff /*EIP */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+23) + 4084)))/* ins reset + 4084 */ = reflags() ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+23) + 4088)))/* ins reset + 4088 */ = 65535 | ((4325328 & 0xFFFF) << 16) /* Base: 4325328 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+23) + 4092)))/* ins reset + 4092 */ = ((4325328 &0x00FF0000) >> 16) | (137 << 8)|(255) << 16 |( 4325328 & 0xFF000000) /* Type 137 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+24) + 64)))/* IDT dec_odd + 64 */ = 804782080/* TSS 0x2ff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+24) + 68)))/* IDT dec_odd + 68 */ = 58624/* Task gate */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+24) + 112)))/* IDT dec_odd + 112 */ = 536346624/* TSS 0x1ff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+24) + 116)))/* IDT dec_odd + 116 */ = 58624/* Task gate */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+30) + 64)))/* IDT dec_even + 64 */ = 804782080/* TSS 0x2ff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+30) + 68)))/* IDT dec_even + 68 */ = 58624/* Task gate */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+30) + 112)))/* IDT dec_even + 112 */ = 267911168/* TSS 0xff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+30) + 116)))/* IDT dec_even + 116 */ = 58624/* Task gate */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+36) + 64)))/* IDT reset + 64 */ = 536346624/* TSS 0x1ff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+36) + 68)))/* IDT reset + 68 */ = 58624/* Task gate */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+36) + 112)))/* IDT reset + 112 */ = 267911168/* TSS 0xff8 */ ;
*((u_int *)((char *)(PFN2VIRT(base_pfn+36) + 116)))/* IDT reset + 116 */ = 58624/* Task gate */ ;
    }
    void zero_memory()
    {
        int i;
        for(i=0;i<45;i++)
         memset((char *)(PFN2VIRT(base_pfn+i) ), 0,4096);
    }
    void begin_computation(){
      load_cr3((base_pfn+41) << 12); /* Begin the fun */
      __asm __volatile ("ljmp  $0xff8, $0x0");
}
/* Instructions 
ff8 dec_odd 13
1ff8 dec_even 1d
2ff8 reset 23
*/
/* Pages
0 var reset
1000 var evencounter
2000 var oddcounter
3000 gdt 0
4000 gdt 1
5000 gdt 2
6000 gdt 3
7000 gdt 4
8000 gdt 5
9000 gdt 6
a000 gdt 7
b000 gdt 8
c000 gdt 9
d000 gdt 10
e000 gdt 11
f000 gdt 12
10000 gdt 13
11000 gdt 14
12000 gdt 15
13000 pd dec_odd
14000 stack_page
15000 ins dec_odd
16000 ins dec_even
17000 ins reset
18000 IDT dec_odd
19000 pt dec_odd 0
1a000 pt dec_odd 1
1b000 pt dec_odd 4
1c000 pt dec_odd 6
1d000 pd dec_even
1e000 IDT dec_even
1f000 pt dec_even 0
20000 pt dec_even 1
21000 pt dec_even 4
22000 pt dec_even 6
23000 pd reset
24000 IDT reset
25000 pt reset 0
26000 pt reset 1
27000 pt reset 4
28000 pt reset 6
29000 pd initial_pd
2a000 pt initial_pd 0
2b000 pt initial_pd 1
2c000 pt initial_pd 6
*/