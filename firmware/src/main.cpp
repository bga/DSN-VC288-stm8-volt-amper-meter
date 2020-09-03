//#define F_CPU 8000000UL

#include <stdint.h>
#include <stm8s.h>
#include <intrinsics.h>
#include <delay.h>

#define BAUDRATE (9600 / F_CPU_PRESCALER)
#include <uart.h>

#include <!cpp/bitManipulations.h>
#include <!cpp/Binary_values_8_bit.h>
#include <!cpp/newKeywords.h>

typedef FU16 TicksCount;
TicksCount getTicksCount();

#include <!mcu/ButtonManager.h>
//#include <!mcu/UartProcessor.h>

#include "_7SegmentsFont.h"

enum { ticksCountPerS = 8 * 256UL };
#define msToTicksCount(msArg) (ticksCountPerS * (msArg) / 1000UL)

//#undef clearBit
//#define clearBit(vArg, bitNumberArg) (__BRES((vArg), (bitNumberArg)), (vArg))
//#undef setBit
//#define setBit(vArg, bitNumberArg) (__BSET(&(vArg), (bitNumberArg)), (vArg))

#if 0
ISR(TIM4_ISR) {

}
#endif

#ifdef __ICCSTM8__
  #define ISR(vectorArg) PRAGMA(vector = vectorArg + 2) \
		__interrupt void _ ## vectorArg ## _vector (void)
#endif //# __ICCSTM8__

enum {
	digit0AnodeD = 5,
	digit1AnodeD = 6,
	digit2AnodeD = 4,
	digit3AnodeA = 1,
	digit4AnodeB = 4,
	digit2AnodeB = 5,
};
enum {
	digit0CathodeD = 3,
	digit1CathodeD = 4,
	digit2CathodeC = 3,
	digit3CathodeC = 6,
	digit4CathodeC = 7,
	digit5CathodeA = 1,
	digit6CathodeA = 2,
};

enum {
	voltageAdcChannelNo = 4,
	voltageAdcPortD = 3,
	currentAdcChannelNo = 3,
	currentAdcPortD = 2
};

#if 1
FU16 divmod10(FU16* in) {
	FU16 div = *in  / 10;
	FU16 mod = *in % 10;

	*in = div;
	return mod;
}
#else
FU16 divmod10(FU16& in) {
  // q = in * 0.8;
  FU16 q = (in >> 1) + (in >> 2);
  q = q + (q >> 4);
  q = q + (q >> 8);
//  q = q + (q >> 16);  // not needed for 16 bit version

  // q = q / 8;  ==> q =  in *0.1;
  q = q >> 3;

  // determine error
  FU16 r = in - ((q << 3) + (q << 1));   // r = in - q*10;
  FU16 div = q;
  FU16 mod = ((r > 9) ? ++div, r - 10 : r);

  in = div;

  return mod;
}
#endif // 1

void ADC_setChannel(FU8 channelNo) {
	setBitMaskedValues(ADC1_CSR, 0, 0x0F, channelNo);
}
FU16 ADC_read() {
	setBit(ADC1_CR1, ADC1_CR1_ADON);
	while (!(ADC1_CSR & _BV(ADC1_CSR_EOC)));
	U8 adcL = ADC1_DRL;
	U8 adcH = ADC1_DRH;
	clearBit(ADC1_CSR, ADC1_CSR_EOC);
	return (adcL | (adcH << 8));
}

void initAdc() {
	//# disable schmitt trigger
	setBit(ADC2_TDRL, currentAdcChannelNo);

	/* right-align data */
	ADC1_CR2 |= _BV(ADC1_CR2_ALIGN);

	/* wake ADC from power down */
	ADC1_CR1 |= _BV(ADC1_CR1_ADON);
}

typedef FU16 Temp;

Temp getCurrentTemp() {
	return 78;
}

void initTimer() {
	/* Prescaler = 128 */
//	TIM4_PSCR = B00000111;
	TIM4_PSCR = 7;

	enum {
		prescaler = 128,
		arr = F_CPU / ticksCountPerS / 2 / prescaler - 1
	};

	//TODO static_assert for IAR
	static_assert(0 < arr, "0 < TIM4_ARR");
	static_assert(arr < 256, "TIM4_ARR < 256");
	/* ticksCountPerS = F_CLK / (2 * prescaler * (1 + ARR))
	 *           = 2 MHz / (2 * 128 * (1 + 77)) = 100 Hz */
	TIM4_ARR = arr;

	setBit(TIM4_IER, TIM4_IER_UIE); // Enable Update Interrupt
	setBit(TIM4_CR1, TIM4_CR1_CEN); // Enable TIM4
}


struct Display {
	volatile FU8 displayChars[2][3];
	volatile FU8 currentDisplayIndex;

	void init() volatile {
		PA_DDR |= _BV(digit1AnodeD)
		#if NDEBUG
		| _BV(digit0AnodeD)
		#endif // NDEBUG
		;
		PC_DDR |= _BV(digit2AnodeD);

		PA_CR1 |= _BV(digit1AnodeD)
		#if NDEBUG
		| _BV(digit0AnodeD)
		#endif // NDEBUG
		;
		PC_CR1 |= _BV(digit2AnodeD);

		setBit(PD_DDR, digit0CathodeD);
		setBit(PD_DDR, digit1CathodeD);
		setBit(PC_DDR, digit2CathodeC);
		setBit(PC_DDR, digit3CathodeC);
		setBit(PC_DDR, digit4CathodeC);
		setBit(PA_DDR, digit5CathodeA);
		setBit(PA_DDR, digit6CathodeA);

		setBit(PD_ODR, digit0CathodeD);
		setBit(PD_ODR, digit1CathodeD);
		setBit(PC_ODR, digit2CathodeC);
		setBit(PC_ODR, digit3CathodeC);
		setBit(PC_ODR, digit4CathodeC);
		setBit(PA_ODR, digit5CathodeA);
		setBit(PA_ODR, digit6CathodeA);

	}

	void turnOffDisplay() volatile {
		//# do not touch SWIM during debug
		#if NDEBUG
			setBitValue(PD_ODR, digit0AnodeD, 0);
		#endif
		setBitValue(PA_ODR, digit1AnodeD, 0);
		setBitValue(PC_ODR, digit2AnodeD, 0);
	}

	void setDigit(FU8 digitIndex, FU8 digitBitsState) volatile {
		this->turnOffDisplay();

//	setBitMaskedValues(PD_DDR, digit0CathodeD, bitsCountToMask(2), digitBitsState >> 0);
//	setBitValue(PC_DDR, digit2CathodeC, (digitBitsState >> 2) & 1);
//	setBitMaskedValues(PC_DDR, digit3CathodeC, bitsCountToMask(2), digitBitsState >> 3);
//	setBitMaskedValues(PA_DDR, digit5CathodeA, bitsCountToMask(3), digitBitsState >> 5);

		digitBitsState = ~digitBitsState;
		setBitMaskedValues(PD_ODR, digit0CathodeD, bitsCountToMask(2), digitBitsState >> 0);
		setBitValue(PC_ODR, digit2CathodeC, (digitBitsState >> 2) & 1);
		setBitMaskedValues(PC_ODR, digit3CathodeC, bitsCountToMask(2), digitBitsState >> 3);
		setBitMaskedValues(PA_ODR, digit5CathodeA, bitsCountToMask(2), digitBitsState >> 5);


		switch(digitIndex) {
			// do not touch SWIM during debug
			case(0): setBitValue(PC_ODR, digit2AnodeD, 1); break;
			#if NDEBUG
				case(1): setBitValue(PD_ODR, digit0AnodeD, 1); break;
			#endif
			case(2): setBitValue(PA_ODR, digit1AnodeD, 1); break;
		}
	}

	void update() volatile {
		this->setDigit(this->currentDisplayIndex, this->displayChars[0][this->currentDisplayIndex]);
		this->currentDisplayIndex += 1;
		if(this->currentDisplayIndex == 3) this->currentDisplayIndex = 0;
	}

};

volatile Display display;
//Display display;

#if 0
//# ADC + buttons
void testAdc() {
	display.init();
	initAdc();
	//# pullup
	setBitValue(PC_CR1, voltageAdcPortD, 1);

	while (1) {
		ADC_setChannel(voltageAdcChannelNo);
		U16 buttonsAdc = ADC_read();
		FU8 buttonNo = 9;
		if(0) {  }
		else if(buttonsAdc < button0Threshold) {
			buttonNo = 0;
		}
		else if(buttonsAdc < button1Threshold) {
			buttonNo = 1;
		}
		else if(buttonsAdc < button2Threshold) {
			buttonNo = 2;
		}
		else if(buttonsAdc < button3Threshold) {
			buttonNo = 3;
		}
		display.setDigit(2, _7SegmentsFont::digits[buttonNo]);
		delay_ms(50);
	}
}
#endif // 0

#if 0
ISR(EXTI2_ISR) {

	//# debounce
	delay(2);

	ADC_setChannel(voltageAdcChannelNo);
	U16 buttonsAdc = ADC_read();
	ADC_setChannel(currentAdcChannelNo);
	FU8 buttonNo = 9;
	if(0) {  }
	else if(buttonsAdc < button0Threshold) {
		buttonNo = 0;
	}
	else if(buttonsAdc < button1Threshold) {
		buttonNo = 1;
	}
	else if(buttonsAdc < button2Threshold) {
		buttonNo = 2;
	}
	else if(buttonsAdc < button3Threshold) {
		buttonNo = 3;
	}
	display.setDigit(2, _7SegmentsFont::digits[buttonNo]);
}

//# ADC + buttons
void testAdcInterrupt() {
	display.init();
	initAdc();

	//# enable interrupt
	setBitValue(PC_CR2, voltageAdcPortD, 1);

	//# PORTC interrupt to rising and falling edge
	setBitMaskedValues(EXTI_CR1, 4, bitsCountToMask(2), 3);
//	EXTI_CR1_PDIS = 3;

	enable_interrupts();
	while (1) {
		delay(500);
		delay(500);

//		__wait_for_interrupt();
	}
}
#endif // 0

#if 0
volatile FU16 ticksCount = 0;
volatile FU8 buttonsDebounceCountdown = 0;

ISR(TIM4_ISR) {
	clearBit(TIM4_SR, TIM4_SR_UIF);

	ticksCount += 1;
	if(buttonsDebounceCountdown != 0) buttonsDebounceCountdown -= 1;
}


ISR(EXTI2_ISR) {
	buttonsDebounceCountdown = buttonsDebounceCountdownMax;
}

//# ADC + buttons
void testAdcInterruptAndTimer() {
	display.init();
	initAdc();
	initTimer();

	//# enable interrupt
	setBitValue(PC_CR2, voltageAdcPortD, 1);

	//# PORTC interrupt to rising and falling edge
	setBitMaskedValues(EXTI_CR1, 4, bitsCountToMask(2), 3);
//	EXTI_CR1_PDIS = 3;

	enable_interrupts();
	while (1) {
		if(buttonsDebounceCountdown == 1) {
			ADC_setChannel(voltageAdcChannelNo);
			U16 buttonsAdc = ADC_read();
			ADC_setChannel(currentAdcChannelNo);
			FU8 buttonNo = 9;
			if(0) {  }
			else if(buttonsAdc < button0Threshold) {
				buttonNo = 0;
			}
			else if(buttonsAdc < button1Threshold) {
				buttonNo = 1;
			}
			else if(buttonsAdc < button2Threshold) {
				buttonNo = 2;
			}
			else if(buttonsAdc < button3Threshold) {
				buttonNo = 3;
			}
			display.setDigit(2, _7SegmentsFont::digits[buttonNo]);
		}
		else {

		}

	}
}
#endif // 0

#if 1
volatile TicksCount liveTicksCount = 0;
volatile FU16 liveSecsCount = 0;
TicksCount ticksCount = 0;
FU16 secsCount = 0;
ISR(TIM4_ISR) {
	clearBit(TIM4_SR, TIM4_SR_UIF);

	liveTicksCount += 1;
	//TODO extra * 16
	if((liveTicksCount & bitsCountToMask(9 + 4)) == 0) {
		liveSecsCount += 1;
	}
}

volatile FU8 buttonsDebounceCountdown = 0;
ISR(EXTI2_ISR) {
	buttonsDebounceCountdown = buttonsDebounceCountdownMax;
}


TicksCount getTicksCount() {
	return ticksCount;
}

namespace UI {
	FU16 programNo = 0;
	FU16 programStartSecsCount;
	FU16 programSecsTime = 100;

	enum { programListSize = 128 };

	#if 1
	typedef FU16 TimeSec;
	typedef FU16 Temp;
	struct TempAndTime {
		enum {
			tempMultiprier = 2,
			timeMultiprier = 5,
		};

		enum {
			Separator_next = 0,
			Separator_end = 1,
		};

		union {
			struct {
				U8 markTimeSec;
				U8 temp;
			};
			U16 rawWord;
		}

		Temp get_temp() {
			return tempMultiprier * this->temp;
		}
		TimeSec get_markTimeSec() {
			return timeMultiprier * this->markTimeSec;
		}
		Bool isSeparator() {
			return this->rawWord == Separator_next;
		}
		Bool isEnd() {
			return this->rawWord == Separator_end;
		}
	};
	#elif 1
	struct TempAndTime {

//		#pragma pack(push, 1)
		union {
			struct {
				struct {
					unsigned int markTimeSec: 15;
					unsigned int temp: 9;
				};
				unsigned short _2bytes;
			};
			U8 bytes[3];
		};
//		#pragma pack(pop)
		Temp get_temp() {
			return this->temp;
		}
		TimeSec get_markTimeSec() {
			return this->markTimeSec;
		}
		Bool isSeparator() {
			return this->_2bytes == 0;
		}
	};
	#endif // 1



	typedef FU8 ProgramListIndex;
	struct UserData {
		FU8 lastProgramNo;

		TempAndTime programsFlatList[programListSize];
	};

	UserData userData;

	ProgramListIndex seekPrevProgramIndex(ProgramListIndex i) {
		while(0 <= i && !userData.programsFlatList[i].isSeparator()) {
			i -= 1;
		}
		return i;
	}
	ProgramListIndex seekNextProgramIndex(ProgramListIndex i) {
		while(i < programListSize && !userData.programsFlatList[i].isSeparator()) {
			i += 1;
		}
		return i;
	}
	Bool isProgramListEnd(ProgramListIndex i) {
		return userData.programsFlatList[i].isEnd();
	}

	ProgramListIndex currentProgramStartIndex;
	ProgramListIndex currentProgramIndex;

	enum Mode {
		Mode_programChoose = 0,
		Mode_start = 1,
		Mode_menu = 2,
	} mode = Mode_programChoose;

	struct PressDelayPressRepeatConfig {
		enum {
			multiPressDelay = msToTicksCount(1000UL),
			multiPressStep = msToTicksCount(500UL)
		};
	};
	struct UpButton: ButtonManager::PressDelayPressRepeat<PressDelayPressRepeatConfig> {
		void onPress() override {
			if(mode == Mode_programChoose) {
				if(!isProgramListEnd()) {
					programNo += 1;
					currentProgramIndex = seekNextProgramIndex(currentProgramIndex);
				}
			}
			else {

			}

		}
	} upButton;
	struct DownButton: ButtonManager::PressDelayPressRepeat<PressDelayPressRepeatConfig> {
		void onPress() override {
			if(mode == Mode_programChoose) {
				programNo -= 1;
			}
			else {

			}
		}
	} downButton;

	struct PressDelayLongPressConfig {
		enum {
			longPressDelay = msToTicksCount(3000UL)
		};
	};
	struct StartButton: ButtonManager::PressDelayLongPress<PressDelayLongPressConfig> {
		void onPress() override {
			if(mode == Mode_start) {
				mode = Mode_programChoose;
			}
			else if(mode == Mode_programChoose) {
				mode = Mode_start;
				programStartSecsCount = secsCount;
			}
		}
		void onLongPress() override {

		}
	} startButton;
	struct MunuButton: ButtonManager::PressDelayLongPress<PressDelayLongPressConfig> {
		void onPress() override {
//			programNo -= 1;
		}
		void onLongPress() override {

		}
	} menuButton;

	ButtonManager::Base *buttons[] = { &downButton, &upButton, &startButton, &menuButton };

		void displayDigit(FU16 x) {
		for(int i = 3; i--;) {
			display.displayChars[0][i] = _7SegmentsFont::digits[divmod10(&x)];
		}
	}

	void displayProgramNo() {
		displayDigit(programNo);
		display.displayChars[0][0] = _7SegmentsFont::P;
	}
	void displayProgramChoose() {
		displayProgramNo();
	}
	void displayStart() {
		//# display prorgam' no
		if(upButton.isHold()) {
			displayProgramNo();
		}
		//# display current temp
		else if(downButton.isHold()) {
			displayDigit(getCurrentTemp());
		}
		//# display prorgam' countdown
		else {
			displayDigit(programSecsTime -  (secsCount - programStartSecsCount));
		}
	}
	void displayMenu() {

	}

	typedef void (*DisplayFn)();

	DisplayFn displayFns[] = { &displayProgramChoose, &displayStart, &displayMenu };

	void displayAll() {
		displayFns[mode]();
	}
};

//# ADC + buttons
void testAdcInterruptAndTimer() {
	display.init();
	initAdc();
	initTimer();

	//# enable interrupt
	setBitValue(PC_CR2, voltageAdcPortD, 1);

	//# PORTC interrupt to rising and falling edge
	setBitMaskedValues(EXTI_CR1, 4, bitsCountToMask(2), 3);
//	EXTI_CR1_PDIS = 3;

	FU8 activeButtonNo = 0;

	enable_interrupts();
	loop {
		//# run each tick
		wait(ticksCount == liveTicksCount);
		//# fix time
		ticksCount = liveTicksCount;
		secsCount = liveSecsCount;

		forInc(FU8, i, 0, arraySize(UI::buttons)) {
			UI::buttons[i]->timerThread();
		}
//		display.setDigit(2, _7SegmentsFont::digits[UI::programNo % 10]);
//		display.displayChars[2] =  _7SegmentsFont::digits[UI::programNo % 10];
		UI::displayAll();
		display.update();

		//# buttons
		block {
			if(buttonsDebounceCountdown != 0) {
				buttonsDebounceCountdown -= 1;
			}
			else {

			}

			if(buttonsDebounceCountdown == 1) {
				ADC_setChannel(voltageAdcChannelNo);
				U16 buttonsAdc = ADC_read();
				ADC_setChannel(currentAdcChannelNo);
				FU8 buttonNo = 9;
				//# release
				if(adcMaxValue / 2 < buttonsAdc) {
//					activeButtonNo = 9;
					UI::buttons[activeButtonNo]->onUp();
				}
				//# press
				else {
					activeButtonNo = (buttonsAdc + adcStep - adcShift) / adcStep;
					UI::buttons[activeButtonNo]->onDown();
				}
			}
			else {

			}
		}

	}
}
#endif // 0



#if 0
void testDisplay() {
	display.init();

	FU8 counter = 0;
	while (1) {
		setDigit(counter % 3, _7SegmentsFont::digits[counter % 10]);
		counter += 1;
		delay_ms(50);
	}
}
#endif // 0

void displayUpdateThread() {
	if((ticksCount & bitsCountToMask(2)) == 0) {
		display.update();
	}
}

volatile FU16 countDown = 0;

#if 0
void buttonsThread() {
	if((ticksCount & bitsCountToMask(10)) == 0) {
		countDown += 1;
		FU16 t = countDown;
		display.displayChars[0][0] = _7SegmentsFont::digits[divmod10(t)];
		display.displayChars[0][2] = _7SegmentsFont::digits[divmod10(t)];
		display.displayChars[0][1] = _7SegmentsFont::digits[divmod10(t)];
	}
}

ISR(TIM4_ISR) {
	clearBit(TIM4_SR, TIM4_SR_UIF);

	ticksCount += 1;

	displayUpdateThread();
	buttonsThread();
}
#endif // 0


#if 0
volatile FU16 counter = 0;
volatile FU16 ticksCount = 0;

ISR(TIM4_ISR) {
	clearBit(TIM4_SR, TIM4_SR_UIF);

	ticksCount += 1;
	if(ticksCount & _BV(7)) {
		setDigit(2, _7SegmentsFont::digits[counter % 10]);
		counter += 1;
	}
	else {

	}
}

void testTimer() {
	display.init();

	/* Prescaler = 128 */
	TIM4_PSCR = B00000111;

	/* Frequency = F_CLK / (2 * prescaler * (1 + ARR))
	 *           = 2 MHz / (2 * 128 * (1 + 77)) = 100 Hz */
	TIM4_ARR = 7;

	setBit(TIM4_IER, TIM4_IER_UIE); // Enable Update Interrupt
	setBit(TIM4_CR1, TIM4_CR1_CEN); // Enable TIM4

	enable_interrupts();
	while (1) {
		// do nothing
	}
}
#endif // 1

#if 0
void main() {
	testTimer();
}
#endif

#if 1

/*
 * Redirect stdout to UART
 */
int putchar(int c) {
	uart_write(c);
	return 0;
}

/*
 * Redirect stdin to UART
 */
int getchar() {
	return uart_read();
}

ISR(UART1_RXC_ISR) {

}

void testUart() {
	uart_init();
	FU8 x = '0';
	while(1) {
//		uart_write(B01010101);
		uart_write(x);
		x = ((x == '9') ? '0' : x + 1);
		delay(25);
	}
}
#endif // 1

void main() {
	testUart();
	#if 0
	display.init();
	initTimer();

	display.displayChars[0][0] = _7SegmentsFont::digits[3];
	display.displayChars[0][1] = _7SegmentsFont::digits[3];
	display.displayChars[0][2] = _7SegmentsFont::digits[3];

	enable_interrupts();

	while(1) {

	}
	#endif // 0
}
