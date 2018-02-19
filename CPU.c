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
  struct trace_item noop = {.type = ti_NOP, .sReg_a = 255, .sReg_b = 255, .dReg = 255, .PC = 0, .Addr = 0};
  struct trace_item squash = {.type = ti_NOP, .sReg_a = 255, .sReg_b = 255, .dReg = 0, .PC = 0, .Addr = 0}; //difference for squashed is that dReg is 0
  
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
  struct trace_item* pipeline[7];
  //Changed by jen ####################################################################
  pipeline[0] = &noop;
  pipeline[1] = &noop;
  pipeline[2] = &noop;
  pipeline[3] = &noop;
  pipeline[4] = &noop;
  pipeline[5] = &noop;
  pipeline[6] = &noop;
  // ####################################################################
  
  int pipe_occupancy = 7;
  int prediction_method = 0;
  unsigned int cycle_number = 0;
  int stalled = NO_HAZ, squashed = NO_HAZ;
  int num_squash = 0;
  unsigned int bp_hash_table[128][2];	//should this be 64? i wanted to not have to change the size of the array, the bit shifting will allow only 64 hash locations
  unsigned int hash_table_size = 64;
  
  //Other variable declarations
  unsigned char tempWB, tempID;
  int data_haz_type = 0;
  int branch_taken;
  char index;  
  int i = 0, j = 0;

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character> <branch prediction method>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }
    
  trace_file_name = argv[1];
  if (argc == 4){
	  trace_view_on = atoi(argv[3]) ;
	  prediction_method = atoi(argv[2]);
  }
  else if (argc == 3) {
	  prediction_method = atoi(argv[2]);
  }
  
  
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  size = 1;
  

  while(1) {
	
    if (!size) {       /* no more instructions (trace_items) to simulate */
	  pipe_occupancy--;
	  tr_entry = &noop;
	  if (pipe_occupancy == 0) {
        printf("+ Simulation terminates at cycle : %u\n", cycle_number);
        break;
	  }
    }
    else if (stalled == NO_HAZ && squashed == NO_HAZ){              /* parse the next instruction to simulate */

		size = trace_get_item(&tr_entry);

    } 
	cycle_number++;	
	if(num_squash == 0)
		squashed = NO_HAZ;
	stalled = NO_HAZ;
	
	/**
	 **Check for hazards here. Suggested order is structural hazards then data hazards.
	 **If there are both, fixing only the data hazard should fix both, so setting it second
	 **gives it some higher priority.
	 **Branch is kinda weird because it doesn't stall really so that might end up being its
	 **own thing.
	 **/
	 
	 //structural
	 tempWB = pipeline[6]->type;
	 tempID = pipeline[2]->type;
	 
	 //check if the WB stage is writing to the register file: i.e. is an RType, IType, or Load instruction
	 //also check if the ID stage is reading from the register file: i.e. is an RType, Load, Store, Branch, JRType, or IType (as long as sReg_a is used) instruction
	 if ((tempWB == ti_RTYPE) || (tempWB == ti_ITYPE) || (tempWB == ti_LOAD))	
	 {
		 if((tempID == ti_RTYPE) || ((tempID == ti_ITYPE) && (pipeline[2]->sReg_a != 255)) || (tempID == ti_LOAD) 
		   || (tempID == ti_STORE) || (tempID == ti_BRANCH) || (tempID == ti_JRTYPE)) 
			{
			 stalled = STRUCT_HAZ;
			}
	 }
	 
	 //data
	 data_haz_type = 0;
	 
	 if (pipeline[3]->type == ti_LOAD) 
	 {
		 if (pipeline[2]->type == ti_RTYPE || pipeline[2]->type == ti_STORE || pipeline[2]->type == ti_BRANCH) 
		 {
			 if ((pipeline[3]->dReg == pipeline[2]->sReg_a) || (pipeline[3]->dReg == pipeline[2]->sReg_b)) 
			 {
				 stalled = DATA_HAZ;
				 data_haz_type = 0;
			 } 
		 } 
		 else if (pipeline[2]->type == ti_ITYPE || pipeline[2]->type == ti_LOAD || pipeline[2]->type == ti_JRTYPE)
		 { 
			 if (pipeline[3]->dReg == pipeline[2]->sReg_a) 
			 {
				 stalled = DATA_HAZ;
				 data_haz_type = 0;
			 }
		 }  
	 } 
	 
	 if (pipeline[4]->type == ti_LOAD) 
	 {
		 if (pipeline[2]->type == ti_RTYPE || pipeline[2]->type == ti_STORE || pipeline[2]->type == ti_BRANCH)
		 {	 
			 if ((pipeline[4]->dReg == pipeline[2]->sReg_a) || (pipeline[4]->dReg == pipeline[2]->sReg_b)) 
			 {
				 stalled = DATA_HAZ;
				 data_haz_type = 1;
			 } 
		 } 
		 else if (pipeline[2]->type == ti_ITYPE || pipeline[2]->type == ti_LOAD || pipeline[2]->type == ti_JRTYPE) 
		 {			 
			 if (pipeline[4]->dReg == pipeline[2]->sReg_a) {
				 stalled = DATA_HAZ;
				 data_haz_type = 1;
			 }
		 } 
	 } 

	 //printf("cont hazard %d", squashed);
	 
	 //branch
     if (pipeline[0]->type == ti_BRANCH) {
	   branch_taken = pipeline[0]->Addr == tr_entry->PC;
	   index = (pipeline[0]->Addr >> 3 ) % hash_table_size; //if you want bits 8-3 should this be left 24 and right 27? probably...gonna use mod to pick bits now
	   if (prediction_method == 0) {
		   
		   if (pipeline[0]->Addr == tr_entry->PC) {//can this condition just be if(branch_taken).  Yes, i think its form before i made the 'branch_taken' variable
			   squashed = CONT_HAZ;
			   num_squash = 3;
		   }
	   }
	   else if (prediction_method == 1) {
		   if (bp_hash_table[index][0] - pipeline[0]->Addr == 0) {
			   if (branch_taken != bp_hash_table[index][1]) {
				   squashed = CONT_HAZ;
				   num_squash = 3;
				   bp_hash_table[index][1] = branch_taken;
				   //printf("branchhaz 1.0");
			   }
	       }
		   else {
			   bp_hash_table[index][0] = pipeline[0]->Addr;
			   bp_hash_table[index][1] = branch_taken;
			   if (branch_taken) {
				   squashed = CONT_HAZ;
				   num_squash = 3;			
				   //printf("branchhaz 1.1");	   
			   }
		   }
		   
	   }
	   else if (prediction_method == 2) {
		   if (bp_hash_table[index][0] == pipeline[0]->Addr) {
			   if (bp_hash_table[index][1] == 0) {
				   if (branch_taken) {
					   bp_hash_table[index][1] = 1;
					   squashed = CONT_HAZ;
					   num_squash = 3;
				   }
				}
				else if (bp_hash_table[index][1] == 1) {
					if (branch_taken) {
						bp_hash_table[index][1] = 3;
						squashed = CONT_HAZ;
						num_squash = 3;
					}
					else  bp_hash_table[index][1] = 0;
				}
				else if (bp_hash_table[index][1] == 2) {
					if (!branch_taken) {
						bp_hash_table[index][1] = 0;
						squashed = CONT_HAZ;
						num_squash = 3;
					}
					else bp_hash_table[index][1] = 3;
				}
				else if (bp_hash_table[index][1] == 3) {
					if (!branch_taken) {
						bp_hash_table[index][1] = 2;
						squashed = CONT_HAZ;
						num_squash = 3;
					}
				}
		   }
		   else {
			   bp_hash_table[index][0] = pipeline[0]->Addr;
			   bp_hash_table[index][1] = branch_taken*3;
			   if (branch_taken) {
				   squashed = CONT_HAZ;
				   num_squash = 3;				   
			   }			   
		   }
	   }
	 
	 }
	 	 //printf("cont hazard %d", squashed);


	

// SIMULATION OF A SINGLE CYCLE cpu IS TRIVIAL - EACH INSTRUCTION IS EXECUTED
// IN ONE CYCLE

    /**
	--From the assignment (page 3)
	The project is to replace the simple single cycle simulation with a simulation of the 7-stages pipeline 
	which will also output the total number of execution cycles as well as the instruction that exits the pipeline 
	in each cycle (if the switch trace_view_on is set to 1). 
	*/
	
    if (trace_view_on) {/* print the executed instruction if trace_view_on=1	*/
	  if(pipeline[6] != NULL)
	  {
		  switch(pipeline[6]->type) {
			case ti_NOP:
			  if(pipeline[6]->dReg == 255) {
				printf("[cycle %d] NOP\n",cycle_number) ;
			  }
			  else printf("[cycle %d] SQUASHED\n", cycle_number) ;
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
    }
    
    //printf("\n haz on last inst: %d", stalled);
	
	/*
	//print pipeline, DEBUGGING	
	printf("\n[cycle %d]\n ",cycle_number) ;	
	for (j = 0; j <= 6; j++)
	{
		switch(pipeline[j]->type) {
        case ti_NOP:
         if(pipeline[j]->dReg == 255) {
			printf("[cycle %d] NOP\n",cycle_number) ;
			}
		 else printf("[cycle %d] SQUASHED\n", cycle_number) ;
	     break;
        case ti_RTYPE:
          printf("RTYPE:") ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", pipeline[j]->PC, pipeline[j]->sReg_a, pipeline[j]->sReg_b, pipeline[j]->dReg);
          break;
        case ti_ITYPE:
          printf("ITYPE:") ;
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[j]->PC, pipeline[j]->sReg_a, pipeline[j]->dReg, pipeline[j]->Addr);
          break;
        case ti_LOAD:
          printf("LOAD:") ;      
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[j]->PC, pipeline[j]->sReg_a, pipeline[j]->dReg, pipeline[j]->Addr);
          break;
        case ti_STORE:
          printf("STORE:") ;      
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[j]->PC, pipeline[j]->sReg_a, pipeline[j]->sReg_b, pipeline[j]->Addr);
          break;
        case ti_BRANCH:
          printf("BRANCH:") ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[j]->PC, pipeline[j]->sReg_a, pipeline[j]->sReg_b, pipeline[j]->Addr);
          break;
        case ti_JTYPE:
          printf("JTYPE:") ;
          printf(" (PC: %x)(addr: %x)\n", pipeline[j]->PC,pipeline[j]->Addr);
          break;
        case ti_SPECIAL:
          printf("SPECIAL:\n") ;      	
          break;
        case ti_JRTYPE:
          printf("JRTYPE:") ;
          printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", pipeline[j]->PC, pipeline[j]->dReg, pipeline[j]->Addr);
          break;
		}
	}
	*/
	//Pipeline advancing loop
	for (i = 6; i >= 1; i = i - 1)
	{
		/** 
		 **insert no-ops as needed below
		 **/
		if ((i == 3)) {        /*hazard with writing to the register file*/
			if(stalled == STRUCT_HAZ)
			{
				pipeline[3] = &noop;
				break;
			}
			else if(stalled == DATA_HAZ && data_haz_type == 0)
			{
				pipeline[3] = &noop;
				break;
			}
		}
		
		else if ((i==4) && (stalled == DATA_HAZ)) {	/*hazard when instruction expects data that is still being loaded*/
			if (data_haz_type == 1) {
				pipeline[4] = &noop;
				break;
			}
		}

		pipeline[i] = pipeline[i - 1];
	}
	if(squashed == CONT_HAZ) {	/*hazard when branches are incorrectly predicted*/
		pipeline[0] = &squash;
		num_squash--;
	}
	else if(!stalled) {
		pipeline[0] = tr_entry;
	}
	//printf("Current instruction: Type: %d (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", pipeline[0]->type, pipeline[0]->PC, pipeline[0]->sReg_a, pipeline[0]->sReg_b, pipeline[0]->dReg);
  }

  trace_uninit();

  exit(0);
}


