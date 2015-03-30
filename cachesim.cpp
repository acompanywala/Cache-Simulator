#include "cachesim.hpp"
#include <iostream>
#include <string>
#include "sstream"
#include "fstream"
#include <stdint.h> 
#include"math.h"
using namespace std;

/* Global Variables */
uint64_t C,B,S,V,K;                                                // Define all the Parameters
uint64_t block, sets, nsets, rowsize, dindex, blockadddecimal;     // Define block size, no of blocks, no of rows, decimal index, block decimal index
uint64_t** tagary;                                                 // Define L1 Tag store
uint64_t** validary;						   // Define L1 Valid bit array
uint64_t** dirtyary;                                               // Define L1 dirty bit array
uint64_t** timeary;                                                // Define L1 LRU counter
uint64_t** usefulprefetch;					   // Define L1 Prefetch bit array
uint64_t** victimtagary;					   // Define VC Tag store
uint64_t* victimdirtyary;					   // Define VC Dirty bit array	
uint64_t* victimvalidary;					   // Define VC valid bit array
uint64_t* victimusefulprefetch;					   // Define VC Prefetch bit array
double vcmissrate;						   // To store VC miss rate
uint64_t XB, lma, pcounter;				   // Current miss block address, last missed block address, counter for first two accesses
int64_t PS, d;						   // Pending Stride and Current Stride


void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) 
{
	B=b;                                    // Offset Bits
	S=s;                                    // L1 Sets Bits
	C=c;					// L1 Cache size Bits 
	V=v;       				// VC size Bits
	K=k;					// Depth of Prefetcher
	block = pow(2,B);             		// Size of block
	sets=pow(2,S);                		// no of blocks in a set/ no of ways
	nsets=pow(2,C)/(block*sets);  		// no of sets
	rowsize = block * sets;                 // no of bytes in a set
	
	vcmissrate=0;	
	XB=0;
	lma=0; 
	pcounter=0;
	PS=0;
	d=0;
	tagary = new uint64_t*[nsets];      	// tag store array  
	validary = new uint64_t*[nsets];   	// valid bit array
	dirtyary = new uint64_t*[nsets];   	// dirty bit array
	timeary = new uint64_t*[nsets];     	// time stamp array
	usefulprefetch = new uint64_t*[nsets];  // prefetch bit                   

	victimtagary = new uint64_t*[V];     	// victim tag store
	victimvalidary = new uint64_t[V];    	// victim valid bit
	victimdirtyary = new uint64_t[V];    	// victim dirty bit
	victimusefulprefetch=new uint64_t[V];   // victim prefetch bit

	/*initialize tag store array, valid bit array, time stamp array, dirty bit & prefetch array to 2D*/
	for(uint64_t i = 0; i < nsets; ++i)
	{    
		tagary[i] = new uint64_t[sets];
		validary[i] = new uint64_t[sets];
		timeary[i] = new uint64_t[sets];
		dirtyary[i] = new uint64_t[sets];
		usefulprefetch[i] = new uint64_t[sets];
	
	}

	/*initialize the tag store array, valid bit array, time stamp array, dirty bit array & prefetch array to zero*/
	for(uint64_t i=0; i < nsets; i++)
	{
		for(uint64_t j=0; j < sets; j++)
		{
			tagary[i][j]=0;
			validary[i][j]=0;
			timeary[i][j]=0;
			dirtyary[i][j]=0;
			usefulprefetch[i][j]=0;
		}
	}
	/*initialize victim tag array to 2D*/
	for(uint64_t i = 0; i < V; i++)
	{    
		victimtagary[i] = new uint64_t[2];
	}
	
	/*initialize victim dirty array, victim valid array and victim prefetch to zero*/
	for(uint64_t i = 0; i < V; i++)
	{    
		victimdirtyary[i]=0;
		victimusefulprefetch[i]=0;
		victimvalidary[i]=0;
	}

	/*initialize the victim tag array to zero*/
	for(uint64_t i=0; i < V; i++)
	{
		for(uint64_t j=0; j < 2; j++)
		{
			victimtagary[i][j]=0;
		}
	}

} //void setup cache ends 


void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) 
{

	/*Find wether the access is a read or a write*/
	if(rw == 'r')
	{
		p_stats->reads++;
		p_stats->accesses++;
	}
	else     
	{ 
		p_stats->writes++;
		p_stats->accesses++;
	}    


	uint64_t currentadd = address;                    // take address from the trace file and store in 'n'

	/*Convert the address from decimal to binary*/
	uint64_t binaryValue[100];              	// to store binary Value of address
	for(int i=0;i<100;i++)                    	// Initialize binaryValue to zero
	{
		binaryValue[i]=0;
	}

	int tagsize = 0;                               // Counter to convert address from decimal to binary (equal to tagsize)
	for(int i=0;currentadd!=0;i++) 
	{
		if (currentadd%2==0)
		{
			currentadd=currentadd/2;
			binaryValue[i]=0;
			tagsize++;                     
		}
		else
		{
			currentadd=currentadd/2;
			binaryValue[i]=1;
			tagsize++;
		}
	}

	/*Split the offset in binary*/

	uint64_t offset[100];

	for(int i=0;i<100;i++)                     // Initialize offset to zero
	{
		offset[i]=0;
	}

	for (uint64_t i=0;i<B;i++)                 // Split the offset
	{
		offset[i]=binaryValue[i];          
	}

	uint64_t binminusoffset[100];             // Array to store block address in binary

	for(int i=0;i<100;i++) 			  // Initialize binminusoffset to zero
	{
		binminusoffset[i]=0;		  
	}

	for (uint64_t i=0;i<tagsize;i++)          // Calculate the block address in binary
	{ 
		binminusoffset[i]= binaryValue[i] - offset[i];
	}

	int z=0;
	blockadddecimal=0;

	for(uint64_t i=0;i<tagsize;i++)           // Convert the block address to decimal
	{
		blockadddecimal+=binminusoffset[i]*(pow(2,z));
		z++;
	}

	/*Split the index in binary*/

	uint64_t index[100];
	for(int i=0;i<100;i++)                    // Initilaize index to zero
	{
		index[i]=0;
	}
	uint64_t x = C-S;

	for (int i=B; i<x ;i++)                   // Calculate index in binary
	{
		index[i]= binaryValue[i];
	}

	/*Split the tag in decimal*/

	uint64_t tag= address/(block*nsets);     // Calculate the tag in decimal 

	/*Convert the index to decimal*/

	int y=0;
	dindex=0;

	for(uint64_t i=B;i<x;i++)
	{
		dindex+=index[i]*(pow(2,y));     // Calculate the index in decimal
		y++;
	}

	/*Place the address in the tag store*/

	bool flag= false;		// flag for checking vc hit
	bool flag1=false;		// flag for checking empty space in L1
	bool flag2=false;		// flag for checking empty space in VC for L1 LRU block
	bool flag3=false;		// flag for performing FIFO in VC for making place for LRU block 
	bool flag4=false;		// flag to check for prefetching on a miss
	uint64_t swap=0;

	/*Check Whether its a hit in L1 Cache*/
	for(uint64_t i=0;i<sets;i++)
	{
		if(tag==tagary[dindex][i] && validary[dindex][i]==1)  // enter loop if a hit
		{
			validary[dindex][i]=1;			 // set valid bit
			if(usefulprefetch[dindex][i]==1)         // if prefetch bit set increment useful prefetch
			{
				p_stats->useful_prefetches++;
			}
			usefulprefetch[dindex][i]=0;             // set prefetch bit to zero
			flag=true;                               
			flag1=true;                              
			flag2=true;				 
			flag3=true;				 
			flag4=true;				 
			if(rw == 'w')				 // set dirty bit if access is a write
			{
				dirtyary[dindex][i]=1;
			}
			else if(rw == 'r' && dirtyary[dindex][i]!=1) // if data already in memory is dirty then dont reset the dirty bit  
			{					     // even if the incoming request is just a read
				dirtyary[dindex][i]=0;
			}                      				 	
			
			/*Update the time stamp*/
			timeary[dindex][i]=0;                      
			for(uint64_t x=0;x<i;x++)
			{
				timeary[dindex][x]+=1;			
			}		
			for(uint64_t x=i+1;x<sets;x++)
			{
				timeary[dindex][x]+=1; 		
			}		
			break;
		}	
	}

	bool prefetch=false;                                        // flag for the first access
	bool prefetch1=false;					    // flag for the second access
	bool prefetch2=false;					    // flag for the third access

	if(flag==false)                         // Enter loop if a miss in L1 cache
	{
		p_stats->misses++;      	// Increment total misses by 1
    
		if(rw == 'r')
		{
			p_stats->read_misses++;          // Update the read misses
		}
		else
		{
			p_stats->write_misses++;         // Update the write misses
		}

		if(pcounter==0)                          // set the pre-prefetching conditions for first access
		{
			lma=blockadddecimal;
			pcounter++;
			prefetch=true;
			prefetch1=true;
			prefetch2=true;
		}

		if(pcounter==1 && prefetch==false)	 // set the pre-prefetching conditions for second access
		{
			XB=blockadddecimal;
			d= XB-lma;
			pcounter++;
			prefetch1=true;
			prefetch2=true;
		}

		if(pcounter>1 && prefetch==false && prefetch1==false)	// set the pre-prefetching conditions for all accesses above 2
		{
			lma=XB;
			XB=blockadddecimal;
			PS=d;
			d=XB-lma;
		} 

		
		for(uint64_t q=0; q<V; q++)
		{
			if(tag == victimtagary[q][0] && dindex== victimtagary[q][1] && victimvalidary[q]==1) // check for a VC hit
			{ 
				flag1=true;
				flag2=true;
				flag3=true;
				
				uint64_t temp1 = 0;
				uint64_t u1 =0;
				
				for(uint64_t a=0;a<sets;a++)			// find LRU block in L1 cache
    				{
        				if(timeary[dindex][a]>temp1)
        				{
						temp1=timeary[dindex][a];
						u1=a;
					}
				}
				/*Swap L1 LRU block with the hit in VC */
				victimtagary[q][0]=tagary[dindex][u1];          
				victimtagary[q][1]=dindex;
				swap=victimdirtyary[q];
				victimdirtyary[q]=dirtyary[dindex][u1];
				victimvalidary[q]=1;
		
				if(victimusefulprefetch[q]==1)    // if the prefetch bit set for the VC hit block increment useful prefetch
				{
					p_stats->useful_prefetches++;
				}
					
				victimusefulprefetch[q]=usefulprefetch[dindex][u1];
				usefulprefetch[dindex][u1]=0;	 // set prefetch bit zero in L1 cache
				tagary[dindex][u1]=tag;
				dirtyary[dindex][u1]=swap;
				validary[dindex][u1]=1;
					
				if(rw == 'w')			// check wether the access is a write
				{	
					dirtyary[dindex][u1]=1;
				}
				else if(rw == 'r' && dirtyary[dindex][u1]!=1) // if data already in memory is dirty then dont reset the dirty bit even if the incoming request is a read
				{
					dirtyary[dindex][u1]=0;
				}
				/*Update timestamp*/	
				timeary[dindex][u1]=0;	
				for(uint64_t x=0;x<u1;x++)
				{
					timeary[dindex][x]+=1;			
				}		
				for(uint64_t x=u1+1;x<sets;x++)
				{
					timeary[dindex][x]+=1; 		
				}
					
				break;
			}
		}
	}

	/*Enter this Loop if not a VC hit*/
	
	if(flag1==false)
	{
		p_stats->vc_misses++;	   // Update VC misses	

		if(rw == 'r')
		{
			p_stats->read_misses_combined++;          // Update the read misses combined
		}
		else
		{
			p_stats->write_misses_combined++;         // Update the write misses combined
		}
		
		
		for(uint64_t i=0;i<sets;i++)
		{
			if(tagary[dindex][i]==0 && validary[dindex][i]==0) // Check for empty space in L1 cache
			{
				/*Place the block in the empty space*/				
				tagary[dindex][i]=tag;
				validary[dindex][i]=1;
			
				if(rw == 'w')				// check wether the access is a write
				{
					dirtyary[dindex][i]=1;
				}
				else					// check wether the access is a read
				{
					dirtyary[dindex][i]=0;
				}
				/*Update Timestamp*/
				timeary[dindex][i]=0;		
				for(uint64_t x=0;x<i;x++)
				{
					timeary[dindex][x]+=1;			
				}		
				for(uint64_t x=i+1;x<sets;x++)
				{
		 			timeary[dindex][x]+=1; 
				}
				
				flag2=true;
				flag3=true;
				break;	
			}
		}
	}

	if(flag2==false)	// Enter this loop if no empty space in L1 Cache
	{
		uint64_t temp = 0;
		uint64_t u =0;
		/*Find L1 LRU block*/
		for(uint64_t i=0;i<sets;i++)
    		{
        		if(timeary[dindex][i]>temp)
        		{
				temp=timeary[dindex][i];
				u=i;
			}
		}
		if(V!=0)                                 // Enter this loop only if victim cache is present
		{
			for(uint64_t y=0; y<V; y++)
			{
				if(victimtagary[y][0]==0 && victimtagary[y][1]==0 && victimvalidary[y]==0) // Check if empty space is avalable in VC	
				{
					/*Put L1 LRU in empty space in VC*/
					victimtagary[y][0]=tagary[dindex][u];		
					victimtagary[y][1]=dindex;
					victimdirtyary[y]=dirtyary[dindex][u];
					victimvalidary[y]=1;
					victimusefulprefetch[y]=usefulprefetch[dindex][u];
					/*Put the incoming data in L1 cache*/
					tagary[dindex][u]=tag;
					usefulprefetch[dindex][u]=0;
					validary[dindex][u]=1;
					
					flag3=true;
		
					if(rw == 'w')				// check wether the access is a write
					{
						dirtyary[dindex][u]=1;
					}
					else					// check wether the access is a read
					{
						dirtyary[dindex][u]=0;
					}
					/*Update Timestamp*/
					timeary[dindex][u]=0;				
					for(uint64_t x=0;x<u;x++)
					{
						timeary[dindex][x]+=1;			
					}		
					for(uint64_t x=u+1;x<sets;x++)
					{
						timeary[dindex][x]+=1; 
					}
				
					break;
				}		
			}
		}
	}

	if(flag3==false) // Enter this loop if no empty space is found in VC
	{
		uint64_t temp = 0;
		uint64_t u =0;
		/*Find L1 LRU block*/
		for(uint64_t i=0;i<sets;i++)		
    		{
        		if(timeary[dindex][i]>temp)
        		{
				temp=timeary[dindex][i];
				u=i;
			}
		}

		if(V==0)     // Enter this loop if there is no VC
		{
			if(dirtyary[dindex][u]==1)	// If dirty bit of L1 LRU block is set perform writeback
			{
				p_stats->write_backs++;
			}
		}

		if(V!=0)	// Enter this loop if VC is present
		{
			if(victimdirtyary[0]==1)        // If dirty bit of FIFO block is set in VC perform writeback
			{
				p_stats->write_backs++;
			}	
		}
		if(V!=0)	// Enter this loop if VC is present
		{
			for(uint64_t ab=0; ab<(V-1); ab++)		// Perform FIFO sorting in VC
			{
				victimtagary[ab][0]=victimtagary[ab+1][0];
				victimtagary[ab][1]=victimtagary[ab+1][1];
				victimdirtyary[ab]=victimdirtyary[ab+1];
				victimusefulprefetch[ab]=victimusefulprefetch[ab+1];
			}
			/*Put LRU block in VC*/
			victimtagary[V-1][0]=tagary[dindex][u];
			victimtagary[V-1][1]=dindex;
			victimdirtyary[V-1]=dirtyary[dindex][u];
			victimusefulprefetch[V-1]=usefulprefetch[dindex][u];
			victimvalidary[V-1]=1;
		}
		/*Put incoming block in L1 cache*/
		tagary[dindex][u]=tag;
		usefulprefetch[dindex][u]=0;
		validary[dindex][u]=1;
	
		if(rw == 'w')				// check wether the access is a write
		{
			dirtyary[dindex][u]=1;
		}
		else					// check wether the access is a read
		{
			dirtyary[dindex][u]=0;
		}
		/*Update Timestamp*/	
		timeary[dindex][u]=0;				
		for(uint64_t x=0;x<u;x++)
		{
			timeary[dindex][x]+=1;			
		}		
		for(uint64_t x=u+1;x<sets;x++)
		{
			timeary[dindex][x]+=1; 
		}
	}

	uint64_t preadd=0;		// Address of the block to be prefetched in decimal
	uint64_t xbtag=0;		// tag of the block to be prefetched in decimal
	uint64_t xbtagindex=0;		// tag plus index of the block to be prefetched in decimal
	uint64_t xbindex=0;		// index of the block to be prefetched in decimal
	
	if(flag4==false && prefetch2==false)	// Enter this loop after first two miss accesses
	{

		if(PS==d)			// Check if Pending stride is equal to Current stride and if yes then prefetch
		{
			for(int kk=1; kk<=K; kk++)
			{
				p_stats->prefetched_blocks++;   // increment the prefetch block counter
				preadd=XB+(kk*d);
				xbtag= preadd>>(C-S);
				xbtagindex= preadd>>B;
				xbindex=xbtagindex<<(64-C+S+B);
				xbindex=xbindex>>(64-C+S+B);
				bool flag5=false;		// flag for checking vc hit
				bool flag6=false;		// flag for checking empty space in L1
				bool flag7=false;		// flag for checking empty space in VC for L1 LRU block
				bool flag8=false;		// flag for performing FIFO in VC for making place for LRU block
				
		
				for(uint64_t i=0;i<sets;i++)
				{
					if(xbtag==tagary[xbindex][i] && validary[xbindex][i]==1) // Check for hit in L1 cache
					{
						flag5=true;
						flag6=true;
						flag7=true;
						flag8=true;

						break;
					} 
				}
				
				if(flag5==false)   // Enter this loop if it is not a hit in L1 cache
				{
					for(uint64_t q=0; q<V; q++)
					{
						if(xbtag == victimtagary[q][0] && xbindex== victimtagary[q][1] && victimvalidary[q]==1) //Check if its a VC hit
						{ 
							flag6=true;
							flag7=true;
							flag8=true;
		
							uint64_t temp2 = 0;
							uint64_t u2 =0;
							/*Find the L1 LRU Block*/
							for(uint64_t a=0;a<sets;a++)
    							{
        							if(timeary[xbindex][a]>temp2)
        							{
									temp2=timeary[xbindex][a];
									u2=a;
								}
							}
							/*Put the L1 LRU Block in VC*/
							victimtagary[q][0]=tagary[xbindex][u2];
							victimtagary[q][1]=xbindex;
							victimvalidary[q]=1;
	
							

							victimusefulprefetch[q]=usefulprefetch[xbindex][u2];
							usefulprefetch[xbindex][u2]=1;
							swap=victimdirtyary[q];
							victimdirtyary[q]=dirtyary[xbindex][u2];
							/*Put the prefetched block in L1 cache*/
							tagary[xbindex][u2]=xbtag;
							dirtyary[xbindex][u2]=swap;
							validary[xbindex][u2]=1;
							timeary[xbindex][u2]=temp2+1;
		
				
							break;
						}
					}
				}

				if(flag6==false) // Enter this loop if not a hit in the VC
				{
					for(uint64_t i=0;i<sets;i++)
					{
						if(tagary[xbindex][i]==0 && validary[xbindex][i]==0) // Check if there is empty space in L1 cache
						{
							/*Put the data in the empty space */					
							tagary[xbindex][i]=xbtag;		
							validary[xbindex][i]=1;
							usefulprefetch[xbindex][i]=1;   // set L1 prefetch bit
							uint64_t temp2 = 0;
							uint64_t u2 =0;
							/*Find the LRU block in L1*/
							for(uint64_t a=0;a<sets;a++)
    							{
        							if(timeary[xbindex][a]>temp2)
        							{
									temp2=timeary[xbindex][a];
									u2=a;
								}
							}
			
							timeary[xbindex][i]=temp2+1; // Update Timestamp		
							flag7=true;
							flag8=true;
							
							break;	
						}
					}
				}

				if(flag7==false) // Enter this loop if no empty space is found in L1
				{
					uint64_t temp2 = 0;
					uint64_t u2 =0;
					/*Find the LRU block in L1*/
					for(uint64_t i=0;i<sets;i++)
    					{
        					if(timeary[xbindex][i]>temp2)
        					{
							temp2=timeary[xbindex][i];
							u2=i;
						}
					}
					if(V!=0) // Enter this loop only if victim cache is present
					{
						for(uint64_t y=0; y<V; y++)
						{
							if(victimtagary[y][0]==0 && victimtagary[y][1]==0 && victimvalidary[y]==0)	// Check if there is any empty space in VC	
							{
								/*Put the L1 LRU in empty block in VC*/
								victimtagary[y][0]=tagary[xbindex][u2];		
								victimtagary[y][1]=xbindex;
								victimdirtyary[y]=dirtyary[xbindex][u2];
								victimvalidary[y]=1;
								victimusefulprefetch[y]=usefulprefetch[xbindex][u2];
								/*Put the data in the L1 cache*/
								tagary[xbindex][u2]=xbtag;
								dirtyary[xbindex][u2]=0;
								validary[xbindex][u2]=1;
								usefulprefetch[xbindex][u2]=1; // set L1 prefetch bit 
								timeary[xbindex][u2]=temp2+1;// update timestamp
								flag8=true;
							
								break;
							}		
						}
					}
				} 

				if(flag8==false)    // Enter this loop if there is no empty space in VC
				{
					uint64_t temp2 = 0;
					uint64_t u2 =0;
					/*Find the L1 LRU block*/
					for(uint64_t i=0;i<sets;i++)
    					{
        					if(timeary[xbindex][i]>temp2)
        					{
							temp2=timeary[xbindex][i];
							u2=i;
						}
					}

					if(V==0) // Enter this loop only if there is no victim cache
					{
						if(dirtyary[xbindex][u2]==1)  // if dirty bit of LRU block is set then perform writeback
						{
							p_stats->write_backs++;
						}
					}

					if(V!=0) // Enter this loop only if there is a victim cache
					{
						if(victimdirtyary[0]==1)   // if the dirty bit of the VC FIFO block is set then perfrom writeback
						{
							p_stats->write_backs++;
						}
					}
	
					if(V!=0)  // Enter this loop only if there is a victim cache
					{
						/*VC FIFO sorting*/
						for(uint64_t ab=0; ab<(V-1); ab++)
						{
							victimtagary[ab][0]=victimtagary[ab+1][0];
							victimtagary[ab][1]=victimtagary[ab+1][1];
							victimdirtyary[ab]=victimdirtyary[ab+1];
							victimusefulprefetch[ab]=victimusefulprefetch[ab+1];
						}
						/*Put the L1 LRU in VC*/
						victimtagary[V-1][0]=tagary[xbindex][u2];
						victimtagary[V-1][1]=xbindex;
						victimdirtyary[V-1]=dirtyary[xbindex][u2];
						victimusefulprefetch[V-1]=usefulprefetch[xbindex][u2];
					}
					/*put the data in L1 cache*/
					tagary[xbindex][u2]=xbtag;
					timeary[xbindex][u2]=temp2+1;	// update timestamp			
					dirtyary[xbindex][u2]=0;        // make L1 dirty bit zero
					usefulprefetch[xbindex][u2]=1;  // set the prefetch bit in L1 cache
					validary[xbindex][u2]=1;
				}
			}
		}
	}
} //void cache access ends


void complete_cache(cache_stats_t *p_stats) 
{
	p_stats->hit_time= 2+ (0.2*S);
	p_stats->miss_penalty = 200;
	p_stats->miss_rate = (double)p_stats->misses/(double)p_stats->accesses;
	vcmissrate = (double)p_stats->vc_misses/(double)p_stats->accesses;
	p_stats->avg_access_time = p_stats->hit_time + vcmissrate*(double)p_stats->miss_penalty;
	p_stats->bytes_transferred = (double)p_stats->vc_misses*block + (double)p_stats->write_backs*block +(double)p_stats->prefetched_blocks*block;
	delete[] tagary;
	delete[] validary;
	delete[] dirtyary;
	delete[] timeary;
	delete[] usefulprefetch;
	delete[] victimtagary;
	delete[] victimdirtyary;
	delete[] victimvalidary;
	delete[] victimusefulprefetch;
	
}
