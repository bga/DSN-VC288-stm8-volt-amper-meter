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
#include <string.h>
#include <stm8s.h>
#include <intrinsics.h>
#include <eeprom.h>

#include <!cpp/bitManipulations.h>
#include <!cpp/Binary_values_8_bit.h>
#include <!cpp/RunningAvg.h>
#include <!cpp/newKeywords.h>

#include "_7SegmentsFont.h"

enum { ticksCountPerS = 6000UL };
#define msToTicksCount(msArg) (ticksCountPerS * (msArg) / 1000UL)

enum { adcMaxBufferSize = 32 };

enum { adcSpeedPrescaler = 7 };

volatile GPIO_TypeDef* const digit2CathodeGpioPort = (GPIO_TypeDef*)PD_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit1CathodeGpioPort = (GPIO_TypeDef*)PD_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit0CathodeGpioPort = (GPIO_TypeDef*)PD_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit5CathodeGpioPort = (GPIO_TypeDef*)PA_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit4CathodeGpioPort = (GPIO_TypeDef*)PB_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit3CathodeGpioPort = (GPIO_TypeDef*)PB_BASE_ADDRESS;

enum {
	digit2CathodeGpioPortBit = 5,
	digit1CathodeGpioPortBit = 6,
	digit0CathodeGpioPortBit = 4,
	digit5CathodeGpioPortBit = 1,
	digit4CathodeGpioPortBit = 4,
	digit3CathodeGpioPortBit = 5,
};

volatile GPIO_TypeDef* const digit0AnodeGpioPort = (GPIO_TypeDef*)PC_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit1AnodeGpioPort = (GPIO_TypeDef*)PC_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit2AnodeGpioPort = (GPIO_TypeDef*)PC_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit3AnodeGpioPort = (GPIO_TypeDef*)PC_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit4AnodeGpioPort = (GPIO_TypeDef*)PA_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit5AnodeGpioPort = (GPIO_TypeDef*)PA_BASE_ADDRESS;
volatile GPIO_TypeDef* const digit6AnodeGpioPort = (GPIO_TypeDef*)PD_BASE_ADDRESS;
volatile GPIO_TypeDef* const digitDotAnodeGpioPort = (GPIO_TypeDef*)PC_BASE_ADDRESS;

enum {
	digit0AnodeGpioPortBit = 6,
	digit1AnodeGpioPortBit = 7,
	digit2AnodeGpioPortBit = 3,
	digit3AnodeGpioPortBit = 4,
	digit4AnodeGpioPortBit = 3,
	digit5AnodeGpioPortBit = 2,
	digit6AnodeGpioAndSwimPortBit = 1,
	digitDotAnodeGpioPortBit = 5,
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
		struct F {
			static inline void initCathode(volatile GPIO_TypeDef* port, U8 bit) {
				setBit(port->DDR, bit);
				setBit(port->ODR, bit);
			}
			static inline void initAnode(volatile GPIO_TypeDef* port, U8 bit) {
				setBit(port->DDR, bit);
				setBit(port->CR1, bit);
			}
		};

		F::initCathode(digit0CathodeGpioPort, digit0CathodeGpioPortBit);
		F::initCathode(digit1CathodeGpioPort, digit1CathodeGpioPortBit);
		F::initCathode(digit2CathodeGpioPort, digit2CathodeGpioPortBit);
		F::initCathode(digit3CathodeGpioPort, digit3CathodeGpioPortBit);
		F::initCathode(digit4CathodeGpioPort, digit4CathodeGpioPortBit);
		F::initCathode(digit5CathodeGpioPort, digit5CathodeGpioPortBit);

		F::initAnode(digit0AnodeGpioPort, digit0AnodeGpioPortBit);
		F::initAnode(digit1AnodeGpioPort, digit1AnodeGpioPortBit);
		F::initAnode(digit2AnodeGpioPort, digit2AnodeGpioPortBit);
		F::initAnode(digit3AnodeGpioPort, digit3AnodeGpioPortBit);
		F::initAnode(digit4AnodeGpioPort, digit4AnodeGpioPortBit);
		F::initAnode(digit5AnodeGpioPort, digit5AnodeGpioPortBit);
		#if NDEBUG
			F::initAnode(digit6AnodeGpioPort, digit6AnodeGpioAndSwimPortBit);
		#endif // NDEBUG
		F::initAnode(digitDotAnodeGpioPort, digitDotAnodeGpioPortBit);
	}

	void turnOffDisplay() {
		//# do not touch SWIM during debug
		setBitValue(digit0CathodeGpioPort->ODR, digit0CathodeGpioPortBit, 1);
		setBitValue(digit1CathodeGpioPort->ODR, digit1CathodeGpioPortBit, 1);
		setBitValue(digit2CathodeGpioPort->ODR, digit2CathodeGpioPortBit, 1);
		setBitValue(digit5CathodeGpioPort->ODR, digit5CathodeGpioPortBit, 1);
		setBitValue(digit4CathodeGpioPort->ODR, digit4CathodeGpioPortBit, 1);
		setBitValue(digit3CathodeGpioPort->ODR, digit3CathodeGpioPortBit, 1);
	}

	void setDigit(FU8 digitIndex, FU8 digitBitsState) {
		this->turnOffDisplay();

//	setBitMaskedValues(PD_DDR, digit0CathodeD, bitsCountToMask(2), digitBitsState >> 0);
//	setBitValue(PC_DDR, digit2CathodeC, (digitBitsState >> 2) & 1);
//	setBitMaskedValues(PC_DDR, digit3CathodeC, bitsCountToMask(2), digitBitsState >> 3);
//	setBitMaskedValues(PA_DDR, digit5CathodeA, bitsCountToMask(3), digitBitsState >> 5);

//		digitBitsState = ~digitBitsState;
		#if 1
		setBitValue(digit0AnodeGpioPort->ODR, digit0AnodeGpioPortBit, hasBit(digitBitsState, 0));
		setBitValue(digit1AnodeGpioPort->ODR, digit1AnodeGpioPortBit, hasBit(digitBitsState, 1));
		setBitValue(digit2AnodeGpioPort->ODR, digit2AnodeGpioPortBit, hasBit(digitBitsState, 2));
		setBitValue(digit3AnodeGpioPort->ODR, digit3AnodeGpioPortBit, hasBit(digitBitsState, 3));
		setBitValue(digit4AnodeGpioPort->ODR, digit4AnodeGpioPortBit, hasBit(digitBitsState, 4));
		setBitValue(digit5AnodeGpioPort->ODR, digit5AnodeGpioPortBit, hasBit(digitBitsState, 5));
		// do not touch SWIM during debug
		#if NDEBUG
			setBitValue(digit6AnodeGpioPort->ODR, digit6AnodeGpioAndSwimPortBit, hasBit(digitBitsState, 6));
		#endif
		setBitValue(digitDotAnodeGpioPort->ODR, digitDotAnodeGpioPortBit, hasBit(digitBitsState, 7));

		#else
		setBitMaskedValues(digit0AnodeGpioPort->ODR, digit0AnodeGpioPortBit, bitsCountToMask(2), digitBitsState >> 0);
		setBitValue(digit2AnodeGpioPort->ODR, digit2AnodeGpioPortBit, (digitBitsState >> 2) & 1);
		setBitMaskedValues(digit4AnodeGpioPort->ODR, digit4AnodeGpioPortBit, bitsCountToMask(2), digitBitsState >> 3);
		setBitMaskedValues(digit3AnodeGpioPort->ODR, digit3AnodeGpioPortBit, bitsCountToMask(2), digitBitsState >> 5);
		#endif // 1

		switch(digitIndex) {
			case(0): setBitValue(digit0CathodeGpioPort->ODR, digit0CathodeGpioPortBit, 0); break;
			case(1): setBitValue(digit0CathodeGpioPort->ODR, digit1CathodeGpioPortBit, 0); break;
			case(2): setBitValue(digit2CathodeGpioPort->ODR, digit2CathodeGpioPortBit, 0); break;
			case(3): setBitValue(digit3CathodeGpioPort->ODR, digit3CathodeGpioPortBit, 0); break;
			case(4): setBitValue(digit4CathodeGpioPort->ODR, digit4CathodeGpioPortBit, 0); break;
			case(5): setBitValue(digit5CathodeGpioPort->ODR, digit5CathodeGpioPortBit, 0); break;
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
	forDec(int, i,  0, 3) {
		dest[i] = _7SegmentsFont::digits[divmod10(&x)];
	}
}

void displayDecrimal6(FU16 x, FU8* dest) {
	forDec(int, i,  0, 6) {
		dest[i] = _7SegmentsFont::digits[divmod10(&x)];
	}
}

//# 0 < x <= 999(9.99V) => x.xx V
//# 1000(10.0V) <= x <= 9999(99.99V) => xx.x / 10 V
void displayVoltage(FU16 x, FU8* dest) {
	const FU16 xVal = x;
	(1000 < xVal) && (divmod10(&x));

	forDec(int, i,  0, 3) {
		dest[i] = _7SegmentsFont::digits[divmod10(&x)];
	}

	if(1000 < xVal) {
		dest[1] |= _7SegmentsFont::dot;
	}
	else {
		dest[0] |= _7SegmentsFont::dot;
	}
}

void display_fixLastDigit(FU16 x, FU8* dest, void (*display)(FU16 x, FU8* dest)) {
	//# small value
	if(1000 <= x) {
		display(x, dest);
	}
	else {
		FU8 newDigits[3];
		display(x, newDigits);
		//# prevent display last digit small changes (ADC error) 
		if(newDigits[0] == dest[0] && newDigits[1] == dest[1]) {
			//# keep old digits
		}
		else {
			memcpy(dest, newDigits, 3);
		}
	}
}

//# 0 < x <= 999(999mA) => xxx mA
//# 999(999mA) < x <= 9999(9.999A) => x.xx / 10 A
void displayCurrent(FU16 x, FU8* dest) {
	const FU16 xVal = x;
	(1000 < xVal) && (divmod10(&x));

	forDec(int, i,  0, 3) {
		dest[i] = _7SegmentsFont::digits[divmod10(&x)];
	}

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

enum { clockDivider = 1 };

void Timer_init() {
	/* Prescaler = 128 */
//	TIM4_PSCR = B00000111;
	TIM4_PSCR = 5;

	enum {
		prescaler = 32,
		// arr = F_CPU / (1 << clockDivider ) / ticksCountPerS / 2 / prescaler - 1
		arr = 180
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

RunningAvg<FU16[adcMaxBufferSize], FU32> voltageAdcRunningAvg;
RunningAvg<FU16[adcMaxBufferSize], FU32> currentAdcRunningAvg;

FU16 ticksCount = 0;

ISR(TIM4_ISR) {
	clearBit(TIM4_SR, TIM4_SR_UIF);

	ticksCount += 1;

	display.update();

	#if 1
	if((ticksCount & bitsCountToMask(adcSpeedPrescaler))) {
	}
	else {
		if((ticksCount & bitsCountToMask(1 + adcSpeedPrescaler))) {
			ADC_setChannel(voltageAdcChannelNo);
			ADC_readStart();
			display_fixLastDigit(settings.voltageAdcFix.fix(voltageAdcRunningAvg.computeAvg(), FU8(settings.shift)), &(display.displayChars[0]), displayVoltage);
			FU16 voltageAdcValue = ADC_read();
			voltageAdcRunningAvg.add(voltageAdcValue);
		}
		else {
			ADC_setChannel(currentAdcChannelNo);
			ADC_readStart();
			display_fixLastDigit(settings.currentAdcFix.fix(currentAdcRunningAvg.computeAvg(), settings.shift), &(display.displayChars[3]), displayCurrent);
			FU16 currentAdcValue = ADC_read();
			currentAdcRunningAvg.add(currentAdcValue);
		}
	}
	#endif
}

void Clock_setCpuFullSpeed() {
	CLK_CKDIVR = 0;
}

void Hw_enable() {
	enum { 
		CLK_PCKENR1_TIM4 = 4 
	};
	CLK_PCKENR1 = _BV(CLK_PCKENR1_TIM4);
	
	enum { 
		CLK_PCKENR12_ADC = 3 
	};
	CLK_PCKENR2 = _BV(CLK_PCKENR12_ADC);
}

void main() {

	Clock_setCpuFullSpeed();
	Hw_enable();
	display.init();

	forInc(FU8, i, 0, 6) {
		display.displayChars[i] = _7SegmentsFont::digits[i];
	}

	ADC_init();
	ADC_initChannel(currentAdcChannelNo);
	ADC_initChannel(voltageAdcChannelNo);

	Timer_init();
	enable_interrupts();

	while(1) __wait_for_interrupt();
}
