/*
// File: Assignment_3.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
// Assignment 2
//
// Extending the code from Task 1, you are to simulate a multiprocessing system with a shared memory
// architecture. Your simulation should be able to support multiple processors, all working in parallel.
// Each of the processors is associated with a local cache, and all cache modules are connected to a
// single bus. A main memory is also connected to the bus, where all the correct data is supposed to
// be stored, although, again, the memory and its contents does not have to be simulated. Assume a
// pipelined memory which can handle a request from each processor in parallel with the same latency.
// In order to make cache data coherent within the system, implement a bus snooping protocol. This
// means that each cache controller connected to the bus receives every memory request on the bus made
// by other caches, and makes the proper modification on its local cache line state. In this task, you
// should implement the simplest VALID-INVALID protocol. The cache line state transition diagram for
// this protocol is shown in Figure 1.
//
//
*/

#include "aca2009.h"
#include <systemc.h>
#include <iostream>

using namespace std;


#define COMMENTS 0
static const unsigned int TAG_MASK = 0xFFFFF000;
static const unsigned int SET_MASK = 0xFE0;
static const unsigned int OFFSET_MASK = 0x1F;

static const int MEM_SIZE = 32768;
static const int BLOCK_SIZE = 32;
static const int TOTAL_WAYS	= 8;
static const int TOTAL_SETS	= MEM_SIZE / (BLOCK_SIZE * TOTAL_WAYS);

static const unsigned int REQ_Rdx = 0;
static const unsigned int REQ_Wr  = 1;
static const unsigned int REQ_Rd  = 2;

static const unsigned int state_invalid    = 0;
static const unsigned int state_shared     = 1;
static const unsigned int state_owner      = 2;
static const unsigned int state_exclusive  = 3;
static const unsigned int state_modified   = 4;

unsigned long long int cache_bus_rdx;
unsigned long long int cache_bus_rd;
unsigned long long int cache_bus_wr;

unsigned long long int stat_local_transfer =0;


typedef struct {
    
    unsigned int moesi_stat;
    unsigned int addr;
    sc_int<32>   data;
    
}transfer_block;


typedef struct 
    { 
        unsigned int tag_field;
		unsigned int valid_flag;
		unsigned int word_data[8];
    } set_lines;
 
 
typedef struct 
    {
        unsigned int lru_counter; 
        set_lines line[TOTAL_WAYS]; 

		void cache_sets()
		{
			for(int i=0; i<TOTAL_WAYS;i++)
				line[i].valid_flag = state_invalid;
			
		}
    } cache_sets;


class Bus_if : public virtual sc_interface 
{
    public:
		virtual bool lock() = 0;
		virtual bool unlock() = 0;
        virtual bool write(int bus_owner, int addr, int data) = 0;
        virtual bool read(int bus_owner, int addr) = 0;		
		virtual bool read_exclusive(int bus_owner, int addr, int data) = 0;

};


	
SC_MODULE(Cache) 
{

    public:
    	enum Function 
    	{
            FUNC_READ,
            FUNC_WRITE,
        };

    	enum RetCode 
    	{
            RET_READ_DONE,
            RET_WRITE_DONE,
    	};
	
    	enum Bus_Function 
        {
	    	BUS_Wr,			
	    	BUS_Rd,
	    	BUS_Rdx,
	    	BUS_Rst,
        };
	
    	enum MOESI_Func 
        {
	    	local_invalid,
	    	local_valid,
        };

    	sc_in<bool>              Port_CLK;
    	sc_in<Function>          Port_Func;
    	sc_in<int>               Port_Addr;
    	sc_out<RetCode>          Port_Done;
    	sc_inout_rv<32>          Port_Data;
        sc_inout_rv<32>          Port_local_transfer;

		sc_port<Bus_if>          Port_Bus;
    	sc_in<int>               Port_BusOwner;	
    	sc_inout_rv<32>	         Port_BusAddr;
    	sc_in<Bus_Function>      Port_BusValid;                
        sc_inout_rv<1>           Port_local_valid;
                       
    	unsigned int             local_id;

		//auxiliary functions
    	unsigned int process_read_request(int local_id, unsigned int read_addr);
    	void process_write_request(int local_id, unsigned int write_addr);	
		int select_lru_line(int set_no);
		void update_lru_counter(unsigned int set_no, unsigned int line_no);		
		inline void invalidate_way(unsigned int invalidate_addr);
        inline void  process_bus_request(unsigned int addr, unsigned int local_id, unsigned int req_type);

    	SC_CTOR(Cache) 
    	{
			m_data = new cache_sets[TOTAL_SETS];
            SC_THREAD(bus_controller);
            SC_THREAD(execute);
		
            sensitive << Port_CLK.pos();
            dont_initialize();		         
	    
        }		

        ~Cache() 
        {
            delete[] m_data;
        }

    private:

    	cache_sets* m_data;
		//cache_def cache_i;
		unsigned int bus_rx_addr, addr, temp;
		unsigned int sender_cache_id;
		unsigned long long int bus_access_write;
		unsigned long long int bus_access_read;

		
        void bus_controller() 
    	{

            while(true)
            {				
				wait(Port_BusValid.value_changed_event());
	    		sender_cache_id = Port_BusOwner.read();
		
				if (sender_cache_id != local_id)
				{ 
	    			bus_rx_addr = Port_BusAddr.read().to_int();
	
            		switch(Port_BusValid.read())
            		{
						case BUS_Rdx:
									if (sender_cache_id != local_id)
									{ process_bus_request(bus_rx_addr, local_id, REQ_Rdx );
									
									if(COMMENTS == 2)
										cout<<"cache "<<local_id<<" got readX at "<<bus_rx_addr<<"\n";
										
									}
                                    break;
                        
						case BUS_Wr :
									if (sender_cache_id != local_id)
									{ process_bus_request(bus_rx_addr, local_id, REQ_Wr );
									
									if(COMMENTS == 2)
										cout<<"cache "<<local_id<<" got write at "<<bus_rx_addr<<"\n";
										
									}
									break;
									
		 				case BUS_Rd :
									if (sender_cache_id != local_id)
									{ process_bus_request(bus_rx_addr, local_id, REQ_Rd );
									
									if(COMMENTS == 2)
										cout<<"cache "<<local_id<<" got read at "<<bus_rx_addr<<"\n";
										
									}									
									break;
										
						default:	//reset state. nothing to do.
									break;
 
						
					} //switch-statements

				wait();

				}//if()
					
			}//while()
				
		} //bus_controller()
	
		
        void execute() 
		{
			while (true)
			{
				wait(Port_Func.value_changed_event());
				
				Function f = Port_Func.read();
				addr       = Port_Addr.read();				
								
				if (f == FUNC_READ) 
				{		
					if(COMMENTS == 3)
						cout<<"cache "<<local_id<<" received read at address "<<addr<<"\n";			
					temp = process_read_request(local_id, addr);
					Port_Data.write( temp );
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
				else
				{	
					if(COMMENTS == 3)
						cout<<"cache "<<local_id<<" received write at address "<<addr<<"\n";				
					
                    process_write_request(local_id, addr);
					Port_Done.write( RET_WRITE_DONE );
				}
			}//end of while()
			
		}//end of execute()

};//end of cache


unsigned int Cache::process_read_request(int local_id, unsigned int read_addr)
{
    int idx, tag_in_memory =0, line_to_replace; 
    unsigned int read_data, tag_val, set_no, byte_in_line, prev_state;
    

    tag_val = (read_addr & TAG_MASK) >> 12;
    set_no = (read_addr & SET_MASK) >> 5;
    byte_in_line = (read_addr & OFFSET_MASK) >> 2;		
    

    for (idx=0; idx < TOTAL_WAYS; idx++)
    {
        if ((m_data[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0))
        {
            if ( m_data[set_no].line[idx].valid_flag != state_invalid)
            {
                tag_in_memory = 1;
                read_data = m_data[set_no].line[idx].word_data[byte_in_line];
                update_lru_counter(set_no, idx);
                stats_readhit(local_id);

                if(COMMENTS == 3)
                {cout<<"cache "<<local_id<<"got read hit at address"<<read_addr;}

                
                wait();
                break;	
            }

        }
    }
    
    if (tag_in_memory == 0)
    {
        tag_in_memory = 1;
        Port_Bus->read(local_id, read_addr);
        
        if( Port_local_valid.read() == local_valid)
        {
            stat_local_transfer++;       
            read_data = Port_local_transfer.read().to_int();
            
            line_to_replace = select_lru_line(set_no) ;
            prev_state = m_data[set_no].line[line_to_replace].valid_flag;
            
            if( prev_state == state_owner || prev_state == state_exclusive || prev_state == state_modified )
            {
                wait(100);
            }
            
            m_data[set_no].line[line_to_replace].valid_flag = state_shared;
            m_data[set_no].line[line_to_replace].tag_field = tag_val;	    
            m_data[set_no].line[line_to_replace].word_data[byte_in_line] = read_data;
            update_lru_counter(set_no, line_to_replace);    

            if(COMMENTS == 3)
            {cout<<"cache "<<local_id<<"got data locally at address"<<read_addr;}			

            wait();
            return read_data;
        }
        else
        {
            
            wait(100);
            line_to_replace = select_lru_line(set_no) ;	
            m_data[set_no].line[line_to_replace].valid_flag = state_exclusive;
            m_data[set_no].line[line_to_replace].tag_field = tag_val;
            read_data = rand();		    
            m_data[set_no].line[line_to_replace].word_data[byte_in_line] = read_data;
            update_lru_counter(set_no, line_to_replace);
            stats_readmiss(local_id);

            if(COMMENTS == 3)
            {cout<<"cache "<<local_id<<"got read miss (invalid tag) at address"<<read_addr;}			
            
        }


    }
    
    return read_data;
    
}


void Cache::process_write_request(int local_id, unsigned int write_addr)
{		
    int idx = 0, tag_in_memory = 0, line_to_replace, data= 0;
    unsigned int tag_val, set_no, byte_in_line, line_state, prev_state;

    
    tag_val = (write_addr & TAG_MASK) >> 12;
    set_no = (write_addr & SET_MASK) >> 5;
    byte_in_line = (write_addr & OFFSET_MASK) >> 2;	


    for (idx =0; idx < TOTAL_WAYS; idx++)
    {
        line_state = m_data[set_no].line[idx].valid_flag;
        
        if ((m_data[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0) && (line_state != state_invalid))
        {
            tag_in_memory = 1;
            data= rand();
            
            Port_Bus->write(local_id, write_addr, data);
            Port_Bus->lock();								
            
            m_data[set_no].line[idx].valid_flag = state_modified;
            m_data[set_no].line[idx].word_data[byte_in_line] = data;							
            update_lru_counter(set_no, idx);
            stats_writehit(local_id);

            if(COMMENTS == 3)
            {cout<<"cache "<<local_id<<"got write hit at address"<<write_addr;}
            
            
            Port_Bus->unlock();
            wait();
            break ;
        }
    }
    if (tag_in_memory == 0)
    {
        data = rand();
        Port_Bus->read_exclusive(local_id, write_addr, data); 
        
        Port_Bus->lock();
        //wait(100);

        line_to_replace = select_lru_line(set_no);
        prev_state = m_data[set_no].line[line_to_replace].valid_flag;
        
        if( prev_state == state_owner || prev_state == state_exclusive || prev_state == state_modified )
        {
            wait(100);
        }
        
        m_data[set_no].line[line_to_replace].valid_flag = state_modified;
        m_data[set_no].line[line_to_replace].tag_field = tag_val;
        m_data[set_no].line[line_to_replace].word_data[byte_in_line] = data;		    
        update_lru_counter(set_no, line_to_replace);
        stats_writemiss(local_id);
        
        Port_Bus->unlock();
        
        if(COMMENTS == 3)
        {cout<<"cache "<<local_id<<"got write miss at address"<<write_addr;}
        
    }
    
    wait(100);	
    Port_Bus->unlock();		
    
}


inline void  Cache::process_bus_request(unsigned int bus_addr, unsigned int local_id, unsigned int req_type)
{
    int idx, tag_in_memory =0, prev_state=0, temp; 
    unsigned int tag_val, set_no;
    
    tag_val = (bus_addr & TAG_MASK) >> 12;
    set_no = (bus_addr & SET_MASK) >> 5;
        
    
    for (idx=0; idx < TOTAL_WAYS; idx++)
    {
        if ((m_data[set_no].line[idx].tag_field == tag_val)  && (tag_in_memory == 0) 
            && (m_data[set_no].line[idx].valid_flag != state_invalid))
        {
            tag_in_memory = 1;							            
            
            if (req_type == REQ_Wr)
            { 
                m_data[set_no].line[idx].valid_flag = state_invalid; 
                
                if(COMMENTS == 3)
                {cout<<"cache "<<local_id<<" got Wr @" <<"at addr "<< bus_addr;}
            } //Wr
        
            if (req_type == REQ_Rd)
            {
                prev_state = m_data[set_no].line[idx].valid_flag;
                
                if( prev_state == state_exclusive || prev_state == state_modified )
                    m_data[set_no].line[idx].valid_flag = state_owner; 

                if( prev_state == state_exclusive || prev_state == state_modified 
                    || prev_state == state_owner )
                {
                    temp = rand();
                    Port_local_transfer.write(temp);
                    Port_local_valid.write(local_valid);
                    stat_local_transfer++;

					wait();
					Port_local_transfer.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");                    

                    if(COMMENTS == 3)
                    {cout<<"cache "<<local_id<<" shares data locally @" <<"at addr "<< bus_addr;}
                    
                }
            } //Rd

            if (req_type == REQ_Rdx)
            { 
                m_data[set_no].line[idx].valid_flag = state_invalid;

                if(COMMENTS == 3)
                {cout<<"cache "<<local_id<<" got Rdx @" <<"at addr "<< bus_addr;}

            } // Rdx       
                       

        } //flag search
        
    } // tag search
     
} // process_bus_request() ends

inline	void Cache::invalidate_way(unsigned int invalidate_addr)
{
    unsigned int tag, set;
    
    
    tag = (invalidate_addr & TAG_MASK) >> 12;
    set = (invalidate_addr & SET_MASK) >> 5;


    for (int idx=0; idx < TOTAL_WAYS; idx++)
    {
        if ( m_data[set].line[idx].valid_flag == 1 )
        {
            if ( m_data[set].line[idx].tag_field == tag)
            {
                m_data[set].line[idx].valid_flag = 0;
            }
        }
    }
}


int Cache::select_lru_line(int set_no)
{

    int curr_lru_tree = m_data[set_no].lru_counter;
    
    if( (curr_lru_tree & 0b0001011 ) == 0b0000000)
    {
        return 0;
    } 
    else if( (curr_lru_tree & 0b0001011 ) == 0b0001000 ) 
    {
        return 1;
    } 
    else if ( (curr_lru_tree & 0b0010011 ) == 0b0000010 )
    {
        return 2;
    } 
    else if ( (curr_lru_tree & 0b0010011 ) == 0b0010010 )
    {
        return 3;
    } 
    else if ( (curr_lru_tree & 0b0100101 ) == 0b0000001 )
    {
        return 4;
    } 
    else if ( (curr_lru_tree & 0b0100101 ) == 0b0100001 ) 
    {
        return 5;
    }
    else if ( (curr_lru_tree & 0b1000101 ) == 0b0000101 )
    {
        return 6;
    } 
    else 
    {
        return 7;  
    }

        
}


void Cache::update_lru_counter(unsigned int set_no, unsigned int line_no)
{
    
    if ( line_no == 0 ) 
    {
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0001011; 
    }
    else if ( line_no == 1 )
    { 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000011; 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1110111; 
    }
    else if ( line_no == 2 )
    { 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0010001; 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111101; 
    }
    else if ( line_no == 3 )
    { 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000001; 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1101101;
    }
    else if( line_no ==4) 
    {
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0100100; 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111110; 
    }
    else if ( line_no == 5 ) 
    {      
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b0000100;  
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1011110; 
    }
    else if ( line_no == 6 )
    { 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter | 0b1000000; 
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b1111010;  
    }
    else 
    {    
        m_data[set_no].lru_counter = m_data[set_no].lru_counter & 0b111010;    
    } 
    
    
}

	
class Bus : public Bus_if, public sc_module
{

public:

    sc_in<bool> Port_CLK;
	
    sc_out<int>                 Port_BusOwner;
	sc_signal_rv<32>            Port_BusAddr;
    sc_out<Cache::Bus_Function> Port_BusValid;
    
    sc_in_rv<32>        Port_local_transfer;
    sc_in_rv<1>         Port_local_valid;    

    sc_mutex bus_lock;
    unsigned long long int stat_bus_write_cycles;
    unsigned long long int stat_bus_read_cycles;
    unsigned long long int stat_bus_read_exclusive_cycles;    
    unsigned long long int stat_bus_wait_cycles;
	
public:
    SC_CTOR(Bus) 
    {
        sensitive << Port_CLK.pos();

        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");

        stat_bus_write_cycles = 0;
        stat_bus_read_cycles = 0;
        stat_bus_wait_cycles = 0;

    }


    virtual bool lock()
    {
		while(bus_lock.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		return true;
    }

    virtual bool unlock()
    {
		bus_lock.unlock();
		return true;
    }

    virtual bool write(int bus_owner, int addr, int data)
    {
        while(bus_lock.trylock() == -1)
		{
            stat_bus_wait_cycles++;
            wait();
        }

        stat_bus_write_cycles++;

        Port_BusAddr.write(addr);
        Port_BusOwner.write(bus_owner);
        Port_BusValid.write(Cache::BUS_Wr);

        wait();

        Port_BusValid.write(Cache::BUS_Rst);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        bus_lock.unlock();

        return true;
    }

    virtual bool read(int bus_owner, int addr)
    {
        while(bus_lock.trylock() == -1)
		{
            stat_bus_wait_cycles++;
            wait();
        }

        stat_bus_read_cycles++;

        Port_BusAddr.write(addr);
        Port_BusOwner.write(bus_owner);
        Port_BusValid.write(Cache::BUS_Rd);

        wait();

        Port_BusValid.write(Cache::BUS_Rst);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        bus_lock.unlock();

        return true;
    }
	
    virtual bool read_exclusive(int bus_owner, int addr, int data) 
    {
    	while(bus_lock.trylock() == -1)
		{
	    	stat_bus_wait_cycles++;
	    	wait();
		}
		
		stat_bus_read_exclusive_cycles++;

		Port_BusAddr.write(addr);
		Port_BusValid.write(Cache::BUS_Rdx);
		Port_BusOwner.write(bus_owner);

		wait();
		
        Port_BusValid.write(Cache::BUS_Rst);
        Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
        bus_lock.unlock();

	return true;
    }


    void b_stats_print()
    {      
        cout<<"\n\n1-Bus Reads\t"<<stat_bus_read_cycles<< "\n2-Bus Writes\t"<< stat_bus_write_cycles;
        cout<< "\n3-Bus Rd-exclusive\t"<< stat_bus_read_exclusive_cycles;
		cout<<"\n3-Bus Wait Cycles\t"<<stat_bus_wait_cycles;
        cout<<"\nTotal Useful Accesses (without waits)\t"<< (stat_bus_read_cycles + stat_bus_write_cycles+stat_bus_read_exclusive_cycles);
        cout<<"\nAverage waiting time per access:\t"<< (double)(stat_bus_wait_cycles) / (stat_bus_read_cycles + stat_bus_write_cycles+stat_bus_read_exclusive_cycles);	
        cout<<"\nTotal-local cache transfers (Coherence)\t"<<stat_local_transfer;
    
/*
1. Per cache, please report the following in tabular format: (use the framework functions)
#Reads RHit RMiss #Writes WHit WMiss Hitrate
2. Please also add two additional rows for total and average statistics.
3. Average memory access time
4. Bus acquisition statistics: total number of wait cycles trying to lock the bus, and the average
waiting time per access (= #waits/(bus reads + bus writes))
5. Number of bus reads, bus writes, and total accesses (reads+writes).
6. Total simulated execution time
*/		

    }
};

		

	
SC_MODULE(CPU) 
{

public:
    sc_in<bool>                	Port_CLK;
    sc_in<Cache::RetCode>   	Port_MemDone;
    sc_out<Cache::Function> 	Port_MemFunc;
    sc_out<int>                	Port_MemAddr;
    sc_inout_rv<32>            	Port_MemData;
	
	int cpu_id;
	

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
        Cache::Function  f;
		
        int addr, data;
		
        while(!tracefile_ptr->eof())
        {            
            if(!tracefile_ptr->next(cpu_id, tr_data))
            {
                cerr << "Error reading trace for CPU" << endl;
                break;
            }

	    	addr = tr_data.addr;

            switch(tr_data.type)
            {
                case TraceFile::ENTRY_TYPE_READ:
                    f = Cache::FUNC_READ;
                    break;

                case TraceFile::ENTRY_TYPE_WRITE:
                    f = Cache::FUNC_WRITE;
                    break;

                case TraceFile::ENTRY_TYPE_NOP:
                    break;

                default:
                    cerr << "Error, got invalid data from Trace" << endl;
                    exit(0);
            }

            if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
            {
                Port_MemAddr.write(addr);
                Port_MemFunc.write(f);

                if (f == Cache::FUNC_WRITE) 
                {
					if(COMMENTS == 1)
					{ cout << sc_time_stamp() << ": CPU " << cpu_id << " sends write\n"; }

                    data = rand();
                    Port_MemData.write(data);
                    wait();
                    Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
                }
                else
                {
					if(COMMENTS == 1)
					{ cout << sc_time_stamp() << ": CPU_" << cpu_id << " sends read" << endl; }
                }

                wait(Port_MemDone.value_changed_event());

               if (f == Cache::FUNC_READ)
                {
                    if(COMMENTS == 1)
                    { cout << sc_time_stamp() << ": CPU "<< cpu_id<<" reads: " << Port_MemData.read() << "\n"; }
                }
            }
            else
            {
                if(COMMENTS == 1)
				{ cout << sc_time_stamp() << ": CPU " << cpu_id << " executes NOP \n" ; }
            }
            
            wait();
        }
        
        
        sc_stop();
		
    }
};


int sc_main(int argc, char* argv[])
{
    try
    {
        init_tracefile(&argc, &argv);
		
		cout << "Available CPUs: "<< num_cpus << endl;				
        stats_init();		
        sc_clock clk("clk", sc_time(1, SC_NS));

        Cache* cache[num_cpus];
        CPU* cpu[num_cpus];
		Bus bus("bus");
        
        sc_buffer<Cache::Function> sigCacheFunc[num_cpus];
        sc_buffer<Cache::RetCode>  sigCacheDone[num_cpus];
        sc_signal<int>             sigCacheAddr[num_cpus];
        sc_signal_rv<32>           sigCacheData[num_cpus];		        				
 
		sc_signal<int>              	sigBusOwner;
        sc_signal<Cache::Bus_Function>  sigBusValid;

        sc_signal_rv<32>            siglocal_transfer;
        sc_signal_rv<1>             siglocal_valid;
		
		bus.Port_CLK(clk);
        bus.Port_BusOwner(sigBusOwner);
        bus.Port_BusValid(sigBusValid);

        bus.Port_local_transfer(siglocal_transfer);
        bus.Port_local_valid(siglocal_valid);
		
		char str_cache[20], str_cpu[20];
		char str_sigCacheFunc[20], str_sigCacheDone[20], str_sigCacheAddr[20], str_sigCacheData[20];		
		char str_siglocal_transfer[20], str_siglocal_valid[20];
        
        sc_trace_file *wf = sc_create_vcd_trace_file("group14_assignment03");        
        sc_trace(wf, clk, "clock");
        sc_trace(wf, sigBusOwner, "sigBusOwner");
		sc_trace(wf, sigBusValid, "sigBusvalid");

		unsigned int idx;

		for ( idx =0; idx < num_cpus; idx++)
		{				
	    	sprintf(str_cache, "cache_%d", idx);
            sprintf(str_cpu, "cpu_%d", idx);

            cache[idx] = new Cache(str_cache);
            cpu[idx] = new CPU(str_cpu);

	    	cpu[idx]->Port_CLK(clk);
			cache[idx]->Port_CLK(clk);
			
            cpu[idx]->cpu_id = idx;
            cache[idx]->local_id = idx;
						
	    	cache[idx]->Port_Func(sigCacheFunc[idx]);
	    	cache[idx]->Port_Done(sigCacheDone[idx]);
	    	cache[idx]->Port_Addr(sigCacheAddr[idx]);
	    	cache[idx]->Port_Data(sigCacheData[idx]);
            
	    	cache[idx]->Port_local_transfer( siglocal_transfer );
	    	cache[idx]->Port_local_valid( siglocal_valid );

	    	cpu[idx]->Port_MemFunc(sigCacheFunc[idx]);
			cpu[idx]->Port_MemDone(sigCacheDone[idx]);
	    	cpu[idx]->Port_MemAddr(sigCacheAddr[idx]);
	    	cpu[idx]->Port_MemData(sigCacheData[idx]);	    	
			
			cache[idx]->Port_Bus(bus);
            cache[idx]->Port_BusOwner(sigBusOwner);
            cache[idx]->Port_BusValid(sigBusValid);            
            cache[idx]->Port_BusAddr(bus.Port_BusAddr);
	    				
		}
		
		
		for (idx = 0; idx < num_cpus; idx++)
		{
	    	sprintf(str_sigCacheFunc, "cpu%d_func", idx);	
	    	sprintf(str_sigCacheDone, "cpu%d_done", idx);			
	    	sprintf(str_sigCacheAddr, "cpu%d_addr", idx);
	    	sprintf(str_sigCacheData, "cpu%d_data", idx);

	   		sc_trace(wf, sigCacheFunc[idx], str_sigCacheFunc);
	    	sc_trace(wf, sigCacheDone[idx], str_sigCacheDone);	    
            sc_trace(wf, sigCacheAddr[idx], str_sigCacheAddr);
            sc_trace(wf, sigCacheData[idx], str_sigCacheData);

		}
			
        sc_trace(wf, siglocal_transfer, str_siglocal_transfer);
        sc_trace(wf, siglocal_valid, str_siglocal_valid);

		sc_report_handler::set_actions(SC_ID_MORE_THAN_ONE_SIGNAL_DRIVER_, SC_DO_NOTHING);
        sc_start();

		sc_close_vcd_trace_file(wf);
        
        stats_print();
		bus.b_stats_print();
		
		cout << "\nSimulation ends at\t" << sc_time_stamp() << endl;
		
		cout<<"\n\nComments are by default at level 0 (turned off)\n";
		cout<<"Put COMMENTS equal to desired level at line 31\n";
		cout<<"Put COMMENTS equal 1 for cpus\n";
		cout<<"Put COMMENTS equal 1 for bus\n";
		cout<<"Put COMMENTS equal 3 for caches\n";
    } 

    catch (exception& e)
    {
        cerr << e.what() << endl;
    }
    
    return 0;
}