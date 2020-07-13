		
int read_write_cache(int local_id, int addr, int type)		
{	
		
        int data   = 0;
		int tag_search = 0;
		int valid_data = 0;
		int temp;
		int replace_line;

		unsigned int tag_val = (addr & TAG_MASK)  >> 12;
		unsigned int set_no = (addr & SET_MASK) >> 5;
		unsigned int byte_in_line = (addr & OFFSET_MASK) >> 2;
		
		
		if (type == 0) // read
		{
			for (int i=0; i < TOTAL_SETS; i++)
			{
				if ((m_data[set_no].line[i].tag_field == tag_val)  && (tag_search == 0) && ( m_data[set_no].line[i].valid_flag == 1))
				{
					
					cout << "Read Hit" << endl;
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

					cout << "Read Miss" << endl;
					temp_RMiss++;
					tag_search= 1;
					stats_readmiss(local_id);
					wait(MAIN_MEM_LATENCY);
					
					replace_line = select_lru_line(set_no);
					
					m_data[set_no].line[replace_line].valid_flag = 1;					
					temp = rand();
					m_data[set_no].line[replace_line].word_data[byte_in_line] = temp;
					m_data[set_no].line[replace_line].tag_field = tag_val;
					
					valid_data = m_data[set_no].line[i].word_data[byte_in_line];
				
					update_lru_counter(set_no, i);
					break;
					
				}
			}
			
		if (tag_search == 0)
		{

			//readmiss
			temp_RMiss++;
			tag_search = 1;
			
			Port_Bus->read(local_id, addr);
			stats_readmiss(local_id);
			wait(MAIN_MEM_LATENCY);
			
			replace_line = select_lru_line(set_no) ;
				
			m_data[set_no].line[replace_line].word_data[byte_in_line] = rand();
			m_data[set_no].line[replace_line].valid_flag = 1;
			m_data[set_no].line[replace_line].tag_field = tag_val;
			
			update_lru_counter(set_no, replace_line);
			
			valid_data = m_data[set_no].line[replace_line].word_data[byte_in_line];
					
		} //end of read operation
		
		return valid_data;
		
		}
		
		else
        { // Write Operation
			//tag_search = 0;
			for (int i=0; i < TOTAL_SETS; i++)
			{
				if ((m_data[set_no].line[i].tag_field == tag_val)  && (tag_search == 0)&&(m_data[set_no].line[i].valid_flag == 1))
				{
					//write hit
					cout << " write Hit" << endl;
				
					Port_Bus->write(local_id, addr, data);
					temp_WHit++;
					
					Port_Bus->lock();
					
					m_data[set_no].line[i].word_data[byte_in_line] = data;
					stats_writehit(local_id);
					
					m_data[set_no].line[i].valid_flag = 1;

					update_lru_counter(set_no, i);
					valid_data = m_data[set_no].line[i].word_data[byte_in_line];
					tag_search = 1;
					wait();
					break ;
				}
			}
				if (tag_search == 0)
				{
					//write miss
			
					Port_Bus->wr_exclusive(local_id, addr, data);
			
					stats_writemiss(local_id);
					temp_WMiss++;
					Port_Bus->lock();
					wait(MAIN_MEM_LATENCY);
			
					replace_line = select_lru_line(set_no);
					m_data[set_no].line[replace_line].valid_flag = 1;
					m_data[set_no].line[replace_line].tag_field = tag_val;
					m_data[set_no].line[replace_line].word_data[byte_in_line] = rand();

					update_lru_counter(set_no, replace_line);
			 	
				}	
				
				wait(100);	
				Port_Bus->unlock();
				return 0;
		}
} // end of read_write_cache