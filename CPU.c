/**************************************************************/
/* CS/COE 1541				 			
   just compile with gcc -o pipeline pipeline.c			
   and execute using							
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0  
***************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "CPU.h" 

enum hazard_type {
	NO_HAZ = 0,
	STRUCT_HAZ,
	DATA_HAZ,
	CONT_HAZ
};

enum pipeline_stage {
	IF1 = 0,
	IF2,
	ID,
	EX,
	MEM1,
	MEM2,
	WB
};

int main(int argc, char **argv)
{
  struct trace_item *tr_entry;
  size_t size = 0;
  char *trace_file_name;
  int trace_view_on = 0;
  
  //constant noop that can be used to insert when stalling
  const struct trace_item *noop;
  noop->type = NOP;
  
  unsigned char t_type = 0;
  unsigned char t_sReg_a= 0;
  unsigned char t_sReg_b= 0;
  unsigned char t_dReg= 0;
  unsigned int t_PC = 0;
  unsigned int t_Addr = 0;
  
  /**Array of trace items that represents the pipeline 
   **Each index represents a stage:
   ** 0     1    2    3     4      5     6
   **IF1 | IF2 | ID | EX | MEM1 | MEM2 | WB
   **/
  struct trace_item pipeline[7];
  int pipe_occupancy = 7;
  int prediction_method = 0;
  unsigned int cycle_number = 0;
  hazard_type stalled, squashed;
  
  int i = 0, j = 0;

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character> <branch prediction method>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }
    
  trace_file_name = argv[1];
  if (argc == 4){
	  trace_view_on = atoi(argv[2]) ;
	  prediction_method = atoi(argv[3]);
  }
  else if (arc == 3) {
	  trace_view_on = atoi(argv[2]);
  }
  
  
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  size = trace_get_item(&tr_entry);

  while(1) {
   
    if (!size) {       /* no more instructions (trace_items) to simulate */
	  pipe_occupancy--;
	  if (pipe_occupancy == 0) {
        printf("+ Simulation terminates at cycle : %u\n", cycle_number);
        break;
	  }
    }
    else if (!stalled && !squashed){              /* parse the next instruction to simulate */
      cycle_number++;
      t_type = tr_entry->type;
      t_sReg_a = tr_entry->sReg_a;
      t_sReg_b = tr_entry->sReg_b;
      t_dReg = tr_entry->dReg;
      t_PC = tr_entry->PC;
      t_Addr = tr_entry->Addr;
	  
	  //get the next trace item so the branch code can check if it should insert squashes or not:
	  size = trace_get_item(&tr_entry);
    }  
	
	
	
	/**
	 **Check for hazards here. Suggested order is structural hazards then data hazards.
	 **If there are both, fixing only the data hazard should fix both, so setting it second
	 **gives it some higher priority.
	 **Branch is kinda weird because it doesn't stall really so that might end up being its
	 **own thing.
	 **/
	 
	 //structural
	 unsigned char tempWB = pipeline[6]->type;
	 unsigned char tempID = pipeline[1]->type;
	 
	 //check if the WB stage is writing to the register file: i.e. is an RType, IType, or Load instruction
	 //also check if the ID stage is reading from the register file: i.e. is an RType, Load, Store, Branch, JRType, or IType (as long as sReg_a is used) instruction
	 if ((tempWB == RTYPE) || (tempWB == ITYPE) || (tempWB == LOAD))	
		 if((tempID == RTYPE) || ((tempID == ITYPE) && (pipeline[1]->sReg_a != 255)) || (tempID == LOAD) 
		   || (tempID == STORE) || (tempID == BRANCH) || (tempID == JRTYPE))
			 stalled = STRUCT_HAZ;
	 
	 
	 //data
	 
	 
	 
	 
	 
	 
	 
	 //branch
     if (pipeline[0]->type == ti_BRANCH) {

	   if (prediction_method == 0) {
		   
		   if (pipeline[0]->Addr != tr_entry->PC) {
			   squashed = CONT_HAZ;
		   }
	   }
	   else if (prediction_method == 1) {
		   
	   }
	   else if (prediction_method == 2) {
		   
	   }
	 
	 }
	 
	 
	 
	

// SIMULATION OF A SINGLE CYCLE cpu IS TRIVIAL - EACH INSTRUCTION IS EXECUTED
// IN ONE CYCLE

    /**
	--From the assignment (page 3)
	The project is to replace the simple single cycle simulation with a simulation of the 7-stages pipeline 
	which will also output the total number of execution cycles as well as the instruction that exits the pipeline 
	in each cycle (if the switch trace_view_on is set to 1). 
	*/
    if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
      switch(pipeline[6]->type) {
        case ti_NOP:
          printf("[cycle %d] NOP\n:",cycle_number) ;
          break;
        case ti_RTYPE:
          printf("[cycle %d] RTYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", pipeline[6]->PC, pipeline[6]->sReg_a, pipeline[6]->sReg_b, pipeline[6]->dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[6]->PC, pipeline[6]->sReg_a, pipeline[6]->dReg, pipeline[6]->Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[6]->PC, pipeline[6]->sReg_a, pipeline[6]->dReg, pipeline[6]->Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[6]->PC, pipeline[6]->sReg_a, pipeline[6]->sReg_b, pipeline[6]->Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[6]->PC, pipeline[6]->sReg_a, pipeline[6]->sReg_b, pipeline[6]->Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
          printf(" (PC: %x)(addr: %x)\n", pipeline[6]->PC,pipeline[6]->Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
          printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", pipeline[6]->PC, pipeline[6]->dReg, pipeline[6]->Addr);
          break;
      }
    }
	
	
	//Pipeline advancing loop
	int i;
	for (i = 6; i >= 1; i = i - 1)
	{
		/** 
		 **insert no-ops as needed below
		 **/
		if ((i == 3) && (stalled == STRUCT_HAZ)) {        /*hazard with writing to the register file*/
			pipeline[3] = noop;
			break;
		}
		else if (stalled == DATA_HAZ) {     /*hazard when instruction expects data that is still being loaded*/
			
			
		}
		if (squashed == CONT_HAZ) {     /*hazard when branches are incorrectly predicted*/
			
			pipeline[0] = noop;
			pipeline[1] = noop;
			pipeline[2] = noop;
			break;
		}
		pipeline[i] = pipeline[i - 1];
	}
	
	pipeline[0] = tr_entry;
  }

  trace_uninit();

  exit(0);
}


