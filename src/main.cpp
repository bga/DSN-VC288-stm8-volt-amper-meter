/*
  Copyright 2020 Bga <bga.email@gmail.com>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

//#define F_CPU 8000000UL

#include <stdint.h>
#include <stm8s.h>
#include <intrinsics.h>
#include <delay.h>

#define BAUDRATE (9600 / F_CPU_PRESCALER)
#include <uart.h>

#include <!cpp/bitManipulations.h>
#include <!cpp/Binary_values_8_bit.h>
#include <!cpp/RunningAvg.h>
#include <!cpp/newKeywords.h>

typedef FU16 TicksCount;
TicksCount getTicksCount();

#include <!mcu/ButtonManager.h>
//#include <!mcu/UartProcessor.h>

#include "_7SegmentsFont.h"

enum { ticksCountPerS = 8 * 256UL };
#define msToTicksCount(msArg) (ticksCountPerS * (msArg) / 1000UL)

enum { adcMaxBufferSize = 16 };

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
	digit2Cathode_D = 5,
	digit1Cathode_D = 6,
	digit0Cathode_D = 4,
	digit5Cathode_A = 1,
	digit4Cathode_B = 4,
	digit3Cathode_B = 5,
};
enum {
	digit0Anode_C = 6,
	digit1Anode_C = 7,
	digit2Anode_C = 3,
	digit3Anode_C = 4,
	digit4Anode_A = 3,
	digit5Anode_A = 2,
	digit6Anode_DAndSwim = 1,
	digitDotAnode_C = 5,
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

void ADC_readStart() {
	setBit(ADC1_CR1, ADC1_CR1_ADON);
}

FU16 ADC_read() {
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
	FU8 displayChars[3 + 3];
	FU8 currentDisplayIndex;

	void init() {
		#define initAnode(portLetterArg, bitNoArg) setBit(CONCAT(CONCAT(P, portLetterArg), _DDR), bitNoArg); setBit(CONCAT(CONCAT(P, portLetterArg), _CR1), bitNoArg);
		#define initCathode(portLetterArg, bitNoArg) setBit(CONCAT(CONCAT(P, portLetterArg), _DDR), bitNoArg); setBit(CONCAT(CONCAT(P, portLetterArg), _ODR), bitNoArg);

		initCathode(D, digit0Cathode_D);
		initCathode(D, digit1Cathode_D);
		initCathode(D, digit2Cathode_D);
		initCathode(B, digit3Cathode_B);
		initCathode(B, digit4Cathode_B);
		initCathode(A, digit5Cathode_A);

		initAnode(C, digit0Anode_C);
		initAnode(C, digit1Anode_C);
		initAnode(C, digit2Anode_C);
		initAnode(C, digit3Anode_C);
		initAnode(A, digit4Anode_A);
		initAnode(A, digit5Anode_A);
		#if NDEBUG
			initAnode(D, digit6Anode_DAndSwim);
		#endif // NDEBUG
		initAnode(C, digitDotAnode_C);

		#undef initAnode
		#undef initCathode
	}

	void turnOffDisplay() {
		//# do not touch SWIM during debug
		setBitValue(PD_ODR, digit0Cathode_D, 1);
		setBitValue(PD_ODR, digit1Cathode_D, 1);
		setBitValue(PD_ODR, digit2Cathode_D, 1);
		setBitValue(PA_ODR, digit5Cathode_A, 1);
		setBitValue(PB_ODR, digit4Cathode_B, 1);
		setBitValue(PB_ODR, digit3Cathode_B, 1);
	}

	void setDigit(FU8 digitIndex, FU8 digitBitsState) {
		this->turnOffDisplay();

//	setBitMaskedValues(PD_DDR, digit0CathodeD, bitsCountToMask(2), digitBitsState >> 0);
//	setBitValue(PC_DDR, digit2CathodeC, (digitBitsState >> 2) & 1);
//	setBitMaskedValues(PC_DDR, digit3CathodeC, bitsCountToMask(2), digitBitsState >> 3);
//	setBitMaskedValues(PA_DDR, digit5CathodeA, bitsCountToMask(3), digitBitsState >> 5);

//		digitBitsState = ~digitBitsState;
		#if 1
		setBitValue(PC_ODR, digit0Anode_C, hasBit(digitBitsState, 0));
		setBitValue(PC_ODR, digit1Anode_C, hasBit(digitBitsState, 1));
		setBitValue(PC_ODR, digit2Anode_C, hasBit(digitBitsState, 2));
		setBitValue(PC_ODR, digit3Anode_C, hasBit(digitBitsState, 3));
		setBitValue(PA_ODR, digit4Anode_A, hasBit(digitBitsState, 4));
		setBitValue(PA_ODR, digit5Anode_A, hasBit(digitBitsState, 5));
		// do not touch SWIM during debug
		#if NDEBUG
			setBitValue(PD_ODR, digit6Anode_DAndSwim, hasBit(digitBitsState, 6));
		#endif
		setBitValue(PC_ODR, digitDotAnode_C, hasBit(digitBitsState, 7));

		#else
		setBitMaskedValues(PD_ODR, digit0Anode_C, bitsCountToMask(2), digitBitsState >> 0);
		setBitValue(PC_ODR, digit2Anode_C, (digitBitsState >> 2) & 1);
		setBitMaskedValues(PC_ODR, digit4Anode_A, bitsCountToMask(2), digitBitsState >> 3);
		setBitMaskedValues(PA_ODR, digit3Anode_C, bitsCountToMask(2), digitBitsState >> 5);
		#endif // 1

		switch(digitIndex) {
			case(0): setBitValue(PD_ODR, digit0Cathode_D, 0); break;
			case(1): setBitValue(PD_ODR, digit1Cathode_D, 0); break;
			case(2): setBitValue(PD_ODR, digit2Cathode_D, 0); break;
			case(3): setBitValue(PB_ODR, digit3Cathode_B, 0); break;
			case(4): setBitValue(PB_ODR, digit4Cathode_B, 0); break;
			case(5): setBitValue(PA_ODR, digit5Cathode_A, 0); break;
		}
	}

	void update() {
		this->setDigit(this->currentDisplayIndex, this->displayChars[this->currentDisplayIndex]);
		this->currentDisplayIndex += 1;
		if(this->currentDisplayIndex == 6) this->currentDisplayIndex = 0;
	}

};

Display display;

#if 0

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

void displayDecrimal(FU16 x, FU8* dest) {
	dest[2] = _7SegmentsFont::digits[divmod10(&x)];
	dest[1] = _7SegmentsFont::digits[divmod10(&x)];
	dest[0] = _7SegmentsFont::digits[divmod10(&x)];
}

typedef FU16 U1_15;
struct Settings {
	struct AdcUserFix {
		U1_15 mul;
		U1_15 add;
		FU16 fix(FU16 v) {
			return (v * this->mul + this->add) >> 15;
		}
	};

	AdcUserFix voltageAdcFix;
	AdcUserFix currentAdcFix;
};

Settings settings;
const Settings defaultSettings = { .voltageAdcFix = { .mul = U1_15(1 << 15), .add = 0 } };

void main() {
	settings = defaultSettings;

	RunningAvg<FU16[adcMaxBufferSize], FU32> voltageAdcRunningAvg;
	RunningAvg<FU16[adcMaxBufferSize], FU32> currentAdcRunningAvg;

	display.init();
	initAdc();
	FU16 counter = 0;
	FU16 ticksCount = 0;

	while(1) {
		if(ticksCount & bitsCountToMask(6)) {
		}
		else {
			display.update();
		}

		if((ticksCount & bitsCountToMask(12))) {
		}
		else {
			ADC_setChannel(voltageAdcChannelNo);
			ADC_readStart();
			displayDecrimal(settings.voltageAdcFix.fix(voltageAdcRunningAvg.computeAvg()), &(display.displayChars[0]));
			FU16 voltageAdcValue = ADC_read();
			voltageAdcRunningAvg.add(voltageAdcValue);
		}

		if(0 && (ticksCount & bitsCountToMask(10)) == 0) {
			displayDecrimal(counter, &(display.displayChars[0]));
			displayDecrimal(counter + 1, &(display.displayChars[3]));
//			display.displayChars[5] = bitRotate(display.displayChars[5], 1);
			counter += 1;
		}
		ticksCount += 1;

	}
}
