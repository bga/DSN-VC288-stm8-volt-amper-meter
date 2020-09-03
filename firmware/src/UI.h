typedef FU16 Temp;

struct UI {
	enum Mode {
		Mode_simple, 
		Mode_program
	};
	
	struct Data {
//		enum Mode mode;
		
		union {
			struct {
				Temp maxTemp;
				TimeSec heatTime;
			}
		}
	}
	
	void onDecButtonPress() {
		
	}
	void onIncButtonPress() {
		
	}
	void onParamButtonPress() {
		
	}
	void onModeButtonPress() {
		
	}
	void onParamButtonLongPress() {
		
	}
	void onModeButtonLongPress() {
		
	}


};
