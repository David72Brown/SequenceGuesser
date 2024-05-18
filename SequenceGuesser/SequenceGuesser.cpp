/*
Improvements to make:

Function main() should be able to initiate a search of a certain string length and using the most common operators etc, then if that fails it starts a new search using sqrt() etc
and the search must only try formulas which include the new operators and at the new extended length.

Add missing values, possibly multiple ones, giving the formula a bye for generating them. For each correct formula, use it to predict the missing value.

To predict next value after last given value, regard that as a "missing value" in that end position.

Need new operators:
	- Modulo - should be easy.
	- Accessing individual digits of previous values in series e.g. digits counted from least-sig digit or digits from most-sig.
	- Sum and Product operators (Sigma and Pi).

Once got timings recorded for various sequences, try changing S to be just another unary operator, "S", which operates on the last value on the stack.
*/

#include "stdafx.h"
#include <assert.h>
#include <math.h>
#include <time.h>
#include <chrono>
#include <atlstr.h>
#include <iostream>
#include <string>
using namespace std;

typedef enum { CONSTANT, S, I, OPERATOR, NUM_ITEM_TYPES } eItemType; // Don't change order.
#define INC_TYPE(x) x = static_cast<eItemType>(1 + static_cast<int>(x))

#define MAX_POSS_ITEMS_IN_EXPRESSION	20	// Beyond that and it would probably take a very long time.
#define MAX_SEQ_LEN	50 // Number of numbers in a sequence, including missing ones in the middle or at the end.

static struct {
	int SeqLen;
	int Seq[MAX_SEQ_LEN];
} Samples[] = {
	{ 10,	1, 2, 3, 5, 8, 13, 21, 34, 55, 89 },		// Fibonacci series.
	{  7,	1, 2, 5, 13, 34, 89, 233 },					// Every other Fibonacci number.
	{  9,	1, 3, 7, 15, 31, 63, 127, 255, 511 },		// Number of moves required to solve Tower of Hanoi puzzle given how many discs are involved.
	{  6,	1, 36, 1225, 41616, 1413721, 48024900 },	// Numbers which are both square numbers and triangular numbers.
};
#define NUM_SAMPLES (sizeof(Samples)/sizeof(Samples[0]))

typedef double(*pOperatorFn) (double a, double b);
typedef struct {
	char		*Display;	// "+"
	int			NumOperands;
	pOperatorFn	pOpFunction;
	bool		Available;	// Whether we're using it in this attempt.
} sOperator;
double AddOpFn(double a, double b) { return a + b; }
double SubtractOpFn(double a, double b) { return a - b; }
double MultiplyOpFn(double a, double b) { return a * b; }
double DivideOpFn(double a, double b) { return a / b; }
double SquareOpFn(double a, double b) { return b * b; }
double CubeOpFn(double a, double b) { return b * b * b; }
double SqRootOpFn(double a, double b) { return sqrt(b); }
double DigitFromRightOpFn(double a, double b) { return int(a / pow(10, b)) % 10; }				// Digit 0 is units, 1 is tens, 2 is hundreds etc.
double DigitFromLeftOpFn(double a, double b) { return DigitFromRightOpFn(a, log10(a) - b); }	// Digit 0 is leftmost digit, 1 is next etc.

sOperator AddOp				= { "+", 2, AddOpFn };
sOperator SubtractOp		= { "-", 2, SubtractOpFn };
sOperator MultiplyOp		= { "*", 2, MultiplyOpFn };
sOperator DivideOp			= { "/", 2, DivideOpFn };
sOperator SquareOp			= { "^2", 1, SquareOpFn };
sOperator CubeOp			= { "^3", 1, CubeOpFn };
sOperator SqRootOp			= { "sqrt", 1, SqRootOpFn };
sOperator DigitFromRightOp	= { "rdigit", 2, DigitFromRightOpFn };
sOperator DigitFromLeftOp	= { "ldigit", 2, DigitFromLeftOpFn };

#define MAX_OP_CONSUMPTION 2 // The greatest number of operands any operator can take.
#define MAX_POSS_OPERATORS 20
sOperator *Operators[MAX_POSS_OPERATORS];

static struct {
	int MaxItemsInExpression;	// 1+2*3 makes 5 items.
	sOperator* Operators[MAX_POSS_OPERATORS];
	int MaxRetroS;	// S => x in S(i-x).
	int NumConstants; // 3 means we can use 1, 2 and 3. We never need 0 in a formula.
} Attempts[] = {
	// Start off with short strings and most likely operators, then check longer strings and more ops on later attempts.
	//	Max	Ops																														MaxRetro	Constants
	{	3,	{ &AddOp },																												2,			1	},
	{	3,	{ &AddOp, &SubtractOp },																								2,			2	},
	{	5,	{ &AddOp, &SubtractOp, &MultiplyOp, &DivideOp },																		2,			9	},
	{	10,	{ &AddOp, &SubtractOp, &MultiplyOp, &DivideOp, &SquareOp },																2,			2	},
	{	12,	{ &AddOp, &SubtractOp, &MultiplyOp, &DivideOp, &SquareOp },																2,			5	},
	{	20,	{ &AddOp, &SubtractOp, &MultiplyOp, &DivideOp, &SquareOp, &CubeOp, &SqRootOp },											4,			9	},
	{	20,	{ &AddOp, &SubtractOp, &MultiplyOp, &DivideOp, &SquareOp, &CubeOp, &SqRootOp, &DigitFromLeftOp, &DigitFromRightOp },	5,			9	}
};
#define NUM_ATTEMPTS (sizeof(Attempts) / sizeof(Attempts[0]))

typedef struct {
	eItemType	ItemType;
	int			Index;
} sItem;

static int GetStackHeightIncrease(eItemType e, int Index)
{
	if (e == OPERATOR)
	{
		return 1 - Operators[Index]->NumOperands;
	}
	else
	{
		return 1;
	}
}

static bool CheckOperatorValidHere(sItem *Items, int NumItems)
{
	// There is no point testing certain sequences since they will be covered by other, equivalent sequences.
	if (NumItems >= 3)
	{
		if (Items[NumItems - 3].ItemType == OPERATOR && Items[NumItems - 2].ItemType != OPERATOR) // Assume Items[NumItems-1] is an operator because of the name of this fn.
		{
			/* Various combinations of operator operand operator can be skipped. E.g. +x+ = x++ so don't check since "+ operand +" since it will be checked later as "operand + +".
				+x+ = x++
				-x- = x+-
				*x* = x**
				dxd = x*d   'd' means division sign. I can't type it properly because the compiler will think it's a C++ comment.
				+x- = x-+
				-x+ = x--
				*xd = xd*
				dx* = xdd
			*/
			if (Operators[Items[NumItems - 3].Index]->pOpFunction == AddOpFn || Operators[Items[NumItems - 3].Index]->pOpFunction == SubtractOpFn)
			{
				if (Operators[Items[NumItems - 1].Index]->pOpFunction == AddOpFn || Operators[Items[NumItems - 1].Index]->pOpFunction == SubtractOpFn)
					return false;
			}
			else if (Operators[Items[NumItems - 3].Index]->pOpFunction == MultiplyOpFn || Operators[Items[NumItems - 3].Index]->pOpFunction == DivideOpFn)
			{
				if (Operators[Items[NumItems - 1].Index]->pOpFunction == MultiplyOpFn || Operators[Items[NumItems - 1].Index]->pOpFunction == DivideOpFn)
					return false;
			}
		}
		if ((Operators[Items[NumItems - 1].Index] == &AddOp && MultiplyOp.Available)	// xx+ = x2*
			|| (Operators[Items[NumItems - 1].Index] == &MultiplyOp && SquareOp.Available))	// xx* = x squared
		{
			if (Items[NumItems - 3].ItemType != OPERATOR
			 && Items[NumItems - 3].ItemType == Items[NumItems - 2].ItemType
			 && Items[NumItems - 3].Index == Items[NumItems - 2].Index)
			{
				return false;
			}
		}
		// CONSTANT CONSTANT + [or - or *] (since equals a different constant)
		if (Items[NumItems - 3].ItemType == CONSTANT && Items[NumItems - 2].ItemType == CONSTANT
		&&	 (Operators[Items[NumItems - 1].Index] == &AddOp
		   || Operators[Items[NumItems - 1].Index] == &SubtractOp
		   || Operators[Items[NumItems - 1].Index] == &MultiplyOp))
		{
			return false;
		}
		// OPERAND SAME_OPERAND /      e.g. 5 5 / would be 5 divided by 5 = 1.
		if (Items[NumItems - 3].ItemType != OPERATOR && Items[NumItems - 2].ItemType == Items[NumItems - 3].ItemType
		 && Items[NumItems - 3].Index == Items[NumItems - 2].Index
		 && Operators[Items[NumItems - 1].Index] == &DivideOp)
		{
			return false;
		}
		// Should rule out one of ab+ and ba+ (and for *).
		// When we get to ba+ we know that ab+ will already have been done. Ordering of a and b is known from their item type and indices.
		if (Items[NumItems - 3].ItemType != OPERATOR
			&& Items[NumItems - 2].ItemType != OPERATOR
			&& (Operators[Items[NumItems - 1].Index] == &AddOp || Operators[Items[NumItems - 1].Index] == &MultiplyOp)
			&& (Items[NumItems - 3].ItemType > Items[NumItems - 2].ItemType || (Items[NumItems - 3].ItemType == Items[NumItems - 2].ItemType
				&& Items[NumItems - 3].Index > Items[NumItems - 2].Index)))
		{
			return false;
		}
		if (NumItems >= 4)
		{
			// OPERAND + SAME_OPERAND - [or swap + and -]
			// OPERAND * SAME_OPERAND / [or swap * and /]
			if (Items[NumItems - 4].ItemType != OPERATOR && Items[NumItems - 4].ItemType == Items[NumItems - 2].ItemType
				&& Items[NumItems - 4].Index == Items[NumItems - 2].Index)
			{
				if ((Operators[Items[NumItems - 3].Index] == &AddOp && Operators[Items[NumItems - 1].Index] == &SubtractOp)
					|| (Operators[Items[NumItems - 1].Index] == &AddOp && Operators[Items[NumItems - 3].Index] == &SubtractOp))
				{
					return false;
				}
				if ((Operators[Items[NumItems - 3].Index] == &MultiplyOp && Operators[Items[NumItems - 1].Index] == &DivideOp)
					|| (Operators[Items[NumItems - 1].Index] == &MultiplyOp && Operators[Items[NumItems - 3].Index] == &DivideOp))
				{
					return false;
				}
			}
		}
	}
	// Do this one last because it applies to the little-used square and sqrt ops so more efficient to check common ops above first.
	if (NumItems >= 2)
	{
		// sq sqrt or vice versa.
		if (Items[NumItems - 2].ItemType == OPERATOR
			&& ((Operators[Items[NumItems - 2].Index] == &SquareOp && Operators[Items[NumItems - 1].Index] == &SqRootOp)
				|| (Operators[Items[NumItems - 1].Index] == &SquareOp && Operators[Items[NumItems - 2].Index] == &SqRootOp)))
		{
			return false;
		}
	}
	return true;
}

static void SpitFormula(sItem Items[], int NumItems)
{
	int inum;

	cout << "If you can read reverse polish, each number S(i) in the series S is given by:" << endl;
	for (inum = 0; inum < NumItems; inum++)
	{
		switch (Items[inum].ItemType)
		{
		case OPERATOR:
			cout << " " << Operators[Items[inum].Index]->Display;
			break;
		case CONSTANT:
			cout << " " << (Items[inum].Index + 1);
			break;
		case I:
			cout << " i";
			break;
		case S:
			cout << " S(i-" << (Items[inum].Index + 1) << ")";
			break;
		default:
			assert(false);
		}
	}
	cout << endl;
}

static bool GuessSequence (int Seq[], int SeqLen,	int MaxItemsInExpression,	// 1+2*3 makes 5 items.
													int NumOperators,
													int MaxRetroS,				// How far back in the sequence you look. MaxRetroS == x means back as far as S(i-x).
													int NumConstants)			// I.e. we use 1, 2, 3 in expressions. NumConstants == 9 means we can use all 9 constants 1-9.
{
	// NumIndexValsForItem[] assumes the order of item types in eItemType. Not good coding but lends itself to this very fast method using a look-up table. The order is asserted in main().
	static int NoIndexForI = 0;
	static int* NumIndexValsForItem[NUM_ITEM_TYPES] = {
		&NumConstants,	// CONSTANT. See block comment above.
		&MaxRetroS,		// S. See block comment above.
		&NoIndexForI,	// I => index not used.
		&NumOperators	// OPERATOR.
	};
	sItem Items[MAX_POSS_ITEMS_IN_EXPRESSION];
	int StackHeight, sh, NumItems, i, inum;
	bool KeepChecking, ByeIntoNextInSeq, ItemValid, Ready;
	double NewNum;
	double Stack[MAX_POSS_ITEMS_IN_EXPRESSION];
	bool Success;
	eItemType NewItemType;

	assert(NumOperators >= 1 && MaxItemsInExpression <= MAX_POSS_ITEMS_IN_EXPRESSION);

	StackHeight = 0;
	NumItems = 0;

	// When we want to tack another item onto the end of the string, what is that item? Probably a constant but if no constants in use in this
	// attempt then it could be something else.
	if (NumConstants >= 1)
	{
		NewItemType = CONSTANT;
	}
	else if (MaxRetroS >= 1)
	{
		NewItemType = S;
	}
	else
	{
		NewItemType = I;
	}

	for (Success = false, ItemValid = true; !Success; )
	{
		// Enlarge the expression.
		if (ItemValid && NumItems < MaxItemsInExpression)
		{
			Items[NumItems].ItemType = NewItemType;
			Items[NumItems].Index = 0;
			NumItems += 1;
			StackHeight += GetStackHeightIncrease(Items[NumItems - 1].ItemType, Items[NumItems - 1].Index);
		}
		else
		{
			ItemValid = true;
			for (Ready = false; !Ready; )
			{
				Ready = true;
				StackHeight -= GetStackHeightIncrease(Items[NumItems - 1].ItemType, Items[NumItems - 1].Index);
				if (Items[NumItems - 1].Index < *NumIndexValsForItem[Items[NumItems - 1].ItemType] - 1)
				{
					Items[NumItems - 1].Index += 1;
				}
				else
				{
					// Already at last index for this item type so go to next type.
					if (Items[NumItems - 1].ItemType < NUM_ITEM_TYPES - 1)
					{
						INC_TYPE(Items[NumItems - 1].ItemType);
						Items[NumItems - 1].Index = 0;
					}
					else
					{
						// Already at last type so go back to previous item.
						if (NumItems == 1)
						{
							// Already come all the way back to first item and exhausted it, so finish.
							return Success;
						}
						NumItems -= 1;
						Ready = false;
					}
				}
			}
			StackHeight += GetStackHeightIncrease(Items[NumItems - 1].ItemType, Items[NumItems - 1].Index);
		}
		// Need to set ItemValid indicating whether this item can go here, with or without needing further items after it to create a valid formula,
		// E.g. S(i) = 1 i    is not worth checking because a valid solution would need to end up with a stack height of 1, but the 'i' is valid because
		//                    this formula might go on to become S(i) = 1 i +
		if (Items[NumItems - 1].ItemType != OPERATOR)
		{
			// If just added an operand, check stack height isn't so high that we can't come down to 1 by end of expression.
			ItemValid = StackHeight - (MaxItemsInExpression - NumItems) * (MAX_OP_CONSUMPTION - 1) <= 1;
			if (!ItemValid)
			{
				// If stack too high to take another operand, no point just trying another operand so skip them and go to the operators.
				StackHeight -= GetStackHeightIncrease(Items[NumItems - 1].ItemType, Items[NumItems - 1].Index);
				Items[NumItems - 1].ItemType = OPERATOR;
				Items[NumItems - 1].Index = 0;
				StackHeight += GetStackHeightIncrease(OPERATOR, 0);
				// Check for combinations which would be checked for at some other point.
				ItemValid = CheckOperatorValidHere(Items, NumItems);
			}
		}
		else if (Items[NumItems - 1].ItemType == OPERATOR)
		{
			// If just added an operator, check we have enough operands already on the stack for it to operate on.
			ItemValid = StackHeight >= Operators[Items[NumItems - 1].Index]->NumOperands;
			ItemValid = ItemValid && CheckOperatorValidHere(Items, NumItems);
		}
		if (ItemValid && StackHeight == 1)
		{
			// This might now equal S(i). Evaluate if expression true for all i.
			KeepChecking = true;
			// We loop until i == SeqLen, i.e. 1 more than you might expect, because
			// we use that last loop to generate the next number in the sequence after the numbers the user provided.
			for (i = 0; KeepChecking && i <= SeqLen; i++)
			{
				sh = 0;
				for (ByeIntoNextInSeq = false, inum = 0; !ByeIntoNextInSeq && inum < NumItems; inum++)
				{
					switch (Items[inum].ItemType)
					{
					case OPERATOR:
						NewNum = Operators[Items[inum].Index]->pOpFunction(Stack[sh - 2], Stack[sh - 1]);
						sh -= Operators[Items[inum].Index]->NumOperands;
						Stack[sh] = NewNum;
						sh += 1;
						break;
					case CONSTANT:
						Stack[sh] = Items[inum].Index + 1;
						sh += 1;
						break;
					case I:
						Stack[sh] = i;
						sh += 1;
						break;
					case S:
						if (i > Items[inum].Index)
						{
							Stack[sh] = Seq[i - Items[inum].Index - 1];
							sh += 1;
						}
						else
						{
							// Can't refer to S(i-3) if we're only 1 step into the sequence.
							// Skip this but keep checking rest of sequence.
							ByeIntoNextInSeq = true;
						}
						break;
					default:
						assert(false);
					}
				}
				if (!ByeIntoNextInSeq)
				{
					if (i < SeqLen)
					{
						if (Stack[0] != Seq[i])
						{
							// This formula failed to generate the correct number for one of the numbers in the sequence.
							KeepChecking = false;
						}
					}
					else
					{
						// i == SeqLen so we were using this loop to generate the next number in the series after the ones the user provided.
						// ^^^ Use printf until I figure out what type I want the Stack vals to be, so I can do it properly with cout.
						printf("Got it! The next number is %li\n", (long int) Stack[0]);
						SpitFormula(Items, NumItems);
						Success = true;
						KeepChecking = false;
					}
				}
			}
		}
	}
	return Success;
}

static unsigned int NoteTime(void)
{
	std::chrono::system_clock::time_point TimeNow;
	static time_t Last_tt, Next_tt;
	double Diff_tm;
	static bool BeenHereBefore = false;

	if (!BeenHereBefore)
	{
		TimeNow = std::chrono::system_clock::now();
		Last_tt = std::chrono::system_clock::to_time_t(TimeNow);
		BeenHereBefore = true;
		return 0;
	}
	TimeNow = std::chrono::system_clock::now();
	Next_tt = std::chrono::system_clock::to_time_t(TimeNow);
	Diff_tm = difftime(Next_tt, Last_tt);
	Last_tt = Next_tt;
	return (unsigned int)Diff_tm;
}

// Gets a sequence from the user and returns the number of numbers in it.
int GetSequenceFromUser(int Seq[])
{
	string s;
	char *p;
	char* pContext;
	int SampleNum, n;

	// Ask the user to provide a sequence or select one of the samples provided.
	cout << "Please enter a sequence of numbers separated by commas or whitespace," << endl;
	cout << "or a letter corresponding to one of the sample sequences provided below." << endl;
	cout << "or \"q\" or \"quit\" to quit." << endl << endl;
	for (SampleNum = 0; SampleNum < NUM_SAMPLES; SampleNum++)
	{
		cout << "   " << (char)('A' + SampleNum) << ".  " << Samples[SampleNum].Seq[0];
		for (n = 1; n < Samples[SampleNum].SeqLen; n++)
		{
			cout << ", " << Samples[SampleNum].Seq[n];
		}
		cout << endl;
	}
	cout << endl << endl;

	// Keep getting user input until we get a valid one.
	for (;;)
	{
		cin.clear();
		getline(cin, s, '\n');
		if (s[0] == 'Q' || s[0] == 'q')
			return 0;
		if (s.length() == 1)
		{
			s[0] = tolower(s[0]);
			if (s[0] >= 'a' && s[0] < 'a' + NUM_SAMPLES)
			{
				// User has selected one of the sample sequences by entering its letter.
				SampleNum = s[0] - 'a';
				int asdf = Samples[SampleNum].SeqLen * sizeof(Samples[SampleNum].Seq[0]);
				memcpy(Seq, Samples[SampleNum].Seq, Samples[SampleNum].SeqLen * sizeof(Samples[SampleNum].Seq[0]));
				return Samples[SampleNum].SeqLen;
			}
		}
		// User should have entered a sequence of numbers separated by commas or whitespace. Tokenize it into numbers.
		for (n = 0, p = strtok_s(&s[0], ", \t", &pContext); p != nullptr; p = strtok_s(nullptr, ", \t", &pContext))
		{
			Seq[n++] = atoi(p);
		}
		if (n > 1)
			return n;
		cout << "Invalid input. Please enter a letter e.g. \"A\" or a sequence of numbers e.g. \"1, 4, 9, 16, 25\"" << endl;
	}
}

int main()
{
	int a, NumOperators, SeqLen;
	bool Success;
	unsigned int ElapsedTime;
	int Seq[MAX_SEQ_LEN];

	// NumIndexValsForItem[] assumes the order of item types in eItemType. Not good coding but lends itself to this very fast method using a look-up table.
	// The order is asserted in here.
	assert(CONSTANT == 0 && S == 1 && I == 2 && OPERATOR == 3 && NUM_ITEM_TYPES == 4);

	cout << "=====================" << endl;
	cout << "=                   =" << endl;
	cout << "=  SEQUENCE GUESSER =" << endl;
	cout << "=                   =" << endl;
	cout << "=====================" << endl << endl;
	cout << "This program allows you to specify a sequence of numbers and it will guess the next number in the sequence." << endl << endl;

	// Keep asking user for more series to work with until they type "quit".
	for (	SeqLen = GetSequenceFromUser(Seq);
			SeqLen >= 1;
			SeqLen = GetSequenceFromUser(Seq))
	{
		cout << "Hmm. Let's try this..." << endl;
		for (a = 0; a < NUM_ATTEMPTS; a++)
		{
			for (NumOperators = 0; NumOperators < MAX_POSS_OPERATORS && Attempts[a].Operators[NumOperators]; NumOperators++)
			{
				Operators[NumOperators] = Attempts[a].Operators[NumOperators];
				Operators[NumOperators]->Available = true;
			}
			if (NumOperators < MAX_POSS_OPERATORS)
				Operators[NumOperators] = nullptr;
			NoteTime();
			Success = GuessSequence(Seq, SeqLen, Attempts[a].MaxItemsInExpression, NumOperators, Attempts[a].MaxRetroS, Attempts[a].NumConstants);
			ElapsedTime = NoteTime();
			if (Success)
			{
				// Use printf() because it supports the format specifiers we need.
				printf("Took me %02d:%02d:%02d.\n", ElapsedTime / 3600, ElapsedTime / 60 % 60, ElapsedTime % 60);
				break;
			}
			else
			{
				static string CuteComments[] = {
					"...That didn't work. I wonder if I should try...",
					"...No luck. What if I...",
					"...This is harder than I thought. What about...",
					"...Wow, I'm struggling here. No beaten yet though..."
				};
				cout << CuteComments[a % (sizeof(CuteComments) / sizeof(CuteComments[0]))] << endl;
			}
		}
		if (a == NUM_ATTEMPTS)
		{
			cout << "Gah! Sorry, I couldn't work out the next number. :-(" << endl;
		}
		cout << endl << endl << "---------------------------------------------------" << endl << endl;
	}
	return 0;
}
