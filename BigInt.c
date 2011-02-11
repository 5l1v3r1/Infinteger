/*
 *  BigInt.c
 *  Infinteger
 *
 *  Created by Alex Nichol on 2/9/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "BigInt.h"

// private adding method
char * addDecString (const char * string1, const char * string2);
char * reverse (const char * string);
void BigIntRemoveDecimalString (BigIntRef bi);

BigIntRef BigIntNew (BigIntDWORD initialValue) {
	
	// create from the binary, this should be simple
	BigIntRef bi = (BigIntRef)malloc(sizeof(_BigInt));
	bi->b10string = (char *)malloc(20);
	bi->bits = BitBufferNew(4);
	bi->flags = 0;
	bi->referenceCount = 1;
	bi->binDigits = 0;
	// copy the flags
	BitBuffer initValBuf = BitBufferNewBytes((Byte *)(&initialValue), 4);
	for (int i = 0; i < initValBuf->bitCount; i++) {
		UInt8 bit = BitBufferGetBit(initValBuf, i - (i % 8) + (7 - (i % 8)));
		BitBufferAddBit(bi->bits, bit);
		if (bit) {
			bi->binDigits = i + 1;
		}
	}
	BitBufferFree(initValBuf, BBYes);
	sprintf(bi->b10string, "%ld", (unsigned long)initialValue);
	return bi;
}

BigIntRef BigIntCreateCopy (BigIntRef bi, BigIntDWORD shift) {
	
	// create a copy, shifting up `shift' bits
	BigIntRef bir = (BigIntRef)malloc(sizeof(_BigInt));
	bir->binDigits = bi->binDigits + shift;
	bir->flags = bi->flags;
	bir->referenceCount = 1;
	bir->b10string = NULL;
	if (shift == 0) {
		// create an exact replica
		bir->bits = BitBufferNewBytes(bi->bits->bytes, bi->bits->byteCount);
		if (bi->b10string) {
			int btlength = strlen(bi->b10string);
			bir->b10string = (char *)malloc(btlength + 1);
			memcpy(bir->b10string, bi->b10string, btlength + 1);
		}
	} else {
		BitBuffer bb = BitBufferNew(bi->binDigits / 8 + 1);
		// write the zero bytes
		for (register int i = 0; i < shift; i++) {
			BitBufferAddBit(bb, 0);
		}
		for (register int i = 0; i < bi->binDigits; i++) {
			// add a bit from the original
			BitBufferAddBit(bb, BitBufferGetBit(bi->bits, i));
		}
		bir->bits = bb;
	}
	return bir;
}

BigIntRef BigIntCreateDecimal (const char * decstr) {
	
	// read each digit, multiplying something by ten
	// and adding the product of that and the digit
	// to the return value
	BigIntRef result = BigIntNew(0);
	BigIntRef digit = BigIntNew(1);
	BigIntRef base = BigIntNew(10);
	// call these so that we don't waste CPU
	BigIntRemoveDecimalString(digit);
	BigIntRemoveDecimalString(result);
	int len = strlen(decstr);
	for (int i = len - 1; i >= 0; i--) {
		char c = decstr[i];
		int digitVal = c - '0';
		if (digitVal < 0 || digitVal > 9) {
			BigIntRelease(result);
			BigIntRelease(digit);
			BigIntRelease(base);
			return NULL;
		}
		BigIntRef digitInt = BigIntNew(digitVal);
		BigIntRef adding = BigIntMultiply(digit, digitInt);
		BigIntRef newDigit = BigIntMultiply(digit, base);
		BigIntRelease(digit);
		digit = newDigit;
		BigIntRef newSum = BigIntAdd(adding, result);
		BigIntRelease(result);
		result = newSum;
		BigIntRelease(adding);
		BigIntRelease(digitInt);
	}
	BigIntRelease(digit);
	BigIntRelease(base);
	return result;
}

BigIntRef BigIntAdd (BigIntRef bi, BigIntRef bi2) {
	
	BigIntRef newInt = (BigIntRef)malloc(sizeof(_BigInt));
	// create a bitbuffer
	newInt->bits = BitBufferNew((bi->binDigits + bi2->binDigits) / 8 + 1);
	newInt->flags = 0;
	newInt->b10string = NULL;
	newInt->binDigits = 0;
	newInt->referenceCount = 1;
	BigBool carry = 0;
	// maximum answer length
	BigIntDWORD maxLength = bi->binDigits;
	if (maxLength < bi2->binDigits) maxLength = bi2->binDigits;
	
	for (int i = 0; i < maxLength + 1; i++) {
		UInt8 bit1 = 0;
		UInt8 bit2 = 0;
		UInt8 newBit = 0;
		
		//int i1 = (maxLength - bi2->binDigits) - (i + 1);
		//int i2 = (maxLength - bi2->binDigits) - (i + 1);
		int i1 = i;
		int i2 = i;
		
		if (i1 >= 0 && i1 < bi->bits->bitCount) bit1 = BitBufferGetBit(bi->bits, i1);
		if (i2 >= 0 && i2 < bi2->bits->bitCount) bit2 = BitBufferGetBit(bi2->bits, i2);
		// check for carry flag
		if (bit1 && bit2) {
			if (!carry) newBit = 0, carry = 1;
			else carry = 1, newBit = 1;
		} else if (bit1 || bit2) {
			// if we are not carrying,
			// make the next bit 1
			if (!carry) newBit = 1;
		} else {
			if (carry) {
				newBit = 1;
				carry = 0;
			}
		}
		if (newBit == 0 && i >= bi->bits->bitCount && i >= bi2->bits->bitCount) {
			// we have to break
			break;
		} else {
			BitBufferAddBit(newInt->bits, newBit);
		}
		if (newBit) {
			newInt->binDigits = i + 1;
		}
	}
	
	// precalculate the base10 string.
	// this will be removed at a later date.
	// if (bi->b10string && bi2->b10string)
	//	newInt->b10string = addDecString(bi->b10string, bi2->b10string);
	return newInt;
}
BigIntRef BigIntSubtract (BigIntRef bi, BigIntRef bi2) {
	// for now assume that bi > bi2
	BigBool carry = BBNo;
	BigIntRef difference = (BigIntRef)malloc(sizeof(_BigInt));
	difference->flags = 0;
	difference->bits = BitBufferNew((bi->binDigits / 8) + 1);
	difference->referenceCount = 1;
	difference->b10string = NULL;
	difference->binDigits = 0;
	register UInt8 outBit;
	for (int i = 0; i < bi->binDigits; i++) {
		UInt8 bit1 = BitBufferGetBit(bi->bits, i);
		UInt8 bit2 = 0;
		outBit = bit1;
		if (i < bi2->bits->bitCount) bit2 = BitBufferGetBit(bi2->bits, i);
		if (bit2) {
			// we need to change the output buffer
			if (bit1) {
				if (!carry) {
					outBit = 0;
				} else {
					// leave carry as yes
					outBit = 1;
				}
			} else {
				if (!carry) {
					outBit = 1;
					carry = BBYes;
				} else {
					outBit = 0;
				}
			}
		} else {
			if (bit1) {
				if (carry) {
					carry = BBNo;
					outBit = 0;
				}
			} else {
				if (carry) {
					outBit = 1;
				}
			}
		}
		// add the bit
		BitBufferAddBit(difference->bits, outBit);
		if (outBit) difference->binDigits = i + 1;
	}
	return difference;
}
BigIntRef BigIntMultiply (BigIntRef bi, BigIntRef bi2) {
	BigIntRef productSum = BigIntNew(0);
	BigIntRemoveDecimalString(productSum);
	// sum up each new big int and replace
	// the productSum
	for (int i = 0; i < bi2->binDigits; i++) {
		// if we have a positive number in bi2
		// then we have to add up or something
		if (BitBufferGetBit(bi2->bits, i)) {
			// add the shifted sum
			BigIntRef product = BigIntCreateCopy(bi, i);
			// add
			BigIntRef newSum = BigIntAdd(product, productSum);
			BigIntRemoveDecimalString(newSum);
			BigIntRelease(product);
			/*printf("%s + %s = %s\n", 
				   BigIntBase10Rep(product),
				   BigIntBase10Rep(productSum),
				   BigIntBase10Rep(newSum));*/
			BigIntRelease(productSum);
			productSum = newSum;
		}
	}
	return productSum;
}
BigIntRef BigIntDivide (BigIntRef bi, BigIntRef bi2) {
	return NULL;
}

BigBool BigIntOperatorCheck (BigIntRef bi1, BigIntOperator o, BigIntRef bi2) {
	switch (o) {
		case kBigIntEqual:
		{
			// check length
			if (bi1->binDigits != bi2->binDigits) return BBNo;
			// check bits
			for (int i = 0; i < bi1->binDigits; i++) {
				if (BitBufferGetBit(bi1->bits, i) != BitBufferGetBit(bi2->bits, i)) {
					return BBNo;
				}
			}
			return BBYes;
		}
		case kBigIntGreaterThan:
		{
			// compare
			if (BigIntOperatorCheck(bi1, kBigIntEqual, bi2)) return BBNo;
			if (bi1->binDigits > bi2->binDigits) return BBYes;
			if (bi1->binDigits < bi2->binDigits) return BBNo;
			for (int i = bi1->binDigits - 1; i >= 0; i--) {
				if (BitBufferGetBit(bi1->bits, i)) {
					if (!BitBufferGetBit(bi2->bits, i)) {
						return BBYes;
					}
				} else {
					if (BitBufferGetBit(bi2->bits, i)) {
						return BBNo;
					}
				}
			}
			break;
		}
		case kBigIntLessThan:
		{
			if (BigIntOperatorCheck(bi1, kBigIntEqual, bi2)) return BBNo;
			if (!BigIntOperatorCheck(bi1, kBigIntGreaterThan, bi2)) return BBYes;
			return BBNo;
			break;
		}
		default:
			break;
	}
	return BBNo;
}

BigBool BigIntFlagIsSet (BigIntRef bi, BigIntFlag flag) {
	UInt8 _flag = bi->flags & flag;
	if (_flag) return BBYes;
	return BBNo;
}
BigIntDWORD BigInt32BitInt (BigIntRef bi) {
	// go through bits and multiple and such
	BigIntDWORD dw = 0;
	for (int i = 0; i < bi->bits->bitCount; i++) {
		UInt8 bit = BitBufferGetBit(bi->bits, i);
		dw += ((unsigned long long)bit << i);
	}
	return dw;
}
const char * BigIntBase10Rep (BigIntRef bi) {
	if (!bi->b10string) {
		char * currentString = (char *)malloc(2);
		char * string = (char *)malloc(2);
		sprintf(string, "0");
		sprintf(currentString, "1");
		for (int i = 0; i < bi->binDigits; i++) {
			if (BitBufferGetBit(bi->bits, i)) {
				// add to our sum
				char * tmpString = addDecString(string, currentString);
				free(string);
				string = tmpString;
			}
			char * tmpCString = addDecString(currentString, currentString);
			free(currentString);
			currentString = tmpCString;
		}
		free(currentString);
		bi->b10string = string;
	}
	return bi->b10string;
}

BigIntRef BigIntRetain (BigIntRef bi) {
	bi->referenceCount += 1;
	return bi;
}
void BigIntRelease (BigIntRef bi) {
	bi->referenceCount -= 1;
	if (bi->referenceCount == 0) {
		// free it
		BitBufferFree(bi->bits, 1);
		if (bi->b10string) free(bi->b10string);
		free(bi);
	}
}

void BigIntRemoveDecimalString (BigIntRef bi) {
	if (bi->b10string) {
		free(bi->b10string);
		bi->b10string = NULL;
	}
}

char * reverse (const char * string) {
	int length = strlen(string);
	char * rev = (char *)malloc(length + 1);
	
	for (int i = 0; i < length; i++) {
		rev[length - (i + 1)] = string[i];
		// printf("Overwrite %d\n", length - (i + 1));
	}
	rev[length] = 0;
	return rev;
}

// private adding method
char * addDecString (const char * _string1, const char * _string2) {
	char * string1 = reverse(_string1);
	char * string2 = reverse(_string2);
	int l1 = strlen(string1), l2 = strlen(string2);
	int maxLength = l1;
	if (maxLength < l2) maxLength = l2;
	char * newDigits = (char *)malloc(maxLength + 2);
	bzero(newDigits, maxLength + 2);
	// loop through, and make sure to carry
	int carry = 0, i = 0;
	for (i = 0; i < maxLength; i++) {
		int n1 = 0;
		int n2 = 0;
		int answer = 0;
		// char to int conversion
		if (i < l1) n1 = string1[i] - '0';
		if (i < l2) n2 = string2[i] - '0';
		// add, and carry
		answer = n1 + n2 + carry;
		carry = 0;
		if (answer >= 10) {
			carry = answer / 10;
			answer = answer % 10;
		}
		sprintf(&newDigits[i], "%d", answer);
	}
	if (carry > 0) {
		sprintf(&newDigits[i], "%d", carry);
	}
	// reverse the string
	char * rev = reverse(newDigits);
	free(newDigits);
	free(string1);
	free(string2);
	return rev;
}