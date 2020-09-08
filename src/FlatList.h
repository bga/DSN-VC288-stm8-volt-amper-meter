#pragma once

//#define Self FlatList
template<class DataArg, class IndexArg> struct Self {
	typedef DataArg Data;
	typedef IndexArg Index;
	
	Data *data;
	Index dataLength;
	
	Index seekPos(int pos) {
		if(pos == 0) {
			return 0;
		} else {
			Index i = 0;
			while(i < this->dataLength) {
				if(this->data[i].isEnd()) {
					pos -= 1;
					if(pos == 0) {
						break;
					} else {
					
					}
				} else {
				
				}
				i += 1;
			}
		}
	}
	
}
//#undef Self
