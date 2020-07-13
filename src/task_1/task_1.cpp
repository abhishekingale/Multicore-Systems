/*
// File: task_1.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
//
// Author(s): Michiel W. van Tol, Mike Lankamp, Jony Zhang, 
//            Konstantinos Bousias
// Copyright (C) 2005-2009 by Computer Systems Architecture group, 
//                            University of Amsterdam
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
*/

#include "aca2009.h"
#include <systemc.h>
#include <iostream>

using namespace std;

static const int MAX_CACHE_ENTRIES 	= 128;
//static const int MAX_CACHE_SIZE 	= 32768; 	//32KB
//static const int CACHE_LINE_SIZE 	= 32; 		//32 bytes
//static const int MAX_SET 		= 8;
unsigned int temp_RHit=0, temp_RMiss=0, temp_WHit=0, temp_WMiss=0;
//************************************************************//
#define ADDR_LEN 32
#define TAG_LEN 20
#define MAIN_MEM_LATENCY 100

static const int MEM_SIZE = 32768;
static const int BLOCK_SIZE = 32;
static const int ASSOCIATIVITY	= 8;
static const int TOTAL_WAYS	= 8;

static const int TOTAL_SETS	= MEM_SIZE / (BLOCK_SIZE * TOTAL_WAYS);

static const unsigned int TAG_MASK = 0xFFFFF000;
static const unsigned int SET_MASK = 0x00000FE0;
static const unsigned int OFFSET_MASK = 0x0000001F;

typedef struct {
     
        unsigned int tag_field:20;
        unsigned int dirty_flag;
		unsigned int valid_flag:1;
		unsigned int word_data[8]; // sizeof(unsigned int)=4B, 4B * 8 = 32B		

} set_lines;
   
typedef struct 
    {
        unsigned int lru_counter:7; 
        set_lines line[TOTAL_WAYS];        	
		
		
    } cache_sets;

int cpus = 0;

class Bus_if : public virtual sc_interface 
{
    public:
		virtual bool lock() = 0;
		virtual bool unlock() = 0;	
        virtual bool write(int owner, int addr, int data) = 0;
        virtual bool read(int owner, int addr) = 0;
		virtual bool wr_exclusive(int owner, int addr, int data) = 0;

};

/*
SC_MODULE(Memory) 
{

public:
    enum Function 
    {
        FUNC_READ,
        FUNC_WRITE
    };

    enum RetCode 
    {
        RET_READ_DONE,
        RET_WRITE_DONE,
    };

    sc_in<bool>     Port_CLK;
    sc_in<Function> Port_Func;
    sc_in<int>      Port_Addr;
    sc_out<RetCode> Port_Done;
    sc_inout_rv<32> Port_Data;

    SC_CTOR(Memory) 
    {
        SC_THREAD(execute);
        sensitive << Port_CLK.pos();
        dont_initialize();

        m_data = new int[MEM_SIZE];
    }

    ~Memory() 
    {
        delete[] m_data;
    }

private:
    int* m_data;

    void execute() 
    {
        while (true)
        {
            wait(Port_Func.value_changed_event());	// this is fine since we use sc_buffer
            
            Function f = Port_Func.read();
            int addr   = Port_Addr.read();
            int data   = 0;
            if (f == FUNC_WRITE) 
            {
                cout << sc_time_stamp() << ": MEM received write" << endl;
                data = Port_Data.read().to_int();
            }
            else
            {
                cout << sc_time_stamp() << ": MEM received read" << endl;
            }

            // This simulates memory read/write delay
            wait(99);

            if (f == FUNC_READ) 
            {
                Port_Data.write( (addr < MEM_SIZE) ? m_data[addr] : 0 );
                Port_Done.write( RET_READ_DONE );
                wait();
                Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
            }
            else
            {
                if (addr < MEM_SIZE) 
                {
                    m_data[addr] = data;
                }
                Port_Done.write( RET_WRITE_DONE );
            }
        }
    }
}; 
*/


SC_MODULE(Cache) 
{

public:
 public:
    enum Function 
    {
        FUNC_READ,
        FUNC_WRITE
    };

    enum RetCode 
    {
        RET_READ_DONE,
        RET_WRITE_DONE,
    };

	enum Bus_Req 
	{
		BUS_Rd,
		BUS_Wr,
		BUS_RDx,
		BUS_Inv,
	};
	
    sc_in<bool>     Port_CLK;
    sc_in<Function> Port_Func;
    sc_in<int>      Port_Addr;
    sc_out<RetCode> Port_Done;
    sc_inout_rv<32> Port_Data;

	//bus declarations
	sc_in<int>      Port_BusOwner;
	sc_inout_rv<32>	Port_BusAddr;
	sc_in<Bus_Req>  Port_BusValid;

	//bus request
	sc_port<Bus_if> Port_Bus;
	
	unsigned int local_id = 0;

	
    SC_CTOR(Cache) 
    {
        SC_THREAD(execute);
		SC_THREAD(bus_master);
		
		
        sensitive << Port_CLK.pos();
        dont_initialize();

         m_data = new cache_sets[TOTAL_SETS];
		 
		for (int i = 0; i < MAX_CACHE_ENTRIES ; i++)
            {
                for (int j = 0; j < TOTAL_WAYS ; j++)
            	{
                    m_data[i].line[j].tag_field = 0xFFFFF;
		    		m_data[i].line[j].valid_flag = 0; //  invalid cache line set
            	}
            }	
    }	
    

    ~Cache() 
    {
        delete[] m_data;
    }

	private:
	
	int data;
	unsigned int temp_addr;
	unsigned int snooping_cache;
	unsigned long long int bus_access_write;
	unsigned long long int bus_access_read;

	cache_sets* m_data
	
	
	void bus_master(){
		
		while(true){
			// wait for port bus valid to change
			wait(Port_BusValid.value_changed_event());
			
			snooping_cache = Port_BusOwner.read();			
			//port bus valid changed
			if (snooping_cache != local_id)
			{
				temp_addr = Port_BusAddr.read().to_int();
				unsigned int tag = (temp_addr & TAG_MASK) >> 12;
	    		unsigned int set = (temp_addr & SET_MASK) >> 5;
				
				switch(Port_BusValid.read())
				{
					case BUS_Rd:
					{
						bus_access_read++;
						break;
					}
					
					case BUS_Wr:
					case BUS_RDx:
					{
						//blocks.invalidate_from_cache(temp_addr);
						 for (int i = 0; i < ASSOCIATIVITY; i++) {
							if ((m_data[i].line[j].valid_flag == 1) && (m_data[i].line[j].tag_field == tag)) {
									m_data[i].line[j].valid_flag=0;
							//Cache way Invalidated
			
							} 
						}
						
						bus_access_write++;
						break;						
						
					}
					default:
					//Bus invalid Case
					break;
					

				}						

				wait();
			}			
		}
		
	} // bus() ends
	
	   void execute() 
    {
        while (true)
        {
           wait(Port_Func.value_changed_event());
            
            Function f = Port_Func.read();
            int addr   = Port_Addr.read();
            int data   = 0;
			tag_search = 0;

		tag_val = (addr & TAG_MASK)  >> 12;
		set_no = (addr & SET_MASK) >> 5;
		byte_in_line = (addr & OFFSET_MASK) >> 2;			
		
            if (f == FUNC_WRITE) 
            {
                cout << sc_time_stamp() << ": CACHE received write" << endl;
                data = Port_Data.read().to_int();
            }

			
			if (f == FUNC_READ) 
            {	
			//	READ MODE CHECK FOR TAG MATCH, IF TAG MATCHES THEN CHECK VALID.
			//  IF VALID BLOCK IS FOUND, THEN RETURN THE VALUE OTHERWISE IT IS A READ MISS
			for (i=0; i < TOTAL_SETS; i++)
			{
				if ((m_data[set_no].line[i].tag_field == tag_val)  && (tag_search == 0))
				{
				if ( m_data[set_no].line[i].valid_flag == 1)
					{
					tag_search = 1;
					stats_readhit(local_id);
					valid_data = m_data[set_no].line[i].word_data[byte_in_line];
					
					update_lru_counter(set_no, i);
					
					temp_RHit++;
					wait();
					break;	
					}
					
				else
				{

					//stats_readmiss(pid);
					temp_RMiss++;
					tag_search= 1;
					stats_readmiss(local_id);
					wait(MAIN_MEM_LATENCY);
					replace_line = select_lru_line(set_no);
					m_data[set_no].line[replace_line].valid_flag = 1;					
					temp = rand();
					m_data[set_no].line[replace_line].word_data[byte_in_line] = temp;
					valid_data = temp;
					m_data2[set_no].line[replace_line].tag_field = tag_val;
					
					
					update_lru_counter(set_no, i);
					break;
					}
				}
			}
			
			if (tag_search == 0)
		{

			//stats_readmiss(0);
			temp_RMiss++;
			tag_search = 1;
			wait(MAIN_MEM_LATENCY);
			replace_line = select_lru_line(set_no) ;	//lru???
			m_data2[set_no].line[replace_line].word_data[byte_in_line] = rand();
			m_data2[set_no].line[replace_line].valid_flag = 1;
			m_data2[set_no].line[replace_line].tag_field = tag_val;

			update_lru_counter(set_no, i);
			valid_data = m_data2[set_no].line[replace_line].word_data[byte_in_line];
					
		}		
			
				Port_Data.write( valid_data );
                Port_Done.write( RET_READ_DONE );
                wait();
                Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
            }
            else
            {

		tag_search = 0;
		for (i=0; i < TOTAL_SETS; i++)
		{
			if ((m_data2[set_no].line[i].tag_field == tag_val)  && (tag_search == 0))
			{
				tag_search = 1;
				temp_WHit++;
				m_data2[set_no].line[i].word_data[byte_in_line] = data;
				stats_writehit(local_id);
				m_data2[set_no].line[i].valid_flag = 1;

				update_lru_counter(set_no, i);
				valid_data = data;
				wait();
				break ;
			}
		}
		if (tag_search == 0)
		{

			tag_search = 1;
			Port_Bus->wr_exclusive(local_id, addr, data)
			
			stats_writemiss(local_id);;
			temp_WMiss++;
			Port_Bus->lock();
			wait(MAIN_MEM_LATENCY);
			
			replace_line = select_lru_line(set_no);
			m_data2[set_no].line[replace_line].valid_flag = 1;
			m_data2[set_no].line[replace_line].tag_field = tag_val;
			m_data2[set_no].line[replace_line].word_data[byte_in_line] = rand();

			update_lru_counter(set_no, i);
			 	
		}
	
                cout << "Data " << data <<" is written at " << addr  << endl;
                Port_Done.write( RET_WRITE_DONE );
				Port_Bus->unlock();
            }
		}
    } // execute() ends
	
	
	    int Cache::select_lru_line(int set_no) 
    {
    	int present_state = m_data[set_no].lru_counter;
    	cout << "Present State for line "<< line << "is : " << present_state <<endl; 
    	if( (present_state & 0xD ) == 0)
    	{
            return 0;
    	} 
		else if( (present_state & 0xD ) == 0x1 ) 
    	{
            return 1;
    	} 
		else if ( (present_state & 0xD ) == 0x4 )
    	{
            return 2;
		} 
		else if ( (present_state & 0xD ) == 0x5 )
    	{
            return 3;
    	} 
		else if ( (present_state & 0xB ) == 0x8 )
    	{
            return 4;
    	} 
		else if ( (present_state & 0xB ) == 0x9 ) 
    	{
            return 5;
    	}
		else if ( (present_state & 0xB ) == 0xA )
    	{
            return 6;
		} 
		else if ( (present_state & 0xB ) == 0xB )
    	{
            return 7;
    	}
    	else 
		{
            cout << "Something wrong in finding LRU Way :( "<< endl;
            return 0;  
    	}

		
    } //end of Cache::lru
	
	
	 void Cache::update_lru_counter(unsigned int set_no, unsigned int line_no)
    {
		cout << " LRU way is: " << way << " and present LRU is " << m_data[set_no].lru << endl; 
        if ( way == 0 ) //NS  xxx1x11
		{
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0xB;
        }
        else if ( way == 1 )
		{ // L1 is replaced. Next state xxx0x11
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x3;
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x77; 
        }
        else if ( way == 2 )
		{ // NS xx1xx01
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x11; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x7D; 
        }
		else if ( way == 3 )
        { // NS  xx0xx01
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x1;
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x6D;
        }
        else if( way ==4) 
		{// NS x1xx1x0
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x24; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x7E; 
        }
        else if ( way == 5 ) // NS x0xx1x0
        {      
	    	m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x4; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x5E; 
        }
        else if ( way == 6 )
		{ // NS 1xxx0x0
            m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0x40; 
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x7A; 
        }
        else if ( way ==7 ) 
		{ // NS 0xxx0x0    
            m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0x3A;    
        } 
        else
            cout << "Something wrong with LRU update"<< endl;

		cout << " LRU way is: " << way << " and LRU is updated to " << m_data[set_no].lru << endl; 

    } //end of Cache::update_lru
	
}; 

class Bus : public Bus_if, public sc_module
{

public:
	sc_in<bool> Port_CLK;
	
    sc_mutex bus;
    unsigned long long int stat_bus_write_cycles;
    unsigned long long int stat_bus_read_cycles;
    unsigned long long int stat_bus_wait_cycles;

    sc_out<int> Port_BusOwner;
    sc_signal_rv<32> Port_BusAddr;
	sc_out<Cache::Bus_Req> Port_BusValid;

public:
    SC_CTOR(Bus) 
    {
        sensitive << Port_CLK.pos();

        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");

        stat_bus_write_cycles = 0;
        stat_bus_read_cycles = 0;
        stat_bus_wait_cycles = 0;

    }// CTOR() ends here



    virtual bool lock()
    {
		while(bus.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		return true;
    }

    virtual bool unlock()
    {
		bus.unlock();
		return true;
    }


    virtual bool write(int owner, int addr, int data)
    {
		/*
    	while(bus.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		*/

        stat_bus_write_cycles++;

        Port_BusOwner.write(owner);
        Port_BusAddr.write(addr);
        Port_BusValid.write(Cache::BUS_Wr);

        wait();

        Port_BusValid.write(Cache::BUS_Inv);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        //bus.unlock();

        return true;
    }
	
	
    virtual bool read(int owner, int addr)
    {
		/*
    	while(bus.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		*/

        stat_bus_read_cycles++;

        Port_BusOwner.write(owner);
        Port_BusAddr.write(addr);
        Port_BusValid.write(Cache::BUS_Rd);

        wait();

        /* Reset. */
        Port_BusValid.write(Cache::BUS_Inv);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        //bus.unlock();

        return true;
    }	
	
    virtual bool wr_exclusive(int owner, int addr, int data) //WRx & RDx share same behaviour
    {
		/*
    	while(bus.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		*/
		
		stat_bus_write_cycles++;

		Port_BusOwner.write(owner);
		Port_BusAddr.write(addr);
		Port_BusValid.write(Cache::BUS_RDx); 

		wait();
		
        Port_BusValid.write(Cache::BUS_Inv);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        //bus.unlock();

	return true;
    }


    /* Bus output. */
    void b_stats_print()
    {
        double mean = (double)stat_bus_wait_cycles / double(stat_bus_read_cycles + stat_bus_write_cycles);
        printf("    Total %d bus_read_cycles and %d stat_bus_write_cycles.\n", stat_bus_read_cycles, stat_bus_write_cycles);
        printf("    Total %d cycles.\n", stat_bus_read_cycles + stat_bus_write_cycles);
        printf("    Total %d waits cycles.\n", stat_bus_wait_cycles);
        cout<<"Mean: "<<mean<<endl;
    }
};

SC_MODULE(CPU) 
{

public:
    sc_in<bool>                Port_CLK;
    sc_in<Memory::RetCode>   Port_MemDone;
    sc_out<Memory::Function> Port_MemFunc;
    sc_out<int>                Port_MemAddr;
    sc_inout_rv<32>            Port_MemData;

    SC_CTOR(CPU) 
    {
        SC_THREAD(execute);
        sensitive << Port_CLK.pos();
        dont_initialize();
    }

private:
    void execute() 
    {
        TraceFile::Entry    tr_data;
        Memory::Function  f;

        // Loop until end of tracefile
        while(!tracefile_ptr->eof())
        {
            // Get the next action for the processor in the trace
            if(!tracefile_ptr->next(0, tr_data))
            {
                cerr << "Error reading trace for CPU" << endl;
                break;
            }

            // To demonstrate the statistic functions, we generate a 50%
            // probability of a 'hit' or 'miss', and call the statistic
            // functions below
            //int j = rand()%2;

            switch(tr_data.type)
            {
                case TraceFile::ENTRY_TYPE_READ:
                    f = Memory::FUNC_READ;
                 /*   if(j)
                        stats_readhit(0);
                    else
                        stats_readmiss(0); */
                    break;

                case TraceFile::ENTRY_TYPE_WRITE:
                    f = Memory::FUNC_WRITE;
                /*    if(j)
                        stats_writehit(0);
                    else
                        stats_writemiss(0); */
                    break;

                case TraceFile::ENTRY_TYPE_NOP:
                    break;

                default:
                    cerr << "Error, got invalid data from Trace" << endl;
                    exit(0);
            }

            if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
            {
                Port_MemAddr.write(tr_data.addr);
                Port_MemFunc.write(f);

                if (f == Memory::FUNC_WRITE) 
                {
                    cout << sc_time_stamp() << ": CPU sends write" << endl;

                    uint32_t data = rand();
                    Port_MemData.write(data);
                    wait();
                    Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
                }
                else
                {
                    cout << sc_time_stamp() << ": CPU sends read" << endl;
                }

                wait(Port_MemDone.value_changed_event());

                if (f == Memory::FUNC_READ)
                {
                    cout << sc_time_stamp() << ": CPU reads: " << Port_MemData.read() << endl;
                }
            }
            else
            {
                cout << sc_time_stamp() << ": CPU executes NOP" << endl;
            }
            // Advance one cycle in simulated time            
            wait();
        }
        
        // Finished the Tracefile, now stop the simulation
        sc_stop();
    }
};


int sc_main(int argc, char* argv[])
{
    try
    {
        // Get the tracefile argument and create Tracefile object
        // This function sets tracefile_ptr and num_cpus
        init_tracefile(&argc, &argv);

        // Initialize statistics counters
       
		
		cout << "Number of CPU is "<< num_cpus << endl;


		 stats_init();
		 
		 sc_clock clk("clk", sc_time(1, SC_NS));
		 
        // Instantiate Modules
        Memory mem("main_memory");
        CPU    cpu("cpu");
		Bus bus("bus");
		
        bus.Port_CLK(clk);
			
		Cache* cache[num_cpus];
        CPU* cpu[num_cpus];

        // Signals
        sc_buffer<Cache::Function> sigCacheFunc[num_cpus];
        sc_buffer<Cache::RetCode>  sigCacheDone[num_cpus];
        sc_signal<int>              sigCacheAddr[num_cpus];
        sc_signal_rv<32>            sigCacheData[num_cpus];	
        sc_signal<bool>            sigHitState[num_cpus];
		
        sc_signal<Cache::Bus_Req>  sigBusValid;		
        sc_signal<int>             sigBusOwner;

        bus.Port_BusOwner(sigBusOwner);
        bus.Port_BusValid(sigBusValid);

   		char temp_cache[12];
        char temp_cpu[12];
		unsigned int i;
		
		int snoop = 1;
		
		for (i =0; i < num_cpus; i++)
		{
			
	    	sprintf(temp_cache, "cache_%d", i);
            sprintf(temp_cpu, "cpu_%d", i);
           
            cache[i] = new Cache(temp_cache);
            cpu[i] = new CPU(temp_cpu);
            
            cpu[i]->cpu_id = i;
            cache[i]->local_id= i;
			cache[i]->snoop = snoop;

	    	cpu[i]->Port_CLK(clk);			
	    	cache[i]->Port_CLK(clk);

            cache[i]->Port_BusOwner(sigBusOwner);
            cache[i]->Port_BusAddr(bus.Port_BusAddr);
            cache[i]->Port_BusValid(sigBusValid);
            cache[i]->Port_Bus(bus);

	    	cache[i]->Port_Func(sigCacheFunc[i]);
	    	cache[i]->Port_Done(sigCacheDone[i]);			
	    	cache[i]->Port_Addr(sigCacheAddr[i]);
	    	cache[i]->Port_Data(sigCacheData[i]);
			cache[i]->Port_HIT(sigHitState[i]);

	    	cpu[i]->Port_CacheFunc(sigCacheFunc[i]);
	    	cpu[i]->Port_CacheDone(sigCacheDone[i]);			
	    	cpu[i]->Port_CacheAddr(sigCacheAddr[i]);
	    	cpu[i]->Port_CacheData(sigCacheData[i]);
			cpu[i]->Port_HIT(sigHitState[i]);

		}			
		
		char str_sigCacheFunc[12];
		char str_sigCacheDone[12];
	    char str_sigCacheAddr[12];
		char str_sigCacheData[12];	
		char str_sigHitState[12];

        sc_trace_file *wf = sc_create_vcd_trace_file("assignment_02");
		
		sc_trace(wf, clk, "clock");
        sc_trace(wf, sigBusOwner, "sigBusOwner");
		sc_trace(wf, sigBusValid, "sigBusValid");
		

		for (i = 0; i < num_cpus; i++)
		{	
	    	sprintf(str_sigCacheFunc, "cpu%d_func", i);	
	    	sprintf(str_sigCacheDone, "cpu%d_done", i);			
	    	sprintf(str_sigCacheAddr, "cpu%d_addr", i);
	    	sprintf(str_sigCacheData, "cpu%d_data", i);
	    	sprintf(str_sigHitState, "cpu%d_hit", i);			

	   		sc_trace(wf, sigCacheFunc[i], str_sigCacheFunc);
	    	sc_trace(wf, sigCacheDone[i], str_sigCacheDone);			
            sc_trace(wf, sigCacheAddr[i], str_sigCacheAddr);
            sc_trace(wf, sigCacheData[i], str_sigCacheData);
			sc_trace(wf, sigHitState[i], str_sigHitState);			


		}		
	
		sc_report_handler::set_actions(SC_ID_MORE_THAN_ONE_SIGNAL_DRIVER_, SC_DO_NOTHING);
		
        // Start Simulation
        sc_start();
        
        // statistics 
        stats_print();
		bus.b_stats_print();
		
		cout << " \nSimulation Termination time: " << sc_time_stamp() << endl;
    }

    catch (exception& e)
    {
        cerr << e.what() << endl;
    }
    
    return 0;
}
