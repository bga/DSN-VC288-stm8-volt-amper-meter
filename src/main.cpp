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
#include <eeprom.h>

#ifdef __ICCSTM8__
  #define EEMEM __eeprom
#endif //# __ICCSTM8__

#include <!cpp/bitManipulations.h>
#include <!cpp/Binary_values_8_bit.h>
#include <!cpp/RunningAvg.h>
#include <!cpp/newKeywords.h>

#include "_7SegmentsFont.h"


enum { adcMaxBufferSize = 32 };

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

void ADC_init() {
	/* right-align data */
	setBit(ADC1_CR2, ADC1_CR2_ALIGN);

	//# ADC clock = fMasterClock / 18
	setBitMaskedValues(ADC1_CR1, 4, 0x07, 7);

	/* wake ADC from power down */
	setBit(ADC1_CR1, ADC1_CR1_ADON);
}

void ADC_initChannel(FU8 channelNo) {
	if(channelNo < 8) {
		setBit(ADC2_TDRL, channelNo);
	}
	else {
		setBit(ADC2_TDRH, channelNo - 8);
	}
}

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

void displayDecrimal(FU16 x, FU8* dest) {
	dest[2] = _7SegmentsFont::digits[divmod10(&x)];
	dest[1] = _7SegmentsFont::digits[divmod10(&x)];
	dest[0] = _7SegmentsFont::digits[divmod10(&x)];
}

void displayDecrimal6(FU16 x, FU8* dest) {
	dest[5] = _7SegmentsFont::digits[divmod10(&x)];
	dest[4] = _7SegmentsFont::digits[divmod10(&x)];
	dest[3] = _7SegmentsFont::digits[divmod10(&x)];
	dest[2] = _7SegmentsFont::digits[divmod10(&x)];
	dest[1] = _7SegmentsFont::digits[divmod10(&x)];
	dest[0] = _7SegmentsFont::digits[divmod10(&x)];
}

//# 0 < x <= 999(9.99V) => x.xx V
//# 1000(10.0V) <= x <= 9999(99.99V) => xx.x / 10 V
void displayVoltage(FU16 x, FU8* dest) {
	const FU16 xVal = x;
	(1000 < xVal) && (divmod10(&x));

	dest[2] = _7SegmentsFont::digits[divmod10(&x)];
	dest[1] = _7SegmentsFont::digits[divmod10(&x)];
	dest[0] = _7SegmentsFont::digits[divmod10(&x)];

	if(1000 < xVal) {
		dest[1] |= _7SegmentsFont::dot;
	}
	else {
		dest[0] |= _7SegmentsFont::dot;
	}
}

//# 0 < x <= 999(999mA) => xxx mA
//# 999(999mA) < x <= 9999(9.999A) => x.xx / 10 A
void displayCurrent(FU16 x, FU8* dest) {
	const FU16 xVal = x;
	(1000 < xVal) && (divmod10(&x));

	dest[2] = _7SegmentsFont::digits[divmod10(&x)];
	dest[1] = _7SegmentsFont::digits[divmod10(&x)];
	dest[0] = _7SegmentsFont::digits[divmod10(&x)];

	(1000 < xVal) && (dest[0] |= _7SegmentsFont::dot);
}

typedef U16 U16_16SubShift_Shift;
struct Settings {
	struct AdcUserFix {
		U16_16SubShift_Shift mul;
		U16_16SubShift_Shift add;
		FU16 fix(FU16 v, FU8 shift) const {
			return (FU32(v + this->add) * this->mul) >> shift;
		}
	};

	AdcUserFix voltageAdcFix;
	AdcUserFix currentAdcFix;
	U8 shift;
};

EEMEM const Settings defaultSettings = {
	.shift = 10,
//	.voltageAdcFix = { .mul = U16_16SubShift_Shift(1 << (16 - 4)), .add = 1 },
	.voltageAdcFix = { .mul = U16_16SubShift_Shift(1550), .add = 0 },
	.currentAdcFix = { .mul = U16_16SubShift_Shift(500), .add = U16_16SubShift_Shift(-5) }
};
Settings const& settings = ((Settings*)(&defaultSettings))[0];

enum { clockDivider = 3 };

void main() {
	RunningAvg<FU16[adcMaxBufferSize], FU32> voltageAdcRunningAvg;
	RunningAvg<FU16[adcMaxBufferSize], FU32> currentAdcRunningAvg;

	display.init();

	ADC_init();
	ADC_initChannel(currentAdcChannelNo);
	ADC_initChannel(voltageAdcChannelNo);

	FU16 ticksCount = 0;

	displayDecrimal(settings.voltageAdcFix.mul, &display.displayChars[0]);

	while(1) {
		if(ticksCount & bitsCountToMask(9 - clockDivider)) {
		}
		else {
			display.update();
		}

		#if 1
		//# run adc at middle of display update interval to reduce digital noise
		if((ticksCount & bitsCountToMask(16 - clockDivider)) != bitsCountToMask(8 - clockDivider)) {
		}
		else {
			ADC_setChannel(voltageAdcChannelNo);
			ADC_readStart();
			displayVoltage(settings.voltageAdcFix.fix(voltageAdcRunningAvg.computeAvg(), FU8(settings.shift)), &(display.displayChars[0]));
			FU16 voltageAdcValue = ADC_read();
			voltageAdcRunningAvg.add(voltageAdcValue);

			ADC_setChannel(currentAdcChannelNo);
			ADC_readStart();
			displayCurrent(settings.currentAdcFix.fix(currentAdcRunningAvg.computeAvg(), settings.shift), &(display.displayChars[3]));
			FU16 currentAdcValue = ADC_read();
			currentAdcRunningAvg.add(currentAdcValue);
		}
		#endif
		
		ticksCount += 1;

	}
}
